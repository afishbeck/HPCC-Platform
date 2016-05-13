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

void removeEclHiddenStructs(IPropertyTree &depTree)
{
    Owned<IPropertyTreeIterator> it = depTree.getElements("*[@ecl_hide='1']");
    ForEach(*it)
        depTree.removeTree(&it->query());
}
void removeEclHiddenElements(IPropertyTree &depTree)
{
    Owned<IPropertyTreeIterator> it = depTree.getElements("*");
    ForEach(*it)
    {
        StringArray names;
        Owned<IPropertyTreeIterator> elements = it->query().getElements("*[@ecl_hide='1']");
        ForEach(*elements)
            names.append(elements->query().queryProp("@name"));
        ForEachItemIn(i, names)
        {
            VStringBuffer xpath("*[@name='%s']", names.item(i));
            it->query().removeProp(xpath);
        }
    }
}

void removeGetDataFromElements(IPropertyTree &depTree)
{
    Owned<IPropertyTreeIterator> it = depTree.getElements("*");
    ForEach(*it)
    {
        StringArray names;
        Owned<IPropertyTreeIterator> elements = it->query().getElements("*[@get_data_from]");
        ForEach(*elements)
            names.append(elements->query().queryProp("@name"));
        ForEachItemIn(i, names)
        {
            VStringBuffer xpath("*[@name='%s']", names.item(i));
            it->query().removeProp(xpath);
        }
    }
}
void removeEclHidden(IPropertyTree &depTree)
{
    removeEclHiddenStructs(depTree);
    removeEclHiddenElements(depTree);
    removeGetDataFromElements(depTree);
}

void removeEclHidden(StringBuffer &xml)
{
    Owned<IPropertyTree> depTree = createPTreeFromXMLString(xml);
    removeEclHidden(*depTree);
    toXML(depTree, xml.clear());
}

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
        cmdHelper.defHelper->toXSD( objs, target, EsdlXslToXsd, 0, opts, nullptr, optFlags | DEPFLAG_ECL_ONLY);
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
                out.appendf("<%s diff_match=''>%s</%s>", tag,item.str(),tag);
            else
                out.appendf("<%s/>", tag);
        }
        else // simple type
        {
            out.appendf("<%s/>", tag);
        }
    }

    void addMonitoringChildren(StringBuffer &xml, IPropertyTree *esdl, IPropertyTree *cur, unsigned indent)
    {
        const char *base_type = cur->queryProp("@base_type");
        if (base_type && *base_type)
        {
            VStringBuffer xpath("EsdlStruct[@name='%s'][1]", base_type);
            IPropertyTree *base = esdl->queryPropTree(xpath);
            if (base)
                addMonitoringChildren(xml, esdl, base, indent);
        }
        Owned<IPropertyTreeIterator> it = cur->getElements("*");
        ForEach(*it)
        {
            IPropertyTree &item = it->query();
            const char *elem = item.queryName();
            const char *name = item.queryProp("@name");
            if (streq(elem, "EsdlElement"))
            {
                const char *complex_type = item.queryProp("@complex_type");
                if (complex_type && *complex_type)
                {
                    VStringBuffer xpath("EsdlStruct[@name='%s'][1]", complex_type);
                    IPropertyTree *st = esdl->queryPropTree(xpath);
                    if (!st)
                        xml.pad(indent).appendf("<%s type='%s'/>\n", name, complex_type);
                    else
                    {
                        xml.pad(indent).appendf("<%s>\n", name);
                        addMonitoringChildren(xml, esdl, st, indent+2);
                        xml.pad(indent).appendf("</%s>\n", name);
                    }
                }
                else
                {
                    xml.pad(indent).appendf("<%s type='%s'/>\n", name, item.queryProp("@type"));
                }
            }
            else if (streq(elem, "EsdlArray"))
            {
                const char *item_name = item.queryProp("@item_name");
                const char *type = item.queryProp("@type");
                if (!type || !*type)
                    type = "string";

                VStringBuffer xpath("EsdlStruct[@name='%s'][1]", type);
                IPropertyTree *st = esdl->queryPropTree(xpath);
                if (!st)
                {
                    if (!item_name || !*item_name)
                        item_name = "Item";
                    xml.pad(indent).appendf("<%s>\n", name);
                    xml.pad(indent+2).appendf("<%s type='%s'/>\n", item_name, type);
                    xml.pad(indent).appendf("</%s>\n", name);
                }
                else
                {
                    if (!item_name || !*item_name)
                        item_name = type;
                    xml.pad(indent).appendf("<%s diff_match=''>\n", name);
                    xml.pad(indent+2).appendf("<%s>\n", item_name);
                    addMonitoringChildren(xml, esdl, st, indent+4);
                    xml.pad(indent+2).appendf("</%s>\n", item_name);
                    xml.pad(indent).appendf("</%s>\n", name);
                }
            }
            else if (streq(elem, "EsdlEnumRef"))
            {
                xml.pad(indent).appendf("<%s type='enum'/>\n", name);
            }
            else
            {
                xml.pad(indent).appendf("<%s unimplemented='%s'/>\n", name, elem);
            }

        }
    }

    void createMonitoringTemplate(StringBuffer &xml, IPropertyTree *esdl, const char *method)
    {
        xml.append("<ResultMonitoringTemplate>\n");
        VStringBuffer typeName("%sResponse", method);
        xml.append("  <").append(typeName).append(">\n");
        VStringBuffer xpath("*[@name='%s'][1]", typeName.str());
        IPropertyTree *responseType = esdl->queryPropTree(xpath);
        if (responseType)
            addMonitoringChildren(xml, esdl, responseType, 4);
        xml.append("  </").append(typeName).append(">\n");
        xml.append("</ResultMonitoringTemplate>");
    }
    void generateDiffTemplate(const char *serv, const char *method, const char * schemaxml)
    {
        VStringBuffer element("%sResponse", method);
        VStringBuffer filename("generated_%s.xsd", method);
        saveAsFile(".", filename, schemaxml);


        Owned<IXmlSchema> schema = createXmlSchema(schemaxml);
        if (!schema.get())
            throw MakeStringException(-1,"Unknown type: %s", element.str());
        IXmlType* type = schema->queryTypeByName(element);
        if (!type)
            throw MakeStringException(-1,"Unknown type: %s", element.str());
        StringBuffer xml("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<ResultMonitoringTemplate>\n");
        StringBuffer ns("xmlns=\"");
        generateNamespace(ns).append('\"');

        StringStack parent;
        genDiffTemplateXml(parent,type, xml, element, nullptr);
        xml.append("\n</ResultMonitoringTemplate>");
        Owned<IPropertyTree> pt = createPTreeFromXMLString(xml, ipt_ordered);
        VStringBuffer templatefile("diff_template_%s.xml", optMethod.str());
        saveXML(templatefile, pt, 2);
    }

    virtual int processCMD()
    {
        loadServiceDef();

        StringBuffer xml("<esdl>");
        Owned<IEsdlDefObjectIterator> structs = cmdHelper.esdlDef->getDependencies( optService.get(), optMethod.get(), ESDLOPTLIST_DELIMITER, 0, nullptr, optFlags );
        cmdHelper.defHelper->toXML(*structs, xml, 0, NULL, 0);
        xml.append("</esdl>");

        Owned<IPropertyTree> depTree = createPTreeFromXMLString(xml, ipt_ordered);
        removeEclHidden(*depTree);
        toXML(depTree, xml.clear());

        StringBuffer monTemplate;
        createMonitoringTemplate(monTemplate, depTree, optMethod);

        VStringBuffer templatefile("diff_template_%s.xml", optMethod.str());
        saveAsFile(".", templatefile, monTemplate);
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

    const char *findInheritedAttribute(IPropertyTree &depTree, IPropertyTree *structType, const char *attr)
    {
        const char *s = structType->queryProp(attr);
        if (s && *s)
            return s;
        const char *base_type = structType->queryProp("@base_type");
        if (!base_type || !*base_type)
            return NULL;

        VStringBuffer xpath("EsdlStruct[@name='%s'][1]", base_type);
        IPropertyTree *baseType = depTree.queryPropTree(xpath);
        if (!baseType)
            return NULL;
        return findInheritedAttribute(depTree, baseType, attr);
    }

    IPropertyTree *findInheritedXpath(IPropertyTree &depTree, IPropertyTree *structType, const char *path)
    {
        IPropertyTree *t= structType->queryPropTree(path);
        if (t)
            return t;
        const char *base_type = structType->queryProp("@base_type");
        if (!base_type || !*base_type)
            return NULL;

        VStringBuffer xpath("EsdlStruct[@name='%s'][1]", base_type);
        IPropertyTree *baseType = depTree.queryPropTree(xpath);
        if (!baseType)
            return NULL;
        return findInheritedXpath(depTree, baseType, path);
    }

    void addAllDiffIdElementsToMap(IPropertyTree &depTree, IPropertyTree *st, IPropertyTree *map)
    {
        const char *base_type = st->queryProp("@base_type");
        if (base_type && *base_type)
        {
            VStringBuffer xpath("EsdlStruct[@name='%s'][1]", base_type);
            IPropertyTree *baseType = depTree.queryPropTree(xpath);
            if (baseType)
                addAllDiffIdElementsToMap(depTree, baseType, map);
        }
        Owned<IPropertyTreeIterator> children = st->getElements("EsdlElement");
        ForEach(*children)
        {
            IPropertyTree &child = children->query();
            if (!child.hasProp("@complex_type"))
            {
                addDiffIdPartToMap(depTree, st, map, child.queryProp("@name"));
            }
            else
            {
                IPropertyTree *partMap = ensurePTree(map, child.queryProp("@name"));
                VStringBuffer xpath("EsdlStruct[@name='%s']", child.queryProp("@complex_type"));
                IPropertyTree *structType = depTree.queryPropTree(xpath);
                if (!structType)
                    map->setProp("@ftype", "str"); //ECL compiler will find the error
                else
                    addDiffIdPartToMap(depTree, structType, partMap, NULL);
            }
        }
    }
    void addDiffIdPartToMap(IPropertyTree &depTree, IPropertyTree *parent, IPropertyTree *map, const char *id)
    {
        StringBuffer part;
        const char *finger = nullptr;
        if (id && *id)
        {
            finger = strchr(id, '.');
            if (finger)
                part.append(finger-id, id);
            else
                part.append(id);
            part.trim();
        }
        if (!part.length()) //short hand for compare the entire struct
        {
            addAllDiffIdElementsToMap(depTree, parent, map);
        }
        else
        {
            VStringBuffer xpath("EsdlElement[@name='%s']", part.str());
            IPropertyTree *partElement = parent->queryPropTree(xpath);
            if (!partElement)
            {
                StringBuffer idpath(id);
                idpath.replace(' ','_').replace('.', '/');
                IPropertyTree *partMap = ensurePTree(map, idpath.str());
                partMap->setProp("@ftype", "string"); //let ecl compiler complain
            }
            else
            {
                IPropertyTree *partMap = ensurePTree(map, part.str());
                if (!partElement->hasProp("@complex_type")) //simple or none
                {
                    EsdlBasicElementType et = esdlSimpleType(partElement->queryProp("@type"));
                    switch (et)
                    {
                    case ESDLT_INT8:
                    case ESDLT_INT16:
                    case ESDLT_INT32:
                    case ESDLT_INT64:
                    case ESDLT_UINT8:
                    case ESDLT_UINT16:
                    case ESDLT_UINT32:
                    case ESDLT_UINT64:
                    case ESDLT_BYTE:
                    case ESDLT_UBYTE:
                        partMap->setProp("@ftype", "number");
                        break;
                    case ESDLT_BOOL:
                        partMap->setProp("@ftype", "bool");
                        break;
                    case ESDLT_FLOAT:
                    case ESDLT_DOUBLE:
                        partMap->setProp("@ftype", "float");
                        break;
                    case ESDLT_UNKOWN:
                    case ESDLT_STRING:
                    default:
                        partMap->setProp("@ftype", "string");
                        break;
                    }
                }
                else
                {
                    xpath.setf("EsdlStruct[@name='%s']", partElement->queryProp("@complex_type"));
                    IPropertyTree *structType = depTree.queryPropTree(xpath);
                    if (!structType)
                    {
                        partMap->setProp("@ftype", "str"); //ECL compiler will find the error
                        return;
                    }
                    else
                    {
                        addDiffIdPartToMap(depTree, structType, partMap, finger ? finger+1 : NULL);
                    }
                }
            }
        }
    }
    IPropertyTree *createDiffIdTypeMap(IPropertyTree &depTree, IPropertyTree *parent, StringArray &idparts)
    {
        Owned<IPropertyTree> map = createPTree();
        ForEachItemIn(i1, idparts)
            addDiffIdPartToMap(depTree, parent, map, idparts.item(i1));
        return map.getClear();
    }
    void flattenDiffIdTypeMap(IPropertyTree &map, StringArray &flat, XpathTrack &xtrack, const char *name)
    {
        XTrackScope xscope(xtrack, name);

        Owned<IPropertyTreeIterator> children = map.getElements("*");
        ForEach(*children)
        {
            IPropertyTree &child = children->query();
            if (child.hasProp("@ftype"))
            {
                XTrackScope xscope(xtrack, child.queryName());
                StringBuffer s(xtrack.str());
                s.replace('/', '.');
                VStringBuffer xml("<part ftype='%s' name='%s'/>", child.queryProp("@ftype"), s.str());
                flat.append(xml);
            }
            else
            {
                flattenDiffIdTypeMap(child, flat, xtrack, child.queryName());
            }
        }
    }
    IPropertyTree *createDiffIdTree(IPropertyTree &depTree, IPropertyTree *parent, const char *diff_key, StringBuffer &idname)
    {
        StringArray idparts;
        idparts.appendListUniq(diff_key, "+");
        idparts.sortAscii(true);

        Owned<IPropertyTree> map = createDiffIdTypeMap(depTree, parent, idparts);

        ForEachItemIn(i1, idparts)
        {
            StringBuffer s(idparts.item(i1));
            idname.append(s.trim());
        }

        Owned<IPropertyTree> diffIdTree = createPTree("diff_key");
        diffIdTree->setProp("@name", idname.toLowerCase());
        XpathTrack xt;
        StringArray flat;
        flattenDiffIdTypeMap(*map, flat, xt, nullptr);

        ForEachItemIn(i2, flat)
        {
            diffIdTree->addPropTree("part", createPTreeFromXMLString(flat.item(i2)));
        }
        //diffIdTree->addPropTree("map", map.getClear());
        return diffIdTree.getClear();
    }
    bool getMonFirstDiffAttributeBool(IPropertyTree *mon, XpathTrack &xtrack, const char *attribute, bool def)
    {
        VStringBuffer xpath("%s[%s]", xtrack.str(), attribute);
        Owned<IPropertyTreeIterator> it = mon->getElements(xpath.str());
        if (!it->first())
            return def;
        return it->query().getPropBool(attribute, def);
    }
    const char *queryMonFirstDiffAttribute(IPropertyTree *mon, XpathTrack &xtrack, const char *attribute)
    {
        VStringBuffer xpath("%s[%s]", xtrack.str(), attribute);
        Owned<IPropertyTreeIterator> it = mon->getElements(xpath.str());
        if (!it->first())
            return nullptr;
        return it->query().queryProp(attribute);
    }

    void setMonBasesPropBool(IPropertyTree &depTree, IPropertyTree *st, const char *attr, bool value)
    {
        const char *base_type = st->queryProp("@base_type");
        if (base_type && *base_type)
        {
            VStringBuffer xpath("EsdlStruct[@name='%s'][1]", base_type);
            IPropertyTree *baseType = depTree.queryPropTree(xpath);
            if (baseType)
            {
                setMonBasesPropBool(depTree, baseType, attr, value);
                baseType->setPropBool(attr, value);
            }

        }
    }
    bool expandDiffIds(IPropertyTree &depTree, IPropertyTree *st, IPropertyTree *mon, XpathTrack &xtrack, const char *name, bool monitored, bool inMonSection)
    {
        bool mon_child = false; //we have monitored descendants
        XTrackScope xscope(xtrack, name);
        if (monitored)
            st->setPropBool("@diff_monitor", true);

        StringBuffer xpath;
        const char *base_type = st->queryProp("@base_type");
        if (base_type && *base_type)
        {
            xpath.setf("EsdlStruct[@name='%s'][1]", base_type);
            IPropertyTree *baseType = depTree.queryPropTree(xpath);
            if (baseType)
            {
                baseType->setPropBool("@_base", true);
                mon_child = expandDiffIds(depTree, baseType, mon, xtrack, NULL, monitored, inMonSection); //walk the type info
            }
        }

        {
            Owned<IPropertyTreeIterator> children = st->getElements("*");
            ForEach(*children)
            {
                bool childMonSection = inMonSection;
                IPropertyTree &child = children->query();
                bool ecl_hide = child.getPropBool("@ecl_hide");
                if (ecl_hide)
                    continue;
                bool diff_monitor = child.getPropBool("@diff_monitor", monitored); //esdl-xml over rides parent

                XTrackScope xscope(xtrack, child.queryProp("@name"));
                bool template_diff_monitor = getMonFirstDiffAttributeBool(mon, xtrack, "@diff_monitor", diff_monitor);
                if (template_diff_monitor != diff_monitor)
                {
                    child.setPropBool("@diff_monitor", template_diff_monitor);
                    diff_monitor=template_diff_monitor;
                }
                if (diff_monitor && !childMonSection) //start of a monitored section
                {
                    child.setProp("@diff_section", StringBuffer(xtrack.str()).replace('/', '_').str());
                    childMonSection = true;
                }

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
                            childType->setPropBool("@_used", true);
                            bool mon_elem = expandDiffIds(depTree, childType, mon, xtrack, NULL, diff_monitor, childMonSection); //walk the type info
                            if (!monitored && mon_elem)
                            {
                                child.setPropBool("@mon_child", true);
                                if (!mon_child)
                                    mon_child = true;
                            }
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
                                childType->setPropBool("@_used", true);
                                const char *diff_key = queryMonFirstDiffAttribute(mon, xtrack, "@diff_key");
                                if (!diff_key || !*diff_key)
                                    diff_key = child.queryProp("@diff_key");
                                if (!diff_key || !*diff_key)
                                    diff_key = findInheritedAttribute(depTree, childType, "@diff_key");
                                if (diff_key && *diff_key)
                                {
                                    StringBuffer idname;
                                    Owned<IPropertyTree> diffIdTree = createDiffIdTree(depTree, childType, diff_key, idname);
                                    xpath.setf("diff_key[@name='%s']", idname.toLowerCase().str());

                                    IPropertyTree *diffKeys = child.queryPropTree("DiffKeys");
                                    if (!diffKeys)
                                        diffKeys = child.addPropTree("DiffKeys", createPTree("DiffKeys"));
                                    if (diffKeys && !diffKeys->hasProp(xpath))
                                        diffKeys->addPropTree("diff_key", LINK(diffIdTree));
                                    diffKeys = childType->queryPropTree("DiffKeys");
                                    if (!diffKeys)
                                        diffKeys = childType->addPropTree("DiffKeys", createPTree("DiffKeys"));
                                    if (diffKeys && !diffKeys->hasProp(xpath))
                                        diffKeys->addPropTree("diff_key", LINK(diffIdTree));
                                }

                                bool mon_elem = expandDiffIds(depTree, childType, mon, xtrack, child.queryProp("@item_tag"), diff_monitor, childMonSection); //walk the type info
                                if (!monitored && mon_elem)
                                {
                                    child.setPropBool("@mon_child", true);
                                    if (!mon_child)
                                        mon_child = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (monitored)
            return true;
        if (mon_child)
        {
            setMonBasesPropBool(depTree, st, "@mon_child_base", true);
            st->setPropBool("@mon_child", true);
            return true;
        }
        return false;
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
        removeEclHidden(*depTree);

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
                expandDiffIds(*depTree, respTree, monitoringTemplate->queryPropTree(resp_type), xtrack, NULL, false, false);
            }
        }

        toXML(depTree, xml.clear()); //refresh changes

        VStringBuffer filename("generated_%s.xml", optMethod.str());
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
