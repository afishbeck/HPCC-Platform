<?xml version="1.0" encoding="UTF-8"?>
<!--
##############################################################################
# HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##############################################################################
-->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xsd="http://www.w3.org/2001/XMLSchema">
    <xsl:output method="text" version="1.0" encoding="UTF-8" indent="yes"/>
    <xsl:param name="sourceFileName" select="'UNKNOWN'"/>
    <xsl:param name="importsList" select="''"/>
    <xsl:variable name="docname" select="/esxdl/@name"/>
    <xsl:template match="/">
        <xsl:apply-templates select="esxdl"/>
    </xsl:template>
    <xsl:template name="doNotChangeManuallyComment">
        <xsl:text>/*** Generated Code do not hand edit ***/
</xsl:text>
    </xsl:template>
<!--EsdlMethod response_type="SmartLinxReportResponseEx" description="SmartLinx Report." request_type="SmartLinxReportRequest" help="SmartLinx Report." name="SmartLinxReport"/-->

<xsl:template match="esxdl">
  <xsl:call-template name="doNotChangeManuallyComment"/>
IMPORT $.iesp AS iesp;
IMPORT $.iesp.Constants AS Constants;

DiffStatus := MODULE

  // Records in the old and new results are matched by virtual record ID (VID) -- something,
  // which makes "this" record unique. Usually is defined by business logic.
  // If two records have the same VID, they are compared field by field.
  EXPORT State := MODULE
    export integer1 VOID           := 0; // no comparison occured (for example, no alerts were requested)
    export integer1 UNCHANGED      := 1; // no changes
    export integer1 UPDATED        := 2; // there are changes to a scalar field
    export integer1 ADDED          := 4; // record is new (with respect to VID)
    export integer1 DELETED        := 8; // record doesn't exist anymore
    export integer1 PREVIOUS      := 16; // for UPDATED records only: will only have populated fields which are changed
    export integer1 CHILD_UPDATED := 32; // a record has a child dataset which has some records ADDED, DELETED or UPDATED
  END;

  EXPORT string Convert (integer sts) :=
    MAP (sts = State.DELETED   => 'deleted',
         sts = State.ADDED     => 'added',
         sts = State.UPDATED   => 'updated',
         sts = State.UNCHANGED => '',
         '');
END;

layouts := MODULE

  EXPORT DiffMetaRow := RECORD
    string name {XPATH ('@name')};
    string prior {XPATH ('@prior')};
  END;

  EXPORT DiffMetaRec := RECORD
    string7 _child_diff {XPATH('@child_diff')} := '';
    string7 _diff {XPATH('@diff')} := '';
    DATASET (DiffMetaRow) _diffmeta {XPATH ('DiffMeta/Field')} := DATASET ([], DiffMetaRow);
  END;

  <xsl:apply-templates select="EsdlStruct" mode="layouts"/>
  <xsl:apply-templates select="EsdlRequest" mode="layouts"/>
  <xsl:apply-templates select="EsdlResponse" mode="layouts"/>
  <xsl:text>END;
</xsl:text>
<xsl:text>

selectors := MODULE

</xsl:text>
       <xsl:apply-templates select="EsdlStruct" mode="selectors"/>
       <xsl:apply-templates select="EsdlRequest" mode="selectors"/>
       <xsl:apply-templates select="EsdlResponse" mode="selectors"/>
       <xsl:text>END;

difference := MODULE

</xsl:text>
       <xsl:apply-templates select="EsdlStruct" mode="difference"/>
       <xsl:apply-templates select="EsdlRequest" mode="difference"/>
       <xsl:apply-templates select="EsdlResponse" mode="difference"/>
       <xsl:text>END;
</xsl:text>
  <xsl:call-template name="doNotChangeManuallyComment"/>
</xsl:template>



<!-- Difference -->

<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children">
  <xsl:if test="@base_type">
    <xsl:variable name="base_type" select="@base_type"/>
    <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children"/>
  </xsl:if>
  <xsl:apply-templates select="*" mode="difference"/>
</xsl:template>

<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children.changed">
  <xsl:if test="@base_type">
    <xsl:variable name="base_type" select="@base_type"/>
    <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children.changed"/>
  </xsl:if>
  <xsl:apply-templates select="*" mode="difference.changed"/>
</xsl:template>

<xsl:template match="EsdlElement[@complex_type]|EsdlArray[DiffIdSection/diff_id]" mode="difference.changed">
          OR (selector.<xsl:call-template name="output_ecl_name"/> AND (updated_<xsl:call-template name="output_ecl_name"/><xsl:text>._diff != ''))</xsl:text>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="difference.changed">
</xsl:template>

<xsl:template match="EsdlArray" mode="difference.changed">
</xsl:template>

<xsl:template match="*" mode="difference.changed">
</xsl:template>

<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children.updated">
  <xsl:if test="@base_type">
    <xsl:variable name="base_type" select="@base_type"/>
    <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children.updated"/>
  </xsl:if>
  <xsl:apply-templates select="EsdlElement[@type]" mode="difference.updated"/>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="difference.updated">
      OR updated_<xsl:call-template name="output_ecl_name"/>
</xsl:template>


<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children.meta">
  <xsl:if test="@base_type">
    <xsl:variable name="base_type" select="@base_type"/>
    <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children.meta"/>
  </xsl:if>
  <xsl:apply-templates select="EsdlElement[@type]" mode="difference.meta"/>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="difference.meta">
          <xsl:if test="position()!=1">+ </xsl:if> IF (updated_<xsl:call-template name="output_ecl_name"/>, DATASET ([{'<xsl:call-template name="output_ecl_name"/>', R.<xsl:call-template name="output_ecl_name"/><xsl:text>}],   layouts.DiffMetaRow))
          </xsl:text>
</xsl:template>


<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference">
  <xsl:variable name="struct_name"><xsl:call-template name="output_ecl_name"/></xsl:variable>
  <xsl:text>EXPORT </xsl:text><xsl:value-of select="$struct_name"/>(selectors.<xsl:value-of select="$struct_name"/> selector) := MODULE
<xsl:text>
  </xsl:text>layouts.<xsl:value-of select="$struct_name"/> Compare (layouts.<xsl:value-of select="$struct_name"/> L, layouts.<xsl:value-of select="$struct_name"/> R, boolean is_deleted, boolean is_added) :=TRANSFORM

  <xsl:apply-templates select="." mode="difference.children"/>
  <xsl:text>
      is_updated := false</xsl:text>
  <xsl:apply-templates select="." mode="difference.children.updated"/>;
  <xsl:text>    _child_changed := false</xsl:text>
  <xsl:apply-templates select="." mode="difference.children.changed"/>;
      integer _change := MAP (is_deleted  => DiffStatus.State.DELETED,
                              is_added    => DiffStatus.State.ADDED,
                              is_updated  => DiffStatus.State.UPDATED,
                              DiffStatus.State.UNCHANGED);

      SELF._diff := DiffStatus.Convert (_change);
      SELF._child_diff := IF (_child_changed, 'updated', '');

      // Get update information for all scalars
      _meta :=  <xsl:apply-templates select="." mode="difference.children.meta"/>;

      SELF._diffmeta := IF (~is_deleted AND ~is_added AND is_updated, _meta);

      SELF := IF (is_deleted, R, L);
<xsl:text>
    END;

</xsl:text>
    EXPORT AsRecord (layouts.<xsl:call-template name="output_ecl_name"/> _new, layouts.<xsl:call-template name="output_ecl_name"/> _old) := FUNCTION
      RETURN ROW (Compare(_new, _old, false, false));
    END;
  <xsl:for-each select="DiffIdSection/diff_id">
    <xsl:variable name="vid_name" select="translate(@name, ' .', '__')"/>
      EXPORT  AsDataset_<xsl:value-of select="$vid_name"/> (dataset(layouts.<xsl:value-of select="$struct_name"/>) _new, dataset(layouts.<xsl:value-of select="$struct_name"/>) _old) := FUNCTION
    _checked := JOIN (_new, _old,
    <xsl:for-each select="part">
      <xsl:if test="position()!=1">AND </xsl:if>LEFT.<xsl:value-of select="."/> = RIGHT.<xsl:value-of select="."/>
    </xsl:for-each>,
                      Compare (LEFT, RIGHT, <xsl:text>
                      (</xsl:text> 
    <xsl:for-each select="part">
      <xsl:if test="position()!=1"> AND </xsl:if>LEFT.<xsl:value-of select="."/><xsl:text> = ''</xsl:text>
    </xsl:for-each><xsl:text>),
                      (</xsl:text>
    <xsl:for-each select="part">
      <xsl:if test="position()!=1"> AND </xsl:if>RIGHT.<xsl:value-of select="."/><xsl:text> = ''</xsl:text>
    </xsl:for-each><xsl:text>),</xsl:text>
                      FULL OUTER,
                      LIMIT (0));
      RETURN _checked;
    END;
  
  </xsl:for-each>
<xsl:text>END;

</xsl:text>
</xsl:template>

<xsl:template match="EsdlElement[@complex_type]" mode="difference">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:variable name="type" select="@complex_type"/>
      updated_<xsl:call-template name="output_ecl_name"/> := <xsl:value-of select="$type"/>(selector.<xsl:call-template name="output_ecl_name"/>).AsRecord(L.<xsl:call-template name="output_ecl_name"/>, R.<xsl:call-template name="output_ecl_name"/>, selector.<xsl:call-template name="output_ecl_name"/>);
      checked_<xsl:call-template name="output_ecl_name"/> := MAP (is_deleted => ROW (R.<xsl:call-template name="output_ecl_name"/>, service_types.at_SSNInfo),
                              is_added => ROW (L.SSNInfo, layouts.<xsl:value-of select="$type"/>),
                              selector.<xsl:call-template name="output_ecl_name"/>.do => updated_<xsl:call-template name="output_ecl_name"/>,
                              ROW (L.<xsl:call-template name="output_ecl_name"/>, layouts.<xsl:value-of select="$type"/>));
      SELF.<xsl:call-template name="output_ecl_name"/> := checked_<xsl:call-template name="output_ecl_name"/>;    
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="difference">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>      </xsl:text>boolean updated_<xsl:call-template name="output_ecl_name"/> := selector.<xsl:call-template name="output_ecl_name"/> AND (L.<xsl:call-template name="output_ecl_name"/> != R.<xsl:call-template name="output_ecl_name"/><xsl:text>);
</xsl:text>
  </xsl:if>
</xsl:template>


<xsl:template match="EsdlArray[@type='string']" mode="difference">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlArray[starts-with(@ecl_type,'string') or starts-with(@ecl_type,'unicode')]" mode="difference">
</xsl:template>

<xsl:template match="EsdlArray[DiffIdSection/diff_id]" mode="difference">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:variable name="vid_name" select="translate(DiffIdSection/diff_id/@name, ' .', '__')"/>
    <xsl:variable name="type" select="@type"/>
      updated_<xsl:call-template name="output_ecl_name"/> := <xsl:value-of select="$type"/>(selector.<xsl:call-template name="output_ecl_name"/>).AsDataset_<xsl:value-of select="$vid_name"/>(L.<xsl:call-template name="output_ecl_name"/>, R.<xsl:call-template name="output_ecl_name"/>);
      checked_<xsl:call-template name="output_ecl_name"/> := IF (selector.<xsl:call-template name="output_ecl_name"/>.do, updated_<xsl:call-template name="output_ecl_name"/> , PROJECT (L.<xsl:call-template name="output_ecl_name"/>, layouts.<xsl:value-of select="$type"/>));
      SELF.<xsl:call-template name="output_ecl_name"/>  := checked_<xsl:call-template name="output_ecl_name"/><xsl:text>;

</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlArray" mode="difference">
<xsl:text>      SELF.</xsl:text><xsl:call-template name="output_ecl_name"/>  := L.<xsl:call-template name="output_ecl_name"/><xsl:text>;
</xsl:text>
</xsl:template>

<xsl:template match="EsdlEnum" mode="difference">
  <xsl:variable name="entype" select="@enum_type"/>
</xsl:template>

<xsl:template match="DiffIdSection" mode="difference">
</xsl:template>

<!-- 
 EXPORT AlertBpsReportIdentity(layouts.AlertAKAS alerts) := MODULE

//ESDL: For each field check alert selector
    //ESDL: for each scalar field: 
    boolean alert_HasCriminalConviction := alerts.HasCriminalConviction;
    // etc.

    //ESDL: for each DATASET and RECORD: 
    boolean alert_SSNInfo := alerts.SSNInfo.do;
    boolean alert_SSNInfoEx := alerts.SSNInfoEx.do;
    boolean alert_DOB := alerts.DOB.do;

    service_types.at_bpsreportidentity Compare (service_types.at_bpsreportidentity L, service_types.at_bpsreportidentity R) := TRANSFORM
      // check if we're dealing with new or deleted records
      //ESDL: we need to use same VID (or any part of a VID) used in the JOIN condition  
      is_deleted := (L.UniqueID = ''); 
      is_added   := (R.UniqueID = ''); 

      //ESDL: go through all (child) DATASET and RECORD fields to find out if either of them has changed
      boolean _child_changed := (alert_DOB       AND (checked_DOB._diff != '')) OR
                                (alert_SSNInfo   AND (checked_SSNInfo._diff != '' OR checked_SSNInfo._child_diff != '')) OR
                                (alert_SSNInfoEx AND (checked_SSNInfoEx._diff != '' OR checked_SSNInfoEx._child_diff != ''))
                                // etc.
                                ;

      // "updated" refers only to the scalar fields at this level (no child records included)
      //ESDL: for each monitored scalar field: (Individual doesn't have such fields)
      updated_HasCriminalConviction := alert_HasCriminalConviction AND (L.HasCriminalConviction != R.HasCriminalConviction);

      boolean is_updated := updated_HasCriminalConviction
                            // etc. all other scalar fields
                            ;

      //Check whether any scalars at this record have been changed
      integer _change := MAP (is_deleted  => DiffStatus.State.DELETED,
                              is_added    => DiffStatus.State.ADDED,
                              is_updated  => DiffStatus.State.UPDATED,
                              DiffStatus.State.UNCHANGED);

      SELF._diff := DiffStatus.Convert (_change);
      SELF._child_diff := IF (_child_changed, 'updated', '');

      //ESDL: get update information for all scalars at THIS level:
      _meta := IF (updated_HasCriminalConviction,   DATASET ([{'HasCriminalConviction',   R.HasCriminalConviction}],   service_types.DiffMetaRow))
                   // etc.
                  ;
      //ESDL: note, this is different from transforms in FN_AlertAsRecord_<X>
      SELF._diffmeta := IF (~is_deleted AND ~is_added AND is_updated, _meta);

      SELF := IF (is_deleted, R, L);
    END;

  // returns: DATASET (service_types.at_bpsreportidentity)
    EXPORT  AsDataset (dataset(service_types.at_bpsreportidentity) _new, dataset(service_types.at_bpsreportidentity) _old) := FUNCTION
      //ESDL: generate a JOIN with proper VID match condition
      _checked := JOIN (_new, _old,
                      // VID: probably, UniqueID (LexID) + Name
                      LEFT.UniqueID = Right.UniqueID AND
                      LEFT.Name = Right.Name,
                      Compare (LEFT, RIGHT),
                      FULL OUTER,
                      LIMIT (0));
      RETURN _checked;
    END;
    EXPORT AsRecord (service_types.at_Name _new, service_types.at_Name _old) := FUNCTION
      RETURN ROW (Compare(_new, _old));
    END;
  END;

 -->

<!-- Layouts -->

<xsl:template match="EsdlElement[@complex_type]" mode="layouts">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>  </xsl:text><xsl:call-template name="output_ecl_complex_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>};</xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="layouts">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>  </xsl:text><xsl:call-template name="output_basic_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/>
    <xsl:if test="@ecl_max_len"><xsl:text>, maxlength(</xsl:text><xsl:value-of select="@ecl_max_len"/><xsl:text>)</xsl:text></xsl:if>
    <xsl:text>};</xsl:text><xsl:call-template name="output_comments"/>
<xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlEnum" mode="layouts">
  <xsl:variable name="entype" select="@enum_type"/>
  <xsl:text>  </xsl:text><xsl:call-template name="output_enum_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
  <xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>}; </xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="EsdlArray[@type='string']" mode="layouts">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>  set of string </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text>
    <xsl:if test="not(@flat_array)"><xsl:value-of select="@name"/></xsl:if>
    <xsl:text>/</xsl:text><xsl:call-template name="output_item_tag"/>
    <xsl:text>')</xsl:text>
    <xsl:choose>
      <xsl:when test="@max_count_var"><xsl:text>, MAXCOUNT(</xsl:text><xsl:value-of select="@max_count_var"/><xsl:text>)</xsl:text></xsl:when>
      <xsl:when test="@max_count"><xsl:text>, MAXCOUNT(</xsl:text><xsl:value-of select="@max_count"/><xsl:text>)</xsl:text></xsl:when>
      <xsl:otherwise><xsl:text>, MAXCOUNT(1)</xsl:text></xsl:otherwise>
    </xsl:choose>
    <xsl:text>};</xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlArray[starts-with(@ecl_type,'string') or starts-with(@ecl_type,'unicode')]" mode="layouts">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
  <xsl:text>  </xsl:text><xsl:value-of select="@ecl_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
  <xsl:text> {xpath('</xsl:text><xsl:value-of select="@name"/><xsl:text>')};</xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlArray" mode="layouts">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>  dataset(</xsl:text> <xsl:call-template name="output_ecl_array_type"/><xsl:text>) </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text><xsl:if test="not(@flat_array)"><xsl:value-of select="@name"/></xsl:if><xsl:text>/</xsl:text><xsl:call-template name="output_item_tag"/><xsl:text>')</xsl:text>
    <xsl:choose>
      <xsl:when test="@max_count_var"><xsl:text>, MAXCOUNT(</xsl:text><xsl:value-of select="@max_count_var"/><xsl:text>)</xsl:text></xsl:when>
      <xsl:when test="@max_count"><xsl:text>, MAXCOUNT(</xsl:text><xsl:value-of select="@max_count"/><xsl:text>)</xsl:text></xsl:when>
      <xsl:otherwise><xsl:text>, MAXCOUNT(1)</xsl:text></xsl:otherwise>
    </xsl:choose>
    <xsl:text>};</xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="DiffIdSection" mode="layouts">
</xsl:template>

<xsl:template match="EsdlStruct" mode="layouts">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>EXPORT </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD</xsl:text>
    <xsl:call-template name="output_ecl_base_type"/>
    <xsl:if test="@max_len"><xsl:text>, MAXLENGTH (</xsl:text><xsl:value-of select="@max_len"/><xsl:text>)</xsl:text></xsl:if>
    <xsl:text>
</xsl:text>
    <xsl:apply-templates select="*" mode="layouts"/>
    <xsl:if test="@element and not(*[@name='Content_'])">	string Content_ {xpath('')};</xsl:if>
    <xsl:text>END;

</xsl:text>
    </xsl:if>
</xsl:template>

<xsl:template match="EsdlRequest" mode="layouts">
  <xsl:text>EXPORT </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD</xsl:text>
  <xsl:call-template name="output_ecl_base_type"/>
  <xsl:if test="@max_len"><xsl:text>, MAXLENGTH (</xsl:text><xsl:value-of select="@max_len"/><xsl:text>)</xsl:text></xsl:if>
  <xsl:text>
</xsl:text>
  <xsl:apply-templates select="*" mode="layouts"/>
  <xsl:text>END;

</xsl:text>
</xsl:template>

<xsl:template match="EsdlResponse" mode="layouts">
  <xsl:text>EXPORT </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD</xsl:text>
  <xsl:call-template name="output_ecl_base_type"/>
  <xsl:if test="@max_len"><xsl:text>, MAXLENGTH (</xsl:text><xsl:value-of select="@max_len"/><xsl:text>)</xsl:text></xsl:if>
  <xsl:text>
</xsl:text>
  <xsl:apply-templates select="*" mode="layouts"/>
  <xsl:text>END;

</xsl:text>
</xsl:template>


<!-- Selectors -->

<xsl:template match="EsdlElement[@complex_type]" mode="selectors">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>  </xsl:text><xsl:call-template name="output_ecl_complex_type"/><xsl:text> </xsl:text> <xsl:call-template name="output_ecl_name"/>
    <xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>} := FALSE;
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="selectors">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>  boolean </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>} := FALSE;
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlEnum" mode="selectors">
  <xsl:text>  boolean </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>} := FALSE;
</xsl:text>
</xsl:template>
<xsl:template match="EsdlArray[@type='string']" mode="selectors">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>  boolean </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text><xsl:value-of select="@name"/><xsl:text>')}  := FALSE;
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlArray[starts-with(@ecl_type,'string') or starts-with(@ecl_type,'unicode')]">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))" mode="selectors">
    <xsl:text>  boolean </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text><xsl:value-of select="@name"/><xsl:text>')} := FALSE;
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlArray" mode="selectors">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>  </xsl:text><xsl:call-template name="output_ecl_array_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text><xsl:value-of select="@name"/><xsl:text>')} := FALSE;
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="DiffIdSection" mode="selectors">
</xsl:template>

<xsl:template match="EsdlStruct" mode="selectors">
  <xsl:if test="not(@ecl_hide) and (@ecl_keep or not(@get_data_from))">
    <xsl:text>EXPORT </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD
</xsl:text>
    <xsl:if test="@base_type"><xsl:call-template name="output_ecl_base_type"/></xsl:if>
      <xsl:apply-templates select="*" mode="selectors"/>
      <xsl:if test="@element and not(*[@name='Content_'])">	bool Content_ := FALSE;
</xsl:if>
    <xsl:text>END;
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="EsdlResponse" mode="selectors">
  <xsl:text>EXPORT </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD
</xsl:text>
  <xsl:if test="@base_type"><xsl:call-template name="output_ecl_base_type"/></xsl:if>
    <xsl:apply-templates select="*" mode="selectors"/>
    <xsl:text>END;

</xsl:text>
</xsl:template>


<!-- common -->

<xsl:template name="output_ecl_name">
<xsl:choose>
  <xsl:when test="@ecl_name"><xsl:value-of select="@ecl_name"/></xsl:when>
  <xsl:when test="@get_data_from"><xsl:value-of select="@get_data_from"/></xsl:when>
  <xsl:when test="@alt_data_from"><xsl:value-of select="@alt_data_from"/></xsl:when>
  <xsl:otherwise>
    <xsl:variable name="nameword" select="translate(@name, 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz')"/>
    <xsl:choose>
      <xsl:when test="$nameword='shared'"><xsl:text>_</xsl:text></xsl:when>
    </xsl:choose>
    <xsl:value-of select="@name"/>
  </xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template name="output_basic_type">
  <xsl:param name="basic_type" select="@type"/>
  <xsl:param name="size" select="@max_len"/>
  <xsl:choose>
    <xsl:when test="@ecl_type"><xsl:value-of select="@ecl_type"/><xsl:if test="not(@ecl_max_len)"><xsl:value-of select="$size"/></xsl:if></xsl:when>
    <xsl:when test="$basic_type='int'"><xsl:text>integer</xsl:text><xsl:if test="not(@ecl_max_len)"><xsl:value-of select="$size"/></xsl:if></xsl:when>

    <xsl:when test="$basic_type='unsignedInt'"><xsl:text>unsigned</xsl:text><xsl:if test="not(@ecl_max_len)"><xsl:value-of select="$size"/></xsl:if></xsl:when>
    <xsl:when test="$basic_type='unsignedShort'"><xsl:text>unsigned2</xsl:text></xsl:when>
    <xsl:when test="$basic_type='unsignedByte'"><xsl:text>unsigned1</xsl:text></xsl:when>
    <xsl:when test="$basic_type='long'"><xsl:text>integer4</xsl:text><xsl:if test="not(@ecl_max_len)"><xsl:value-of select="$size"/></xsl:if></xsl:when>
    <xsl:when test="$basic_type='short'"><xsl:text>integer2</xsl:text></xsl:when>
    <xsl:when test="$basic_type='int64'"><xsl:text>integer8</xsl:text></xsl:when>
    <xsl:when test="$basic_type='bool'"><xsl:text>boolean</xsl:text></xsl:when>
    <xsl:when test="$basic_type='string'"><xsl:text>string</xsl:text><xsl:if test="not(@ecl_max_len)"><xsl:value-of select="$size"/></xsl:if></xsl:when>
    <xsl:when test="$basic_type='double'"><xsl:text>real8</xsl:text></xsl:when>
    <xsl:when test="$basic_type='float'"><xsl:text>real4</xsl:text></xsl:when>
    <xsl:when test="$basic_type='base64Binary'"><xsl:text>string</xsl:text></xsl:when>
    <xsl:when test="$basic_type"><xsl:value-of select="$basic_type"/><xsl:if test="not(@ecl_max_len)"><xsl:value-of select="$size"/></xsl:if></xsl:when>
    <xsl:otherwise><xsl:text>string</xsl:text><xsl:if test="not(@ecl_max_len)"><xsl:value-of select="$size"/></xsl:if></xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="output_enum_type">
  <xsl:variable name="etype" select="@enum_type"/>
  <xsl:choose>
    <xsl:when test="/expesdl/types/type[@name=$etype]/@base_type">
      <xsl:call-template name="output_basic_type">
        <xsl:with-param name="basic_type" select="/expesdl/types/type[@name=$etype]/@base_type"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise>
      <xsl:call-template name="output_basic_type"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="output_ecl_base_type">
  <xsl:variable name="btype">
    <xsl:choose>
      <xsl:when test="@base_type"><xsl:value-of select="@base_type"/></xsl:when>
      <xsl:otherwise>DiffMetaRec</xsl:otherwise>
    </xsl:choose>
  </xsl:variable>
  <xsl:text> (</xsl:text>
  <xsl:text></xsl:text><xsl:value-of select="$btype"/><xsl:text>)</xsl:text>
</xsl:template>

<xsl:template name="output_ecl_complex_type">
  <xsl:variable name="ctype" select="@complex_type"/>
  <xsl:choose>
    <xsl:when test="@ecl_type"><xsl:value-of select="@ecl_type"/></xsl:when>
    <xsl:otherwise>
      <xsl:text></xsl:text><xsl:value-of select="$ctype"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="output_ecl_array_type">
  <xsl:variable name="ctype" select="@type"/>
  <xsl:choose>
    <xsl:when test="@ecl_type"><xsl:value-of select="@ecl_type"/></xsl:when>
    <xsl:otherwise>
      <xsl:variable name="srcfile" select="/expesdl/types/type[@name=$ctype]/@src"/>
      <xsl:if test="$sourceFileName != $srcfile">
        <xsl:value-of select="$srcfile"/><xsl:text>.</xsl:text>
      </xsl:if>
      <xsl:text></xsl:text><xsl:value-of select="$ctype"/>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template name="output_comments">
	<xsl:if test="@ecl_comment"><xsl:value-of select="@ecl_comment"/></xsl:if>
	<xsl:choose>
		<xsl:when test="@complex_type">
			<xsl:variable name="ctype" select="@complex_type"/>
			<xsl:if test="/expesdl/types/type[@name=$ctype]/@comment">
				<xsl:text>//</xsl:text><xsl:value-of select="/expesdl/types/type[@name=$ctype]/@comment"/>
			</xsl:if>
		</xsl:when>
		<xsl:when test="@enum_type">
			<xsl:variable name="etype" select="@enum_type"/>
			<xsl:if test="/expesdl/types/type[@name=$etype]/@comment">
				<xsl:text>//</xsl:text><xsl:value-of select="/expesdl/types/type[@name=$etype]/@comment"/>
			</xsl:if>
		</xsl:when>
	</xsl:choose>
	<xsl:if test="@optional">
		<xsl:text>//hidden[</xsl:text><xsl:value-of select="@optional"/><xsl:text>]</xsl:text>
	</xsl:if>
	<xsl:if test="@ecl_type and (@type or @complex_type)">
        <xsl:choose>
         <xsl:when test="name()='EsdlArray'">
           <xsl:text> // Real type: </xsl:text>
           <xsl:text>dataset(</xsl:text>
           <xsl:value-of select="@type"/>
           <xsl:text>) </xsl:text>
           <xsl:call-template name="output_ecl_name"/>
           <xsl:text> {xpath('</xsl:text>
           <xsl:value-of select="@name"/>
           <xsl:text>/</xsl:text>
           <xsl:call-template name="output_item_tag"/>
           <xsl:text>')};</xsl:text>
         </xsl:when>
         <xsl:when test="name()='EsdlElement' and starts-with(@ecl_type,'tns:')">
           <xsl:text> // Real type: RECORD </xsl:text>
           <xsl:value-of select="@type|@complex_type"/>
         </xsl:when>
         <xsl:when test="name()='EsdlElement' and not(starts-with(@ecl_type,'tns:'))">
           <xsl:text> // Xsd type: </xsl:text>
           <xsl:value-of select="@type|@complex_type"/>
         </xsl:when>
         <xsl:otherwise>
             <xsl:value-of select="@type"/>
         </xsl:otherwise>
        </xsl:choose>
    </xsl:if>
</xsl:template>

<xsl:template name="output_xpath">
	<xsl:text>xpath('</xsl:text>
	<xsl:choose>
		<xsl:when test="@ecl_path"><xsl:value-of select="@ecl_path"/></xsl:when>
		<xsl:when test="@get_data_from"><xsl:if test="@attribute"><xsl:value-of select="'@'"/></xsl:if> <xsl:value-of select="@get_data_from"/></xsl:when>
		<xsl:when test="@alt_data_from"><xsl:if test="@attribute"><xsl:value-of select="'@'"/></xsl:if> <xsl:value-of select="@alt_data_from"/></xsl:when>
		<xsl:otherwise><xsl:if test="@attribute"><xsl:value-of select="'@'"/></xsl:if> <xsl:value-of select="@name"/></xsl:otherwise>
	</xsl:choose>
	<xsl:text>')</xsl:text>
</xsl:template>

<xsl:template name="output_item_tag">
    <xsl:choose>
         <xsl:when test="@item_tag"><xsl:value-of select="@item_tag"/></xsl:when>
         <xsl:when test="@type and (@type='int' or @type='integer' or @type='bool' or @type='short' or @type='float' or @type='double' or @type='string' or @type='long' or @type='decimal' or @type='byte' or @type='unsignedInt' or @type='unsignedShort' or @type='unsignedByte')">
           <xsl:value-of select="'Item'"/>
         </xsl:when>
         <xsl:when test="@type">
           <xsl:value-of select="@type"/>
         </xsl:when>
         <xsl:otherwise><xsl:value-of select="'Item'"/></xsl:otherwise>
    </xsl:choose>
</xsl:template>

</xsl:stylesheet>
