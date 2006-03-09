/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * This routine generates source code implementing the initial
 * permutation of the DES.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <mit-cpyright.h>
#include <stdio.h>
#include <stdlib.h>
#include <des.h>
#include "des_internal.h"

#define WANT_IP_TABLE
#include "tables.h"

#include "des_prototypes.h"

#define SWAP(x) swap_long_bytes_bit_number(swap_bit_pos_0_to_ansi(x))

void
gen(FILE * stream)
{
    register int i;
#ifdef AFS_DARWIN80_ENV
    int j;

#define swap_long_bytes_bit_number _darwin_swap_long_bytes_bit_number
#endif /* AFS_DARWIN80_ENV */

    /* clear the output */
    fprintf(stream, "    L2 = 0; R2 = 0;\n");

#ifdef AFS_DARWIN80_ENV
  for(j = 0;; j++) {
    fprintf(stream, _darwin_whichstr[j]);
    if (j == 2)
	break;
#endif /* AFS_DARWIN80_ENV */
    /* first setup IP */
    fprintf(stream, "/* IP operations */\n/* first left to left */\n");

    /* first list mapping from left to left */
    for (i = 0; i <= 31; i++)
	if (IP[i] < 32)
	    test_set(stream, "L1", SWAP(IP[i]), "L2", i);

    /* now mapping from right to left */
    fprintf(stream, "\n/* now from right to left */\n");
    for (i = 0; i <= 31; i++)
	if (IP[i] >= 32)
	    test_set(stream, "R1", SWAP(IP[i] - 32), "L2", i);

    fprintf(stream, "\n/* now from left to right */\n");
    /*  list mapping from left to right */
    for (i = 32; i <= 63; i++)
	if (IP[i] < 32)
	    test_set(stream, "L1", SWAP(IP[i]), "R2", i - 32);

    /* now mapping from right to right */
    fprintf(stream, "\n/* last from right to right */\n");
    for (i = 32; i <= 63; i++)
	if (IP[i] >= 32)
	    test_set(stream, "R1", SWAP(IP[i] - 32), "R2", i - 32);
#ifdef AFS_DARWIN80_ENV
    _darwin_which = !_darwin_which;
  }
#endif /* AFS_DARWIN80_ENV */
    exit(0);
}
