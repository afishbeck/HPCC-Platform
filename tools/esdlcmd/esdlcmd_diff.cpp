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
    EsdlDiffTemplateCmd(){}

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

        return true;
    }

    virtual void doTransform(IEsdlDefObjectIterator& objs, StringBuffer &target, double version=0, IProperties *opts=NULL, const char *ns=NULL, unsigned flags=0 )
    {
    }

    virtual void loadTransform( StringBuffer &xsltpath, IProperties *params)
    {
    }

    virtual void setTransformParams(IProperties *params )
    {
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

    virtual int processCMD()
    {
        loadServiceDef();

        StringBuffer xml("<esdl>");
        Owned<IEsdlDefObjectIterator> structs = cmdHelper.esdlDef->getDependencies( optService.get(), optMethod.get(), ESDLOPTLIST_DELIMITER, 0, nullptr, 0 );
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

    virtual void loadServiceDef()
    {
        cmdHelper.loadDefinition(optSource, optService, 0);
    }

    virtual StringBuffer & generateOutputFileNamex( StringBuffer &filename)
    {
        return filename;
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

public:
    StringAttr optService;
    StringAttr optMethod;
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
    IPropertyTree *createDiffIdTree(IPropertyTree &depTree, IPropertyTree *parent, const char *diff_match, StringBuffer &idname)
    {
        StringArray idparts;
        idparts.appendListUniq(diff_match, "+");
        idparts.sortAscii(true);

        Owned<IPropertyTree> map = createDiffIdTypeMap(depTree, parent, idparts);

        ForEachItemIn(i1, idparts)
        {
            StringBuffer s(idparts.item(i1));
            idname.append(s.trim());
        }

        Owned<IPropertyTree> diffIdTree = createPTree("diff_match");
        diffIdTree->setProp("@name", idname.toLowerCase());
        XpathTrack xt;
        StringArray flat;
        flattenDiffIdTypeMap(*map, flat, xt, nullptr);

        ForEachItemIn(i2, flat)
        {
            diffIdTree->addPropTree("part", createPTreeFromXMLString(flat.item(i2)));
        }
        return diffIdTree.getClear();
    }
    bool getMonFirstDiffAttributeBool(IPropertyTree *tmplate, XpathTrack &xtrack, const char *attribute, bool def)
    {
        VStringBuffer xpath("%s[%s]", xtrack.str(), attribute);
        Owned<IPropertyTreeIterator> it = tmplate->getElements(xpath.str());
        if (!it->first())
            return def;
        return it->query().getPropBool(attribute, def);
    }
    const char *queryMonFirstDiffAttribute(IPropertyTree *tmplate, XpathTrack &xtrack, const char *attribute, const char *def)
    {
        VStringBuffer xpath("%s[%s]", xtrack.str(), attribute);
        Owned<IPropertyTreeIterator> it = tmplate->getElements(xpath.str());
        if (it->first())
        {
            const char *val = it->query().queryProp(attribute);
            if (val && *val)
                return val;
        }
        return def;
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

    void addCtxDiffSelectors(IPropertyTree *ctx, IPropertyTree *ctxSelectors, IPropertyTree *stSelectors)
    {
        if (!stSelectors)
            return;
        Owned<IPropertyTreeIterator> selectors = stSelectors->getElements("selector");
        ForEach(*selectors)
        {
            const char *name = selectors->query().queryProp(nullptr);
            if (!name || !*name)
                continue;
            VStringBuffer xpath("selector[@name='%s']", name);
            VStringBuffer flagName("_CheckSelector_%s", name);
            ctx->setPropBool(flagName, ctxSelectors->hasProp(xpath));
        }
    }
    void trimContext(IPropertyTree *origCtx, IPropertyTree &depTree, IPropertyTree *st)
    {
        if (st->hasProp("@base_type"))
        {
            IPropertyTree *baseType = nullptr;
            VStringBuffer xpath("EsdlStruct[@name='%s']", st->queryProp("@base_type"));
            baseType = depTree.queryPropTree(xpath);
            if (baseType)
                trimContext(origCtx, depTree, baseType);
        }
        Owned<IPropertyTreeIterator> children = st->getElements("*");
        ForEach(*children)
        {
            IPropertyTree &child = children->query();
            if (strieq(child.queryName(),"_diff_selectors"))
            {
                addCtxDiffSelectors(origCtx, origCtx->queryPropTree("_diff_selectors"), &child);
                continue;
            }
            const char *name = child.queryProp("@name");
            if (!child.getPropBool("@_mon") || !child.getPropBool("@_nomon")) //having both makes it context sensitive
            {
                VStringBuffer flagName("_CheckField_%s", name);
                origCtx->removeProp(flagName);
            }
            IPropertyTree *childCtx = origCtx->queryPropTree(name);
            if (!childCtx)
                continue;
            const char *typeName = nullptr;
            if (child.hasProp("@complex_type"))
                typeName = child.queryProp("@complex_type");
            else if (strieq(child.queryName(), "EsdlArray"))
                typeName = child.queryProp("@type");

            IPropertyTree *complexType = nullptr;
            VStringBuffer xpath("EsdlStruct[@name='%s']", typeName);
            complexType = depTree.queryPropTree(xpath);
            if (complexType)
                trimContext(childCtx, depTree, complexType);
        }

    }
    bool expandDiffTrees(IPropertyTree *ctxLocal, IPropertyTree &depTree, IPropertyTree *st, IPropertyTree *tmplate, XpathTrack &xtrack, const char *name, bool monitored, bool inMonSection, StringArray &allSelectors)
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
                mon_child = expandDiffTrees(ctxLocal, depTree, baseType, tmplate, xtrack, NULL, monitored, inMonSection, allSelectors); //walk the type info
            }
        }

        {
            Owned<IPropertyTreeIterator> children = st->getElements("*");
            ForEach(*children)
            {
                IPropertyTree &child = children->query();
                const char *name = child.queryProp("@name");
                if (!name || !*name)
                    continue;
                bool childMonSection = inMonSection;
                bool ecl_hide = child.getPropBool("@ecl_hide");
                if (ecl_hide)
                    continue;
                const char *selectorList = child.queryProp("@diff_monitor"); //esdl-xml over rides parent
                XTrackScope xscope(xtrack, child.queryProp("@name"));
                selectorList = queryMonFirstDiffAttribute(tmplate, xtrack, "@diff_monitor", selectorList);
                Owned<IPropertyTree> selectorTree;
                bool childMonitored = monitored;
                if (selectorList && *selectorList)
                {
                    if (strieq(selectorList, "0") || strieq(selectorList, "false") || strieq(selectorList, "no") || strieq(selectorList, "off"))
                        childMonitored = false;
                    else if (strieq(selectorList, "1") || strieq(selectorList, "true") || strieq(selectorList, "yes") || strieq(selectorList, "on"))
                        childMonitored = true;
                    else
                    {
                        StringArray selectors;
                        selectors.appendListUniq(selectorList, "|");
                        allSelectors.appendListUniq(selectorList, "|");
                        if (selectors.length())
                        {
                            selectorTree.setown(createPTree());
                            ForEachItemIn(i1, selectors)
                                selectorTree->addProp("selector", selectors.item(i1));
                            child.setPropTree("_diff_selectors", LINK(selectorTree));
                            IPropertyTree *ctxChild = ensurePTree(ctxLocal, name);
                            ctxChild->setPropTree("_diff_selectors", LINK(selectorTree));
                            childMonitored = true;
                        }
                    }
                }

                if (childMonitored) //structure can be reused in different locations.  track yes and no monitoring separately so we know whether we need to check context
                    child.setPropBool("@_mon", true);
                else if (monitored)
                    child.setPropBool("@_nomon", true);
                VStringBuffer ctxMonFlag("_CheckField_%s", name);
                ctxLocal->setPropBool(ctxMonFlag, childMonitored);

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
                            if (selectorTree)
                                childType->setPropTree("_diff_selectors", LINK(selectorTree));

                            childType->setPropBool("@_used", true);
                            IPropertyTree *ctxChild = ensurePTree(ctxLocal, name);
                            bool mon_elem = expandDiffTrees(ctxChild, depTree, childType, tmplate, xtrack, NULL, childMonitored, childMonSection, allSelectors); //walk the type info
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
                                const char *diff_match = queryMonFirstDiffAttribute(tmplate, xtrack, "@diff_match", nullptr);
                                if (!diff_match || !*diff_match)
                                    diff_match = child.queryProp("@diff_match");
                                if (!diff_match || !*diff_match)
                                    diff_match = findInheritedAttribute(depTree, childType, "@diff_match");
                                if (diff_match && *diff_match)
                                {
                                    StringBuffer idname;
                                    Owned<IPropertyTree> diffIdTree = createDiffIdTree(depTree, childType, diff_match, idname);
                                    xpath.setf("diff_match[@name='%s']", idname.toLowerCase().str());

                                    IPropertyTree *diffKeys = child.queryPropTree("DiffMatchs");
                                    if (!diffKeys)
                                        diffKeys = child.addPropTree("DiffMatchs", createPTree("DiffMatchs"));
                                    if (diffKeys && !diffKeys->hasProp(xpath))
                                        diffKeys->addPropTree("diff_match", LINK(diffIdTree));
                                    diffKeys = childType->queryPropTree("DiffMatchs");
                                    if (!diffKeys)
                                        diffKeys = childType->addPropTree("DiffMatchs", createPTree("DiffMatchs"));
                                    if (diffKeys && !diffKeys->hasProp(xpath))
                                        diffKeys->addPropTree("diff_match", LINK(diffIdTree));
                                }

                                IPropertyTree *ctxChild = ensurePTree(ctxLocal, name);
                                bool mon_elem = expandDiffTrees(ctxChild, depTree, childType, tmplate, xtrack, child.queryProp("@item_tag"), childMonitored, childMonSection, allSelectors); //walk the type info
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
        StringBuffer resp_type = depTree->queryProp(xpath);
        if (resp_type.length() > 2)
        {
            if (resp_type.charAt(resp_type.length()-2)=='E' && resp_type.charAt(resp_type.length()-1)=='x')
                resp_type.setLength(resp_type.length()-2);
        }
        if (resp_type.length())
        {
            xpath.setf("EsdlStruct[@name='%s']", resp_type.str());
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
                Owned<IPropertyTree> ctxTree = createPTree();
                StringArray allSelectors;
                expandDiffTrees(ctxTree, *depTree, respTree, monitoringTemplate->queryPropTree(resp_type), xtrack, NULL, false, false, allSelectors);
                trimContext(ctxTree, *depTree, respTree);
                depTree->addPropTree("Context", LINK(ctxTree));
                if (allSelectors.length())
                {
                    IPropertyTree *selectorTree = depTree->addPropTree("Selectors", LINK(createPTree()));
                    ForEachItemIn(i1, allSelectors)
                        selectorTree->addProp("Selector", allSelectors.item(i1));
                }
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
