/* debug.h */

/*
 * (C) Copyright 1990 Transarc Corporation
 * All Rights Reserved
 */

#if !defined(TRANSARC_DEBUG_H)
#define TRANSARC_DEBUG_H

/*
 * Define debugging levels, from 1 being lowest priority and 7 being
 * highest priority.  DEBUG_LEVEL_0 and DEBUG_FORCE_PRINT mean to
 * print the message regardless of the current setting.
 */
#define DEBUG_LEVEL_0		0
#define DEBUG_LEVEL_1		01
#define DEBUG_LEVEL_2		03
#define DEBUG_LEVEL_3		07
#define DEBUG_LEVEL_4		017
#define DEBUG_LEVEL_5		037
#define DEBUG_LEVEL_6		077
#define DEBUG_LEVEL_7		0177

#define DEBUG_FORCE_PRINT	DEBUG_LEVEL_0

#if defined(AFS_DEBUG)

/*
 * Assert macro
 *
 * In kernel, panic.
 * In user space call abort();
 */

#if defined(KERNEL)
#define assert(x) \
if(!(x)) { printf("assertion failed: line %d, file %s\n",\
		  __LINE__,__FILE__); osi_Panic("assert"); }

/*
 * Debug modules
 */
#define	CM_DEBUG	0	/* Cache Manager */
#define	EX_DEBUG	1	/* Protocol Exporter */
#define	HS_DEBUG	2	/* Host Module */
#define	VL_DEBUG	3	/* Volume Module */

#define	AG_DEBUG	4	/* Aggregate Module */
#define	VR_DEBUG	5	/* Volume Registry Module */
#define	RX_DEBUG	6	/* RPC/Rx Module */
#define	XVFS_DEBUG	7	/* Xvnode Module */

#define	NFSTR_DEBUG	8	/* NFS/AFS Translator Module */
#define XCRED_DEBUG	9	/* Extended Credential Module */
#define FP_DEBUG	10	/* Free Pool Module */
#define ACL_DEBUG	11	/* ACL Module */
#define FSHS_DEBUG	12	/* File server host module */

#define MAXMODS_DEBUG	20

#ifdef	AFSDEBUG_DECLARE
/* 
 *  Should get here only once per kernel instance!
 */
char afsdebug[MAXMODS_DEBUG] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*
 * AFSLOG
 *
 * Compare the given debugging level against the setting of the particular
 * index into the afsdebug char array.  If there are bits in common, or else
 * if if the level is DEBUG_FORCE_PRINT, then produce debugging output with
 * the rest of the arguments provided.
 *
 * NOTE: This should become a macro!
 */
AFSLOG(index,level,a,b,c,d,e,f,g,h,i,j,k,l,m,n)
    char index,level,*a,*b,*c,*d,*e,*f,*g,*h,*i,*j,*k,*l,*m,*n;
{
    if ((afsdebug[index] & level) || !level) 
	osi_dp(a,b,c,d,e,f,g,h,i,j,k,l,m,n);
}
#else
extern char afsdebug[20];
#endif /* AFSDEBUG_DECLARE */

#else /* KERNEL */
#define assert(x) \
if(!(x)) { fprintf(stderr, "assertion failed: line %d, file %s\n",\
		   __LINE__,__FILE__); fflush(stderr); abort(); }
#endif /* KERNEL */

#else /* AFS_DEBUG */

#define assert(x)

#endif /* AFS_DEBUG */

/*
 * Debugging macro package.  The actual variables should be declared in
 * debug.c
 */

#if defined(AFS_DEBUG)
#if defined(lint)
#define dprintf(flag, str) printf str
#define dlprintf(flag, level, str) printf str
#define dmprintf(flag, bit, str) printf str
#else /* lint */
#define dprintf(flag, str) \
      (void)((flag) ? \
	     ( osi_dp str, osi_dp("\t%s, %d\n", __FILE__, __LINE__)):0)
#define dlprintf(flag, level, str) dprintf(((flag) >= (level)), str)
#define dmprintf(flag, bit, str) dprintf(((flag)&(1<<((bit)-1))), str)

#endif /* lint */
	  
#else /* AFS_DEBUG */

#define dprintf(flag, str)
#define dlprintf(flag, level,str)
#define dmprintf(flag, bit, str)

#endif /* AFS_DEBUG */

#endif /* TRANSARC_DEBUG_H */

