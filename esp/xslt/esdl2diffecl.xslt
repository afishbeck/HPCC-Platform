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
    <xsl:param name="responseType" select="'AssetReportResponse'"/>
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

  EXPORT JoinRowType := MODULE
    export integer1 IsInner := 0;
    export integer1 OuterLeft := 1;
    export integer1 OuterRight := 2;
  END;

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

suppression := MODULE

</xsl:text>
       <xsl:apply-templates select="EsdlStruct" mode="suppressors"/>
       <xsl:apply-templates select="EsdlRequest" mode="suppressors"/>
       <xsl:apply-templates select="EsdlResponse" mode="suppressors"/>
END;

difference := MODULE
    <xsl:for-each select="Selectors/Selector">
      shared boolean Monitor<xsl:value-of select="."/> := FALSE : STORED('Monitor_<xsl:value-of select="."/>', FORMAT(sequence(<xsl:value-of select="position()+1"/>)));
    </xsl:for-each>
<xsl:text>
  shared set of string optional_fields := [
</xsl:text>
    <xsl:for-each select="OptionalFields/Path">
      <xsl:if test="position()!=1">,</xsl:if><xsl:text>'/</xsl:text><xsl:value-of select="."/><xsl:text>'
    </xsl:text>
    </xsl:for-each>
       <xsl:text>
    ];
</xsl:text>
       <xsl:apply-templates select="EsdlStruct" mode="difference.struct"/>
       <xsl:apply-templates select="EsdlRequest" mode="difference.struct"/>
       <xsl:apply-templates select="EsdlResponse" mode="difference.struct"/>
       <xsl:text>END;
</xsl:text>

  old_str := '' : STORED ('PriorResponse', FORMAT(FIELDWIDTH(100),FIELDHEIGHT(20), sequence(100)));
  new_str := '' : STORED ('CurrentResponse', FORMAT(FIELDWIDTH(100),FIELDHEIGHT(20), sequence(200)));

  oldResponse := FROMXML (layouts._lt_<xsl:value-of select="$responseType"/>, old_str);
  newResponse := FROMXML (layouts._lt_<xsl:value-of select="$responseType"/>, new_str);

  fieldSuppression := DATASET ([], suppression._sup_<xsl:value-of select="$responseType"/>) : STORED ('FieldSuppression', FEW, FORMAT(sequence(300)));

  res := difference._df_<xsl:value-of select="$responseType"/>(false, '', fieldSuppression[1]).AsRecord(newResponse, oldResponse);

  OUTPUT (res, NAMED ('Results'));

  OUTPUT (FieldSuppression, NAMED ('Suppression'));
  OUTPUT (oldResponse, NAMED ('oldResponse'));
  OUTPUT (newResponse, NAMED ('newResponse'));

  <xsl:call-template name="doNotChangeManuallyComment"/>
</xsl:template>

<xsl:template match="EsdlElement[@complex_type]" mode="selectors.section">
  <xsl:value-of select="@complex_type"/><xsl:text> </xsl:text><xsl:value-of select="@diff_section"/>;
</xsl:template>

<xsl:template match="EsdlArray[DiffMatchs/diff_match]" mode="selectors.section">
  <xsl:value-of select="@type"/><xsl:text> </xsl:text><xsl:value-of select="@diff_section"/>;
</xsl:template>

<xsl:template match="*" mode="selectors.section">
</xsl:template>

<!-- Difference -->

<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children.scalars">
  <xsl:if test="@base_type">
    <xsl:variable name="base_type" select="@base_type"/>
    <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children.scalars"/>
  </xsl:if>
  <xsl:apply-templates select="EsdlElement[@type]|EsdlEnumRef" mode="difference"/>
</xsl:template>

<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children.nested">
  <xsl:if test="@base_type">
    <xsl:variable name="base_type" select="@base_type"/>
    <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children.nested"/>
  </xsl:if>
  <xsl:apply-templates select="EsdlElement[@complex_type]|EsdlArray" mode="difference"/>
</xsl:template>

<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children.changed">
  <xsl:if test="@base_type">
    <xsl:variable name="base_type" select="@base_type"/>
    <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children.changed"/>
  </xsl:if>
  <xsl:apply-templates select="*" mode="difference.changed"/>
</xsl:template>

<xsl:template match="EsdlElement[@complex_type and (not(@_nomon) or @_mon='1')]" mode="difference.changed">
          OR (checked_<xsl:call-template name="output_ecl_name"/><xsl:text>._diff != '')</xsl:text>
</xsl:template>

<xsl:template match="EsdlArray[DiffMatchs/diff_match and (not(@_nomon) or @_mon='1')]" mode="difference.changed">
          OR (EXISTS (checked_<xsl:call-template name="output_ecl_name"/><xsl:text>(_diff != '')))</xsl:text>
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

<xsl:template match="EsdlElement[@type and (not(@_nomon) or @_mon='1')]" mode="difference.updated">
      OR updated_<xsl:call-template name="output_ecl_name"/>
</xsl:template>


<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children.meta">
    <xsl:variable name="base_content">
    <xsl:if test="@base_type">
      <xsl:variable name="base_type" select="@base_type"/>
      <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children.meta"/>
    </xsl:if>
    </xsl:variable>
  <xsl:variable name="local_content">
    <xsl:apply-templates select="EsdlElement[@type]" mode="difference.meta"/>
  </xsl:variable>
  <xsl:if test="string($base_content)">
    <xsl:value-of select="$base_content"/>
    <xsl:if test="string($local_content)">+</xsl:if>
  </xsl:if>
  <xsl:value-of select="$local_content"/>
</xsl:template>

<xsl:template match="EsdlElement[@type and (not(@_nomon) or @_mon='1')]" mode="difference.meta">
          <xsl:if test="position()!=1">+ </xsl:if> IF (updated_<xsl:call-template name="output_ecl_name"/>, DATASET ([{'<xsl:call-template name="output_ecl_name"/>', R.<xsl:call-template name="output_ecl_name"/><xsl:text>}],   layouts.DiffMetaRow))
          </xsl:text>
</xsl:template>

<xsl:template match="*[@mon_child]" mode="difference.struct">
  <xsl:variable name="struct_name"><xsl:call-template name="output_ecl_name"/></xsl:variable>
  <xsl:text>EXPORT _df_</xsl:text><xsl:value-of select="$struct_name"/>(boolean is_active, string path, suppression._sup_<xsl:value-of select="$struct_name"/> suppress) := MODULE<xsl:text>
  </xsl:text>EXPORT layouts._lt_<xsl:value-of select="$struct_name"/> ProcessTx(layouts._lt_<xsl:value-of select="$struct_name"/> L, layouts._lt_<xsl:value-of select="$struct_name"/> R) :=TRANSFORM

  <xsl:apply-templates select="." mode="difference.children.sections"/>
<xsl:text>END;
</xsl:text>
    EXPORT AsRecord (layouts._lt_<xsl:call-template name="output_ecl_name"/> _new, layouts._lt_<xsl:call-template name="output_ecl_name"/> _old) := FUNCTION
      RETURN ROW (ProcessTx(_new, _old));
    END;

<xsl:text>
END;
</xsl:text>
</xsl:template>


<xsl:template match="*[@diff_monitor]" mode="difference.struct">
<xsl:if test="not(@_base) or @_used">
  <xsl:variable name="struct_name"><xsl:call-template name="output_ecl_name"/></xsl:variable>
  <xsl:text>EXPORT _df_</xsl:text><xsl:value-of select="$struct_name"/>(boolean is_active, string path, suppression._sup_<xsl:value-of select="$struct_name"/> suppress) := MODULE

<xsl:if test="EsdlElement[@type]">
<xsl:text>
  </xsl:text>EXPORT DiffScalars (layouts._lt_<xsl:value-of select="$struct_name"/> L, layouts._lt_<xsl:value-of select="$struct_name"/> R, boolean is_deleted, boolean is_added) := MODULE

  <xsl:apply-templates select="." mode="difference.children.scalars"/><xsl:text>

      shared is_updated := false</xsl:text>
  <xsl:apply-templates select="." mode="difference.children.updated"/>;

      shared integer _change := MAP (is_deleted  => DiffStatus.State.DELETED,
                              is_added    => DiffStatus.State.ADDED,
                              is_updated  => DiffStatus.State.UPDATED,
                              DiffStatus.State.UNCHANGED);

      EXPORT _diff := DiffStatus.Convert (_change);
      // Get update information for all scalars
      _meta :=  <xsl:apply-templates select="." mode="difference.children.meta"/>;

      EXPORT _diffmeta := IF (~is_deleted AND ~is_added AND is_updated, _meta);
END;
  </xsl:if>
<xsl:text>
  </xsl:text>EXPORT layouts._lt_<xsl:value-of select="$struct_name"/> ProcessTx(layouts._lt_<xsl:value-of select="$struct_name"/> L, layouts._lt_<xsl:value-of select="$struct_name"/> R, boolean is_deleted, boolean is_added) :=TRANSFORM
<xsl:if test="EsdlElement[@type]">
      m := DiffScalars(L, R, is_deleted, is_added);
      SELF._diff := IF(is_active, m._diff, '');
      SELF._diffmeta := IF(is_active, m._diffmeta);
</xsl:if>
      SELF := IF (is_deleted, R, L);
<xsl:text>
    END;

</xsl:text>

<xsl:if test="@_usedInArray">
<xsl:text>
  </xsl:text>EXPORT layouts._lt_row_<xsl:value-of select="$struct_name"/> ProcessTxRow(layouts._lt_row_<xsl:value-of select="$struct_name"/> L, layouts._lt_row_<xsl:value-of select="$struct_name"/> R, integer1 joinRowType) :=TRANSFORM
      boolean is_deleted := joinRowType = DiffStatus.JoinRowType.OuterRight;
      boolean is_added := joinRowType = DiffStatus.JoinRowType.OuterLeft;

<xsl:if test="EsdlElement[@type]">
      m := DiffScalars(L, R, is_deleted, is_added);
      SELF._diff := IF(is_active, m._diff, '');
      SELF._diffmeta := IF(is_active, m._diffmeta);
</xsl:if>
      SELF._diff_ord := IF (is_added, L._diff_ord, R._diff_ord);
      SELF := IF (is_deleted, R, L);
<xsl:text>
    END;

</xsl:text>
</xsl:if>
    EXPORT AsRecord (layouts._lt_<xsl:call-template name="output_ecl_name"/> _new, layouts._lt_<xsl:call-template name="output_ecl_name"/> _old) := FUNCTION
      RETURN ROW (ProcessTx(_new, _old, false, false));
    END;
  <xsl:for-each select="DiffMatchs/diff_match">
    <xsl:variable name="vid_name" select="translate(@name, ' .', '__')"/>
      EXPORT  integer1 CheckOuter_<xsl:value-of select="$vid_name"/>(layouts._lt_<xsl:value-of select="$struct_name"/> L, layouts._lt_<xsl:value-of select="$struct_name"/> R) := FUNCTION
        boolean IsInner := <xsl:text> (</xsl:text>
        <xsl:for-each select="part">
          <xsl:if test="position()!=1"> AND </xsl:if>L.<xsl:value-of select="@name"/><xsl:text> = </xsl:text>R.<xsl:value-of select="@name"/>
        </xsl:for-each>);

        boolean IsOuterRight :=  <xsl:text> (</xsl:text>
      <xsl:for-each select="part">
        <xsl:if test="position()!=1"> AND </xsl:if>L.<xsl:value-of select="@name"/><xsl:text> = </xsl:text>
        <xsl:choose>
          <xsl:when test="@ftype='number'"><xsl:text>0</xsl:text></xsl:when>
          <xsl:when test="@ftype='bool'"><xsl:text>false</xsl:text></xsl:when>
          <xsl:when test="@ftype='float'"><xsl:text>0.0</xsl:text></xsl:when>
          <xsl:otherwise><xsl:text>''</xsl:text></xsl:otherwise>
        </xsl:choose>
      </xsl:for-each>);
        return IF (IsInner, DiffStatus.JoinRowType.IsInner, IF (IsOuterRight, DiffStatus.JoinRowType.OuterRight, DiffStatus.JoinRowType.OuterLeft));
      END;
      EXPORT  AsDataset_<xsl:value-of select="$vid_name"/> (dataset(layouts._lt_<xsl:value-of select="$struct_name"/>) _n, dataset(layouts._lt_<xsl:value-of select="$struct_name"/>) _o) := FUNCTION

         _new := PROJECT (_n, TRANSFORM (layouts._lt_row_<xsl:value-of select="$struct_name"/>, SELF._diff_ord := 10000 + COUNTER, SELF := LEFT));
         _old := PROJECT (_o, TRANSFORM (layouts._lt_row_<xsl:value-of select="$struct_name"/>, SELF._diff_ord := COUNTER, SELF := LEFT));
         ActiveJoin := JOIN (_new, _old,<xsl:text>
                      </xsl:text>
    <xsl:for-each select="part">
      <xsl:if test="position()!=1"> AND </xsl:if>LEFT.<xsl:value-of select="@name"/> = RIGHT.<xsl:value-of select="@name"/>
    </xsl:for-each>,
                      ProcessTxRow (LEFT, RIGHT,
                      CheckOuter_<xsl:value-of select="$vid_name"/>(LEFT, RIGHT)),
                      FULL OUTER,
                      LIMIT (0));
         PassiveJoin := JOIN (_new, _old,<xsl:text>
                      </xsl:text>
    <xsl:for-each select="part">
      <xsl:if test="position()!=1"> AND </xsl:if>LEFT.<xsl:value-of select="@name"/> = RIGHT.<xsl:value-of select="@name"/>
    </xsl:for-each>,
                      ProcessTxRow (LEFT, RIGHT,
                      CheckOuter_<xsl:value-of select="$vid_name"/>(LEFT, RIGHT)),
                      LEFT OUTER,
                      LIMIT (0));
        RETURN PROJECT(SORT(IF (is_active, ActiveJoin, PassiveJoin), _diff_ord), layouts._lt_<xsl:value-of select="$struct_name"/>);
    END;

  </xsl:for-each>
<xsl:text>END;

</xsl:text>
</xsl:if>
</xsl:template>


<xsl:template match="*" mode="difference.struct"></xsl:template>


<xsl:template match="EsdlStruct|EsdlRequest|EsdlResponse" mode="difference.children.sections">
  <xsl:if test="@base_type">
    <xsl:variable name="base_type" select="@base_type"/>
    <xsl:apply-templates select="/esxdl/EsdlStruct[@name=$base_type]" mode="difference.children.sections"/>
  </xsl:if>
  <xsl:apply-templates select="*" mode="difference.children.sections"/>
</xsl:template>


<xsl:template match="EsdlElement[@complex_type]" mode="difference.children.sections">
<xsl:choose>
  <xsl:when test="@mon_child | @_mon">
    <xsl:variable name="type" select="@complex_type"/>
    <xsl:variable name="field"><xsl:call-template name="output_ecl_name"/></xsl:variable>
      SELF.<xsl:value-of select="$field"/> := _df_<xsl:value-of select="$type"/>(<xsl:call-template name="output_active_check"/>, path + '/' + '<xsl:value-of select="$field"/>', suppress.<xsl:value-of select="$field"/>).AsRecord(L.<xsl:value-of select="$field"/>, R.<xsl:value-of select="$field"/>);
  </xsl:when>
  <xsl:otherwise>
      SELF.<xsl:call-template name="output_ecl_name"/> := L.<xsl:call-template name="output_ecl_name"/>;
  </xsl:otherwise>
</xsl:choose>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="difference.children.sections">
      SELF.<xsl:call-template name="output_ecl_name"/> := L.<xsl:call-template name="output_ecl_name"/>;
</xsl:template>

<xsl:template match="EsdlArray[DiffMatchs/diff_match and (@mon_child='1' or @_mon='1')]" mode="difference.children.sections">
    <xsl:variable name="vid_name" select="translate(DiffMatchs/diff_match/@name, ' .', '__')"/>
    <xsl:variable name="type" select="@type"/>
      SELF.<xsl:call-template name="output_ecl_name"/>  := _df_<xsl:value-of select="$type"/>(<xsl:call-template name="output_active_check"/>, path + '/' + '<xsl:call-template name="output_ecl_name"/>', suppress.<xsl:call-template name="output_ecl_name"/>).AsDataset_<xsl:value-of select="$vid_name"/>(L.<xsl:call-template name="output_ecl_name"/>, R.<xsl:call-template name="output_ecl_name"/>);
</xsl:template>

<xsl:template match="EsdlArray" mode="difference.children.sections">
        SELF.<xsl:call-template name="output_ecl_name"/> := L.<xsl:call-template name="output_ecl_name"/>;
</xsl:template>

<xsl:template match="DiffMatchs|_diff_selectors" mode="difference.children.sections">
</xsl:template>



<xsl:template match="EsdlElement[@complex_type]" mode="difference">
    <xsl:variable name="type" select="@complex_type"/>
    <xsl:variable name="field"><xsl:call-template name="output_ecl_name"/></xsl:variable>
    <xsl:choose>
      <xsl:when test="@_nomon='1' and (not(@_mon) or @_mon='0')">
<xsl:text>      SELF.</xsl:text><xsl:call-template name="output_ecl_name"/>  := L.<xsl:call-template name="output_ecl_name"/><xsl:text>;
</xsl:text>
      </xsl:when>
      <xsl:otherwise>
          path_<xsl:value-of select="$field"/> := path + '/' + '<xsl:value-of select="$field"/>';
    <xsl:choose>
      <xsl:when test="@_nomon='1'">
          updated_<xsl:value-of select="$field"/> := IF (path_<xsl:value-of select="$field"/> IN optional_fields, _df_<xsl:value-of select="$type"/>(<xsl:call-template name="output_active_check"/>, path_<xsl:value-of select="$field"/>, suppress.<xsl:value-of select="$field"/>).AsRecord(L.<xsl:value-of select="$field"/>, R.<xsl:value-of select="$field"/>), L.R.<xsl:value-of select="$field"/>);
        </xsl:when>
        <xsl:otherwise>
          updated_<xsl:value-of select="$field"/> := _df_<xsl:value-of select="$type"/>(<xsl:call-template name="output_active_check"/>, path_<xsl:value-of select="$field"/>, suppress.<xsl:value-of select="$field"/>).AsRecord(L.<xsl:value-of select="$field"/>, R.<xsl:value-of select="$field"/>);
        </xsl:otherwise>
        </xsl:choose>
      checked_<xsl:value-of select="$field"/> := MAP (is_deleted => R.<xsl:value-of select="$field"/>,
                              is_added => L.<xsl:value-of select="$field"/>,
                              updated_<xsl:value-of select="$field"/>);
      SELF.<xsl:value-of select="$field"/> := checked_<xsl:value-of select="$field"/>;
      </xsl:otherwise>
    </xsl:choose>
</xsl:template>

<xsl:template match="EsdlElement[@type and (not(@_nomon) or @_mon='1')]" mode="difference">
    <xsl:variable name="field"><xsl:call-template name="output_ecl_name"/></xsl:variable>
    <xsl:text>      </xsl:text>shared boolean updated_<xsl:value-of select="$field"/> := suppress.<xsl:value-of select="$field"/>=false AND (L.<xsl:value-of select="$field"/> != R.<xsl:value-of select="$field"/><xsl:text>);
</xsl:text>
</xsl:template>

<xsl:template match="EsdlArray" mode="difference">
  <xsl:choose>
  <xsl:when test="DiffMatchs/diff_match and (not(@_nomon) or @_mon='1')">
    <xsl:variable name="vid_name" select="translate(DiffMatchs/diff_match/@name, ' .', '__')"/>
    <xsl:variable name="type" select="@type"/>
    <xsl:variable name="field"><xsl:call-template name="output_ecl_name"/></xsl:variable>
      updated_<xsl:call-template name="output_ecl_name"/> := _df_<xsl:value-of select="$type"/>(<xsl:call-template name="output_active_check"/>, path + '/' + '<xsl:call-template name="output_ecl_name"/>', suppress.<xsl:call-template name="output_ecl_name"/>).AsDataset_<xsl:value-of select="$vid_name"/>(L.<xsl:call-template name="output_ecl_name"/>, R.<xsl:call-template name="output_ecl_name"/>);
      checked_<xsl:value-of select="$field"/> := MAP (is_deleted => R.<xsl:value-of select="$field"/>,
                              is_added => L.<xsl:value-of select="$field"/>,
                              updated_<xsl:value-of select="$field"/>);
      SELF.<xsl:call-template name="output_ecl_name"/>  := checked_<xsl:call-template name="output_ecl_name"/><xsl:text>;
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
<xsl:text>      SELF.</xsl:text><xsl:call-template name="output_ecl_name"/>  := L.<xsl:call-template name="output_ecl_name"/><xsl:text>;
</xsl:text>
  </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="EsdlEnum" mode="difference">
  <xsl:variable name="entype" select="@enum_type"/>
</xsl:template>

<xsl:template match="DiffMatchs|_diff_selectors" mode="difference">
</xsl:template>

<!-- Layouts -->

<xsl:template match="EsdlElement[@complex_type]" mode="layouts">
    <xsl:text>  _lt_</xsl:text><xsl:call-template name="output_ecl_complex_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>};</xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="layouts">
    <xsl:text>  </xsl:text><xsl:call-template name="output_basic_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/>
    <xsl:if test="@ecl_max_len"><xsl:text>, maxlength(</xsl:text><xsl:value-of select="@ecl_max_len"/><xsl:text>)</xsl:text></xsl:if>
    <xsl:text>};</xsl:text><xsl:call-template name="output_comments"/>
<xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="EsdlEnum" mode="layouts">
  <xsl:variable name="entype" select="@enum_type"/>
  <xsl:text>  </xsl:text><xsl:call-template name="output_enum_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
  <xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>}; </xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="EsdlArray[@type='string']" mode="layouts">
    <xsl:text>  set of string </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text>
    <xsl:if test="not(@flat_array)"><xsl:value-of select="@name"/></xsl:if>
    <xsl:text>/</xsl:text><xsl:call-template name="output_item_tag"/>
    <xsl:text>')</xsl:text>
    <xsl:choose>
      <xsl:when test="@max_count_var"><xsl:text>, MAXCOUNT(</xsl:text><xsl:value-of select="@max_count_var"/><xsl:text> * 2)</xsl:text></xsl:when>
      <xsl:when test="@max_count"><xsl:text>, MAXCOUNT(</xsl:text><xsl:value-of select="@max_count"/><xsl:text> * 2)</xsl:text></xsl:when>
      <xsl:otherwise><xsl:text>, MAXCOUNT(1)</xsl:text></xsl:otherwise>
    </xsl:choose>
    <xsl:text>};</xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="EsdlArray[starts-with(@ecl_type,'string') or starts-with(@ecl_type,'unicode')]" mode="layouts">
  <xsl:text>  </xsl:text><xsl:value-of select="@ecl_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
  <xsl:text> {xpath('</xsl:text><xsl:value-of select="@name"/><xsl:text>')};</xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="EsdlArray" mode="layouts">
    <xsl:text>  dataset(_lt_</xsl:text><xsl:call-template name="output_ecl_array_type"/><xsl:text>) </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text><xsl:if test="not(@flat_array)"><xsl:value-of select="@name"/></xsl:if><xsl:text>/</xsl:text><xsl:call-template name="output_item_tag"/><xsl:text>')</xsl:text>
    <xsl:choose>
      <xsl:when test="@max_count_var"><xsl:text>, MAXCOUNT(</xsl:text><xsl:value-of select="@max_count_var"/><xsl:text> * 2)</xsl:text></xsl:when>
      <xsl:when test="@max_count"><xsl:text>, MAXCOUNT(</xsl:text><xsl:value-of select="@max_count"/><xsl:text> * 2)</xsl:text></xsl:when>
      <xsl:otherwise><xsl:text>, MAXCOUNT(1)</xsl:text></xsl:otherwise>
    </xsl:choose>
    <xsl:text>};</xsl:text><xsl:call-template name="output_comments"/><xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="DiffMatchs" mode="layouts">
</xsl:template>

<xsl:template match="_diff_selectors" mode="layouts">
</xsl:template>

<xsl:template match="EsdlStruct" mode="layouts">
    <xsl:text>EXPORT _lt_</xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD</xsl:text>
    <xsl:call-template name="output_lt_base_type"/>
    <xsl:if test="@max_len"><xsl:text>, MAXLENGTH (</xsl:text><xsl:value-of select="@max_len"/><xsl:text>)</xsl:text></xsl:if>
    <xsl:text>
</xsl:text>
    <xsl:apply-templates select="*" mode="layouts"/>
    <xsl:if test="@element and not(*[@name='Content_'])">  string Content_ {xpath('')};</xsl:if>
    <xsl:text>END;

</xsl:text>

    <xsl:if test="@_usedInArray and @diff_monitor">
      <xsl:text>EXPORT _lt_row_</xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD </xsl:text> (_lt_<xsl:call-template name="output_ecl_name"/>)<xsl:text>
    integer _diff_ord {xpath('@diff_ord')} := 0;
  END;
</xsl:text>
    </xsl:if>
</xsl:template>

<xsl:template match="EsdlRequest" mode="layouts">
  <xsl:text>EXPORT _lt_</xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD</xsl:text>
  <xsl:call-template name="output_lt_base_type"/>
  <xsl:if test="@max_len"><xsl:text>, MAXLENGTH (</xsl:text><xsl:value-of select="@max_len"/><xsl:text>)</xsl:text></xsl:if>
    <xsl:text>
</xsl:text>
  <xsl:apply-templates select="*" mode="layouts"/>
  <xsl:text>END;

</xsl:text>
    <xsl:if test="@diff_usedInArray and @diff_monitor">
      <xsl:text>EXPORT _lt_row_</xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD </xsl:text> _lt_<xsl:call-template name="output_ecl_name"/>
      <xsl:text>    integer _diff_ord {xpath('@diff_ord')} := 0;
  END;
</xsl:text>
    </xsl:if>
</xsl:template>

<xsl:template match="EsdlResponse" mode="layouts">
  <xsl:text>EXPORT _lt_</xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD</xsl:text>
  <xsl:call-template name="output_lt_base_type"/>
  <xsl:if test="@max_len"><xsl:text>, MAXLENGTH (</xsl:text><xsl:value-of select="@max_len"/><xsl:text>)</xsl:text></xsl:if>
  <xsl:text>
</xsl:text>
  <xsl:apply-templates select="*" mode="layouts"/>
  <xsl:text>END;

</xsl:text>

    <xsl:if test="@diff_usedInArray and @diff_monitor">
      <xsl:text>EXPORT _lt_row_</xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD </xsl:text> _lt_<xsl:call-template name="output_ecl_name"/>
      <xsl:text>    integer _diff_ord {xpath('@diff_ord')} := 0;
  END;
</xsl:text>
    </xsl:if>
</xsl:template>


<!-- Selectors -->

<xsl:template match="EsdlElement[@complex_type and (not(@_nomon) or @_mon!=0)]" mode="suppressors">
    <xsl:text>  _sup_</xsl:text><xsl:call-template name="output_ecl_complex_type"/><xsl:text> </xsl:text> <xsl:call-template name="output_ecl_name"/>
    <xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>};
</xsl:text>
</xsl:template>

<xsl:template match="EsdlElement[@type]" mode="suppressors">
    <xsl:text>  boolean </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>} := FALSE;
</xsl:text>
</xsl:template>

<xsl:template match="EsdlEnum" mode="suppressors">
  <xsl:text>  boolean </xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> {</xsl:text><xsl:call-template name="output_xpath"/><xsl:text>} := FALSE;
</xsl:text>
</xsl:template>
<xsl:template match="EsdlArray[@type='string']" mode="suppressors">
    <xsl:text>  boolean </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text><xsl:value-of select="@name"/><xsl:text>')}  := FALSE;
</xsl:text>
</xsl:template>

<xsl:template match="EsdlArray[starts-with(@ecl_type,'string') or starts-with(@ecl_type,'unicode')]">
    <xsl:text>  boolean </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text><xsl:value-of select="@name"/><xsl:text>')} := FALSE;
</xsl:text>
</xsl:template>

<xsl:template match="EsdlArray" mode="suppressors">
    <xsl:text>  _sup_</xsl:text><xsl:call-template name="output_ecl_array_type"/><xsl:text> </xsl:text><xsl:call-template name="output_ecl_name"/>
    <xsl:text> {xpath('</xsl:text><xsl:value-of select="@name"/><xsl:text>')};
</xsl:text>
</xsl:template>

<xsl:template match="DiffMatchs" mode="suppressors">
</xsl:template>

<xsl:template match="_diff_selectors" mode="suppressors">
</xsl:template>

<xsl:template match="EsdlStruct" mode="suppressors">
    <xsl:text>EXPORT _sup_</xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD</xsl:text><xsl:if test="@base_type"><xsl:call-template name="output_suppressor_base_type"/></xsl:if><xsl:text>
</xsl:text>
    <xsl:apply-templates select="*" mode="suppressors"/>
    <xsl:text>END;
</xsl:text>
</xsl:template>

<xsl:template match="EsdlResponse" mode="suppressors">
  <xsl:text>EXPORT _sup_</xsl:text><xsl:call-template name="output_ecl_name"/><xsl:text> := RECORD</xsl:text>
  <xsl:if test="@base_type"><xsl:call-template name="output_suppressor_base_type"/></xsl:if><xsl:text>
</xsl:text>
    <xsl:apply-templates select="*" mode="suppressors"/>
    <xsl:text>END;

</xsl:text>
</xsl:template>


<!-- common -->

<xsl:template name="output_ecl_name">
<xsl:choose>
  <xsl:when test="@ecl_name"><xsl:value-of select="@ecl_name"/></xsl:when>
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

<xsl:template name="output_lt_base_type">
    <xsl:choose>
      <xsl:when test="@base_type"><xsl:text> (_lt_</xsl:text><xsl:value-of select="@base_type"/><xsl:text>)</xsl:text>
      </xsl:when>
      <xsl:when test="@diff_monitor or @child_mon or @child_mon_base">
  <xsl:text> (DiffMetaRec)</xsl:text>
      </xsl:when>
      <xsl:otherwise></xsl:otherwise>
    </xsl:choose>
</xsl:template>

<xsl:template name="output_suppressor_base_type">
  <xsl:text> (</xsl:text>_sup_<xsl:value-of select="@base_type"/><xsl:text>)</xsl:text>
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

<xsl:template name="output_active_check">
  <xsl:choose>
    <xsl:when test="_diff_selectors/selector">
      <xsl:for-each select="_diff_selectors/selector">
        <xsl:if test="position()!=1"><xsl:text> OR</xsl:text></xsl:if><xsl:text> Monitor</xsl:text><xsl:value-of select="."/>
      </xsl:for-each>
    </xsl:when>
    <xsl:otherwise>is_active</xsl:otherwise>
  </xsl:choose>
</xsl:template>

</xsl:stylesheet>
