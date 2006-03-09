/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information,
 * please seethe file <mit-cpyright.h>.
 *
 * This file contains most of the routines needed by the various
 * make_foo programs, to account for bit- and byte-ordering on
 * different machine types.  It also contains other routines useful in
 * generating the intermediate source files.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <mit-cpyright.h>
#ifndef KERNEL
#include <stdio.h>
#endif
#include <des.h>
#include "des_internal.h"
#include "des_prototypes.h"

int des_debug;

/*
 * The DES algorithm is defined in terms of MSBFIRST, so sometimes,
 * e.g.  VAXes, we need to fix it up.  ANSI order means the DES
 * MSBFIRST order.
 */

#if 0				/* These don't seem to get used anywhere.... */
void
swap_bits(char *array)
{
#ifdef MSBFIRST
    /* just return */
    return;
#else /* LSBFIRST */
    register int old, new, i, j;

    /* for an eight byte block-- */
    /* flips the bit order within each byte from 0 lsb to 0 msb */
    for (i = 0; i <= 7; i++) {
	old = *array;
	new = 0;
	for (j = 0; j <= 7; j++) {
	    new |= old & 01;	/* copy a bit */
	    if (j < 7) {
		/* rotate in opposite directions */
		old = old >> 1;
		new = new << 1;
	    }
	}
	*array++ = new;
    }
#endif /* MSBFIRST */
}

afs_uint32
long_swap_bits(afs_uint32 x)
{
#ifdef MSBFIRST
    return x;
#else
    char *array = (char *)&x;
    register int old, new, i, j;

    /* flips the bit order within each byte from 0 lsb to 0 msb */
    for (i = 0; i <= (sizeof(afs_int32) - 1); i++) {
	old = *array;
	new = 0;
	for (j = 0; j <= 7; j++) {
	    if (old & 01)
		new = new | 01;
	    if (j < 7) {
		old = old >> 1;
		new = new << 1;
	    }
	}
	*array++ = new;
    }
    return x;
#endif /* LSBFIRST */
}
#endif /* 0 */

afs_uint32
swap_six_bits_to_ansi(afs_uint32 old)
{
    register afs_uint32 new, j;

    /* flips the bit order within each byte from 0 lsb to 0 msb */
    new = 0;
    for (j = 0; j <= 5; j++) {
	new |= old & 01;	/* copy a bit */
	if (j < 5) {
	    /* rotate in opposite directions */
	    old = old >> 1;
	    new = new << 1;
	}
    }
    return new;
}

afs_uint32
swap_four_bits_to_ansi(afs_uint32 old)
{
    register afs_uint32 new, j;

    /* flips the bit order within each byte from 0 lsb to 0 msb */
    new = 0;
    for (j = 0; j <= 3; j++) {
	new |= (old & 01);	/* copy a bit */
	if (j < 3) {
	    old = old >> 1;
	    new = new << 1;
	}
    }
    return new;
}

afs_uint32
swap_bit_pos_1(afs_uint32 x)
{
    /*
     * This corrects for the bit ordering of the algorithm, e.g.
     * bit 0 ==> msb, bit 7 lsb.
     *
     * given the number of a bit position, >=1, flips the bit order
     * each byte. e.g. bit 3 --> bit 6, bit 13 --> bit 12
     */
    register int y, z;

    /* always do it, only used by des_make_key_perm.c so far */
    y = (x - 1) / 8;
    z = (x - 1) % 8;

    x = (8 - z) + (y * 8);

    return x;
}

afs_uint32
swap_bit_pos_0(afs_uint32 x)
{
    /*  zero based version */

    /*
     * This corrects for the bit ordering of the algorithm, e.g.
     * bit 0 ==> msb, bit 7 lsb.
     */

#ifdef MSBFIRST
    return x;
#else /* LSBFIRST */
    register int y, z;

    /*
     * given the number of a bit position, >=0, flips the bit order
     * each byte. e.g. bit 3 --> bit 6, bit 13 --> bit 12
     */
    y = x / 8;
    z = x % 8;

    x = (7 - z) + (y * 8);

    return x;
#endif /* LSBFIRST */
}

afs_uint32
swap_bit_pos_0_to_ansi(afs_uint32 x)
{
    /* zero based version */

    /*
     * This corrects for the bit ordering of the algorithm, e.g.
     * bit 0 ==> msb, bit 7 lsb.
     */

    register int y, z;
    /*
     * given the number of a bit position, >=0, flips the bit order each
     * byte. e.g. bit 3 --> bit 6, bit 13 --> bit 12
     */
    y = x / 8;
    z = x % 8;

    x = (7 - z) + (y * 8);

    return x;
}

afs_uint32
rev_swap_bit_pos_0(afs_uint32 x)
{
    /* zero based version */

    /*
     * This corrects for the bit ordering of the algorithm, e.g.
     *  bit 0 ==> msb, bit 7 lsb.
     *
     * Role of LSB and MSB flipped from the swap_bit_pos_0()
     */

#ifdef LSBFIRST
    return x;
#else /* MSBFIRST */

    register int y, z;

    /*
     * given the number of a bit position, >=0, flips the bit order each
     * byte. e.g. bit 3 --> bit 6, bit 13 --> bit 12
     */
    y = x / 8;
    z = x % 8;

    x = (7 - z) + (y * 8);

    return x;
#endif /* MSBFIRST */
}

afs_uint32
swap_byte_bits(afs_uint32 x)
{
#ifdef MSBFIRST
    return x;
#else /* LSBFIRST */

    char *array = (char *)&x;
    register afs_uint32 old, new, j;

    /* flips the bit order within each byte from 0 lsb to 0 msb */
    old = *array;
    new = 0;
    for (j = 0; j <= 7; j++) {
	new |= (old & 01);	/* copy a bit */
	if (j < 7) {
	    old = old >> 1;
	    new = new << 1;
	}
    }
    return new;
#endif /* LSBFIRST */
}

int
swap_long_bytes_bit_number(afs_uint32 x)
{
    /*
     * given a bit number (0-31) from a vax, swap the byte part of the
     * bit number to change the byte ordering to mSBFIRST type
     */
#ifdef LSBFIRST
    return x;
#else /* MSBFIRST */
    afs_uint32 y, z;

    y = x / 8;			/* initial byte component */
    z = x % 8;			/* bit within byte */

    x = (3 - y) * 8 + z;
    return x;
#endif /* MSBFIRST */
}

#if !defined(KERNEL) && defined(AFS_DARWIN80_ENV)
char *_darwin_whichstr[] = {
    "#if defined(__ppc__)\n",
    "#elif defined(__i386__)\n",
    "#else\n#error architecture unsupported\n#endif\n"
};
int _darwin_which = 1;

int
_darwin_swap_long_bytes_bit_number(afs_uint32 x)
{
    /*
     * given a bit number (0-31) from a vax, swap the byte part of the
     * bit number to change the byte ordering to mSBFIRST type
     */

    afs_uint32 y, z;

    if (!_darwin_which)
	return x;

    y = x / 8;			/* initial byte component */
    z = x % 8;			/* bit within byte */

    x = (3 - y) * 8 + z;
    return x;
}
#endif /* !KERNEL && AFS_DARWIN80_ENV */

void
test_set(FILE * stream, const char *src, int testbit, const char *dest,
	 int setbit)
{
#ifdef DES_SHIFT_SHIFT
    if (testbit == setbit)
	fprintf(stream, "    %s |=  %s & (1<<%2d);\n", dest, src, testbit);
    else
	fprintf(stream, "    %s |= (%s & (1<<%2d)) %s %2d;\n", dest, src,
		testbit, (testbit < setbit) ? "<<" : ">>",
		abs(testbit - setbit));
#else
    fprintf(stream, "    if (%s & (1<<%2d))  %s |= 1<<%2d;\n", src, testbit,
	    dest, setbit);
#endif
}
