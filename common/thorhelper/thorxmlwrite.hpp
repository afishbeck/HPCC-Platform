/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

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

#ifndef THORXMLWRITE_HPP
#define THORXMLWRITE_HPP

#ifdef THORHELPER_EXPORTS
 #define thorhelper_decl DECL_EXPORT
#else
 #define thorhelper_decl DECL_IMPORT
#endif

#include "eclhelper.hpp"
#include "jptree.hpp"
#include "thorhelper.hpp"

interface IXmlStreamFlusher
{
    virtual void flushXML(StringBuffer &current, bool isClose) = 0;
};

interface IXmlWriterExt : extends IXmlWriter
{
    virtual IXmlWriterExt & clear() = 0;
    virtual size32_t length() const = 0;
    virtual const char *str() const = 0;
    virtual IInterface *saveLocation() const = 0;
    virtual void rewindTo(IInterface *location) = 0;
    virtual void outputNumericString(const char *field, const char *fieldname) = 0;
};

class thorhelper_decl CommonXmlPosition : public CInterface, implements IInterface
{
public:
    IMPLEMENT_IINTERFACE;

    CommonXmlPosition(size32_t _pos, unsigned _indent, unsigned _nestLimit, bool _tagClosed, bool _needDelimiter) :
        pos(_pos), indent(_indent), nestLimit(_nestLimit), tagClosed(_tagClosed), needDelimiter(_needDelimiter)
    {}

public:
    size32_t pos = 0;
    unsigned indent = 0;
    unsigned nestLimit = 0;
    bool tagClosed = false;
    bool needDelimiter = false;
};

class thorhelper_decl CommonXmlWriter : implements IXmlWriterExt, public CInterface
{
public:
    CommonXmlWriter(unsigned _flags, unsigned initialIndent=0,  IXmlStreamFlusher *_flusher=NULL);
    ~CommonXmlWriter();
    IMPLEMENT_IINTERFACE;

    void outputBeginNested(const char *fieldname, bool nestChildren, bool doIndent);
    void outputEndNested(const char *fieldname, bool doIndent);

    virtual void outputInlineXml(const char *text){closeTag(); out.append(text); flush(false);} //for appending raw xml content
    virtual void outputQuoted(const char *text);
    virtual void outputQString(unsigned len, const char *field, const char *fieldname);
    virtual void outputString(unsigned len, const char *field, const char *fieldname);
    virtual void outputBool(bool field, const char *fieldname);
    virtual void outputData(unsigned len, const void *field, const char *fieldname);
    virtual void outputInt(__int64 field, unsigned size, const char *fieldname);
    virtual void outputUInt(unsigned __int64 field, unsigned size, const char *fieldname);
    virtual void outputReal(double field, const char *fieldname);
    virtual void outputDecimal(const void *field, unsigned size, unsigned precision, const char *fieldname);
    virtual void outputUDecimal(const void *field, unsigned size, unsigned precision, const char *fieldname);
    virtual void outputUnicode(unsigned len, const UChar *field, const char *fieldname);
    virtual void outputUtf8(unsigned len, const char *field, const char *fieldname);
    virtual void outputBeginDataset(const char *dsname, bool nestChildren);
    virtual void outputEndDataset(const char *dsname);
    virtual void outputBeginNested(const char *fieldname, bool nestChildren);
    virtual void outputEndNested(const char *fieldname);
    virtual void outputBeginArray(const char *fieldname){}; //repeated elements are inline for xml
    virtual void outputEndArray(const char *fieldname){};
    virtual void outputSetAll();
    virtual void outputXmlns(const char *name, const char *uri);

    //IXmlWriterExt
    virtual IXmlWriterExt & clear();
    virtual unsigned length() const                                 { return out.length(); }
    virtual const char * str() const                                { return out.str(); }
    virtual IInterface *saveLocation() const
    {
        if (flusher)
            throwUnexpected();

        return new CommonXmlPosition(length(), indent, nestLimit, tagClosed, false);
    }
    virtual void rewindTo(IInterface *saved)
    {
        if (flusher)
            throwUnexpected();

        CommonXmlPosition *position = dynamic_cast<CommonXmlPosition *>(saved);
        if (!position)
            return;
        if (position->pos < out.length())
        {
            out.setLength(position->pos);
            tagClosed = position->tagClosed;
            indent = position->indent;
            nestLimit = position->nestLimit;
        }
    }

    virtual void outputNumericString(const char *field, const char *fieldname)
    {
        outputCString(field, fieldname);
    }

protected:
    bool checkForAttribute(const char * fieldname);
    void closeTag();
    inline void flush(bool isClose)
    {
        if (flusher)
            flusher->flushXML(out, isClose);
    }

protected:
    IXmlStreamFlusher *flusher;
    StringBuffer out;
    unsigned flags;
    unsigned indent;
    unsigned nestLimit;
    bool tagClosed;
};

class thorhelper_decl CommonJsonWriter : implements IXmlWriterExt, public CInterface
{
public:
    CommonJsonWriter(unsigned _flags, unsigned initialIndent=0,  IXmlStreamFlusher *_flusher=NULL);
    ~CommonJsonWriter();
    IMPLEMENT_IINTERFACE;

    void checkDelimit(int inc=0);
    void checkFormat(bool doDelimit, bool needDelimiter=true, int inc=0);
    void prepareBeginArray(const char *fieldname);

    virtual void outputInlineXml(const char *text) //for appending raw xml content
    {
        if (text && *text)
            outputUtf8(strlen(text), text, "xml");
    }
    virtual void outputQuoted(const char *text);
    virtual void outputQString(unsigned len, const char *field, const char *fieldname);
    virtual void outputString(unsigned len, const char *field, const char *fieldname);
    virtual void outputBool(bool field, const char *fieldname);
    virtual void outputData(unsigned len, const void *field, const char *fieldname);
    virtual void outputInt(__int64 field, unsigned size, const char *fieldname);
    virtual void outputUInt(unsigned __int64 field, unsigned size, const char *fieldname);
    virtual void outputReal(double field, const char *fieldname);
    virtual void outputDecimal(const void *field, unsigned size, unsigned precision, const char *fieldname);
    virtual void outputUDecimal(const void *field, unsigned size, unsigned precision, const char *fieldname);
    virtual void outputUnicode(unsigned len, const UChar *field, const char *fieldname);
    virtual void outputUtf8(unsigned len, const char *field, const char *fieldname);
    virtual void outputBeginDataset(const char *dsname, bool nestChildren);
    virtual void outputEndDataset(const char *dsname);
    virtual void outputBeginNested(const char *fieldname, bool nestChildren);
    virtual void outputEndNested(const char *fieldname);
    virtual void outputBeginArray(const char *fieldname);
    virtual void outputEndArray(const char *fieldname);
    virtual void outputSetAll();
    virtual void outputXmlns(const char *name, const char *uri){}
    virtual void outputNumericString(const char *field, const char *fieldname);

    //IXmlWriterExt
    virtual IXmlWriterExt & clear();
    virtual unsigned length() const                                 { return out.length(); }
    virtual const char * str() const                                { return out.str(); }
    virtual void rewindTo(unsigned int prevlen)                     { if (prevlen < out.length()) out.setLength(prevlen); }
    virtual IInterface *saveLocation() const
    {
        if (flusher)
            throwUnexpected();

        return new CommonXmlPosition(length(), indent, nestLimit, false, needDelimiter);
    }
    virtual void rewindTo(IInterface *saved)
    {
        if (flusher)
            throwUnexpected();

        CommonXmlPosition *position = dynamic_cast<CommonXmlPosition *>(saved);
        if (!position)
            return;
        if (position->pos < out.length())
        {
            out.setLength(position->pos);
            needDelimiter = position->needDelimiter;
            indent = position->indent;
            nestLimit = position->nestLimit;
        }
    }

    void outputBeginRoot(){out.append('{');}
    void outputEndRoot(){out.append('}');}

protected:
    inline void flush(bool isClose)
    {
        if (flusher)
            flusher->flushXML(out, isClose);
    }

    class CJsonWriterItem : public CInterface
    {
    public:
        CJsonWriterItem(const char *_name) : name(_name), depth(0){}

        StringAttr name;
        unsigned depth;
    };

    const char *checkItemName(CJsonWriterItem *item, const char *name, bool simpleType=true);
    const char *checkItemName(const char *name, bool simpleType=true);
    const char *checkItemNameBeginNested(const char *name);
    const char *checkItemNameEndNested(const char *name);
    bool checkUnamedArrayItem(bool begin);


    IXmlStreamFlusher *flusher;
    CIArrayOf<CJsonWriterItem> arrays;
    StringBuffer out;
    unsigned flags;
    unsigned indent;
    unsigned nestLimit;
    bool needDelimiter;
};

thorhelper_decl StringBuffer &buildJsonHeader(StringBuffer  &header, const char *suppliedHeader, const char *rowTag);
thorhelper_decl StringBuffer &buildJsonFooter(StringBuffer  &footer, const char *suppliedFooter, const char *rowTag);

//Writes type encoded XML strings  (xsi:type="xsd:string", xsi:type="xsd:boolean" etc)
class thorhelper_decl CommonEncodedXmlWriter : public CommonXmlWriter
{
public:
    CommonEncodedXmlWriter(unsigned _flags, unsigned initialIndent=0, IXmlStreamFlusher *_flusher=NULL);

    virtual void outputString(unsigned len, const char *field, const char *fieldname);
    virtual void outputBool(bool field, const char *fieldname);
    virtual void outputData(unsigned len, const void *field, const char *fieldname);
    virtual void outputInt(__int64 field, unsigned size, const char *fieldname);
    virtual void outputUInt(unsigned __int64 field, unsigned size, const char *fieldname);
    virtual void outputReal(double field, const char *fieldname);
    virtual void outputDecimal(const void *field, unsigned size, unsigned precision, const char *fieldname);
    virtual void outputUDecimal(const void *field, unsigned size, unsigned precision, const char *fieldname);
    virtual void outputUnicode(unsigned len, const UChar *field, const char *fieldname);
    virtual void outputUtf8(unsigned len, const char *field, const char *fieldname);
};

//Writes all encoded DATA fields as base64Binary
class thorhelper_decl CommonEncoded64XmlWriter : public CommonEncodedXmlWriter
{
public:
    CommonEncoded64XmlWriter(unsigned _flags, unsigned initialIndent=0, IXmlStreamFlusher *_flusher=NULL);

    virtual void outputData(unsigned len, const void *field, const char *fieldname);
};

enum XMLWriterType{WTStandard, WTEncoding, WTEncodingData64, WTJSON} ;
thorhelper_decl CommonXmlWriter * CreateCommonXmlWriter(unsigned _flags, unsigned initialIndent=0, IXmlStreamFlusher *_flusher=NULL, XMLWriterType xmlType=WTStandard);
thorhelper_decl IXmlWriterExt * createIXmlWriterExt(unsigned _flags, unsigned initialIndent=0, IXmlStreamFlusher *_flusher=NULL, XMLWriterType xmlType=WTStandard);

class thorhelper_decl SimpleOutputWriter : implements IXmlWriter, public CInterface
{
    void outputFieldSeparator();
    bool separatorNeeded;
public:
    SimpleOutputWriter();
    IMPLEMENT_IINTERFACE;

    SimpleOutputWriter & clear();
    unsigned length() const                                 { return out.length(); }
    const char * str() const                                { return out.str(); }

    virtual void outputQuoted(const char *text);
    virtual void outputQString(unsigned len, const char *field, const char *fieldname);
    virtual void outputString(unsigned len, const char *field, const char *fieldname);
    virtual void outputBool(bool field, const char *fieldname);
    virtual void outputData(unsigned len, const void *field, const char *fieldname);
    virtual void outputReal(double field, const char *fieldname);
    virtual void outputDecimal(const void *field, unsigned size, unsigned precision, const char *fieldname);
    virtual void outputUDecimal(const void *field, unsigned size, unsigned precision, const char *fieldname);
    virtual void outputUnicode(unsigned len, const UChar *field, const char *fieldname);
    virtual void outputUtf8(unsigned len, const char *field, const char *fieldname);
    virtual void outputBeginNested(const char *fieldname, bool nestChildren);
    virtual void outputEndNested(const char *fieldname);
    virtual void outputBeginDataset(const char *dsname, bool nestChildren){}
    virtual void outputEndDataset(const char *dsname){}
    virtual void outputBeginArray(const char *fieldname){}
    virtual void outputEndArray(const char *fieldname){}
    virtual void outputSetAll();
    virtual void outputInlineXml(const char *text){} //for appending raw xml content
    virtual void outputXmlns(const char *name, const char *uri){}

    virtual void outputInt(__int64 field, unsigned size, const char *fieldname);
    virtual void outputUInt(unsigned __int64 field, unsigned size, const char *fieldname);

    void newline();
protected:
    StringBuffer out;
};

class thorhelper_decl CommonFieldProcessor : implements IFieldProcessor, public CInterface
{
    bool trim;
    StringBuffer &result;
public:
    IMPLEMENT_IINTERFACE;
    CommonFieldProcessor(StringBuffer &_result, bool _trim=false);
    virtual void processString(unsigned len, const char *value, const RtlFieldInfo * field);
    virtual void processBool(bool value, const RtlFieldInfo * field);
    virtual void processData(unsigned len, const void *value, const RtlFieldInfo * field);
    virtual void processInt(__int64 value, const RtlFieldInfo * field);
    virtual void processUInt(unsigned __int64 value, const RtlFieldInfo * field);
    virtual void processReal(double value, const RtlFieldInfo * field);
    virtual void processDecimal(const void *value, unsigned digits, unsigned precision, const RtlFieldInfo * field);
    virtual void processUDecimal(const void *value, unsigned digits, unsigned precision, const RtlFieldInfo * field);
    virtual void processUnicode(unsigned len, const UChar *value, const RtlFieldInfo * field);
    virtual void processQString(unsigned len, const char *value, const RtlFieldInfo * field);
    virtual void processUtf8(unsigned len, const char *value, const RtlFieldInfo * field);

    virtual bool processBeginSet(const RtlFieldInfo * field, unsigned numElements, bool isAll, const byte *data);
    virtual bool processBeginDataset(const RtlFieldInfo * field, unsigned numRows);
    virtual bool processBeginRow(const RtlFieldInfo * field);
    virtual void processEndSet(const RtlFieldInfo * field);
    virtual void processEndDataset(const RtlFieldInfo * field);
    virtual void processEndRow(const RtlFieldInfo * field);

};

extern thorhelper_decl void printKeyedValues(StringBuffer &out, IIndexReadContext *segs, IOutputMetaData *rowMeta);

extern thorhelper_decl void convertRowToXML(size32_t & lenResult, char * & result, IOutputMetaData & info, const void * row, unsigned flags = (unsigned)-1);
extern thorhelper_decl void convertRowToJSON(size32_t & lenResult, char * & result, IOutputMetaData & info, const void * row, unsigned flags = (unsigned)-1);

#endif // THORXMLWRITE_HPP
