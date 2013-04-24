/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems.

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
#include "hql.hpp"
#include "hqlatoms.hpp"

_ATOM abstractAtom;
_ATOM accessAtom;
_ATOM actionAtom;
_ATOM activeAtom;
_ATOM activeFailureAtom;
_ATOM activeNlpAtom;
_ATOM afterAtom;
_ATOM aggregateAtom;
_ATOM algorithmAtom;
_ATOM allAtom;
_ATOM allocatorAtom;
_ATOM alreadyVisitedAtom;
_ATOM _alreadyAssignedNestedTag_Atom;
_ATOM _alreadyVisitedMarker_Atom;
_ATOM alwaysAtom;
_ATOM _array_Atom;
_ATOM asciiAtom;
_ATOM assertAtom;
_ATOM assertConstAtom;
_ATOM atAtom;
_ATOM atmostAtom;
_ATOM _attrAligned_Atom;
_ATOM _attrDiskSerializedForm_Atom;
_ATOM _attrInternalSerializedForm_Atom;
_ATOM _attrLocationIndependent_Atom;
_ATOM _attrRecordCount_Atom;
_ATOM _attrSize_Atom;
_ATOM _attrUnadorned_Atom;
_ATOM aveAtom;
_ATOM backupAtom;
_ATOM bcdAtom;
_ATOM beforeAtom;
_ATOM bestAtom;
_ATOM bindBooleanParamAtom;
_ATOM bindDataParamAtom;
_ATOM bindRealParamAtom;
_ATOM bindSetParamAtom;
_ATOM bindSignedParamAtom;
_ATOM bindStringParamAtom;
_ATOM bindVStringParamAtom;
_ATOM bindUnicodeParamAtom;
_ATOM bindUnsignedParamAtom;
_ATOM bindUtf8ParamAtom;
_ATOM bitmapAtom;
_ATOM blobAtom;
_ATOM cAtom;
_ATOM cardinalityAtom;
_ATOM caseAtom;
_ATOM checkinAtom;
_ATOM checkoutAtom;
_ATOM _childAttr_Atom;
_ATOM choosenAtom;
_ATOM clusterAtom;
_ATOM _colocal_Atom;
_ATOM commonAtom;
_ATOM compileEmbeddedScriptAtom;
_ATOM _complexKeyed_Atom;
_ATOM compressedAtom;
_ATOM __compressed__Atom;
_ATOM _conditionFolded_Atom;
_ATOM constAtom;
_ATOM contextAtom;
_ATOM contextSensitiveAtom;
_ATOM costAtom;
_ATOM countAtom;
_ATOM _countProject_Atom;
_ATOM cppAtom;
_ATOM _cppBody_Atom;
_ATOM csvAtom;
_ATOM ctxmethodAtom;
_ATOM dataAtom;
_ATOM _dataset_Atom;
_ATOM debugAtom;
_ATOM dedupAtom;
_ATOM defaultAtom;
_ATOM _default_Atom;
_ATOM defaultFieldNameAtom;
_ATOM defineAtom;
_ATOM definitionAtom;
_ATOM deprecatedAtom;
_ATOM descAtom;
_ATOM diskAtom;
_ATOM distributedAtom;
_ATOM _distributed_Atom;
_ATOM _dot_Atom;
_ATOM dynamicAtom;
_ATOM ebcdicAtom;
_ATOM eclrtlAtom;
_ATOM embeddedAtom;
_ATOM _empty_str_Atom;
_ATOM encodingAtom;
_ATOM encryptAtom;
_ATOM ensureAtom;
_ATOM enthAtom;
_ATOM entrypointAtom;
_ATOM errorAtom;
_ATOM exceptAtom;
_ATOM exclusiveAtom;
_ATOM expireAtom;
_ATOM exportAtom;
_ATOM extendAtom;
_ATOM externalAtom;
_ATOM failAtom;
_ATOM failureAtom;
_ATOM falseAtom;
_ATOM fastAtom;
_ATOM fewAtom;
_ATOM fieldAtom;
_ATOM fieldsAtom;
_ATOM __fileposAtom;
_ATOM filenameAtom;
_ATOM filepositionAtom;
_ATOM _files_Atom;
_ATOM filterAtom;
_ATOM filteredAtom;
_ATOM _filtered_Atom;
_ATOM firstAtom;
_ATOM firstLeftAtom;
_ATOM firstRightAtom;
_ATOM fixedAtom;
_ATOM flagAtom;
_ATOM flagsAtom;
_ATOM flatAtom;
_ATOM _folded_Atom;
_ATOM formatAtom;
_ATOM forwardAtom;
_ATOM fullonlyAtom;
_ATOM fullouterAtom;
_ATOM _function_Atom;
_ATOM globalContextAtom;
_ATOM gctxmethodAtom;
_ATOM getAtom;
_ATOM getEmbedContextAtom;
_ATOM getBooleanResultAtom;
_ATOM getDataResultAtom;
_ATOM getRealResultAtom;
_ATOM getSetResultAtom;
_ATOM getSignedResultAtom;
_ATOM getStringResultAtom;
_ATOM getUnicodeResultAtom;
_ATOM getUnsignedResultAtom;
_ATOM getUTF8ResultAtom;
_ATOM globalAtom;
_ATOM graphAtom;
_ATOM groupAtom;
_ATOM groupedAtom;
_ATOM hashAtom;
_ATOM headingAtom;
_ATOM _hidden_Atom;
_ATOM hintAtom;
_ATOM holeAtom;
_ATOM holeposAtom;
_ATOM __ifblockAtom;
_ATOM ignoreAtom;
_ATOM ignoreBaseAtom;
_ATOM implementsAtom;
_ATOM _implicitFpos_Atom;
_ATOM _implicitSorted_Atom;
_ATOM importAtom;
_ATOM includeAtom;
_ATOM indeterminateAtom;
_ATOM indexAtom;
_ATOM initfunctionAtom;
_ATOM inlineAtom;
_ATOM innerAtom;
_ATOM interfaceAtom;
_ATOM internalAtom;
_ATOM _internal_Atom;
_ATOM internalFlagsAtom;
_ATOM _isFunctional_Atom;
_ATOM _isBlobInIndex_Atom;
_ATOM isNullAtom;
_ATOM isValidAtom;
_ATOM jobAtom;
_ATOM jobTempAtom;
_ATOM keepAtom;
_ATOM keyedAtom;
_ATOM labeledAtom;
_ATOM languageAtom;
_ATOM lastAtom;
_ATOM leftAtom;
_ATOM leftonlyAtom;
_ATOM leftouterAtom;
_ATOM libraryAtom;
_ATOM lightweightAtom;
_ATOM _lightweight_Atom;
_ATOM limitAtom;
_ATOM lineIdAtom;
_ATOM linkAtom;
_ATOM _linkCounted_Atom;
_ATOM literalAtom;
_ATOM loadAtom;
_ATOM localAtom;
_ATOM localUploadAtom;
_ATOM localeAtom;
_ATOM localFilePositionAtom;
_ATOM _location_Atom;
_ATOM logAtom;
_ATOM logicalFilenameAtom;
_ATOM lookupAtom;
_ATOM lzwAtom;
_ATOM macroAtom;
_ATOM manyAtom;
_ATOM markerAtom;
_ATOM matchxxxPseudoFileAtom;
_ATOM maxAtom;
_ATOM maxCountAtom;
_ATOM maxLengthAtom;
_ATOM maxSizeAtom;
_ATOM mergeAtom;
_ATOM mergeTransformAtom;
_ATOM _metadata_Atom;
_ATOM methodAtom;
_ATOM minAtom;
_ATOM minimalAtom;
_ATOM memoryAtom;
_ATOM moduleAtom;
_ATOM mofnAtom;
_ATOM nameAtom;
_ATOM namedAtom;
_ATOM namespaceAtom;
_ATOM newAtom;
_ATOM newSetAtom;
_ATOM noBoundCheckAtom;
_ATOM noCaseAtom;
_ATOM _noHoist_Atom;
_ATOM noLocalAtom;
_ATOM _nonEmpty_Atom;
_ATOM noOverwriteAtom;
_ATOM _normalized_Atom;
_ATOM noRootAtom;
_ATOM noScanAtom;
_ATOM noSortAtom;
_ATOM _noStreaming_Atom;
_ATOM notAtom;
_ATOM notMatchedAtom;
_ATOM notMatchedOnlyAtom;
_ATOM noTrimAtom;
_ATOM noTypeAtom;
_ATOM noXpathAtom;
_ATOM oldSetFormatAtom;
_ATOM omethodAtom;
_ATOM _omitted_Atom;
_ATOM onceAtom;
_ATOM onFailAtom;
_ATOM onWarningAtom;
_ATOM optAtom;
_ATOM _ordered_Atom;
_ATOM _orderedPull_Atom;
_ATOM _origin_Atom;
_ATOM _original_Atom;
_ATOM outAtom;
_ATOM outoflineAtom;
_ATOM outputAtom;
_ATOM overwriteAtom;
_ATOM ownedAtom;
_ATOM packedAtom;
_ATOM parallelAtom;
_ATOM parameterAtom;
_ATOM _parameterScopeType_Atom;
_ATOM partitionAtom;
_ATOM partitionLeftAtom;
_ATOM partitionRightAtom;
_ATOM _payload_Atom;
_ATOM persistAtom;
_ATOM physicalFilenameAtom;
_ATOM physicalLengthAtom;
_ATOM pluginAtom;
_ATOM prefetchAtom;
_ATOM preloadAtom;
_ATOM priorityAtom;
_ATOM privateAtom;
_ATOM pseudoentrypointAtom;
_ATOM pullAtom;
_ATOM pulledAtom;
_ATOM pureAtom;
_ATOM quoteAtom;
_ATOM randomAtom;
_ATOM rangeAtom;
_ATOM rawAtom;
_ATOM recordAtom;
_ATOM recursiveAtom;
_ATOM referenceAtom;
_ATOM refreshAtom;
_ATOM _remote_Atom;
_ATOM renameAtom;
_ATOM repeatAtom;
_ATOM _resourced_Atom;
_ATOM responseAtom;
_ATOM restartAtom;
_ATOM resultAtom;
_ATOM _results_Atom;
_ATOM retryAtom;
_ATOM rightAtom;
_ATOM rightonlyAtom;
_ATOM rightouterAtom;
_ATOM rollbackAtom;
_ATOM _root_Atom;
_ATOM rowAtom;
_ATOM _rowsid_Atom;
_ATOM rowLimitAtom;
_ATOM ruleAtom;
_ATOM saveAtom;
_ATOM scanAtom;
_ATOM scanAllAtom;
_ATOM scopeAtom;
_ATOM scopeCheckingAtom;
_ATOM sectionAtom;
_ATOM _selectors_Atom;
_ATOM _selectorSequence_Atom;
_ATOM selfAtom;
_ATOM separatorAtom;
_ATOM escapeAtom;
_ATOM sequenceAtom;
_ATOM _sequence_Atom;
_ATOM sequentialAtom;
_ATOM serializationAtom;
_ATOM setAtom;
_ATOM sharedAtom;
_ATOM shutdownAtom;
_ATOM _sideEffect_Atom;
_ATOM sizeAtom;
_ATOM sizeofAtom;
_ATOM skewAtom;
_ATOM skipAtom;
_ATOM singleAtom;
_ATOM snapshotAtom;
_ATOM soapActionAtom;
_ATOM syntaxCheckAtom;
_ATOM httpHeaderAtom;
_ATOM prototypeAtom;
_ATOM proxyAddressAtom;
_ATOM sort_AllAtom;
_ATOM sort_KeyedAtom;
_ATOM sortedAtom;
_ATOM sourceAtom;
_ATOM stableAtom;
_ATOM _state_Atom;
_ATOM steppedAtom;
_ATOM storeAtom;
_ATOM storedAtom;
_ATOM streamedAtom;
_ATOM _streaming_Atom;
_ATOM successAtom;
_ATOM supportsImportAtom;
_ATOM supportsScriptAtom;
_ATOM sysAtom;
_ATOM tempAtom;
_ATOM templateAtom;
_ATOM terminateAtom;
_ATOM terminatorAtom;
_ATOM thorAtom;
_ATOM thresholdAtom;
_ATOM timeoutAtom;
_ATOM timeLimitAtom;
_ATOM timestampAtom;
_ATOM tinyAtom;
_ATOM trimAtom;
_ATOM trueAtom;
_ATOM tomitaAtom;
_ATOM topAtom;
_ATOM typeAtom;
_ATOM _uid_Atom;
_ATOM unnamedAtom;
_ATOM unknownAtom;
_ATOM unknownSizeFieldAtom;
_ATOM unicodeAtom;
_ATOM unorderedAtom;
_ATOM unsortedAtom;
_ATOM unstableAtom;
_ATOM updateAtom;
_ATOM userMatchFunctionAtom;
_ATOM valueAtom;
_ATOM versionAtom;
_ATOM virtualAtom;
_ATOM volatileAtom;
_ATOM warningAtom;
_ATOM wholeAtom;
_ATOM widthAtom;
_ATOM wipeAtom;
_ATOM _workflow_Atom;
_ATOM _workflowPersist_Atom;
_ATOM workunitAtom;
_ATOM wuidAtom;
_ATOM xmlAtom;
_ATOM xmlDefaultAtom;
_ATOM xpathAtom;

#define MAKEATOM(x) x##Atom = createLowerCaseAtom(#x)
#define MAKEID(x)   x##IdAtom = createIdentifierAtom(#x)

SysAtom::SysAtom(const void * k) : Atom(k) 
{ 
    attrId = EAnone; 
}

class SysAtomTable : public KeptHashTableOf<SysAtom, 0U>
{
  public:
    SysAtomTable() : KeptHashTableOf<SysAtom, 0U>(false) {};
    SysAtomTable(bool _nocase) : KeptHashTableOf<SysAtom, 0U>(_nocase) {};
};
SysAtomTable * sysAtomTable;

_ATOM createSystemAtom(const char * s) { return sysAtomTable->create(s); }

#define MAKESYSATOM(x)  _##x##_Atom = createSystemAtom("$_" #x "_")
#define MAKESYSATOMX(x)  static_cast<SysAtom *>(_##x##_Atom = createSystemAtom("$_" #x "_"))

MODULE_INIT(INIT_PRIORITY_HQLATOM)
{
    sysAtomTable = new SysAtomTable;
    MAKEATOM(abstract);
    MAKEATOM(access);
    MAKEATOM(action);
    MAKEATOM(active);
    MAKEATOM(activeFailure);
    MAKEATOM(activeNlp);
    MAKEATOM(after);
    MAKEATOM(aggregate);
    MAKEATOM(algorithm);
    MAKEATOM(all);
    MAKEATOM(allocator);
    MAKEATOM(alreadyVisited);
    MAKESYSATOM(alreadyAssignedNestedTag);
    MAKESYSATOM(alreadyVisitedMarker);
    MAKEATOM(always);
    MAKEATOM(_array_);
    MAKEATOM(ascii);
    MAKEATOM(assert);
    MAKEATOM(assertConst);
    MAKEATOM(at);
    MAKEATOM(atmost);
    MAKESYSATOMX(attrAligned)->setAttrId(EAaligned);
    MAKESYSATOMX(attrLocationIndependent)->setAttrId(EAlocationIndependent);
    MAKESYSATOMX(attrRecordCount)->setAttrId(EArecordCount);
    MAKESYSATOMX(attrDiskSerializedForm)->setAttrId(EAdiskserializedForm);
    MAKESYSATOMX(attrInternalSerializedForm)->setAttrId(EAinternalserializedForm);
    MAKESYSATOMX(attrSize)->setAttrId(EAsize);
    MAKESYSATOMX(attrUnadorned)->setAttrId(EAunadorned);
    MAKEATOM(ave);
    MAKEATOM(backup);
    MAKEATOM(bcd);
    MAKEATOM(before);
    MAKEATOM(best);
    MAKEATOM(bindBooleanParam);
    MAKEATOM(bindDataParam);
    MAKEATOM(bindRealParam);
    MAKEATOM(bindSetParam);
    MAKEATOM(bindSignedParam);
    MAKEATOM(bindStringParam);
    MAKEATOM(bindVStringParam);
    MAKEATOM(bindUnicodeParam);
    MAKEATOM(bindUnsignedParam);
    MAKEATOM(bindUtf8Param);
    MAKEATOM(bitmap);
    MAKEATOM(blob);
    MAKEATOM(c);
    MAKEATOM(cardinality);
    MAKEATOM(case);
    MAKEATOM(checkin);
    MAKEATOM(checkout);
    MAKESYSATOM(childAttr);
    MAKEATOM(choosen);
    MAKEATOM(cluster);
    MAKESYSATOM(colocal);
    MAKEATOM(common);
    MAKEATOM(compileEmbeddedScript);
    MAKESYSATOM(complexKeyed);
    MAKEATOM(compressed);
    MAKEATOM(__compressed__);
    MAKESYSATOM(conditionFolded);
    MAKEATOM(const);
    MAKEATOM(context);
    MAKEATOM(contextSensitive);
    MAKEATOM(cost);
    MAKEATOM(count);
    MAKESYSATOM(countProject);
    MAKEATOM(cpp);
    MAKESYSATOM(cppBody);
    MAKEATOM(csv);
    MAKEATOM(ctxmethod);
    MAKEATOM(data);
    MAKESYSATOM(dataset);
    MAKEATOM(debug);
    MAKEATOM(dedup);
    MAKEATOM(default);
    MAKESYSATOM(default);
    defaultFieldNameAtom = createIdentifierAtom("__f1__");
    MAKEATOM(define);
    MAKEATOM(definition);
    MAKEATOM(deprecated);
    MAKEATOM(desc);
    MAKEATOM(disk);
    MAKEATOM(distributed);
    MAKESYSATOM(distributed);
    MAKESYSATOM(dot);
    MAKEATOM(dynamic);
    MAKEATOM(ebcdic);
    MAKEATOM(eclrtl);
    MAKEATOM(embedded);
    _empty_str_Atom = createAtom("");
    MAKEATOM(encoding);
    MAKEATOM(encrypt);
    MAKEATOM(ensure);
    MAKEATOM(enth);
    MAKEATOM(entrypoint);
    MAKEATOM(error);
    MAKEATOM(except);
    MAKEATOM(exclusive);
    MAKEATOM(expire);
    MAKEATOM(export);
    MAKEATOM(extend);
    MAKEATOM(external);
    MAKEATOM(fail);
    MAKEATOM(failure);
    MAKEATOM(false);
    MAKEATOM(fast);
    MAKEATOM(few);
    MAKEATOM(field);
    MAKEATOM(fields);
    MAKEATOM(filename);
    MAKEATOM(__filepos);
    MAKEATOM(fileposition);
    MAKESYSATOM(files);
    MAKEATOM(filter);
    MAKEATOM(filtered);
    MAKESYSATOM(filtered);
    MAKEATOM(first);
    MAKEATOM(firstLeft);
    MAKEATOM(firstRight);
    MAKEATOM(fixed);
    MAKEATOM(flag);
    MAKEATOM(flags);
    MAKEATOM(flat);
    MAKESYSATOM(folded);
    MAKEATOM(format);
    MAKEATOM(forward);
    fullonlyAtom = createLowerCaseAtom("full only");        // different to get the ECL regeneration correct..
    fullouterAtom = createLowerCaseAtom("full outer");
    MAKESYSATOM(function);
    MAKEATOM(gctxmethod);
    MAKEATOM(get);
    MAKEATOM(getEmbedContext);
    MAKEATOM(getBooleanResult);
    MAKEATOM(getDataResult);
    MAKEATOM(getRealResult);
    MAKEATOM(getSetResult);
    MAKEATOM(getSignedResult);
    MAKEATOM(getStringResult);
    MAKEATOM(getUnicodeResult);
    MAKEATOM(getUnsignedResult);
    MAKEATOM(getUTF8Result);
    MAKEATOM(global);
    MAKEATOM(globalContext);
    MAKEATOM(graph);
    MAKEATOM(group);
    MAKEATOM(grouped);
    MAKEATOM(hash);
    MAKEATOM(heading);
    MAKESYSATOM(hidden);
    MAKEATOM(hint);
    MAKEATOM(hole);
    MAKEATOM(holepos);
    MAKEATOM(httpHeader);
    MAKEATOM(__ifblock);
    MAKEATOM(ignore);
    MAKEATOM(ignoreBase);
    MAKEATOM(implements);
    MAKESYSATOM(implicitFpos);
    MAKESYSATOM(implicitSorted);
    MAKEATOM(import);
    MAKEATOM(include);
    MAKEATOM(index);
    MAKEATOM(indeterminate);
    MAKEATOM(initfunction);
    MAKEATOM(inline);
    MAKEATOM(inner);
    MAKEATOM(interface);
    MAKEATOM(internal);
    MAKESYSATOM(internal);
    MAKEATOM(internalFlags);
    MAKESYSATOM(isBlobInIndex);
    MAKESYSATOM(isFunctional);
    MAKEATOM(isNull);
    MAKEATOM(isValid);
    MAKEATOM(job);
    MAKEATOM(jobTemp);
    MAKEATOM(keep);
    MAKEATOM(keyed);
    MAKEATOM(labeled);
    MAKEATOM(language);
    MAKEATOM(last);
    MAKEATOM(left);
    leftonlyAtom = createLowerCaseAtom("left only");
    leftouterAtom = createLowerCaseAtom("left outer");
    MAKEATOM(library);
    MAKEATOM(lightweight);
    MAKESYSATOM(lightweight);
    MAKEATOM(limit);
    MAKEID(line);
    MAKEATOM(link);
    MAKESYSATOM(linkCounted);
    MAKEATOM(literal);
    MAKEATOM(load);
    MAKEATOM(local);
    MAKEATOM(localUpload);
    MAKEATOM(locale);
    MAKEATOM(localFilePosition);
    MAKESYSATOM(location);
    MAKEATOM(log);
    MAKEATOM(logicalFilename);
    MAKEATOM(lookup);
    MAKEATOM(lzw);
    MAKEATOM(macro);
    MAKEATOM(many);
    MAKEATOM(marker);
    MAKEATOM(matchxxxPseudoFile);
    MAKEATOM(max);
    MAKEATOM(maxCount);
    MAKEATOM(maxLength);
    MAKEATOM(maxSize);
    MAKEATOM(memory);
    MAKEATOM(merge);
    MAKEATOM(mergeTransform);
    MAKESYSATOM(metadata);
    MAKEATOM(method);
    MAKEATOM(min);
    MAKEATOM(minimal);
    MAKEATOM(module);
    MAKEATOM(mofn);
    MAKEATOM(name);
    MAKEATOM(named);
    MAKEATOM(namespace);
    MAKEATOM(new);
    MAKEATOM(newSet);
    MAKEATOM(noBoundCheck);
    MAKEATOM(noCase);
    MAKESYSATOM(noHoist);
    MAKEATOM(noLocal);
    MAKESYSATOM(nonEmpty);
    MAKEATOM(noOverwrite);
    MAKESYSATOM(normalized);
    MAKEATOM(noRoot);
    MAKEATOM(noScan);
    MAKEATOM(noSort);
    MAKESYSATOM(noStreaming);
    MAKEATOM(not);
    notMatchedAtom = createLowerCaseAtom("NOT MATCHED");
    notMatchedOnlyAtom = createLowerCaseAtom("NOT MATCHED ONLY");
    MAKEATOM(noTrim);
    MAKEATOM(noType);
    MAKEATOM(noXpath);
    MAKEATOM(oldSetFormat);
    MAKEATOM(omethod);
    MAKESYSATOM(omitted);
    MAKEATOM(once);
    MAKEATOM(onFail);
    MAKEATOM(onWarning);
    MAKEATOM(opt);
    MAKESYSATOM(ordered);
    MAKESYSATOM(orderedPull);
    MAKESYSATOM(origin);
    MAKESYSATOM(original);
    MAKEATOM(out);
    MAKEATOM(outofline);
    MAKEATOM(output);
    MAKEATOM(overwrite);
    MAKEATOM(owned);
    MAKEATOM(packed);
    MAKEATOM(parallel);
    MAKEATOM(parameter);
    MAKESYSATOM(parameterScopeType);
    MAKEATOM(partition);
    partitionLeftAtom = createLowerCaseAtom("partition left");
    partitionRightAtom = createLowerCaseAtom("partition right");
    MAKESYSATOM(payload);
    MAKEATOM(persist);
    MAKEATOM(physicalFilename);
    MAKEATOM(physicalLength);
    MAKEATOM(plugin);
    MAKEATOM(prefetch);
    MAKEATOM(preload);
    MAKEATOM(priority);
    MAKEATOM(private);
    MAKEATOM(prototype);
    MAKEATOM(proxyAddress);
    MAKEATOM(pseudoentrypoint);
    MAKEATOM(pull);
    MAKEATOM(pulled);
    MAKEATOM(pure);
    MAKEATOM(quote);
    MAKEATOM(random);
    MAKEATOM(range);
    MAKEATOM(raw);
    MAKEATOM(record);
    MAKEATOM(recursive);
    MAKEATOM(reference);
    MAKEATOM(refresh);
    MAKESYSATOM(remote);
    MAKEATOM(rename);
    MAKEATOM(repeat);
    MAKESYSATOM(resourced);
    MAKEATOM(response);
    MAKEATOM(restart);
    MAKEATOM(result);
    MAKESYSATOM(results);
    MAKEATOM(retry);
    MAKEATOM(right);
    rightonlyAtom = createLowerCaseAtom("right only");
    rightouterAtom = createLowerCaseAtom("right outer");
    MAKEATOM(rollback);
    MAKESYSATOM(root);
    MAKEATOM(row);
    MAKESYSATOM(rowsid);
    MAKEATOM(rowLimit);
    MAKEATOM(rule);
    MAKEATOM(save);
    MAKEATOM(scan);
    MAKEATOM(scanAll);
    MAKEATOM(scope);
    MAKEATOM(scopeChecking);
    MAKEATOM(section);
    MAKESYSATOM(selectors);
    MAKESYSATOM(selectorSequence);
    MAKEATOM(self);
    MAKEATOM(separator);
    MAKEATOM(sequence);
    MAKESYSATOM(sequence);
    MAKEATOM(sequential);
    MAKEATOM(serialization);
    MAKEATOM(set);
    MAKEATOM(shared);
    MAKEATOM(shutdown);
    MAKESYSATOM(sideEffect);
    MAKEATOM(single);
    MAKEATOM(size);
    MAKEATOM(sizeof);
    MAKEATOM(skew);
    MAKEATOM(skip);
    MAKEATOM(snapshot);
    MAKEATOM(soapAction);
    MAKEATOM(syntaxCheck);
    sort_AllAtom = createLowerCaseAtom("SORT ALL");
    sort_KeyedAtom = createLowerCaseAtom("SORT KEYED");
    MAKEATOM(sorted);
    MAKEATOM(source);
    MAKEATOM(stable);
    MAKESYSATOM(state);
    MAKEATOM(stepped);
    MAKEATOM(store);
    MAKEATOM(stored);
    MAKEATOM(streamed);
    MAKESYSATOM(streaming);
    MAKEATOM(success);
    MAKEATOM(supportsImport);
    MAKEATOM(supportsScript);
    MAKEATOM(sys);
    MAKEATOM(temp);
    MAKEATOM(template);
    MAKEATOM(terminate);
    MAKEATOM(terminator);
    MAKEATOM(escape);
    MAKEATOM(thor);
    MAKEATOM(threshold);
    MAKEATOM(timeout);
    MAKEATOM(timeLimit);
    MAKEATOM(timestamp);
    MAKEATOM(tiny);
    MAKEATOM(tomita);
    MAKEATOM(top);
    MAKEATOM(trim);
    MAKEATOM(true);
    MAKEATOM(type);
    MAKESYSATOM(uid);
    unnamedAtom = createAtom("<unnamed>");
    MAKEATOM(unknown);
    MAKEATOM(unknownSizeField);
    MAKEATOM(unicode);
    MAKEATOM(unordered);
    MAKEATOM(unsorted);
    MAKEATOM(unstable);
    MAKEATOM(update);
    MAKEATOM(userMatchFunction);
    MAKEATOM(value);
    MAKEATOM(version);
    MAKEATOM(virtual);
    MAKEATOM(volatile);
    MAKEATOM(warning);
    MAKEATOM(whole);
    MAKEATOM(width);
    MAKEATOM(wipe);
    MAKESYSATOM(workflow);
    MAKESYSATOM(workflowPersist);
    MAKEATOM(workunit);
    MAKEATOM(wuid);
    MAKEATOM(xml);
    MAKEATOM(xmlDefault);
    MAKEATOM(xpath);

    return true;
}
MODULE_EXIT()
{
    delete sysAtomTable;
}
