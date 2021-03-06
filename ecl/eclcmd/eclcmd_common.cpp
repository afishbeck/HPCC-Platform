/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

#include <stdio.h>
#include "jlog.hpp"
#include "jfile.hpp"
#include "jargv.hpp"
#include "junicode.hpp"
#include "build-config.h"
#include "workunit.hpp"

#include "eclcmd_common.hpp"

void outputMultiExceptions(const IMultiException &me)
{
    fprintf(stderr, "\nException(s):\n");
    aindex_t count = me.ordinality();
    for (aindex_t i=0; i<count; i++)
    {
        IException& e = me.item(i);
        StringBuffer msg;
        fprintf(stderr, "%d: %s\n", e.errorCode(), e.errorMessage(msg).str());
    }
    fprintf(stderr, "\n");
}

bool extractEclCmdOption(StringBuffer & option, IProperties * globals, const char * envName, const char * propertyName, const char * defaultPrefix, const char * defaultSuffix)
{
    if (option.length())        // check if already specified via a command line option
        return true;
    if (propertyName && globals->getProp(propertyName, option))
        return true;
    if (envName && *envName)
    {
        const char * env = getenv(envName);
        if (env)
        {
            option.append(env);
            return true;
        }
    }
    if (defaultPrefix)
        option.append(defaultPrefix);
    if (defaultSuffix)
        option.append(defaultSuffix);
    return false;
}

bool extractEclCmdOption(StringAttr & option, IProperties * globals, const char * envName, const char * propertyName, const char * defaultPrefix, const char * defaultSuffix)
{
    if (option)
        return true;
    StringBuffer temp;
    bool ret = extractEclCmdOption(temp, globals, envName, propertyName, defaultPrefix, defaultSuffix);
    option.set(temp.str());
    return ret;
}

bool extractEclCmdOption(bool & option, IProperties * globals, const char * envName, const char * propertyName, bool defval)
{
    StringBuffer temp;
    bool ret = extractEclCmdOption(temp, globals, envName, propertyName, defval ? "1" : "0", NULL);
    option=(streq(temp.str(),"1")||strieq(temp.str(),"true"));
    return ret;
}

bool extractEclCmdOption(unsigned & option, IProperties * globals, const char * envName, const char * propertyName, unsigned defval)
{
    StringBuffer temp;
    bool ret = extractEclCmdOption(temp, globals, envName, propertyName, NULL, NULL);
    option = (ret) ? strtoul(temp.str(), NULL, 10) : defval;
    return ret;
}

static bool looksLikeOnlyAWuid(const char * wuid)
{
    if (!wuid)
        return false;
    if (wuid[0] != 'W')
        return false;
    if (!isdigit(wuid[1]) || !isdigit(wuid[2]) || !isdigit(wuid[3]) || !isdigit(wuid[4]))
        return false;
    if (!isdigit(wuid[5]) || !isdigit(wuid[6]) || !isdigit(wuid[7]) || !isdigit(wuid[8]))
        return false;
    if (wuid[9]!='-')
        return false;
    for (int i=10; wuid[i]; i++)
        if (wuid[i]!='-' && !isdigit(wuid[i]))
            return false;
    return true;
}

//=========================================================================================

#define PE_OFFSET_LOCATION_IN_DOS_SECTION 0x3C

EclObjectParameter::EclObjectParameter(unsigned _accept) : type(eclObjTypeUnknown), accept(_accept)
{
}

bool EclObjectParameter::isElfContent()
{
    const char *s = mb.toByteArray();
    return (s[0]==0x7F && s[1]=='E' && s[2]=='L' && s[3]=='F');
}

bool EclObjectParameter::isPEContent()
{
    if (mb.length()<=PE_OFFSET_LOCATION_IN_DOS_SECTION)
        return false;
    const char *buffer = mb.toByteArray();
    if (memcmp("MZ", buffer, 2)!=0)
        return false;
    unsigned long PESectStart = *((unsigned long *)(buffer + PE_OFFSET_LOCATION_IN_DOS_SECTION));
    if (mb.length()<PESectStart+3)
        return false;

    if (memcmp("PE", buffer + PESectStart, 2)!=0)
        return false;
    return true;
}

void EclObjectParameter::ensureUtf8Content()
{
    //If looks like utf16 then convert it to utf8
    const void * zero = memchr(mb.bufferBase(), 0, mb.length());
    if (zero)
    {
        MemoryBuffer translated;
        if (convertToUtf8(translated, mb.length(), mb.bufferBase()))
            mb.swapWith(translated);
        else
            throw MakeStringException(1, "%s content doesn't appear to be UTF8", value.get());
    }
}

eclObjParameterType EclObjectParameter::finalizeContentType()
{
    if (accept & eclObjSharedObject && (isElfContent() || isPEContent()))
        type = eclObjSharedObject;
    else if (accept & eclObjSourceOrArchive)
    {
        ensureUtf8Content();
        size32_t len = mb.length();
        mb.append((byte)0);
        const char *root = skipLeadingXml(mb.toByteArray());
        if (isArchiveQuery(root))
            type = eclObjArchive;
        else if (isQueryManifest(root))
            type = eclObjManifest;
        else
            type = eclObjSource;
        mb.setLength(len);
    }
    else
        mb.setLength(0);
    mb.truncate();
    return type;
}
#define STDIO_BUFFSIZE 0x10000     // 64K

void EclObjectParameter::loadStdIn()
{
    Owned<IFile> file = createIFile("stdin:");
    Owned<IFileIO> io = file->openShared(IFOread, IFSHread);
    if (!io)
        throw MakeStringException(1, "stdin could not be opened");
    size32_t rd;
    size32_t sizeRead = 0;
    do {
        rd = io->read(sizeRead, STDIO_BUFFSIZE, mb.reserve(STDIO_BUFFSIZE+1)); // +1 as will need
        sizeRead += rd;
        mb.setLength(sizeRead);
    } while (rd);
    finalizeContentType();
}

void EclObjectParameter::loadFile()
{
    Owned<IFile> file = createIFile(value.get());
    Owned<IFileIO> io = file->openShared(IFOread, IFSHread);
    if (!io)
        throw MakeStringException(1, "File %s could not be opened", file->queryFilename());

    offset_t size = io->size();
    size32_t sizeToRead = (size32_t)size;
    if (sizeToRead != size)
        throw MakeStringException(1, "File %s is larger than 4Gb", file->queryFilename());

    mb.ensureCapacity(sizeToRead+1);
    byte * contents = static_cast<byte *>(mb.reserve(sizeToRead));
    size32_t sizeRead = io->read(0, sizeToRead, contents);
    if (sizeRead != sizeToRead)
        throw MakeStringException(1, "File %s only read %u of %u bytes", file->queryFilename(), sizeRead, sizeToRead);
    finalizeContentType();
}

eclObjParameterType EclObjectParameter::set(const char *param)
{
    value.set(param);
    if (streq(param, "stdin"))
        loadStdIn();
    else if (checkFileExists(param))
        loadFile();
    else if (looksLikeOnlyAWuid(param))
        type = eclObjWuid;
    return type;
}

const char *EclObjectParameter::queryTypeName()
{
    switch (type)
    {
    case eclObjWuid:
        return "WUID";
    case eclObjSource:
        return "ECL Text";
    case eclObjArchive:
        return "ECL Archive";
    case eclObjManifest:
        return "ECL Manifest";
    case eclObjSharedObject:
        return "ECL Shared Object";
    default:
        return "Unknown Type";
    }
}

StringBuffer &EclObjectParameter::getDescription(StringBuffer &s)
{
    s.append(queryTypeName()).append(' ');
    if (streq(value.sget(), "stdin"))
        s.append("from ");
    return s.append(value.get());
}

eclCmdOptionMatchIndicator EclCmdCommon::matchCommandLineOption(ArgvIterator &iter, bool finalAttempt)
{
    bool boolValue;
    if (iter.matchFlag(boolValue, ECLOPT_VERSION))
    {
        fprintf(stdout, "%s\n", BUILD_TAG);
        return EclCmdOptionCompletion;
    }
    if (iter.matchOption(optServer, ECLOPT_SERVER)||iter.matchOption(optServer, ECLOPT_SERVER_S))
        return EclCmdOptionMatch;
    if (iter.matchOption(optPort, ECLOPT_PORT))
        return EclCmdOptionMatch;
    if (iter.matchOption(optUsername, ECLOPT_USERNAME)||iter.matchOption(optUsername, ECLOPT_USERNAME_S))
        return EclCmdOptionMatch;
    if (iter.matchOption(optPassword, ECLOPT_PASSWORD)||iter.matchOption(optPassword, ECLOPT_PASSWORD_S))
        return EclCmdOptionMatch;
    if (iter.matchFlag(optVerbose, ECLOPT_VERBOSE) || iter.matchFlag(optVerbose, ECLOPT_VERBOSE_S))
        return EclCmdOptionMatch;

    StringAttr tempArg;
    if (iter.matchOption(tempArg, "-brk"))
    {
#if defined(_WIN32) && defined(_DEBUG)
        unsigned id = atoi(tempArg.sget());
        if (id == 0)
            DebugBreak();
        else
            _CrtSetBreakAlloc(id);
#endif
        return EclCmdOptionMatch;
    }
    if (finalAttempt)
        fprintf(stderr, "\n%s option not recognized\n", iter.query());
    return EclCmdOptionNoMatch;
}

bool EclCmdCommon::finalizeOptions(IProperties *globals)
{
    extractEclCmdOption(optServer, globals, ECLOPT_SERVER_ENV, ECLOPT_SERVER_INI, ECLOPT_SERVER_DEFAULT, NULL);
    extractEclCmdOption(optPort, globals, ECLOPT_PORT_ENV, ECLOPT_PORT_INI, ECLOPT_PORT_DEFAULT, NULL);
    extractEclCmdOption(optUsername, globals, ECLOPT_USERNAME_ENV, ECLOPT_USERNAME_INI, NULL, NULL);
    extractEclCmdOption(optPassword, globals, ECLOPT_PASSWORD_ENV, ECLOPT_PASSWORD_INI, NULL, NULL);

    if (!optVerbose)
    {
        Owned<ILogMsgFilter> filter = getCategoryLogMsgFilter(MSGAUD_user, MSGCLS_error);
        queryLogMsgManager()->changeMonitorFilter(queryStderrLogMsgHandler(), filter);
    }

    return true;
}

class ConvertEclParameterToArchive
{
public:
    ConvertEclParameterToArchive(EclCmdWithEclTarget &_cmd) : cmd(_cmd)
    {
    }

    void appendOptPath(StringBuffer &cmdLine, const char opt, const char *path)
    {
        if (!path || !*path)
            return;
        if (*path==';')
            path++;
        cmdLine.append(" -").append(opt).append(path);
    }

    void buildCmd(StringBuffer &cmdLine)
    {
        cmdLine.set("eclcc -E");
        appendOptPath(cmdLine, 'I', cmd.optImpPath.str());
        appendOptPath(cmdLine, 'L', cmd.optLibPath.str());
        if (cmd.optAttributePath.length())
            cmdLine.append(" -main ").append(cmd.optAttributePath.get());
        if (cmd.optObj.type == eclObjManifest)
            cmdLine.append(" -manifest ").append(cmd.optObj.value.get());
        else
        {
            if (cmd.optManifest.length())
                cmdLine.append(" -manifest ").append(cmd.optManifest.get());
            if (cmd.optObj.value.get())
                cmdLine.append(" ").append(streq(cmd.optObj.value.get(), "stdin") ? "- " : cmd.optObj.value.get());
        }
        if (cmd.debugValues.length())
        {
            ForEachItemIn(i, cmd.debugValues)
            {
                IEspNamedValue &item = cmd.debugValues.item(i);
                const char *name = item.getName();
                const char *value = item.getValue();
                cmdLine.append(" -f").append(name);
                if (value)
                    cmdLine.append('=').append(value);
            }
        }
        if ((int)cmd.optResultLimit > 0)
        {
            cmdLine.append(" -fapplyInstantEclTransformations=1");
            cmdLine.append(" -fapplyInstantEclTransformationsLimit=").append(cmd.optResultLimit);
        }
    }

    bool eclcc(StringBuffer &out)
    {
        StringBuffer cmdLine;
        buildCmd(cmdLine);

        Owned<IPipeProcess> pipe = createPipeProcess();
        bool hasInput = streq(cmd.optObj.value.sget(), "stdin");
        pipe->run(cmd.optVerbose ? "EXEC" : NULL, cmdLine.str(), NULL, hasInput, true, true);

        StringBuffer errors;
        Owned<EclCmdErrorReader> errorReader = new EclCmdErrorReader(pipe, errors);
        errorReader->start();

        if (pipe->hasInput())
        {
            pipe->write(cmd.optObj.mb.length(), cmd.optObj.mb.toByteArray());
            pipe->closeInput();
        }
        if (pipe->hasOutput())
        {
           byte buf[4096];
           loop
           {
                size32_t read = pipe->read(sizeof(buf),buf);
                if (!read)
                    break;
                out.append(read, (const char *) buf);
            }
        }
        int retcode = pipe->wait();
        errorReader->join();

        if (errors.length())
            fprintf(stderr, "%s\n", errors.str());

        return (retcode == 0);
    }

    bool process()
    {
        if ((cmd.optObj.type!=eclObjSource && cmd.optObj.type!=eclObjManifest) || cmd.optObj.value.isEmpty())
            return false;

        StringBuffer output;
        if (eclcc(output) && output.length() && isArchiveQuery(output.str()))
        {
            cmd.optObj.type = eclObjArchive;
            cmd.optObj.mb.clear().append(output.str());
            return true;
        }
        fprintf(stderr,"\nError creating archive\n");
        return false;
    }

private:
    EclCmdWithEclTarget &cmd;

    class EclCmdErrorReader : public Thread
    {
    public:
        EclCmdErrorReader(IPipeProcess *_pipe, StringBuffer &_errs)
            : Thread("EclToArchive::ErrorReader"), pipe(_pipe), errs(_errs)
        {
        }

        virtual int run()
        {
           byte buf[4096];
           loop
           {
                size32_t read = pipe->readError(sizeof(buf), buf);
                if (!read)
                    break;
                errs.append(read, (const char *) buf);
            }
            return 0;
        }
    private:
        IPipeProcess *pipe;
        StringBuffer &errs;
    };
};

bool matchVariableOption(ArgvIterator &iter, const char prefix, IArrayOf<IEspNamedValue> &values)
{
    const char *arg = iter.query();
    if (*arg++!='-' || *arg++!=prefix || !*arg)
        return false;
    Owned<IEspNamedValue> nv = createNamedValue();
    const char *eq = strchr(arg, '=');
    if (!eq)
        nv->setName(arg);
    else
    {
        StringAttr name(arg, eq - arg);
        nv->setName(name.get());
        nv->setValue(eq+1);
    }
    values.append(*nv.getClear());
    return true;
}
eclCmdOptionMatchIndicator EclCmdWithEclTarget::matchCommandLineOption(ArgvIterator &iter, bool finalAttempt)
{
    const char *arg = iter.query();
    if (streq(arg, "-"))
    {
        optObj.set("stdin");
        return EclCmdOptionMatch;
    }
    if (*arg!='-')
    {
        if (optObj.value.length())
        {
            if (optObj.type==eclObjTypeUnknown && (optObj.accept & eclObjQuery) && !optObj.query.length())
            {
                optObj.type=eclObjQuery;
                optObj.query.set(arg);
                return EclCmdOptionMatch;
            }

            fprintf(stderr, "\nunrecognized argument %s\n", arg);
            return EclCmdOptionCompletion;
        }
        optObj.set(arg);
        return EclCmdOptionMatch;
    }
    if (matchVariableOption(iter, 'f', debugValues))
        return EclCmdOptionMatch;
    if (iter.matchPathFlag(optLibPath, ECLOPT_LIB_PATH_S))
        return EclCmdOptionMatch;
    if (iter.matchPathFlag(optImpPath, ECLOPT_IMP_PATH_S))
        return EclCmdOptionMatch;
    if (iter.matchOption(optManifest, ECLOPT_MANIFEST) || iter.matchOption(optManifest, ECLOPT_MANIFEST_DASH))
        return EclCmdOptionMatch;
    if (iter.matchOption(optAttributePath, ECLOPT_MAIN) || iter.matchOption(optAttributePath, ECLOPT_MAIN_S))
        return EclCmdOptionMatch;
    if (iter.matchFlag(optNoArchive, ECLOPT_ECL_ONLY))
        return EclCmdOptionMatch;
    if (iter.matchOption(optResultLimit, ECLOPT_RESULT_LIMIT))
        return EclCmdOptionMatch;

    return EclCmdCommon::matchCommandLineOption(iter, finalAttempt);
}

bool EclCmdWithEclTarget::finalizeOptions(IProperties *globals)
{
    if (!EclCmdCommon::finalizeOptions(globals))
        return false;

    if (optObj.type == eclObjTypeUnknown)
    {
        if (optAttributePath.length())
        {
            optNoArchive=true;
            optObj.type = eclObjSource;
            optObj.value.set(optAttributePath.get());
            StringBuffer text(optAttributePath.get());
            text.append("();");
            optObj.mb.append(text.str());
        }
        else if (optManifest.length())
        {
            optObj.set(optManifest.get()); //treat as stand alone manifest that must declare ecl to compile
            optManifest.clear();
        }
    }

    if (!optNoArchive && (optObj.type == eclObjSource || optObj.type == eclObjManifest))
    {
        ConvertEclParameterToArchive conversion(*this);
        if (!conversion.process())
            return false;
    }
    if (optResultLimit == (unsigned)-1)
        extractEclCmdOption(optResultLimit, globals, ECLOPT_RESULT_LIMIT_ENV, ECLOPT_RESULT_LIMIT_INI, 0);

    return true;

}

eclCmdOptionMatchIndicator EclCmdWithQueryTarget::matchCommandLineOption(ArgvIterator &iter, bool finalAttempt)
{
    const char *arg = iter.query();
    if (*arg!='-')
    {
        if (!optQuerySet.length())
            optQuerySet.set(arg);
        else if (!optQuery.length())
            optQuery.set(arg);
        else
        {
            fprintf(stderr, "\nunrecognized argument %s\n", arg);
            return EclCmdOptionCompletion;
        }
        return EclCmdOptionMatch;
    }
    StringAttr optTemp; //backward compatible with --queryset option
    if (iter.matchOption(optTemp, ECLOPT_QUERYSET)||iter.matchOption(optTemp, ECLOPT_QUERYSET_S))
    {
        if (!optQuerySet.length())
            optQuerySet.set(optTemp.get());
        else if (streq(optQuerySet.get(), optTemp.get()))
            return EclCmdOptionMatch;
        else if (optQuery.isEmpty())
        {
            optQuery.set(optQuerySet.get());
            optQuerySet.set(optTemp.get());
            return EclCmdOptionMatch;
        }
        fprintf(stderr, "\nunrecognized argument %s\n", optQuery.sget());
            return EclCmdOptionCompletion;
    }
    return EclCmdCommon::matchCommandLineOption(iter, true);
}

bool EclCmdWithQueryTarget::finalizeOptions(IProperties *globals)
{
    if (optQuerySet.isEmpty())
    {
        fprintf(stderr, "\nError: queryset parameter required\n");
        return false;
    }
    if (optQuery.isEmpty())
    {
        fprintf(stderr, "\nError: query parameter required\n");
        return false;
    }

    return EclCmdCommon::finalizeOptions(globals);
}

bool EclCmdWithQueryTarget::parseCommandLineOptions(ArgvIterator &iter)
{
    if (iter.done())
    {
        usage();
        return false;
    }

    for (; !iter.done(); iter.next())
    {
        if (EclCmdWithQueryTarget::matchCommandLineOption(iter, true)!=EclCmdOptionMatch)
            return false;
    }
    return true;
}
