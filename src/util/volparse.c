/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header: /cvs/openafs/src/util/volparse.c,v 1.11.14.3 2008/07/01 18:33:38 shadow Exp $");

#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/**
 * map a partition id from any partition-style name.
 *
 * @param[in] aname  partition name string
 *
 * @return partition index number
 *   @retval -1  invalid partition name
 *
 * @see volutil_PartitionName2_r
 * @see volutil_PartitionName_r
 * @see volutil_PartitionName
 */
afs_int32
volutil_GetPartitionID(char *aname)
{
    register char tc;
    afs_int32 temp;
    char ascii[3];

    tc = *aname;
    if (tc == 0)
	return -1;		/* unknown */
    /* numbers go straight through */
    if (tc >= '0' && tc <= '9') {
	temp = atoi(aname);
	/* this next check is to make the syntax less ambiguous when discriminating
	 * between volume numbers and partition IDs.  This less things like
	 * bos salvage do some reasonability checks its input w/o checking
	 * to see if the partition is really on the server.
	 */
	if (temp < 0 || temp > 25)
	    return -1;
	else
	    return temp;
    }
    /* otherwise check for vicepa or /vicepa, or just plain "a" */
    ascii[2] = 0;
    if (strlen(aname) <= 2) {
	strcpy(ascii, aname);
    } else if (!strncmp(aname, "/vicep", 6)) {
	strncpy(ascii, aname + 6, 2);
    } else if (!strncmp(aname, "vicep", 5)) {
	strncpy(ascii, aname + 5, 2);
    } else
	return -1;		/* bad partition name */
    /* now partitions are named /vicepa ... /vicepz, /vicepaa, /vicepab, .../vicepzz,
     * and are numbered from 0.  Do the appropriate conversion */
    if (ascii[1] == 0) {
	/* one char name, 0..25 */
	if (ascii[0] < 'a' || ascii[0] > 'z')
	    return -1;		/* wrongo */
	return ascii[0] - 'a';
    } else {
	/* two char name, 26 .. <whatever> */
	if (ascii[0] < 'a' || ascii[0] > 'z')
	    return -1;		/* wrongo */
	if (ascii[1] < 'a' || ascii[1] > 'z')
	    return -1;		/* just as bad */
	return (ascii[0] - 'a') * 26 + (ascii[1] - 'a') + 26;
    }
}

/**
 * convert a partition index number into a partition name string (/vicepXX).
 *
 * @param[in]  part     partition index number
 * @param[out] tbuffer  buffer in which to store name
 * @param[in]  buflen   length of tbuffer
 *
 * @return operation status
 *   @retval 0   success
 *   @retval -1  buffer too short
 *   @retval -2  invalid partition id
 *
 * @see volutil_PartitionName_r
 * @see volutil_PartitionName
 * @see volutil_GetPartitionID
 */
afs_int32
volutil_PartitionName2_r(afs_int32 part, char *tbuffer, size_t buflen)
{
    char tempString[3];
    register int i;

    if (part < 0 || part >= (26 * 26 + 26)) {
	return -2;
    }

    tempString[1] = tempString[2] = 0;
    strncpy(tbuffer, "/vicep", buflen);
    if (part <= 25) {
	tempString[0] = 'a' + part;
    } else {
	part -= 26;
	i = (part / 26);
	tempString[0] = i + 'a';
	tempString[1] = (part % 26) + 'a';
    }
    if (strlcat(tbuffer, tempString, buflen) >= buflen) {
	return -1;
    }
    return 0;
}

#define BAD_VID "BAD VOLUME ID"
#define BAD_VID_LEN (sizeof(BAD_VID))
/**
 * convert a partition index number into a partition name string (/vicepXX).
 *
 * @param[in]  part     partition index number
 * @param[out] tbuffer  buffer in which to store name
 * @param[in]  buflen   length of tbuffer
 *
 * @return partition name string
 *   @retval ""               buffer too short
 *   @retval "SPC"            buffer too short
 *   @retval "BAD VOLUME ID"  avalue contains an invalid partition index
 *
 * @note you may wish to consider using volutil_PartitionName2_r, as its
 *       error handling is more standard
 *
 * @see volutil_PartitionName2_r
 * @see volutil_PartitionName
 * @see volutil_GetPartitionID
 */
char *
volutil_PartitionName_r(int part, char *tbuffer, int buflen)
{
    afs_int32 code;

    if (buflen < BAD_VID_LEN) {
	strlcpy(tbuffer, "SPC", buflen);
	return tbuffer;
    }

    code = volutil_PartitionName2_r(part, tbuffer, buflen);

    if (code == -2) {
	strlcpy(tbuffer, BAD_VID, buflen);
    }

    return tbuffer;
}

/**
 * convert a partition index number into a partition name string (/vicepXX).
 *
 * @param[in] avalue  partition index number
 *
 * @return partition name string
 *   @retval "BAD VOLUME ID"  avalue contains an invalid partition index
 *
 * @warning this interface is not re-entrant
 *
 * @see volutil_PartitionName2_r
 * @see volutil_PartitionName_r
 * @see volutil_GetPartitionID
 */
char *
volutil_PartitionName(int avalue)
{
#define VPN_TBUFLEN 64
    static char tbuffer[VPN_TBUFLEN];
    return volutil_PartitionName_r(avalue, tbuffer, VPN_TBUFLEN - 1);
}

/* is this a digit or a digit-like thing? */
static int
ismeta(register int ac, register int abase)
{
/*    if (ac == '-' || ac == 'x' || ac == 'X') return 1; */
    if (ac >= '0' && ac <= '7')
	return 1;
    if (abase <= 8)
	return 0;
    if (ac >= '8' && ac <= '9')
	return 1;
    if (abase <= 10)
	return 0;
    if (ac >= 'a' && ac <= 'f')
	return 1;
    if (ac >= 'A' && ac <= 'F')
	return 1;
    return 0;
}

/* given that this is a digit or a digit-like thing, compute its value */
static int
getmeta(register int ac)
{
    if (ac >= '0' && ac <= '9')
	return ac - '0';
    if (ac >= 'a' && ac <= 'f')
	return ac - 'a' + 10;
    if (ac >= 'A' && ac <= 'F')
	return ac - 'A' + 10;
    return 0;
}

afs_int32
util_GetInt32(register char *as, afs_int32 * aval)
{
    register afs_int32 total;
    register int tc;
    int base;
    int negative;

    total = 0;			/* initialize things */
    negative = 0;

    /* skip over leading spaces */
    while ((tc = *as)) {
	if (tc != ' ' && tc != '\t')
	    break;
    }

    /* compute sign */
    if (*as == '-') {
	negative = 1;
	as++;			/* skip over character */
    }

    /* compute the base */
    if (*as == '0') {
	as++;
	if (*as == 'x' || *as == 'X') {
	    base = 16;
	    as++;
	} else
	    base = 8;
    } else
	base = 10;

    /* compute the # itself */
    while ((tc = *as)) {
	if (!ismeta(tc, base))
	    return -1;
	total *= base;
	total += getmeta(tc);
	as++;
    }

    if (negative)
	*aval = -total;
    else
	*aval = total;
    return 0;
}

afs_uint32
util_GetUInt32(register char *as, afs_uint32 * aval)
{
    register afs_uint32 total;
    register int tc;
    int base;

    total = 0;			/* initialize things */

    /* skip over leading spaces */
    while ((tc = *as)) {
	if (tc != ' ' && tc != '\t')
	    break;
    }

    /* compute the base */
    if (*as == '0') {
	as++;
	if (*as == 'x' || *as == 'X') {
	    base = 16;
	    as++;
	} else
	    base = 8;
    } else
	base = 10;

    /* compute the # itself */
    while ((tc = *as)) {
	if (!ismeta(tc, base))
	    return -1;
	total *= base;
	total += getmeta(tc);
	as++;
    }

    *aval = total;
    return 0;
}
