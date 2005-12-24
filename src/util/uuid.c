/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* String conversion routines have the following copyright */

/*
 * Copyright (c) 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <afsconfig.h>
#ifdef KERNEL
#include "afs/param.h"
#else
#include <afs/param.h>
#endif

RCSID
    ("$Header$");

#ifdef KERNEL
#include "afs/sysincludes.h"
#include "afsincludes.h"
#define uuid_memcmp(A,B,C)	memcmp(A, B, C)
#define uuid_memcpy(A,B,C)	memcpy(A, B, C)
#else /* KERNEL */
#include <stdio.h>
#include <errno.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#include <process.h>
#else
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef ITIMER_REAL
#include <sys/time.h>
#endif /* ITIMER_REAL */
#include <net/if.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#if !defined(AFS_NT40_ENV) && !defined(AFS_LINUX20_ENV)
#include <netinet/if_ether.h>
#endif
#include "afsutil.h"

#define uuid_memcmp(A,B,C)	memcmp(A,B,C)
#define uuid_memcpy(A,B,C)	memcpy(A,B,C)
#endif /* KERNEL */


typedef struct {
    char eaddr[6];		/* 6 bytes of ethernet hardware address */
} uuid_address_t, *uuid_address_p_t;


typedef struct {
    afs_uint32 lo;
    afs_uint32 hi;
} uuid_time_t, *uuid_time_p_t;

static int uuid_get_address(uuid_address_p_t addr);
void uuid__get_os_time(uuid_time_t * os_time);

/*
 * |<------------------------- 32 bits -------------------------->|
 *
 * +--------------------------------------------------------------+
 * |                     low 32 bits of time                      |  0-3  .time_low
 * +-------------------------------+-------------------------------
 * |     mid 16 bits of time       |  4-5               .time_mid
 * +-------+-----------------------+
 * | vers. |   hi 12 bits of time  |  6-7               .time_hi_and_version
 * +-------+-------+---------------+
 * |Res|  clkSeqHi |  8                                 .clock_seq_hi_and_reserved
 * +---------------+
 * |   clkSeqLow   |  9                                 .clock_seq_low
 * +---------------+----------...-----+
 * |            node ID               |  8-16           .node
 * +--------------------------...-----+
 */

afsUUID afs_uuid_g_nil_uuid = { 0 };
static uuid_time_t time_now, time_last;
static u_short uuid_time_adjust, clock_seq;
static afs_uint32 rand_m, rand_ia, rand_ib, rand_irand, uuid_init_done = 0;

#define	uuid_create_nil(uuid) memset(uuid, 0, sizeof(afsUUID))

afs_int32
afs_uuid_equal(afsUUID * u1, afsUUID * u2)
{
    return (uuid_memcmp((void *)u1, (void *)u2, sizeof(afsUUID)) == 0);
}

afs_int32
afs_uuid_is_nil(afsUUID * u1)
{
    if (!u1)
	return 1;
    return (uuid_memcmp
	    ((void *)u1, (void *)&afs_uuid_g_nil_uuid, sizeof(afsUUID)) == 0);
}

void
afs_htonuuid(afsUUID * uuidp)
{
    uuidp->time_low = htonl(uuidp->time_low);
    uuidp->time_mid = htons(uuidp->time_mid);
    uuidp->time_hi_and_version = htons(uuidp->time_hi_and_version);
}

void
afs_ntohuuid(afsUUID * uuidp)
{
    uuidp->time_low = ntohl(uuidp->time_low);
    uuidp->time_mid = ntohs(uuidp->time_mid);
    uuidp->time_hi_and_version = ntohs(uuidp->time_hi_and_version);
}

static u_short
true_random(void)
{
    rand_m += 7;
    rand_ia += 1907;
    rand_ib += 73939;
    if (rand_m >= 9973)
	rand_m -= 9871;
    if (rand_ia >= 99991)
	rand_ia -= 89989;
    if (rand_ib >= 224729)
	rand_ib -= 96233;
    rand_irand = (rand_irand * rand_m) + rand_ia + rand_ib;
    return (((rand_irand) >> 16) ^ (rand_irand & 0x3fff));
}


static afs_int32
time_cmp(uuid_time_p_t time1, uuid_time_p_t time2)
{
    if (time1->hi < time2->hi)
	return (-1);
    if (time1->hi > time2->hi)
	return (1);
    if (time1->lo < time2->lo)
	return (-1);
    if (time1->lo > time2->lo)
	return (1);
    return (0);
}

/*
 *    Converts a string UUID to binary representation.
 */

#if !defined(KERNEL) && !defined(UKERNEL)
int
afsUUID_from_string(const char *str, afsUUID * uuid)
{
    unsigned int time_low, time_mid, time_hi_and_version;
    unsigned int clock_seq_hi_and_reserved, clock_seq_low;
    unsigned int node[6];
    int i;

    i = sscanf(str, "%08x-%04x-%04x-%02x-%02x-%02x%02x%02x%02x%02x%02x",
	       &time_low, &time_mid, &time_hi_and_version,
	       &clock_seq_hi_and_reserved, &clock_seq_low, &node[0], &node[1],
	       &node[2], &node[3], &node[4], &node[5]);
    if (i != 11)
	return -1;

    uuid->time_low = time_low;
    uuid->time_mid = time_mid;
    uuid->time_hi_and_version = time_hi_and_version;
    uuid->clock_seq_hi_and_reserved = clock_seq_hi_and_reserved;
    uuid->clock_seq_low = clock_seq_low;

    for (i = 0; i < 6; i++)
	uuid->node[i] = node[i];

    return 0;
}

/*
 *    Converts a UUID from binary representation to a string representation.
 */

int
afsUUID_to_string(const afsUUID * uuid, char *str, size_t strsz)
{
    snprintf(str, strsz, "%08x-%04x-%04x-%02x-%02x-%02x%02x%02x%02x%02x%02x",
	     uuid->time_low, uuid->time_mid, uuid->time_hi_and_version,
	     (unsigned char)uuid->clock_seq_hi_and_reserved,
	     (unsigned char)uuid->clock_seq_low, (unsigned char)uuid->node[0],
	     (unsigned char)uuid->node[1], (unsigned char)uuid->node[2],
	     (unsigned char)uuid->node[3], (unsigned char)uuid->node[4],
	     (unsigned char)uuid->node[5]);

    return 0;
}
#endif

afs_int32
afs_uuid_create(afsUUID * uuid)
{
    uuid_address_t eaddr;
    afs_int32 got_no_time = 0, code;

    if (!uuid_init_done) {
	uuid_time_t t;
	u_short *seedp, seed = 0;
	rand_m = 971;;
	rand_ia = 11113;
	rand_ib = 104322;
	rand_irand = 4181;
	/*
	 * Generating our 'seed' value
	 *
	 * We start with the current time, but, since the resolution of clocks is
	 * system hardware dependent (eg. Ultrix is 10 msec.) and most likely
	 * coarser than our resolution (10 usec) we 'mixup' the bits by xor'ing
	 * all the bits together.  This will have the effect of involving all of
	 * the bits in the determination of the seed value while remaining system
	 * independent.  Then for good measure to ensure a unique seed when there
	 * are multiple processes creating UUID's on a system, we add in the PID.
	 */
	uuid__get_os_time(&t);
	seedp = (u_short *) (&t);
	seed ^= *seedp++;
	seed ^= *seedp++;
	seed ^= *seedp++;
	seed ^= *seedp++;
#if defined(KERNEL) && defined(AFS_XBSD_ENV)
	rand_irand += seed + (afs_uint32) curproc->p_pid;
#else
	rand_irand += seed + (afs_uint32) getpid();
#endif
	uuid__get_os_time(&time_last);
	clock_seq = true_random();
#ifdef AFS_NT40_ENV
	if (afs_winsockInit() < 0) {
	    return WSAGetLastError();
	}
#endif
	uuid_init_done = 1;
    }
    if ((code = uuid_get_address(&eaddr)))
	return code;		/* get our hardware network address */
    do {
	/* get the current time */
	uuid__get_os_time(&time_now);
	/*
	 * check that our clock hasn't gone backwards and handle it
	 *    accordingly with clock_seq
	 * check that we're not generating uuid's faster than we
	 *    can accommodate with our uuid_time_adjust fudge factor
	 */
	if ((code = time_cmp(&time_now, &time_last)) == -1) {
	    /* A clock_seq value of 0 indicates that it hasn't been initialized. */
	    if (clock_seq == 0) {
		clock_seq = true_random();
	    }
	    clock_seq = (clock_seq + 1) & 0x3fff;
	    if (clock_seq == 0)
		clock_seq = clock_seq + 1;
	    uuid_time_adjust = 0;
	} else if (code == 1) {
	    uuid_time_adjust = 0;
	} else {
	    if (uuid_time_adjust == 0x7fff)	/* spin while we wait for the clock to tick */
		got_no_time = 1;
	    else
		uuid_time_adjust++;
	}
    } while (got_no_time);
    time_last.lo = time_now.lo;
    time_last.hi = time_now.hi;
    if (uuid_time_adjust != 0) {
	if (time_now.lo & 0x80000000) {
	    time_now.lo += uuid_time_adjust;
	    if (!(time_now.lo & 0x80000000))
		time_now.hi++;
	} else
	    time_now.lo += uuid_time_adjust;
    }
    uuid->time_low = time_now.lo;
    uuid->time_mid = time_now.hi & 0x0000ffff;
    uuid->time_hi_and_version = (time_now.hi & 0x0fff0000) >> 16;
    uuid->time_hi_and_version |= (1 << 12);
    uuid->clock_seq_low = clock_seq & 0xff;
    uuid->clock_seq_hi_and_reserved = (clock_seq & 0x3f00) >> 8;
    uuid->clock_seq_hi_and_reserved |= 0x80;
    uuid_memcpy((void *)uuid->node, (void *)&eaddr, sizeof(uuid_address_t));
    return 0;
}

u_short
afs_uuid_hash(afsUUID * uuid)
{
    short c0 = 0, c1 = 0, x, y;
    char *next_uuid = (char *)uuid;

    /*
     * For speed lets unroll the following loop:
     *
     *   for (i = 0; i < UUID_K_LENGTH; i++)
     *   {
     *       c0 = c0 + *next_uuid++;
     *       c1 = c1 + c0;
     *   }
     */
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    c0 = c0 + *next_uuid++;
    c1 = c1 + c0;
    /*  Calculate the value for "First octet" of the hash  */
    x = -c1 % 255;
    if (x < 0) {
	x = x + 255;
    }
    /*  Calculate the value for "second octet" of the hash */
    y = (c1 - c0) % 255;
    if (y < 0) {
	y = y + 255;
    }
    return ((y * 256) + x);
}

#ifdef KERNEL

extern struct interfaceAddr afs_cb_interface;

static int
uuid_get_address(uuid_address_p_t addr)
{
    uuid_memcpy((void *)addr->eaddr, (void *)&afs_cb_interface.addr_in[0], 4);
    addr->eaddr[4] = 0xaa;
    addr->eaddr[5] = 0x77;
    return 0;
}

void
uuid__get_os_time(uuid_time_t * os_time)
{
    osi_timeval_t tp;

    osi_GetTime(&tp);
    os_time->hi = tp.tv_sec;
    os_time->lo = tp.tv_usec * 10;
}

#else /* KERNEL */

char hostName1[128] = "localhost";
static int
uuid_get_address(uuid_address_p_t addr)
{
    afs_int32 code;
    afs_uint32 addr1;
    struct hostent *he;

    code = gethostname(hostName1, 64);
    if (code) {
	printf("gethostname() failed\n");
#ifdef AFS_NT40_ENV
	return ENOENT;
#else
	return errno;
#endif
    }
    he = gethostbyname(hostName1);
    if (!he) {
	printf("Can't find address for '%s'\n", hostName1);
#ifdef AFS_NT40_ENV
	return ENOENT;
#else
	return errno;
#endif
    } else {
	uuid_memcpy(&addr1, he->h_addr_list[0], 4);
	addr1 = ntohl(addr1);
	uuid_memcpy(addr->eaddr, &addr1, 4);
	addr->eaddr[4] = 0xaa;
	addr->eaddr[5] = 0x77;
#ifdef  UUID_DEBUG
	printf("uuid_get_address: %02x-%02x-%02x-%02x-%02x-%02x\n",
	       addr->eaddr[0], addr->eaddr[1], addr->eaddr[2], addr->eaddr[3],
	       addr->eaddr[4], addr->eaddr[5]);
#endif
    }
    return 0;
}

void
uuid__get_os_time(uuid_time_t * os_time)
{
    struct timeval tp;

    if (gettimeofday(&tp, NULL)) {
	perror("uuid__get_time");
	exit(-1);
    }
    os_time->hi = tp.tv_sec;
    os_time->lo = tp.tv_usec * 10;
}

#endif /* KERNEL */
