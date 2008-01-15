/*
 *	bit map routines (in-core).
 */
/*
 * Copyright (c) 1995, 1996, 2007 Marcus D. Watts  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * MARCUS D. WATTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#ifdef SUPERGROUPS
#include <errno.h>
#include "map.h"
#ifdef STDLIB_HAS_MALLOC_PROTOS
#include <stdlib.h>
#else
#include "malloc.h"
#endif

#undef PRINT_MAP_ERROR
/* #define MAP_DEBUG /**/
/* #define NEED_READ_WRITE /**/

#define LSHIFT 5
#define MSHIFT 8		/* XXX make this 8 in the production version... */
#define MDATA (1<<MSHIFT)
struct bitmap {
    struct bitmap *m_next;
    int m_page;
    int m_data[MDATA];
};

#define MAP(p)	((struct bitmap*)((int)(p)&~1))
#define NEGMAP(p)	(((int)(p))&1)
#define POSMAP(p)	(!NEGMAP(p))
#define NOT_MAP(mp)	((struct map *) (((int)(mp)) ^ 1))

#define NUMBERTOBIT(n)	((n) & ((1<<LSHIFT)-1))
#define NUMBERTOINDEX(n)	((n>>LSHIFT) & ((1<<MSHIFT)-1))
#define NUMBERTOPAGE(n)	((n>>(LSHIFT+MSHIFT)))
#define TONUMBER(p,x,b)	((b) + ((x)<<LSHIFT) + ((p)<<((LSHIFT+MSHIFT))))

#define Mflag	(debug_mask & (1L<<('Y'-'@')))
#define Aflag	(debug_mask & (1L<<('Z'-'@')))
extern int debug_mask;

int
in_map(struct map *parm, int node)
{
    struct bitmap *map;
    int bit;
    int x, page;
    int result;

#ifdef MAP_DEBUG
    if (Aflag) {
	printf("in_map: is %d in [", node);
	print_map(parm);
	printf(" ]");
    }
#endif
    bit = NUMBERTOBIT(node);
    x = NUMBERTOINDEX(node);
    page = NUMBERTOPAGE(node);
#ifdef MAP_DEBUG
    if (Aflag)
	if (TONUMBER(page, x, bit) != node) {
	    printf
		("bxp mixup: node=%d -> p=%d x=%d b=%d -> %d, %d, %d = %d\n",
		 node, page, x, bit, TONUMBER(page, 0, 0), TONUMBER(0, x, 0),
		 TONUMBER(0, 0, bit), TONUMBER(page, x, bit));
	}
#endif
    bit = 1L << bit;;

    for (map = MAP(parm); map; map = map->m_next)
	if (map->m_page == page)
	    return NEGMAP(parm) ^ (!!(map->m_data[x] & bit));
    result = !POSMAP(parm);
#ifdef MAP_DEBUG
    if (Aflag)
	printf(" -> %s\n", result ? "yes" : "no");
#endif
    return result;
}

void
free_map(struct map *parm)
{
    struct bitmap *map, *next;
#ifdef MAP_DEBUG
    if (Aflag) {
	printf("Freeing map");
	print_map(parm);
	printf("\n");
    }
#endif
    map = MAP(parm);
    while (map) {
	next = map->m_next;
	free(map);
	map = next;
    }
}

struct map *
add_map(struct map *parm, int node)
{
    struct bitmap *map;
    int bit;
    int x, page;

#ifdef MAP_DEBUG
    if (Aflag) {
	printf("add_map: adding %d to [", node);
	print_map(parm);
	printf(" ] ");
    }
#endif
    bit = NUMBERTOBIT(node);
    x = NUMBERTOINDEX(node);
    page = NUMBERTOPAGE(node);

    bit = 1L << bit;;

    for (map = MAP(parm); map; map = map->m_next)
	if (map->m_page == page)
	    break;
    if (!map) {
	map = (struct bitmap *)malloc(sizeof *map);
	if (!map) {
#ifdef PRINT_MAP_ERROR
	    printf("No memory!\n");
#endif
	    free_map((struct map *)map);
	    return 0;
	}
	map->m_page = page;
	memset((char *) map->m_data, 0, sizeof map->m_data);
	if (NEGMAP(parm)) {
	    int i;
	    for (i = 0; i < MDATA; ++i)
		map->m_data[i] = ~0;
	}
	map->m_next = MAP(parm);
	if (POSMAP(parm))
	    parm = (struct map *)map;
	else
	    parm = NOT_MAP(map);
    }
    if (POSMAP(parm))
	map->m_data[x] |= bit;
    else
	map->m_data[x] &= ~bit;
#ifdef MAP_DEBUG
    if (Aflag) {
	printf(" ->");
	print_map(parm);
	printf("\n");
    }
#endif
    return (struct map *)parm;
}

struct bitmap *
simplify_bitmap(struct bitmap *map)
{
    struct bitmap **mpp, *mp2;
    int i;
    for (mpp = &map; (mp2 = *mpp);) {
	for (i = 0; i < MDATA; ++i)
	    if (mp2->m_data[i])
		break;
	if (i == MDATA) {
#ifdef PRINT_MAP_ERROR
	    printf("simplify_bitmap: failed to eliminate zero page %d\n",
		   mp2->m_page);
#endif
	    *mpp = mp2->m_next;
	    free((char *)mp2);
	} else
	    mpp = &mp2->m_next;
    }
    return map;
}

struct bitmap *
or_bitmap(struct bitmap *left, struct bitmap *right)
{
    struct bitmap **rightmp, *lmap, *rmap;
    int i;
    for (lmap = left; lmap; lmap = lmap->m_next) {
	for (rightmp = &right; (rmap = *rightmp); rightmp = &rmap->m_next)
	    if (rmap->m_page == lmap->m_page) {
		for (i = 0; i < MDATA; ++i)
		    lmap->m_data[i] |= rmap->m_data[i];
		*rightmp = rmap->m_next;
		free((char *)rmap);
		break;
	    }
    }
    for (rightmp = &left; *rightmp; rightmp = &(*rightmp)->m_next)
	;
    *rightmp = right;
    return left;
}

struct bitmap *
and_bitmap(struct bitmap *left, struct bitmap *right)
{
    struct bitmap **rightmp, *lmap, *rmap, **leftmp;
    int i;
    int sig;
    for (leftmp = &left; (lmap = *leftmp);) {
	sig = 0;
	for (rightmp = &right; (rmap = *rightmp); rightmp = &rmap->m_next)
	    if (rmap->m_page == lmap->m_page) {
		for (i = 0; i < MDATA; ++i)
		    sig |= (lmap->m_data[i] &= rmap->m_data[i]);
		*rightmp = rmap->m_next;
		free((char *)rmap);
		break;
	    }
	if (rmap && sig) {
	    leftmp = &lmap->m_next;
	} else {
	    *leftmp = lmap->m_next;
	    free((char *)lmap);
	}
    }
    free_map((struct map *)right);
    return simplify_bitmap(left);
}

/* bit set in left, but not in right */
struct bitmap *
bic_bitmap(struct bitmap *left, struct bitmap *right)
{
    struct bitmap **rightmp, *lmap, *rmap, **leftmp;
    int i;
    int sig;
#ifdef MAP_DEBUG
    if (Mflag) {
	printf("bic_bitmap: left=%#lx right=%#lx\n", (long)left, (long)right);
    }
#endif
    for (leftmp = &left; (lmap = *leftmp);) {
	sig = 0;
	for (rightmp = &right; (rmap = *rightmp); rightmp = &rmap->m_next)
	    if (rmap->m_page == lmap->m_page) {
		for (i = 0; i < MDATA; ++i)
		    sig |= (lmap->m_data[i] &= ~rmap->m_data[i]);
		*rightmp = rmap->m_next;
		free((char *)rmap);
		break;
	    }
	if (!rmap || sig) {
	    leftmp = &lmap->m_next;
	} else {
	    *leftmp = lmap->m_next;
	    free((char *)lmap);
	}
    }
    free_map((struct map *)right);
    left = simplify_bitmap(left);
#ifdef MAP_DEBUG
    if (Mflag) {
	printf("bic_bitmap: result=%#lx\n", (long) left);
    }
#endif
    return left;
}

struct map *
and_map(struct map *mp1, struct map *mp2)
{
#ifdef MAP_DEBUG
    if (Mflag) {
	printf("Anding maps");
	print_map(mp1);
	printf(" and");
	print_map(mp2);
    }
#endif
    if (POSMAP(mp1))
	if (POSMAP(mp2))
	    mp1 = (struct map *)and_bitmap((struct bitmap *) mp1,
		(struct bitmap *) mp2);
	else
	    mp1 = (struct map *)bic_bitmap((struct bitmap *) mp1, MAP(mp2));
    else if (POSMAP(mp2))
	mp1 = (struct map *)bic_bitmap((struct bitmap *) mp2, MAP(mp1));
    else
	mp1 = NOT_MAP(or_bitmap(MAP(mp1), MAP(mp2)));
#ifdef MAP_DEBUG
    if (Mflag) {
	printf(" ->");
	print_map(mp1);
	printf("\n");
    }
#endif
    return mp1;
}

struct map *
or_map(struct map *mp1, struct map *mp2)
{
#ifdef MAP_DEBUG
    if (Mflag) {
	printf("Oring maps");
	print_map(mp1);
	printf(" and");
	print_map(mp2);
    }
#endif
    if (POSMAP(mp1))
	if (POSMAP(mp2))
	    mp1 = (struct map *)or_bitmap((struct bitmap *) mp1,
		(struct bitmap *) mp2);
	else
	    mp1 = NOT_MAP(bic_bitmap(MAP(mp2), (struct bitmap *) mp1));
    else if (POSMAP(mp2))
	mp1 = NOT_MAP(bic_bitmap(MAP(mp1), (struct bitmap *) mp2));
    else
	mp1 = NOT_MAP(and_bitmap(MAP(mp1), MAP(mp2)));
#ifdef MAP_DEBUG
    if (Mflag) {
	printf(" ->");
	print_map(mp1);
	printf("\n");
    }
#endif
    return mp1;
}

struct map *
not_map(struct map *map)
{
#ifdef MAP_DEBUG
    if (Mflag) {
	printf("Notting map");
	print_map(map);
	printf("\n");
    }
#endif
    return NOT_MAP(map);
}

struct map *
copy_map(struct map *parm)
{
    struct bitmap *result, **mpp, *map;
#ifdef MAP_DEBUG
    if (Mflag) {
	printf("copymap:");
	print_map(parm);
	printf("\n");
    }
#endif
    map = MAP(parm);
    for (mpp = &result; (*mpp = 0), map; map = map->m_next) {
	*mpp = (struct bitmap *)malloc(sizeof **mpp);
	if (!*mpp) {
#ifdef MAP_DEBUG
	    if (Mflag)
		printf("copy_map: out of memory\n");
#endif
	    free_map((struct map *)result);
	    result = 0;
	    break;
	}
	**mpp = *map;
	mpp = &(*mpp)->m_next;
    }
    if (NEGMAP(parm))
	return NOT_MAP(result);
    else
	return (struct map *)result;
}

int
count_map(struct map *parm)
{
    int nf;
    struct bitmap *map;
    int i, j;

    nf = 0;
    for (map = MAP(parm); map; map = map->m_next) {
	for (i = 0; i < MDATA; ++i) {
	    if (!map->m_data[i])
		;
	    else if (!~map->m_data[i])
		nf += (1 << LSHIFT);
	    else
		for (j = 0; j < (1L << LSHIFT); ++j)
		    if (map->m_data[i] & (1L << j))
			++nf;
	}
    }
    if (NEGMAP(parm))
	nf = ~nf;		/* note 1's complement */
#ifdef MAP_DEBUG
    if (Mflag) {
	printf("countmap:");
	print_map(parm);
	printf(" -> %d\n", nf);
    }
#endif
    return nf;
}

int
next_map(struct map *parm, int node)
{
    struct bitmap *map, *lowest;
    int bit, mask;
    int x, page;
    int best;
    int i;
    int bn;

#ifdef MAP_DEBUG
    if (Aflag) {
	printf("next_map: selecting next after %d from", node);
	print_map(parm);
    }
#endif
    if (NEGMAP(parm)) {
#ifdef MAP_DEBUG
	if (Aflag)
	    printf("next_map failed; negative map\n");
#endif
	return -1;
    }

    ++node;
    bit = NUMBERTOBIT(node);
    x = NUMBERTOINDEX(node);
    page = NUMBERTOPAGE(node);
    bit = 1L << bit;
    best = -1;
    lowest = 0;
    for (map = MAP(parm); map; map = map->m_next)
	if (map->m_page >= page && (!lowest || lowest->m_page > map->m_page)) {
	    i = 0;
	    mask = ~0;
	    if (page == map->m_page)
		i = x, mask = -bit;
	    for (; i < MDATA; ++i, mask = ~0)
		if (map->m_data[i] & mask)
		    break;
	    if (i < MDATA) {
		for (bn = 0; bn < (1 << LSHIFT); ++bn)
		    if (map->m_data[i] & mask & (1L << bn)) {
			break;
		    }
#ifdef MAP_DEBUG
		if (Aflag) {
		    if (bn == (1 << LSHIFT)) {
			printf
			    ("next_map: botch; pageno %d index %d data %#x mask %#x x,bit %d,%#x\n",
			     map->m_page, i, map->m_data[i], mask, x, bit);
			continue;
		    }
		}
#endif
		best = bn + ((i + ((map->m_page) << MSHIFT)
			     ) << LSHIFT);
		lowest = map;
	    }
	}
#ifdef MAP_DEBUG
    if (Aflag) {
	printf(" -> %d\n", best);
	if (best >= 0 && !in_map(parm, best)) {
	    printf("next_map: botch; %d not in map\n", best);
	    return -1;
	}
    }
#endif
    return best;
}

int
first_map(struct map *parm)
{
    return next_map(parm, -9999);
}

int
prev_map(struct map *parm, int node)
{
    struct bitmap *map, *lowest;
    int bit, mask;
    int x, page;
    int best;
    int i;
    int bn;

#ifdef MAP_DEBUG
    if (Aflag) {
	printf("prev_map: selecting previous before %d from", node);
	print_map(parm);
    }
#endif
    if (NEGMAP(parm)) {
#ifdef MAP_DEBUG
	if (Aflag)
	    printf("prev_map failed; negative map\n");
#endif
	return -1;
    }

    if (node < 0)
	node = ((unsigned int)~0) >> 1;

    --node;
    bit = NUMBERTOBIT(node);
    x = NUMBERTOINDEX(node);
    page = NUMBERTOPAGE(node);
    bit = 1L << bit;
    best = -1;
    lowest = 0;
    for (map = MAP(parm); map; map = map->m_next)
	if (map->m_page <= page && (!lowest || lowest->m_page < map->m_page)) {
	    i = MDATA - 1;
	    mask = ~0;
	    if (page == map->m_page)
		i = x, mask = (bit << 1) - 1;
	    for (; i >= 0; --i, mask = ~0)
		if (map->m_data[i] & mask)
		    break;
	    if (i >= 0) {
		for (bn = (1 << LSHIFT) - 1; bn >= 0; --bn)
		    if (map->m_data[i] & mask & (1L << bn)) {
			break;
		    }
#ifdef MAP_DEBUG
		if (Aflag) {
		    if (bn < 0) {
			printf
			    ("prev_map: botch; pageno %d index %d data %#x mask %#x x,bit %d,%#x\n",
			     map->m_page, i, map->m_data[i], mask, x, bit);
			continue;
		    }
		}
#endif
		best = bn + ((i + ((map->m_page) << MSHIFT)
			     ) << LSHIFT);
		lowest = map;
	    }
	}
#ifdef MAP_DEBUG
    if (Aflag) {
	printf(" -> %d\n", best);
	if (best >= 0 && !in_map(parm, best)) {
	    printf("prev_map: botch; %d not in map\n", best);
	    return -1;
	}
    }
#endif
    return best;
}

int
last_map(struct map *parm)
{
    return prev_map(parm, 0x7fffffff);
}

struct map *
negative_map(struct map *map)
{
    return (struct map *)NEGMAP(map);
}

struct map *
bic_map(struct map *mp1, struct map *mp2)
{
#ifdef MAP_DEBUG
    if (Mflag) {
	printf("Bic maps");
	print_map(mp1);
	printf(" minus");
	print_map(mp2);
    }
#endif
    mp1 = and_map(mp1, NOT_MAP(mp2));
#ifdef MAP_DEBUG
    if (Mflag) {
	printf(" ->");
	print_map(mp1);
	printf("\n");
    }
#endif
    return mp1;
}

#ifdef PRINT_MAP_ERROR
void 
print_map(struct map *parm)
{
    struct bitmap *map;
    int i, j;
    int bitno, firstbitno, lastbitno;
    if (NEGMAP(parm)) {
	printf(" NOT");
    }
    map = MAP(parm);
    if (!map)
	printf(" nil(%lx)", (long)parm);
    else
	printf(" %lx", (long)parm);
    lastbitno = -100;
    firstbitno = -100;
    for (; map; map = map->m_next)
	for (i = 0; i < MDATA; ++i)
	    if (map->m_data[i])
		for (j = 0; j < (1 << LSHIFT); ++j) {
		    bitno =
			j + (i << LSHIFT) +
			((map->m_page) << (LSHIFT + MSHIFT));
		    if (map->m_data[i] & (1 << j)) {
			if (bitno == lastbitno + 1) {
			    ++lastbitno;
			    continue;
			}
			if (bitno != (lastbitno + 1)) {
			    if (firstbitno >= 0 && firstbitno != lastbitno)
				printf("-%d", lastbitno);
			    firstbitno = -100;
			}
			printf(" %d", bitno);
			firstbitno = lastbitno = bitno;
		    } else {
			if (firstbitno >= 0 && firstbitno != lastbitno)
			    printf("-%d", lastbitno);
			firstbitno = -100;
		    }
		}
    if (firstbitno >= 0 && firstbitno != lastbitno)
	printf("-%d", lastbitno);
}
#endif

#ifdef NEED_READ_WRITE
struct map *
read_map(int (*f) (void *), char *arg)
{
    struct bitmap *map, *result, **mp;
    int page;
    int bitno, lastno;
    int data;

/* count, then startbitno, then bits. */
    lastno = ((*f) (arg));
    if (lastno == -1)
	return 0;
    if (lastno & ((1 << LSHIFT) - 1)) {
#ifdef PRINT_MAP_ERROR
	printf("read_map: bad count\n");
#endif
	return 0;
    }
    bitno = ((*f) (arg));
    if (bitno & ((1 << LSHIFT) - 1)) {
#ifdef PRINT_MAP_ERROR
	printf("read_map: bad start\n");
#endif
	return 0;
    }
    lastno += bitno;
    map = result = 0;
    for (; bitno < lastno; bitno += (1 << LSHIFT)) {
	data = (*f) (arg);
	if (!data)
	    continue;
	page = NUMBERTOPAGE(bitno);
	if (!map || map->m_page != page)
	    for (mp = &result; (map = *mp); mp = &map->m_next)
		if (map->m_page == page)
		    break;
	if (!map) {
	    map = (struct bitmap *)malloc(sizeof *map);
	    if (!map) {
#ifdef PRINT_MAP_ERROR
		printf("No memory!\n");
#endif
		if (result)
		    free_map((struct map *)result);
		return 0;
	    }
	    memset((char *) map->m_data, 0, sizeof map->m_data);
	    map->m_page = page;
	    map->m_next = 0;
	    *mp = map;
	}
	map->m_data[NUMBERTOINDEX(bitno)] = data;
    }
    return (struct map *)result;
}

int 
write_map(struct map *parm, int (*f) (void *, int), char *arg)
{
    struct bitmap *map;
    int page;
    int bitno, lastno, thisno, prevno;
    int i, j;

    bitno = first_map(parm);
    bitno &= ~((1 << LSHIFT) - 1);

    lastno = last_map(parm);
    lastno -= bitno;
    lastno += ((1 << LSHIFT));
    lastno &= ~((1 << LSHIFT) - 1);
/* count, then startbitno, then bits. */
    if ((*f) (arg, lastno) == -1)
	return -1;
    /* word: number of bits */
    if ((*f) (arg, bitno) == -1)
	return -1;
    lastno += bitno;
    prevno = bitno;
    for (bitno = first_map(parm); bitno >= 0; bitno = next_map(parm, bitno)) {
	page = NUMBERTOPAGE(bitno);
	for (map = MAP(parm); map; map = map->m_next)
	    if (map->m_page == page)
		break;
	if (!map) {
#ifdef PRINT_MAP_ERROR
	    printf("write_map: botch #1: can't find page %d\n", page);
#endif
	    continue;
	}
	for (i = 0; i < MDATA; ++i) {
	    if (!map->m_data[i])
		continue;
	    thisno = TONUMBER(page, i, 0);
	    for (j = thisno - prevno; j > 0; j -= (1 << LSHIFT))
		if ((*f) (arg, 0) == -1)
		    return -1;
	    prevno = thisno;
	    for (;;) {
		if ((*f) (arg, map->m_data[i]) == -1)
		    return -1;
		prevno += (1 << LSHIFT);
		++i;
		if (i >= MDATA || !map->m_data[i])
		    break;
	    }
	    --i;
	}
	bitno = TONUMBER(page, i, 0) - 1;
    }
#ifdef PRINT_MAP_ERROR
    if (prevno != lastno)
	printf("write_map: botch #2: bitno=%d prevno=%d lastno=%d\n", bitno,
	       prevno, lastno);
#endif
    return 0;
}
#endif

#endif
