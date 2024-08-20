#include <iostream>
#include <string>
#include <string.h>

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include <iso639map.cpp>

static void TestHelper( const char *lang, LangCodePreference langCodePreference, const char *expectedResult )
{
    printf( "%s[", lang );
    switch( langCodePreference )
    {
    case ISO639_NO_LANGCODE_PREFERENCE:
        printf( "ISO639_NO_LANGCODE_PREFERENCE" );
        break;
    case ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE:
        printf( "ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE" );
        break;
    case ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE:
        printf( "ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE" );
        break;
    case ISO639_PREFER_2_CHAR_LANGCODE:
        printf( "ISO639_PREFER_2_CHAR_LANGCODE" );
        break;
    }
    char temp[256];
    strcpy( temp, lang );
    iso639map_NormalizeLanguageCode( temp, langCodePreference );
    printf( "] -> %s : ", temp );
    EXPECT_STREQ(temp, expectedResult);
}

TEST(_Iso639Map, test1)
{
    TestHelper( "en", ISO639_NO_LANGCODE_PREFERENCE, "en" );
    TestHelper( "en", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "eng" );
    TestHelper( "en", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "eng" );
    TestHelper( "en", ISO639_PREFER_2_CHAR_LANGCODE, "en" );

    TestHelper( "eng", ISO639_NO_LANGCODE_PREFERENCE, "eng" );
    TestHelper( "eng", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "eng" );
    TestHelper( "eng", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "eng" );
    TestHelper( "eng", ISO639_PREFER_2_CHAR_LANGCODE, "en" );

    TestHelper( "de", ISO639_NO_LANGCODE_PREFERENCE, "de" );
    TestHelper( "de", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "de", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "de", ISO639_PREFER_2_CHAR_LANGCODE, "de" );
    
    TestHelper( "deu", ISO639_NO_LANGCODE_PREFERENCE, "deu" );
    TestHelper( "deu", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "deu", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "deu", ISO639_PREFER_2_CHAR_LANGCODE, "de" );
    
    TestHelper( "ger", ISO639_NO_LANGCODE_PREFERENCE, "ger" );
    TestHelper( "ger", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "ger", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "ger", ISO639_PREFER_2_CHAR_LANGCODE, "de" );

    //XIONE-503: Uppercase lang codes case:
    TestHelper( "DE", ISO639_NO_LANGCODE_PREFERENCE, "de" );
    TestHelper( "DE", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "DE", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "DE", ISO639_PREFER_2_CHAR_LANGCODE, "de" );

    TestHelper( "DEU", ISO639_NO_LANGCODE_PREFERENCE, "deu" );
    TestHelper( "DEU", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "DEU", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "DEU", ISO639_PREFER_2_CHAR_LANGCODE, "de" );

    TestHelper( "GER", ISO639_NO_LANGCODE_PREFERENCE, "ger" );
    TestHelper( "GER", ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE, "ger" );
    TestHelper( "GER", ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE, "deu" );
    TestHelper( "GER", ISO639_PREFER_2_CHAR_LANGCODE, "de" );
}


