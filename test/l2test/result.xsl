<?xml version='1.0'?>
<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

    <xsl:template match="L2_TEST_RUN_REPORT">
        <html>
            <head>
                <title> All Test Run Summary Report </title>
            </head>
            <body bgcolor="e0e0f0">
                <xsl:apply-templates/>
            </body>
        </html>
    </xsl:template>
    <xsl:template match="L2_HEADER">
        <div align="center">
            <h3> <b> L2 Test framework for AAMP </b>
            </h3>
        </div>
    </xsl:template>
    <xsl:template match="L2_RESULT_LISTING">
        <table cols="3" width="90%" align="center">
            <td width="5%"> </td>
            <td width="75%"> </td>
            <td width="20%"> </td>
            <xsl:apply-templates/>
        </table>
    </xsl:template>
    <xsl:template match="L2_RUN_TEST_SUCCESS">
        <tr bgcolor="e0f0d0">
            <td> </td> <td colspan="1"> Run test <xsl:apply-templates/>... </td> <td bgcolor="50ff50"> Passed </td>
        </tr>
    </xsl:template>
    <xsl:template match="L2_TEST_NOT_RUN">
        <tr bgcolor="e0f0d0">
            <td> </td> <td colspan="1"> Run test <xsl:apply-templates/>... </td> <td bgcolor="ffd580"> Skipped </td>
        </tr>
    </xsl:template>
    <xsl:template match="L2_RUN_TEST_FAILURE">
        <tr bgcolor="e0f0d0">
            <td> </td> <td colspan="1"> Run test <xsl:value-of select="TEST_NAME"/>... </td> <td bgcolor="ff5050"> Failed </td>
        </tr>
        <tr>
            <td colspan="4" bgcolor="ff9090">
                <table width="100%">
                    <tr> <th width="15%"> File Name </th> <td width="50%" bgcolor="e0eee0"> <xsl:value-of select="FILE_NAME"/> </td> <th width="20%"> Line Number </th> <td width="10%" bgcolor="e0eee0"> <xsl:value-of select="LINE_NUMBER"/> </td>  </tr>
                    <tr> <th width="15%"> Exception </th> <td colspan="3" width="85%" bgcolor="e0eee0"> <xsl:value-of select="Exception"/> </td> </tr>
                </table>
            </td>
        </tr>
    </xsl:template>
</xsl:stylesheet>
