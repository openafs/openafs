#include<windows.h>
#include<wchar.h>
#include<strsafe.h>
#include "..\cm_nls.h"

typedef struct wtest_pair {
    const wchar_t * str1;
    const wchar_t * str2;
    int comparison_result;
} wtest_pair;

wtest_pair wtest_pairs[] = {
    { L"abc", L"aBc", 0 },
    { L"Abc", L"Acb", -1 },
    { L"", L"", 0 },
    { L"", L"", 0 },
    { L"", L"", 0 },
    { L"", L"", 0 },
    { L"", L"", 0 },
    { L"", L"", 0 },
};

typedef struct utf8test_pair {
    const char * str1;
    const char * str2;
    int comparison_result;
} utf8test_pair;

utf8test_pair utf8test_pairs[] = {
    {"abc", "abc", 0},
    {"abc", "AbC", 0},
    {"abc", "ABC", 0},
    {"ÆOn", "æoN", 0},
    {"Ǽaaa", "ǽaaa", 0},
    {"Ằ  ", "ằ  ", 0},
    {"Ĺ378", "ĺ378", 0},
    {"Abc", "Acb", -1},
    {"ǼaEa", "ǽaaa", 1},
    {"ùÒz±", "ÙòZ±", 0},
    {"ÀÁÂÃÄÅÆÇÈÉÊË", "àáâãäåæçèéêë", 0},
    {"", "", 0},
    {"", "", 0},
};

int wmain(int argc, wchar_t ** argv)
{

    int i;

    printf("Starting test--\n");
    for (i=0; i<sizeof(utf8test_pairs)/sizeof(utf8test_pairs[0]); i++) {
        printf("Comparing [%s] and [%s]:", utf8test_pairs[i].str1, utf8test_pairs[i].str2);
        printf("strcmp=%d, stricmp=%d, cm_stricmp_utf=%d, expected=%d\n",
               strcmp(utf8test_pairs[i].str1, utf8test_pairs[i].str2),
               stricmp(utf8test_pairs[i].str1, utf8test_pairs[i].str2),
               cm_stricmp_utf8(utf8test_pairs[i].str1, utf8test_pairs[i].str2),
               utf8test_pairs[i].comparison_result);
    }
    printf("End of test\n");

    printf("String navigation test\n");
    {
        char * str="abcdefghijklДڰ١╚☼ﮓﮚﻂﺧﱞ⅔";
        /*                    1111111111222*/
        /*          01234567890123456789012 */
        char * strend;
        char * p;
        int i;

        strend = str + strlen(str);

        printf ("Forward direction:\n");

        for (i=0, p=str; *p && p <= strend; p = char_next_utf8(p)) {
            printf ("Char %d at offset %d\n", i++, p-str);
        }

        printf ("Reverse direction:\n");

        for (i=23,p=strend; p >= str; p = char_prev_utf8(p)) {
            printf ("Char %d at offset %d\n", i--, p-str);
        }

    }
    printf("End of test\n");

    printf("Strupr test\n");
    for (i=0; i<sizeof(utf8test_pairs)/sizeof(utf8test_pairs[0]); i++) {
        char tbuf[MAX_PATH];

        strcpy(tbuf, utf8test_pairs[i].str1);
        strupr_utf8(tbuf, sizeof(tbuf));
        printf("Original [%s]->[%s]", utf8test_pairs[i].str1, tbuf);

        strcpy(tbuf, utf8test_pairs[i].str2);
        strupr_utf8(tbuf, sizeof(tbuf));
        printf(" [%s]->[%s]\n", utf8test_pairs[i].str2, tbuf);
    }
    printf("End of test\n");

    return 0;
}
