/*
 * CMUCS AFStools
 * dumpscan - routines for scanning and manipulating AFS volume dumps
 *
 * Copyright (c) 1998 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/* int64.c - Support for 64-bit integers */

#include <stdio.h>
#include <string.h>
#include "intNN.h"

char *hexify_int64(u_int64 *X, char *buf)
{
  static char mybuf[17];

#ifdef NATIVE_INT64
  char c, *p;
  u_int64 x = *X;

  if (!buf) buf = mybuf;
  p = buf + 16;
  *p-- = 0;
  while (x && p >= buf) {
    c = x & 0xf;
    c += ((c < 10) ? '0' : 'a' - 10);
    *p-- = c;
    x >>= 4;
  }
  while (p >= buf) *p-- = '0';

#else
  if (!buf) buf = mybuf;
  sprintf(buf, "%08lx%08lx", X->hi, X->lo);
#endif

  return buf;
}


#ifdef NATIVE_INT64
char *decimate_int64(u_int64 *X, char *buf)
{
  static char mybuf[21];
  char *p;
  u_int64 x = *X;

  if (!buf) buf = mybuf;
  p = buf + 21;
  *--p = 0;
  while (x && p > buf) {
    *--p = ((x % 10) + '0');
    x /= 10;
  }
  if (!*p) *--p = '0';
  return p;
}

#else
static char bitvals[64][21] = {
/*                1 */ "00000000000000000001",
/*                2 */ "00000000000000000002",
/*                4 */ "00000000000000000004",
/*                8 */ "00000000000000000008",
/*               10 */ "00000000000000000016",
/*               20 */ "00000000000000000032",
/*               40 */ "00000000000000000064",
/*               80 */ "00000000000000000128",
/*              100 */ "00000000000000000256",
/*              200 */ "00000000000000000512",
/*              400 */ "00000000000000001024",
/*              800 */ "00000000000000002048",
/*             1000 */ "00000000000000004096",
/*             2000 */ "00000000000000008192",
/*             4000 */ "00000000000000016384",
/*             8000 */ "00000000000000032768",
/*            10000 */ "00000000000000065536",
/*            20000 */ "00000000000000131072",
/*            40000 */ "00000000000000262144",
/*            80000 */ "00000000000000524288",
/*           100000 */ "00000000000001048576",
/*           200000 */ "00000000000002097152",
/*           400000 */ "00000000000004194304",
/*           800000 */ "00000000000008388608",
/*          1000000 */ "00000000000016777216",
/*          2000000 */ "00000000000033554432",
/*          4000000 */ "00000000000067108864",
/*          8000000 */ "00000000000134217728",
/*         10000000 */ "00000000000268435456",
/*         20000000 */ "00000000000536870912",
/*         40000000 */ "00000000001073741824",
/*         80000000 */ "00000000002147483648",
/*        100000000 */ "00000000004294967296",
/*        200000000 */ "00000000008589934592",
/*        400000000 */ "00000000017179869184",
/*        800000000 */ "00000000034359738368",
/*       1000000000 */ "00000000068719476736",
/*       2000000000 */ "00000000137438953472",
/*       4000000000 */ "00000000274877906944",
/*       8000000000 */ "00000000549755813888",
/*      10000000000 */ "00000001099511627776",
/*      20000000000 */ "00000002199023255552",
/*      40000000000 */ "00000004398046511104",
/*      80000000000 */ "00000008796093022208",
/*     100000000000 */ "00000017592186044416",
/*     200000000000 */ "00000035184372088832",
/*     400000000000 */ "00000070368744177664",
/*     800000000000 */ "00000140737488355328",
/*    1000000000000 */ "00000281474976710656",
/*    2000000000000 */ "00000562949953421312",
/*    4000000000000 */ "00001125899906842624",
/*    8000000000000 */ "00002251799813685248",
/*   10000000000000 */ "00004503599627370496",
/*   20000000000000 */ "00009007199254740992",
/*   40000000000000 */ "00018014398509481984",
/*   80000000000000 */ "00036028797018963968",
/*  100000000000000 */ "00072057594037927936",
/*  200000000000000 */ "00144115188075855872",
/*  400000000000000 */ "00288230376151711744",
/*  800000000000000 */ "00576460752303423488",
/* 1000000000000000 */ "01152921504606846976",
/* 2000000000000000 */ "02305843009213693952",
/* 4000000000000000 */ "04611686018427387904",
/* 8000000000000000 */ "09223372036854775808" };


static void prep_table(void)
{
  int bit, digit;

  if (bitvals[0][0] < '0') return;
  for (bit = 0; bit < 64; bit++)
    for (digit = 0; digit < 20; digit++)
      bitvals[bit][digit] -= '0';
}


static void add_bit(int bit, char *answer)
{
  int digit;

  for (digit = 19; digit >= 0; digit--) {
    answer[digit] += bitvals[bit][digit];
    if (!digit) break;
    while(answer[digit] > 9) {
      answer[digit] -= 10;
      answer[digit-1]++;
    }
  }
}


static void decimate(unsigned long hi, unsigned long lo, char *answer)
{
  unsigned long mask;
  int bit, digit;

  memset(answer, 0, 21);
  for (bit = 0, mask = 1; bit < 32; bit++, mask <<= 1)
    if (lo&mask) add_bit(bit, answer);
  for (bit = 0, mask = 1; bit < 32; bit++, mask <<= 1)
    if (hi&mask) add_bit(bit + 32, answer);

  for (digit = 0; digit < 20; digit++)
    answer[digit] += '0';
}

char *decimate_int64(u_int64 *X, char *buf)
{
  static char mybuf[21];
  char *p;

  prep_table();
  if (!buf) buf = mybuf;
  decimate(X->hi, X->lo, buf);
  for (p = buf; *p == '0'; p++);
  return (*p) ? p : p-1;
}

#endif /* NATIVE_INT64 */


void shift_int64(u_int64 *X, int bits)
{
#ifdef NATIVE_INT64
  if (bits < 0) *X >>= (-bits);
  else          *X <<= bits;
#else
  if (bits < 0) {
    bits = -bits;
    if (bits >= 32) {
      X->lo = ((X->hi & 0xffffffffL) >> (bits - 32));
      X->hi = 0;
    } else {
      X->lo = ((X->lo & 0xffffffffL) >> bits)
            | ((X->hi & ((1 << (32 - bits)) - 1)) << (32 - bits));
      X->hi = ((X->hi & 0xffffffffL) >> bits);
    }
  } else {
    if (bits >= 32) {
      X->hi = ((X->lo & 0xffffffffL) << (bits - 32));
      X->lo = 0;
    } else {
      X->hi = ((X->hi & 0xffffffffL) << bits)
            | ((X->lo & (((1 << bits) - 1) << (32 - bits))) >> (32 - bits));
      X->lo = ((X->lo & 0xffffffffL) << bits);
    }
  }
#endif
}


#ifdef TEST_INT64

/** the rest of this is for testing the int64 suite **/

#ifdef NATIVE_INT64

#define xize(x) #x
#define stringize(x) xize(x)
#define INT64_NAME stringize(unsigned NATIVE_INT64)


#endif /* NATIVE_INT64 */


void verify_int64_size () {
#ifdef NATIVE_INT64
  signed char testchar = -1;
  unsigned int testint = (unsigned char)testchar;

  printf("We think '%s' is a native 64-bit type\n", INT64_NAME);

  if (testint != 0xff) {
    printf("testint = 0x%x; should be 0xff\n", testint);
    fprintf(stderr, "Hmm...  char's are not 8 bits.  That sucks!\n");
    exit(-1);
  }
  printf("Looks like a char is 8 bits...\n");

  if (sizeof(unsigned NATIVE_INT64) != 8) {
    printf("sizeof(%s) = %d; should be 8\n", INT64_NAME, sizeof(unsigned NATIVE_INT64));
    fprintf(stderr, "Hey!  You said a %s was 64-bits wide!\n", INT64_NAME);
    exit(-1);
  }
  printf("Yippee!  We have a native 64-bit type (%s)\n\n", INT64_NAME);

#else /* !NATIVE_INT64 */

  printf("Using fake 64-bit integers...\n\n");
#endif /* NATIVE_INT64 */
}


void test_int64_constructs(void)
{
  u_int64 x, y;
  afs_uint32 hi, lo;
  int failures = 0, pass;
  char buf[17];

  printf("Constructor/accessor tests:\n");
  printf("Setting x := %s\n", INT64_TEST_STR);
  mk64(x, INT64_TEST_HI, INT64_TEST_LO);

#ifdef NATIVE_INT64
  pass = (x == INT64_TEST_CONST);
  hexify_int64(&x, buf);
  printf("NATIVE mk64: x       = 0x%16s                %s\n",
         buf, pass ? "PASSED" : "FAILED");
  if (!pass) failures++;
#else
  pass = (x.hi == INT64_TEST_HI && x.lo == INT64_TEST_LO);
  printf("FAKE mk64:   x.hi    = 0x%08lx  x.lo    = 0x%08lx  %s\n",
         x.hi, x.lo, pass ? "PASSED" : "FAILED");
  if (!pass) failures++;
#endif

  pass = (hi64(x) == INT64_TEST_HI && lo64(x) == INT64_TEST_LO);
  printf("hi64/lo64:   hi64(x) = 0x%08lx  lo64(x) = 0x%08lx  %s\n",
         hi64(x), lo64(x), pass ? "PASSED" : "FAILED");
  if (!pass) failures++;

  ex64(x, hi, lo);
  pass = (hi == INT64_TEST_HI && lo == INT64_TEST_LO);
  printf("ex64:        hi      = 0x%08lx  lo      = 0x%08lx  %s\n",
         hi, lo, pass ? "PASSED" : "FAILED");
  if (!pass) failures++;

  cp64(y, x);
  pass = (hi64(y) == INT64_TEST_HI && lo64(y) == INT64_TEST_LO);
  printf("cp64:        hi64(y) = 0x%08lx  lo64(y) = 0x%08lx  %s\n",
         hi64(y), lo64(y), pass ? "PASSED" : "FAILED");
  if (!pass) failures++;

  if (failures) printf("%d/4 tests FAILED\n\n", failures);
  else          printf("All 4 tests PASSED\n\n");
}


void test_int64_compares()
{
#define NCOMPARE 9
  u_int64 control, test[NCOMPARE];
  char cbuf[17], tbuf[17];
  int i, r, result[NCOMPARE];
  int pass, failures, FAILURES = 0;

  printf("Comparison tests:\n");

  mk64(control, 0x12345678, 0xabcdabcd);
  mk64(test[0], 0x12340000, 0xabcd0000);  result[0] = +1;
  mk64(test[1], 0x12340000, 0xabcdabcd);  result[1] = +1;
  mk64(test[2], 0x12340000, 0xabcdffff);  result[2] = +1;
  mk64(test[3], 0x12345678, 0xabcd0000);  result[3] = +1;
  mk64(test[4], 0x12345678, 0xabcdabcd);  result[4] =  0;
  mk64(test[5], 0x12345678, 0xabcdffff);  result[5] = -1;
  mk64(test[6], 0x1234ffff, 0xabcd0000);  result[6] = -1;
  mk64(test[7], 0x1234ffff, 0xabcdabcd);  result[7] = -1;
  mk64(test[8], 0x1234ffff, 0xabcdffff);  result[8] = -1;

  for (i = 0; i < NCOMPARE; i++) {
    failures = 0;
    hexify_int64(&control, cbuf);
    hexify_int64(&test[i], tbuf);

    r = eq64(control, test[i]);
    pass = (r == (result[i] == 0));   if (!pass) failures++;
    printf("0x%s == 0x%s %s\n", cbuf, tbuf, pass ? "PASSED" : "FAILED");

    r = ne64(control, test[i]);
    pass = (r == (result[i] != 0));   if (!pass) failures++;
    printf("0x%s != 0x%s %s\n", cbuf, tbuf, pass ? "PASSED" : "FAILED");

    r = lt64(control, test[i]);
    pass = (r == (result[i] <  0));   if (!pass) failures++;
    printf("0x%s <  0x%s %s\n", cbuf, tbuf, pass ? "PASSED" : "FAILED");

    r = le64(control, test[i]);
    pass = (r == (result[i] <= 0));   if (!pass) failures++;
    printf("0x%s <= 0x%s %s\n", cbuf, tbuf, pass ? "PASSED" : "FAILED");

    r = gt64(control, test[i]);
    pass = (r == (result[i] >  0));   if (!pass) failures++;
    printf("0x%s >  0x%s %s\n", cbuf, tbuf, pass ? "PASSED" : "FAILED");

    r = ge64(control, test[i]);
    pass = (r == (result[i] >= 0));   if (!pass) failures++;
    printf("0x%s >= 0x%s %s\n", cbuf, tbuf, pass ? "PASSED" : "FAILED");

    r = zero64(test[i]);
    pass = !r;                        if (!pass) failures++;
    printf("0x%s is nonzero            %s\n",
           tbuf, pass ? "PASSED" : "FAILED");

    if (failures) printf("%d/7 tests on this pair FAILED\n\n", failures);
    else          printf("All 7 tests on this pair PASSED\n\n");
  }

  mk64(control, 0, 0);
  pass = zero64(control);  if (!pass) FAILURES++;
  printf("0x0000000000000000 is zero               %s\n",
         pass ? "PASSED" : "FAILED");
  
  if (FAILURES)
    printf("%d/%d comparison tests FAILED\n\n", FAILURES, 7 * NCOMPARE + 1);
  else
    printf("All %d comparison tests PASSED\n\n", 7 * NCOMPARE + 1);
}


void test_int64_arith()
{
  printf("No arithmetic tests yet!!!\n");
}


void main() {
  verify_int64_size();
  test_int64_constructs();
  test_int64_compares();
  test_int64_arith();
  exit(0);
}
#endif
