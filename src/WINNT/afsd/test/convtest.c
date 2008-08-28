#include<stdio.h>
#include<wchar.h>
#include "..\cm_nls.h"

#define elements_in(a) (sizeof(a)/sizeof(a[0]))

struct norm_pair {
    const cm_unichar_t * left;
    const cm_normchar_t * right;
};

struct norm_pair normalization_pairs[] = {
    { L"abcdefghijklmnopq", L"abcdefghijklmnopq" },
    { L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234",

      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234"
      L"abcdefghijklmnopqrstuvwxyz1234" },
    { L"12839481flalfoo_)()(*&#@(*&",
      L"12839481flalfoo_)()(*&#@(*&" }
};

void dumputf8(const unsigned char * s) {
    while (*s) {
        printf("%02X ", (int) *s++);
    }
}

void dumpunicode(const wchar_t * s) {
    while (*s) {
        printf("%04X ", (int) *s++);
    }
}

int cm_NormalizeStringAllocTest(void)
{
    int i;

    for (i=0; i < elements_in(normalization_pairs); i++) {
        cm_normchar_t * nstr;
        int cchdest = 0;

        printf ("Test #%d:", i);

        nstr = cm_NormalizeStringAlloc(normalization_pairs[i].left, -1, &cchdest);

        if (nstr == NULL) {
            printf ("FAILED! returned a NULL\n");
            return 1;
        }

        if (wcscmp(nstr, normalization_pairs[i].right)) {
            printf ("FAILED: Expected [");
            dumpunicode(normalization_pairs[i].right);
            printf ("] Received [");
            dumpunicode(nstr);
            printf ("]\n");
            return 1;
        }

        if (wcslen(nstr) != cchdest - 1) {
            printf ("FAILED: Length is wrong\n");
            return 1;
        }

        printf ("PASS\n");

        free (nstr);
    }

    return 0;
}

typedef struct norm_test_entry {
    const wchar_t * str;
    const wchar_t * nfc;
    const wchar_t * nfd;
    const wchar_t * nfkc;
    const wchar_t * nfkd;
} norm_test_entry;

extern norm_test_entry norm_tests[];
extern int n_norm_tests;

int cm_NormalizeStringTest(void)
{
    int i;
    int n_failed = 0;

    for (i=0; i < n_norm_tests; i++) {
        cm_normchar_t * nfc;

        nfc = cm_NormalizeStringAlloc(norm_tests[i].nfd, -1, NULL);
        if (nfc == NULL) {
            printf ("FAILED: returned a NULL\n");
            return 1;
        }

        if (wcscmp(nfc, norm_tests[i].nfc)) {
            printf ("FAILED: Expected [");
            dumpunicode(norm_tests[i].nfc);
            printf ("] Received [");
            dumpunicode(nfc);
            printf ("]\n");
            n_failed ++;
        }

        free(nfc);
    }

    if (n_failed)
        printf ("Number of failed tests: %d\n", n_failed);

    return 0;
}

typedef struct conv_test_entry {
    const cm_utf8char_t * str;
    const cm_unichar_t  * wstr;
} conv_test_entry;

#define CTEST(a) { a, L ## a }

conv_test_entry conv_tests[] = {
    CTEST(""),
    CTEST("a"),
    CTEST("abcdefghijkl"),
    CTEST("osidfja*(2312835"),
    {"\xee\x80\x80", L"\xe000"},
    {"\xef\xbf\xbd", L"\xfffd"},
    {"\xf0\x9f\xbf\xbf", L"\xd83f\xdfff"}, /* Surrogates */
    {"\xF1\x9F\xBF\xBE", L"\xD93F\xDFFE"},
    {"\xf0\x90\x80\x80", L"\xd800\xdc00"},
};

int cm_Utf16ToUtf8AllocTest(void)
{
    int i;

    for (i=0; i < sizeof(conv_tests)/sizeof(conv_tests[0]); i++) {
        cm_utf8char_t * c;
        int len = 0;

        printf ("Test #%d:", i);

        c = cm_Utf16ToUtf8Alloc(conv_tests[i].wstr, -1, &len);

        if (c == NULL) {
            printf ("FAILED: returned NULL\n");
            return 1;
        }

        if (strlen(c) + 1 != len) {
            printf ("FAILED: Returned wrong length [%d]. Actual length [%d]\n", len,
                    strlen(c) + 1);
            return 1;
        }

        if (strcmp(c, conv_tests[i].str)) {
            printf ("FAILED: Expected [");
            dumputf8(conv_tests[i].str);
            printf ("]. Returned [");
            dumputf8(c);
            printf ("]\n");
            return 1;
        }

        printf("PASS\n");

        free(c);
    }

    return 0;
}

int cm_Utf16ToUtf8Test(void)
{
    int i;
    cm_utf8char_t c[1024];

    for (i=0; i < sizeof(conv_tests)/sizeof(conv_tests[0]); i++) {
        int len;

        printf ("Test #%d:", i);

        len = cm_Utf16ToUtf8(conv_tests[i].wstr, -1, c, sizeof(c)/sizeof(c[0]));

        if (len == 0) {
            printf ("FAILED: returned 0\n");
            return 1;
        }

        if (strlen(c) + 1 != len) {
            printf ("FAILED: Returned wrong length [%d]. Actual length [%d]\n", len,
                    strlen(c) + 1);
            return 1;
        }

        if (strcmp(c, conv_tests[i].str)) {
            printf ("FAILED: Expected [%s]. Returned [%s]\n", conv_tests[i].str, c);
            return 1;
        }

        printf("PASS\n");
    }

    return 0;
}

int cm_Utf8ToUtf16AllocTest(void)
{
    int i;

    for (i=0; i < sizeof(conv_tests)/sizeof(conv_tests[0]); i++) {
        cm_unichar_t * c;
        int len = 0;

        printf ("Test #%d:", i);

        c = cm_Utf8ToUtf16Alloc(conv_tests[i].str, -1, &len);

        if (c == NULL) {
            printf ("FAILED: returned NULL\n");
            return 1;
        }

        if (wcslen(c) + 1 != len) {
            printf ("FAILED: Returned wrong length [%d]. Actual length [%d]\n", len,
                    wcslen(c) + 1);
            return 1;
        }

        if (wcscmp(c, conv_tests[i].wstr)) {
            printf ("FAILED: Expected [");
            dumpunicode(conv_tests[i].wstr);
            printf ("]. Returned [");
            dumpunicode(c);
            printf ("]\n");
            return 1;
        }

        printf("PASS\n");

        free(c);
    }

    return 0;
}

int cm_Utf8ToUtf16Test(void)
{
    int i;
    cm_unichar_t c[1024];

    for (i=0; i < sizeof(conv_tests)/sizeof(conv_tests[0]); i++) {
        int len = 0;

        printf ("Test #%d:", i);

        len = cm_Utf8ToUtf16(conv_tests[i].str, -1, c, sizeof(c)/sizeof(c[0]));

        if (len == 0) {
            printf ("FAILED: returned 0\n");
            return 1;
        }

        if (wcslen(c) + 1 != len) {
            printf ("FAILED: Returned wrong length [%d]. Actual length [%d]\n", len,
                    wcslen(c) + 1);
            return 1;
        }

        if (wcscmp(c, conv_tests[i].wstr)) {
            printf ("FAILED: Expected [");
            dumpunicode(conv_tests[i].wstr);
            printf ("]. Returned [");
            dumpunicode(c);
            printf ("]\n");
            return 1;
        }

        printf("PASS\n");
    }

    return 0;
}

int main(int argc, char ** argv)
{
    int trv;

    cm_InitNormalization();

#define RUNTEST(f) printf("Begin " #f "\n"); trv = f(); printf ("End " #f "\n\n"); if (trv != 0) return trv;

    RUNTEST(cm_NormalizeStringAllocTest);
    RUNTEST(cm_NormalizeStringTest);
    RUNTEST(cm_Utf16ToUtf8AllocTest);
    RUNTEST(cm_Utf16ToUtf8Test);
    RUNTEST(cm_Utf8ToUtf16AllocTest);
    RUNTEST(cm_Utf8ToUtf16Test);
    return 0;
}
