/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2016 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#include <stdio.h>
#include "jlog.hpp"
#include "jfile.hpp"
#include "jargv.hpp"
#include "build-config.h"

#include "esdlcmd_common.hpp"
#include "esdlcmd_core.hpp"

#include "esdl2ecl.cpp"
#include "esdl-publish.cpp"
#include "xsdparser.hpp"

StringBuffer &getEsdlCmdComponentFilesPath(StringBuffer & path)
{
    if (getComponentFilesRelPathFromBin(path))
        return path;
    return path.set(COMPONENTFILES_DIR);
}


class EsdlDiffTemplateCmd : public EsdlHelperConvertCmd
{
public:
    EsdlDiffTemplateCmd() : optRawOutput(false), optFlags(DEPFLAG_COLLAPSE|DEPFLAG_ARRAYOF){}

    virtual bool parseCommandLineOptions(ArgvIterator &iter)
    {
        if (iter.done())
        {
            usage();
            return false;
        }

        for (; !iter.done(); iter.next())
        {
            const char *arg = iter.query();
            if (*arg != '-')
            {
                if (optSource.isEmpty())
                    optSource.set(arg);
                else if (optService.isEmpty())
                    optService.set(arg);
                else if (optMethod.isEmpty())
                    optMethod.set(arg);
                else
                {
                    fprintf(stderr, "\nunrecognized argument: %s\n", arg);
                    usage();
                    return false;
                }
            }
            else
            {
                if (iter.matchOption(optXsltPath, ESDLOPT_XSLT_PATH))
                    continue;
                if (iter.matchOption(optPreprocessOutputDir, ESDLOPT_PREPROCESS_OUT))
                    continue;
                if (EsdlConvertCmd::parseCommandLineOption(iter))
                    continue;
                if (EsdlConvertCmd::matchCommandLineOption(iter, true)!=EsdlCmdOptionMatch)
                    return false;
            }
        }

        return true;
    }

    virtual bool finalizeOptions(IProperties *globals)
    {
        if (optSource.isEmpty())
        {
            usage();
            throw( MakeStringException(0, "\nError: Source esdl parameter required\n"));
        }

        if( optService.isEmpty() )
        {
            usage();
            throw( MakeStringException(0, "A service name must be provided") );
        }

        if( optMethod.isEmpty() )
        {
            usage();
            throw( MakeStringException(0, "A method name must be provided") );
        }

        if (optXsltPath.isEmpty())
        {
            StringBuffer tmp;
            if (getComponentFilesRelPathFromBin(tmp))
                optXsltPath.set(tmp.str());
            else
                optXsltPath.set(COMPONENTFILES_DIR);
        }

        fullxsltpath.set(optXsltPath);
        fullxsltpath.append("/xslt/esxdl2xsd.xslt");

        if (!optPreprocessOutputDir.isEmpty())
            optRawOutput = true;

        return true;
    }

    virtual void doTransform(IEsdlDefObjectIterator& objs, StringBuffer &target, double version=0, IProperties *opts=NULL, const char *ns=NULL, unsigned flags=0 )
    {
        TimeSection ts("transforming via XSLT");
        cmdHelper.defHelper->toXSD( objs, target, EsdlXslToXsd, 0, opts, nullptr, optFlags );
    }

    virtual void loadTransform( StringBuffer &xsltpath, IProperties *params)
    {
        TimeSection ts("loading XSLT");
        cmdHelper.defHelper->loadTransform( xsltpath, params, EsdlXslToXsd );
    }

    virtual void setTransformParams(IProperties *params )
    {
        cmdHelper.defHelper->setTransformParams(EsdlXslToXsd, params);
    }

    static IXmlSchema* createXmlSchema(const char* schema)
    {
        const char* name = SharedObjectPrefix "xmllib" SharedObjectExtension;
        HINSTANCE xmllib = LoadSharedObject(name,true,false);
        if (!LoadSucceeded(xmllib))
            throw MakeStringException(-1,"load %s failed with code %d", name, GetSharedObjectError());
        typedef IXmlSchema* (*XmlSchemaCreator)(const char*);
        XmlSchemaCreator creator = (XmlSchemaCreator)GetSharedProcedure(xmllib, "createXmlSchemaFromString");
        if (!creator)
            throw MakeStringException(-1,"load XmlSchema factory failed: createXmlSchemaFromString()");

        return creator(schema);
    }

    void genDiffTemplateXml(StringStack& parent, IXmlType* type, StringBuffer& out, const char* tag, const char* ns=NULL)
    {
        assertex(type!=NULL);

        const char* typeName = type->queryName();

        if (type->isComplexType())
        {
            if (typeName && std::find(parent.begin(),parent.end(),typeName) != parent.end())
                return;

            out.appendf("<%s", tag);
            if (ns)
                out.append(' ').append(ns);
            for (unsigned i=0; i<type->getAttrCount(); i++)
            {
                IXmlAttribute* attr = type->queryAttr(i);
                out.appendf(" %s='", attr->queryName());
                out.append('\'');
            }
            out.append('>');
            if (typeName)
                parent.push_back(typeName);

            int flds = type->getFieldCount();

            switch (type->getSubType())
            {
            case SubType_Complex_SimpleContent:
                assertex(flds==0);
                break;

            default:
                for (int idx=0; idx<flds; idx++)
                    genDiffTemplateXml(parent,type->queryFieldType(idx),out,type->queryFieldName(idx));
                break;
            }

            if (typeName)
                parent.pop_back();
            out.appendf("</%s>",tag);
        }
        else if (type->isArray())
        {
            if (typeName && std::find(parent.begin(),parent.end(),typeName) != parent.end())
                return; // recursive

            const char* itemName = type->queryFieldName(0);
            IXmlType*   itemType = type->queryFieldType(0);
            if (!itemName || !itemType)
                throw MakeStringException(-1,"*** Invalid array definition: tag=%s, itemName=%s", tag, itemName?itemName:"NULL");

            StringBuffer item;
            if (typeName)
                parent.push_back(typeName);
            genDiffTemplateXml(parent, itemType, item, itemName);
            if (typeName)
                parent.pop_back();

            if (itemType->isComplexType())
                out.appendf("<%s diff:key=''>%s</%s>", tag,item.str(),tag);
            else
                out.appendf("<%s simple='true'>%s</%s>", tag,item.str(),tag);
        }
        else // simple type
        {
            out.appendf("<%s/>", tag);
        }
    }

    void generateDiffTemplate(const char *serv, const char *method, const char * schemaxml)
    {
        VStringBuffer element("%sResponse", method);

        Owned<IXmlSchema> schema = createXmlSchema(schemaxml);
        if (!schema.get())
            throw MakeStringException(-1,"Unknown type: %s", element.str());
        IXmlType* type = schema->queryTypeByName(element);
        if (!type)
            throw MakeStringException(-1,"Unknown type: %s", element.str());
        StringBuffer xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<ResultDiff>\n");
        StringBuffer ns("xmlns=\"");
        generateNamespace(ns).append('\"');

        StringStack parent;
        genDiffTemplateXml(parent,type, xml, element, nullptr);
        xml.append("\n</ResultDiff>");
        Owned<IPropertyTree> pt = createPTreeFromXMLString(xml, ipt_ordered);
        saveXML("difftemplate.xml", pt, 2);
    }

    virtual int processCMD()
    {
        loadServiceDef();

        Owned<IEsdlDefObjectIterator> structs = cmdHelper.esdlDef->getDependencies( optService.get(), optMethod.get(), ESDLOPTLIST_DELIMITER, 0, nullptr, optFlags );

        if( optRawOutput )
            outputRaw(*structs);

        if(optXsltPath.isEmpty())
            throw( MakeStringException(0, "Path to /xslt/esxdl2xsd.xslt is empty, cannot perform transform.") );
        createParams();

        loadTransform( fullxsltpath, params);
        StringBuffer xsd;
        doTransform( *structs, xsd, 0, nullptr, nullptr, optFlags);
        DBGLOG("\nXSD:\n%s\n\n", xsd.str());
        generateDiffTemplate(optService, optMethod, xsd);

        return 0;
    }

    void printOptions()
    {
        puts("Options:");
        puts("  --xslt <xslt file path> : Path to '/xslt/esxdl2xsd.xslt' file to transform EsdlDef to XSD\n" );
        puts("  --preprocess-output <raw output directory> : Output pre-processed xml file to specified directory before applying XSLT transform\n" );
        puts("  --annotate <all | none> : Flag turning on either all annotations or none. By default annotations are generated " );
        puts("                    for Enumerations. Setting the flag to 'none' will disable even those. Setting it" );
        puts("                    to 'all' will enable additional annotations such as collapsed, cols, form_ui, html_head and rows.\n");
    }

    virtual void usage()
    {
        puts("Usage:");
        puts("esdl ecl-diff-template sourcePath serviceName methodname [options]\n" );
        puts("\nsourcePath must be absolute path to the ESDL Definition file containing the" );
        puts("EsdlService definition for the service you want to work with.\n" );
        puts("serviceName EsdlService definition for the service you want to work with.\n" );

        printOptions();
        EsdlConvertCmd::usage();
    }

    virtual void outputRaw( IEsdlDefObjectIterator& obj)
    {
        if( optRawOutput )
        {
            StringBuffer xmlOut;
            StringBuffer empty;

            xmlOut.appendf( "<esxdl name=\"%s\">", optService.get());
            cmdHelper.defHelper->toXML( obj, xmlOut, 0, nullptr, optFlags );
            xmlOut.append("</esxdl>");

            saveAsFile( optPreprocessOutputDir.get(), empty, xmlOut.str(), NULL );
        }
    }

    void createParams()
    {
        params.set(createProperties());
        generateNamespace(tns);
        params->setProp( "tnsParam", tns );
    }

    virtual void loadServiceDef()
    {
        cmdHelper.loadDefinition(optSource, optService, 0);
    }

    virtual void outputToFile()
    {
        if (!optOutDirPath.isEmpty())
        {
            StringBuffer filename;
            generateOutputFileName(filename);
            saveAsFile(optOutDirPath.get(), filename, outputBuffer.str(), NULL);
        }
    }

    StringBuffer & generateNamespace(StringBuffer &ns)
    {
        ns.appendf("http://webservices.seisint.com/%s", optService.get());
        return ns.toLowerCase();
    }

    virtual StringBuffer & generateOutputFileName( StringBuffer &filename)
    {
        filename.appendf("%s", optService.get());
        if (!optMethod.isEmpty() && !strstr(optMethod.get(), ESDLOPTLIST_DELIMITER))
            filename.append('-').append(optMethod.get());

        filename.append(outfileext);

        return filename.toLowerCase();
    }

    void saveAsFile(const char * dir, StringBuffer &outname, const char *text, const char *ext="")
    {
        StringBuffer path(dir);

        if( outname.length()>0 && path.charAt(path.length()) != PATHSEPCHAR &&  outname.charAt(0) != PATHSEPCHAR)
        {
            path.append(PATHSEPCHAR);
            path.append(outname);
        }

        if( ext && *ext )
        {
            path.append(ext);
        }

        Owned<IFile> file = createIFile(path.str());
        Owned<IFileIO> io;
        io.setown(file->open(IFOcreaterw));

        DBGLOG("Writing to file %s", file->queryFilename());

        if (io.get())
            io->write(0, strlen(text), text);
        else
            DBGLOG("File %s can't be created", file->queryFilename());
    }

    void setFlag( unsigned f ) { optFlags |= f; }
    void unsetFlag( unsigned f ) { optFlags &= ~f; }


public:
    StringAttr optService;
    StringAttr optXsltPath;
    StringAttr optMethod;
    StringAttr optPreprocessOutputDir;
    bool optRawOutput;
    unsigned optFlags;

protected:
    Owned<IFile> serviceDef;
    StringBuffer outputBuffer;
    StringBuffer fullxsltpath;
    Owned<IProperties> params;
    StringBuffer tns;
    StringBuffer outfileext;
};

class EsdlDiffGenCmd : public EsdlHelperConvertCmd
{
public:
    EsdlDiffGenCmd() : optRawOutput(false), optFlags(DEPFLAG_COLLAPSE|DEPFLAG_ARRAYOF){}

    virtual bool parseCommandLineOptions(ArgvIterator &iter)
    {
        if (iter.done())
        {
            usage();
            return false;
        }

        for (; !iter.done(); iter.next())
        {
            const char *arg = iter.query();
            if (*arg != '-')
            {
                if (optSource.isEmpty())
                    optSource.set(arg);
                else if (optService.isEmpty())
                    optService.set(arg);
                else if (optMethod.isEmpty())
                    optMethod.set(arg);
                else if (templatePath.isEmpty())
                    templatePath.set(arg);
                else
                {
                    fprintf(stderr, "\nunrecognized argument: %s\n", arg);
                    usage();
                    return false;
                }
            }
            else
            {
                if (iter.matchOption(optXsltPath, ESDLOPT_XSLT_PATH))
                    continue;
                if (iter.matchOption(optPreprocessOutputDir, ESDLOPT_PREPROCESS_OUT))
                    continue;
                if (EsdlConvertCmd::parseCommandLineOption(iter))
                    continue;
                if (EsdlConvertCmd::matchCommandLineOption(iter, true)!=EsdlCmdOptionMatch)
                    return false;
            }
        }

        return true;
    }

    virtual bool finalizeOptions(IProperties *globals)
    {
        if (optSource.isEmpty())
        {
            usage();
            throw( MakeStringException(0, "\nError: Source esdl parameter required\n"));
        }

        if( optService.isEmpty() )
        {
            usage();
            throw( MakeStringException(0, "A service name must be provided") );
        }

        if( optMethod.isEmpty() )
        {
            usage();
            throw( MakeStringException(0, "A method name must be provided") );
        }

        if (!templatePath.length())
        {
            usage();
            throw( MakeStringException(0, "A differencing template name must be provided") );
        }

        if (optXsltPath.isEmpty())
        {
            StringBuffer tmp;
            optXsltPath.set(getEsdlCmdComponentFilesPath(tmp));
        }

        templateContent.loadFile(templatePath);

        fullxsltpath.set(optXsltPath);
        fullxsltpath.append("/xslt/esxdl2xsd.xslt");

        if (!optPreprocessOutputDir.isEmpty())
            optRawOutput = true;

        return true;
    }

    virtual void doTransform(IEsdlDefObjectIterator& objs, StringBuffer &target, double version=0, IProperties *opts=NULL, const char *ns=NULL, unsigned flags=0 )
    {
        TimeSection ts("transforming via XSLT");
        cmdHelper.defHelper->toXSD( objs, target, EsdlXslToXsd, 0, opts, nullptr, optFlags );
    }

    virtual void loadTransform( StringBuffer &xsltpath, IProperties *params)
    {
        TimeSection ts("loading XSLT");
        cmdHelper.defHelper->loadTransform( xsltpath, params, EsdlXslToXsd );
    }

    virtual void setTransformParams(IProperties *params )
    {
        cmdHelper.defHelper->setTransformParams(EsdlXslToXsd, params);
    }

    static IXmlSchema* createXmlSchema(const char* schema)
    {
        const char* name = SharedObjectPrefix "xmllib" SharedObjectExtension;
        HINSTANCE xmllib = LoadSharedObject(name,true,false);
        if (!LoadSucceeded(xmllib))
            throw MakeStringException(-1,"load %s failed with code %d", name, GetSharedObjectError());
        typedef IXmlSchema* (*XmlSchemaCreator)(const char*);
        XmlSchemaCreator creator = (XmlSchemaCreator)GetSharedProcedure(xmllib, "createXmlSchemaFromString");
        if (!creator)
            throw MakeStringException(-1,"load XmlSchema factory failed: createXmlSchemaFromString()");

        return creator(schema);
    }

    void genDiffTemplateXml(StringStack& parent, IXmlType* type, StringBuffer& out, const char* tag, const char* ns=NULL)
    {
        assertex(type!=NULL);

        const char* typeName = type->queryName();

        if (type->isComplexType())
        {
            if (typeName && std::find(parent.begin(),parent.end(),typeName) != parent.end())
                return;

            out.appendf("<%s", tag);
            if (ns)
                out.append(' ').append(ns);
            for (unsigned i=0; i<type->getAttrCount(); i++)
            {
                IXmlAttribute* attr = type->queryAttr(i);
                out.appendf(" %s='", attr->queryName());
                out.append('\'');
            }
            out.append('>');
            if (typeName)
                parent.push_back(typeName);

            int flds = type->getFieldCount();

            switch (type->getSubType())
            {
            case SubType_Complex_SimpleContent:
                assertex(flds==0);
                break;

            default:
                for (int idx=0; idx<flds; idx++)
                    genDiffTemplateXml(parent,type->queryFieldType(idx),out,type->queryFieldName(idx));
                break;
            }

            if (typeName)
                parent.pop_back();
            out.appendf("</%s>",tag);
        }
        else if (type->isArray())
        {
            if (typeName && std::find(parent.begin(),parent.end(),typeName) != parent.end())
                return; // recursive

            const char* itemName = type->queryFieldName(0);
            IXmlType*   itemType = type->queryFieldType(0);
            if (!itemName || !itemType)
                throw MakeStringException(-1,"*** Invalid array definition: tag=%s, itemName=%s", tag, itemName?itemName:"NULL");

            StringBuffer item;
            if (typeName)
                parent.push_back(typeName);
            genDiffTemplateXml(parent,itemType,item,itemName);
            if (typeName)
                parent.pop_back();

            if (itemType->isComplexType())
                out.appendf("<%s diff:key=''>%s</%s>", tag,item.str(),tag);
            else
                out.appendf("<%s simple='true'>%s</%s>", tag,item.str(),tag);
        }
        else // simple type
        {
            out.appendf("<%s/>", tag);
        }
    }

    void generateDiffTemplate(const char *serv, const char *method, const char * schemaxml)
    {
        VStringBuffer element("%sResponse", method);

        Owned<IXmlSchema> schema = createXmlSchema(schemaxml);
        if (!schema.get())
            throw MakeStringException(-1,"Unknown type: %s", element.str());
        IXmlType* type = schema->queryTypeByName(element);
        if (!type)
            throw MakeStringException(-1,"Unknown type: %s", element.str());
        StringBuffer xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<ResultDiff>\n");
        StringBuffer ns("xmlns=\"");
        generateNamespace(ns).append('\"');

        StringStack parent;
        genDiffTemplateXml(parent,type, xml, element, nullptr);
        xml.append("\n</ResultDiff>");
        Owned<IPropertyTree> pt = createPTreeFromXMLString(xml, ipt_ordered);
        saveXML("difftemplate.xml", pt, 2);
    }

    class XpathTrack
    {
    public:
        XpathTrack(){}
        void push(const char *node)
        {
            size32_t len = xpath.length();
            pos.append(len);
            if (len)
                xpath.append('/');
            xpath.append(node);
        }
        void pop()
        {
            if (!pos.length())
                return;
            size32_t len = pos.popGet();
            xpath.setLength(len);
        }
        const char *str(){return xpath.str();}
    private:
        ArrayOf<size32_t> pos;
        StringBuffer xpath;
    };

    class XTrackScope
    {
    public:
        XTrackScope(XpathTrack &_xt, const char *name) : xt(_xt)
        {
            if (name && *name)
            {
                xt.push(name);
                expanded = true;
            }
        }
        ~XTrackScope()
        {
            if (expanded)
                xt.pop();
        }
        XpathTrack &xt;
        bool expanded = false;
    };
    void expandDiffIds(IPropertyTree &depTree, IPropertyTree *st, IPropertyTree *mon, XpathTrack &xtrack, const char *name)
    {
        XTrackScope xscope(xtrack, name);

        StringBuffer xpath;
        const char *base_type = st->queryProp("@base_type");
        if (base_type && *base_type)
        {
            xpath.setf("EsdlStruct[@name='%s'][1]", base_type);
            IPropertyTree *baseType = depTree.queryPropTree(xpath);
            if (baseType)
            {
                baseType->setPropBool("@_base", true);
                expandDiffIds(depTree, baseType, mon, xtrack, NULL); //walk the type info
            }
        }
        Owned<IPropertyTreeIterator> children = st->getElements("*");
        ForEach(*children)
        {
            IPropertyTree &child = children->query();
            const char *childElementName =child.queryName();
            if (strieq(childElementName, "EsdlElement"))
            {
                const char *complex_type = child.queryProp("@complex_type");
                if (complex_type && *complex_type)
                {
                    xpath.setf("EsdlStruct[@name='%s'][1]", complex_type);
                    IPropertyTree *childType = depTree.queryPropTree(xpath);
                    if (childType)
                    {
                        childType->setPropBool("@_nested", true);
                        expandDiffIds(depTree, childType, mon, xtrack, child.queryProp("@name")); //walk the type info
                    }
                }
            }
            else
            {

                if (strieq(childElementName, "EsdlArray"))
                {
                    const char *item_type = child.queryProp("@type");
                    if (item_type && *item_type)
                    {
                        xpath.setf("EsdlStruct[@name='%s'][1]", item_type);
                        IPropertyTree *childType = depTree.queryPropTree(xpath);
                        if (childType)
                        {
                            XTrackScope xscope(xtrack, child.queryProp("@name"));
                            xpath.setf("%s/@diff_id", xtrack.str());
                            const char *diff_id = mon->queryProp(xpath);
                            if (!diff_id || !*diff_id)
                                diff_id = child.queryProp("@diff_id");
                            if (diff_id && *diff_id)
                            {
                                StringArray idparts;
                                idparts.appendListUniq(diff_id, "+");
                                idparts.sortAscii(true);
                                StringBuffer idname;
                                ForEachItemIn(i1, idparts)
                                {
                                    StringBuffer s(idparts.item(i1));
                                    idname.append(s.trim());
                                }
                                xpath.setf("diff_id[@name='%s']", idname.toLowerCase().str());

                                IPropertyTree *diffIdTree = createPTree("diff_id");
                                diffIdTree->setProp("@name", idname);
                                ForEachItemIn(i2, idparts)
                                    diffIdTree->setProp("part", idparts.item(i2));

                                IPropertyTree *diffIdSection = child.queryPropTree("DiffIdSection");
                                if (!diffIdSection)
                                    diffIdSection = child.addPropTree("DiffIdSection", createPTree("DiffIdSection"));
                                if (diffIdSection && !diffIdSection->hasProp(xpath))
                                    diffIdSection->addPropTree("diff_id", LINK(diffIdTree));
                                diffIdSection = childType->queryPropTree("DiffIdSection");
                                if (!diffIdSection)
                                    diffIdSection = childType->addPropTree("DiffIdSection", createPTree("DiffIdSection"));
                                if (diffIdSection && !diffIdSection->hasProp(xpath))
                                    diffIdSection->addPropTree("diff_id", LINK(diffIdTree));
                            }
                            expandDiffIds(depTree, childType, mon, xtrack, NULL); //walk the type info
                        }
                    }
                }
            }
        }
    }
    virtual int processCMD()
    {
        cmdHelper.loadDefinition(optSource, optService, 0);
        Owned<IEsdlDefObjectIterator> structs = cmdHelper.esdlDef->getDependencies(optService, optMethod, 0, nullptr, DEPFLAG_INCLUDE_RESPONSE | DEPFLAG_INCLUDE_METHOD | DEPFLAG_ECL_ONLY);
        Owned<IEsdlDefinitionHelper> defHelper = createEsdlDefinitionHelper();
        StringBuffer xml("<esxdl>\n");
        defHelper->toXML(*structs, xml, 0, nullptr, 0);
        xml.append("\n</esxdl>");

        Owned<IPropertyTree> monitoringTemplate = createPTreeFromXMLString(templateContent, ipt_ordered);
        Owned<IPropertyTree> depTree = createPTreeFromXMLString(xml, ipt_ordered);

        VStringBuffer xpath("EsdlMethod[@name='%s']/@response_type", optMethod.str());
        const char *resp_type = "SmartLinxReportResponse"; //depTree->queryProp(xpath);
        if (resp_type && *resp_type)
        {
            xpath.setf("EsdlStruct[@name='%s']", resp_type);
            IPropertyTree *respTree = depTree->queryPropTree(xpath);
            if (respTree)
            {
                const char *alt_type = respTree->queryProp("EsdlElement[@name='response']/@complex_type");
                if (alt_type && *alt_type)
                {
                    xpath.setf("EsdlStruct[@name='%s']", alt_type);
                    IPropertyTree *altTree = depTree->queryPropTree(xpath);
                    if (altTree)
                        respTree = altTree;
                }
                XpathTrack xtrack;
                expandDiffIds(*depTree, respTree, monitoringTemplate->queryPropTree(resp_type), xtrack, NULL);
            }
        }

        toXML(depTree, xml.clear()); //refresh changes

        StringBuffer filename("generated.xml");
        saveAsFile(".", filename, xml);

        return 0;
    }

    void printOptions()
    {
        puts("Options:");
        puts("  --xslt <xslt file path> : Path to '/xslt/esxdl2xsd.xslt' file to transform EsdlDef to XSD\n" );
        puts("  --preprocess-output <raw output directory> : Output pre-processed xml file to specified directory before applying XSLT transform\n" );
        puts("  --annotate <all | none> : Flag turning on either all annotations or none. By default annotations are generated " );
        puts("                    for Enumerations. Setting the flag to 'none' will disable even those. Setting it" );
        puts("                    to 'all' will enable additional annotations such as collapsed, cols, form_ui, html_head and rows.\n");
    }

    virtual void usage()
    {
        puts("Usage:");
        puts("esdl ecl-diff-gen sourcePath serviceName methodname diff_template [options]\n" );
        puts("\nsourcePath must be absolute path to the ESDL Definition file containing the" );
        puts("EsdlService definition for the service you want to work with.\n" );
        puts("serviceName EsdlService definition for the service you want to work with.\n" );

        printOptions();
        EsdlConvertCmd::usage();
    }

    virtual void outputRaw( IEsdlDefObjectIterator& obj)
    {
        if( optRawOutput )
        {
            StringBuffer xmlOut;
            StringBuffer empty;

            xmlOut.appendf( "<esxdl name=\"%s\">", optService.get());
            cmdHelper.defHelper->toXML( obj, xmlOut, 0, nullptr, optFlags );
            xmlOut.append("</esxdl>");

            saveAsFile( optPreprocessOutputDir.get(), empty, xmlOut.str(), NULL );
        }
    }

    void createParams()
    {
        params.set(createProperties());
        generateNamespace(tns);
        params->setProp( "tnsParam", tns );
    }

    virtual void outputToFile()
    {
        if (!optOutDirPath.isEmpty())
        {
            StringBuffer filename;
            generateOutputFileName(filename);
            saveAsFile(optOutDirPath.get(), filename, outputBuffer.str(), NULL);
        }
    }

    StringBuffer & generateNamespace(StringBuffer &ns)
    {
        ns.appendf("http://webservices.seisint.com/%s", optService.get());
        return ns.toLowerCase();
    }

    virtual StringBuffer & generateOutputFileName( StringBuffer &filename)
    {
        filename.appendf("%s", optService.get());
        if (!optMethod.isEmpty() && !strstr(optMethod.get(), ESDLOPTLIST_DELIMITER))
            filename.append('-').append(optMethod.get());

        filename.append(outfileext);

        return filename.toLowerCase();
    }

    void saveAsFile(const char * dir, StringBuffer &outname, const char *text, const char *ext="")
    {
        StringBuffer path(dir);

        if( outname.length()>0 && path.charAt(path.length()) != PATHSEPCHAR &&  outname.charAt(0) != PATHSEPCHAR)
        {
            path.append(PATHSEPCHAR);
            path.append(outname);
        }

        if( ext && *ext )
        {
            path.append(ext);
        }

        Owned<IFile> file = createIFile(path.str());
        Owned<IFileIO> io;
        io.setown(file->open(IFOcreaterw));

        DBGLOG("Writing to file %s", file->queryFilename());

        if (io.get())
            io->write(0, strlen(text), text);
        else
            DBGLOG("File %s can't be created", file->queryFilename());
    }

    void setFlag( unsigned f ) { optFlags |= f; }
    void unsetFlag( unsigned f ) { optFlags &= ~f; }


public:
    StringAttr optService;
    StringAttr optXsltPath;
    StringAttr optMethod;
    StringAttr optPreprocessOutputDir;
    bool optRawOutput;
    unsigned optFlags;

protected:
    Owned<IFile> serviceDef;
    StringBuffer outputBuffer;
    StringBuffer fullxsltpath;
    Owned<IProperties> params;
    StringBuffer tns;
    StringBuffer outfileext;
    StringBuffer templatePath;
    StringBuffer templateContent;
};


IEsdlCommand *createEsdlDiffCommand(const char *cmdname)
{
    if (strieq(cmdname, "ECL-DIFF-TEMPLATE"))
        return new EsdlDiffTemplateCmd();
    if (strieq(cmdname, "ECL-DIFF-GEN"))
        return new EsdlDiffGenCmd();

    return nullptr;
}
