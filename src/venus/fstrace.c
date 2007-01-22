/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * All Rights Reserved
 */
#include <osi/osi.h>
#include <osi/osi_trace.h>
#include <trace/consumer/consumer.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <sys/types.h>
#if !defined(AFS_SUN3_ENV) && !defined(sys_vax_ul43) 
#include <time.h>
/*#ifdef	AFS_AIX_ENV*/
#include <sys/time.h>
/*#endif*/
#include <errno.h>
#undef abs
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include <afs/stds.h>
#include <afs/cmd.h>
#include <afs/afs_args.h>
#include <afs/afsutil.h>


#define BUFFER_MULTIPLIER     1024

/* make it big enough to snapshot everything at once, since
 * decoding takes so long.
 */
#define IBSIZE		100000	/* default size */


/* display a single record.
 * alp points at the first word in the array to be interpreted
 * rsize gives the # of words in the array
 */
#if defined(AFS_SGI61_ENV) && !defined(AFS_SGI62_ENV)
#define uint64_t long long
#endif
static
DisplayRecord(outFilep, alp, rsize)
     FILE *outFilep;
     register afs_int32 *alp;
     afs_int32 rsize;
{
    char msgBuffer[1024];
#if defined(AFS_SGI61_ENV) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
    char outMsgBuffer[1024];
    uint64_t tempParam;
    uint64_t printfParms[ICL_MAXEXPANSION * /* max parms */ 4];
    char *printfStrings[ICL_MAXEXPANSION * /* max parms */ 4];
#else /* AFS_SGI61_ENV */
    long printfParms[ICL_MAXEXPANSION * /* max parms */ 4];
#endif /* AFS_SGI61_ENV */
    int printfTypes[ICL_MAXEXPANSION * 4];
    int i;
    afs_int32 done = 0;
    afs_int32 temp;
    int j;
    int type;
    int pix;			/* index in alp */
    int pfpix;			/* index in printfParms */
    int pftix;			/* index in printfTypes */
    int status;
    int printed;		/* did we print the string yet? */
    afs_int32 *tlp;
    time_t tmv;

    /* decode parameters */
    temp = alp[0];		/* type encoded in low-order 24 bits, t0 high */
    pix = 4;
    pfpix = 0;
    pftix = 0;
    /* init things */

    for (i = 0; i < 4 * ICL_MAXEXPANSION; i++)
	printfParms[i] = 0;
    /* decode each parameter, getting addrs for afs_hyper_t and strings */
    for (i = 0; !done && i < 4; i++) {
	type = (temp >> (18 - i * 6)) & 0x3f;
	switch (type) {
	case ICL_TYPE_NONE:
	    done = 1;
	    break;
	case ICL_TYPE_LONG:
	case ICL_TYPE_POINTER:
	    printfTypes[pftix++] = 0;
#if defined(AFS_SGI61_ENV) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
	    printfParms[pfpix] = alp[pix];
	    printfParms[pfpix] &= 0xffffffff;
	    if (afs_64bit_kernel) {
		printfParms[pfpix] <<= 32;
		printfParms[pfpix] |= alp[pix + 1];
	    }
#elif defined(AFS_OSF_ENV)
	    printfParms[pfpix] = alp[pix + 1];
	    printfParms[pfpix] |= (alp[pix] <<= 32);
#else /* !AFS_OSF_ENV && !AFS_SGI61_ENV */
	    printfParms[pfpix] = alp[pix];
#endif
	    pfpix++;
	    break;
	case ICL_TYPE_INT32:
	    printfTypes[pftix++] = 0;
	    printfParms[pfpix++] = alp[pix];
	    break;
	case ICL_TYPE_HYPER:
	case ICL_TYPE_INT64:
	    printfTypes[pftix++] = 0;
	    printfParms[pfpix++] = alp[pix];
	    printfTypes[pftix++] = 0;
	    printfParms[pfpix++] = alp[pix + 1];
	    break;
	case ICL_TYPE_FID:
	    printfTypes[pftix++] = 0;
	    printfParms[pfpix++] = alp[pix];
	    printfTypes[pftix++] = 0;
	    printfParms[pfpix++] = alp[pix + 1];
	    printfTypes[pftix++] = 0;
	    printfParms[pfpix++] = alp[pix + 2];
	    printfTypes[pftix++] = 0;
	    printfParms[pfpix++] = alp[pix + 3];
	    break;
	case ICL_TYPE_STRING:
	    printfTypes[pftix++] = 1;
#ifdef AFS_SGI64_ENV
	    printfStrings[pfpix++] = (char *)&alp[pix];
#else /* AFS_SGI64_ENV */
#if defined(AFS_SGI61_ENV) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
	    printfStrings[pfpix++] = (char *)&alp[pix];
#else /* AFS_SGI61_ENV */
	    printfParms[pfpix++] = (long)&alp[pix];
#endif /* AFS_SGI61_ENV */
#endif /* AFS_SGI64_ENV */
	    break;
	case ICL_TYPE_UNIXDATE:
	    tmv = alp[pix];
	    printfParms[pfpix++] = (long)ctime(&tmv);
	    break;
	default:
	    printf("DisplayRecord: Bad type %d in decode switch.\n", type);
	    done = 1;
	    break;
	}
	if (done)
	    break;

	pix += icl_GetSize(type, (char *)&alp[pix]);
    }

    /* next, try to decode the opcode into a printf string */
    dce1_error_inq_text(alp[1], msgBuffer, &status);

    /* if we got a string back, and it is compatible with the
     * parms we've got, then print it.
     */
    printed = 0;
    if (status == 0) {
#if defined(AFS_SGI61_ENV) || (defined(AFS_AIX51_ENV) && defined(AFS_64BIT_KERNEL))
	if (CheckTypes(msgBuffer, printfTypes, pftix, outMsgBuffer)) {
	    /* we have a string to use, but it ends "(dfs / zcm)",
	     * so we remove the extra gunk.
	     */
	    j = strlen(outMsgBuffer);
	    if (j > 12) {
		outMsgBuffer[j - 11] = 0;
		j -= 11;
	    }
	    pfpix = 0;
	    fprintf(outFilep, "time %d.%06d, pid %u: ", alp[3] / 1000000,
		    alp[3] % 1000000, alp[2]);
	    for (i = 0; i < j; i++) {
		if ((int)outMsgBuffer[i] > 5)
		    fputc(outMsgBuffer[i], outFilep);
		else {
		    switch (outMsgBuffer[i]) {
		    case 0:	/* done */
			break;
		    case 1:	/* string */
			fprintf(outFilep, "%s", printfStrings[pfpix++]);
			break;
		    case 2:	/* signed integer */
			fprintf(outFilep, "%lld", printfParms[pfpix++]);
			break;
		    case 3:	/* unsigned integer */
			fprintf(outFilep, "%llu", printfParms[pfpix++]);
			break;
		    case 4:	/* octal integer */
			fprintf(outFilep, "%llo", printfParms[pfpix++]);
			break;
		    case 5:	/* hex integer */
			fprintf(outFilep, "%llx", printfParms[pfpix++]);
			break;
		    default:
			fprintf(outFilep,
				"fstrace: Bad char %d in outMsgBuffer for parm %d\n",
				outMsgBuffer[i], pfpix);
			fprintf(outFilep, "fstrace: msgBuffer='%s'\n",
				msgBuffer);
			break;
		    }
		}
	    }
	    fprintf(outFilep, "\n");
	    printed = 1;
	}
#else /* AFS_SGI61_ENV */
	if (CheckTypes(msgBuffer, printfTypes, pftix)) {
	    /* we have a string to use, but it ends "(dfs / zcm)",
	     * so we remove the extra gunk.
	     */
	    j = strlen(msgBuffer);
	    if (j > 12)
		msgBuffer[j - 11] = 0;
	    fprintf(outFilep, "time %d.%06d, pid %u: ", alp[3] / 1000000,
		    alp[3] % 1000000, alp[2]);
	    fprintf(outFilep, msgBuffer, printfParms[0], printfParms[1],
		    printfParms[2], printfParms[3], printfParms[4],
		    printfParms[5], printfParms[6], printfParms[7],
		    printfParms[8], printfParms[9], printfParms[10],
		    printfParms[11], printfParms[12], printfParms[13],
		    printfParms[14], printfParms[15]);
	    fprintf(outFilep, "\n");
	    printed = 1;
	}
#endif /* AFS_SGI61_ENV */
	else {
	    fprintf(outFilep, "Type mismatch, using raw print.\n");
	    fprintf(outFilep, "%s", msgBuffer);
	}
    }
    if (!printed) {
	if (alp[1] == ICL_INFO_TIMESTAMP) {
	    tmv = alp[4];
	    fprintf(outFilep, "time %d.%06d, pid %u: %s\n", alp[3] / 1000000,
		    alp[3] % 1000000, alp[2], ctime(&tmv));
	} else {
	    fprintf(outFilep, "raw op %d, time %d.%06d, pid %u\n", alp[1],
		    alp[3] / 1000000, alp[3] % 1000000, alp[2]);
	    /* now decode each parameter and print it */
	    pix = 4;
	    done = 0;
	    for (i = 0; !done && i < 4; i++) {
		type = (temp >> (18 - i * 6)) & 0x3f;
		switch (type) {
		case ICL_TYPE_NONE:
		    done = 1;
		    break;
		case ICL_TYPE_INT32:
		    fprintf(outFilep, "p%d:%d ", i, alp[pix]);
		    break;
		case ICL_TYPE_LONG:
#ifdef AFS_SGI61_ENV
		    tempParam = alp[pix];
		    tempParam <<= 32;
		    tempParam |= alp[pix + 1];
		    fprintf(outFilep, "p%d:%lld ", i, tempParam);
#else /* AFS_SGI61_ENV */
		    fprintf(outFilep, "p%d:%d ", i, alp[pix]);
#endif /* AFS_SGI61_ENV */
		    break;
		case ICL_TYPE_POINTER:
#ifdef AFS_SGI61_ENV
		    tempParam = alp[pix];
		    tempParam <<= 32;
		    tempParam |= alp[pix + 1];
		    fprintf(outFilep, "p%d:0x%llx ", i, tempParam);
#else /* AFS_SGI61_ENV */
		    fprintf(outFilep, "p%d:0x%x ", i, alp[pix]);
#endif /* AFS_SGI61_ENV */
		    break;
		case ICL_TYPE_HYPER:
		case ICL_TYPE_INT64:
		    fprintf(outFilep, "p%d:%x.%x ", i, alp[pix],
			    alp[pix + 1]);
		    break;
		case ICL_TYPE_FID:
		    fprintf(outFilep, "p%d:%d.%d.%d.%d ", i, alp[pix],
			    alp[pix + 1], alp[pix + 2], alp[pix + 3]);
		    break;
		case ICL_TYPE_STRING:
		    fprintf(outFilep, "p%d:%s ", i, (char *)&alp[pix]);
		    break;
		case ICL_TYPE_UNIXDATE:
		    tmv = alp[pix];
		    fprintf(outFilep, "p%d:%s ", i,
			    ctime(&tmv));
		    break;
		default:
		    printf
			("DisplayRecord: Bad type %d in raw print switch.\n",
			 type);
		    done = 1;
		    break;
		}
		if (done)
		    break;

		pix += icl_GetSize(type, (char *)&alp[pix]);
	    }
	}
	fprintf(outFilep, "\n");	/* done with line */
    }
}



#include <locale.h>
#ifdef	AFS_OSF_ENV
#include <limits.h>
#endif
#include <nl_types.h>

#if	defined(AFS_OSF_ENV) && !defined(AFS_OSF20_ENV)
#include <fcntl.h>
static nl_catd catopen1();
nl_catd NLcatopen();
static nl_catd _do1_open();
static nl_catd cat_already_open();
static int make_sets();
static FILE *open1catfile();
static void add_open_cat();
static void cat_hard_close();
extern char *strchr();

static int catpid[NL_MAXOPEN];
static CATD *catsopen[NL_MAXOPEN];
#define PATH_FORMAT     "/usr/lib/nls/msg/%L/%N:/etc/nls/msg/%L/%N"
#define DEFAULT_LANG    "C"
#define TOO_MANY_HOLES(num_holes, num_non_holes) \
    (((num_holes) > 100) && ((num_holes) > (num_non_holes)))

/*----  n: the number of bytes to be malloc'ed  ----*/
static char *
rmalloc(size_t n)
{
    char *t;

    t = (char *)malloc(n);
    if (!t)
	printf("Failed to get mem\n");
    return (t);
}

/*---- char *cat:  the name of the cat to be opened ----*/
/*---- int dummy:  dummy variable  ----*/
nl_catd
catopen1(char * cat, int dummy)
{
    int errno_save;
    nl_catd _do_open();		    /*---- routine that actually opens 
					   the catalog ---- */
    CATD *catd;

    errno_save = errno;

/*
	if (catd = cat_already_open(cat)) {
		catd->_count = catd->_count + 1;
		return(catd);
	}
*/
    catd = (CATD *) rmalloc(sizeof(CATD));
    if (catd == NULL)
	return (CATD_ERR);
    catd->_name = (char *)rmalloc(strlen(cat) + 1);
    if (catd->_name == NULL)
	return (CATD_ERR);
    strcpy(catd->_name, cat);
    catd->_fd = FALSE;
    catd->_magic = CAT_MAGIC;
    catd->_mem = FALSE;
#ifndef	AFS_OSF20_ENV
    catd->_pid = getpid();
#endif
    catd->_count = 1;
    if (_do1_open(catd) != CATD_ERR)
	return (catd);
    else {
	free(catd->_name);
	free(catd);
	return (CATD_ERR);
    }
}



/*---- catd is pointer to the partially set up cat descriptor ----*/
nl_catd
_do1_open(nl_catd catd)
{
    int make_sets();		/*---- routine to unpack the sets into 
						fast acccess mode ----*/
    void add_open_cat();	/*---- routine to keep a list of 
                                               opened cats ----*/
    /*long */ int magic;
    int i;			/*---- Misc counter(s) used for loop */
    struct _catset cs;
    int errno_save;
    int num_holes;

    errno_save = errno;

    catd->_fd = open1catfile(catd->_name);
    if (!catd->_fd) {
	return (CATD_ERR);
    }
    fread((void *)&magic, (size_t) 4, (size_t) 1, catd->_fd);
    if (magic != CAT_MAGIC) {
	printf("Magic was %x instead of %x -> %x\n", magic, CAT_MAGIC,
	       CATD_ERR);
/*
		fclose(catd->_fd);
		catd->_fd = NULL;
		return( CATD_ERR );
*/
    }
/*	if ((catd->_mem = shmat((int)fileno(catd->_fd), NULL, SHM_MAP | SHM_RDONLY))
           == (char * )ERR ) {   */

    if (1) {			/* disable the shmat, share memory segemnt */

/*______________________________________________________________________
	If the file can not be mapped then simulate mapping for the index
	table so that make_sets cat set things up. (rmalloc an area big
 	enough for the index table and read the whole thing in)
  ______________________________________________________________________*/

	/* reset the file pointer to the beginning of catalog */
	fseek(catd->_fd, (long)0, 0);

	/* malloc the header, if fails return error */
	catd->_hd = (struct _header *)rmalloc(sizeof(struct _header));
	if (catd->_hd == NULL)
	    return (CATD_ERR);

	/* read in the whole header */
	fread((void *)catd->_hd, (size_t) sizeof(struct _header), (size_t) 1,
	      catd->_fd);

	/* cs is a dummpy to hold a set temperorily. The purpose of */
	/* this for loop is to fread the whole catalog so that the  */
	/* file pointer will be moved to the end of the catalog.    */
	for (i = 0; i < catd->_hd->_n_sets; i++) {
	    fread((void *)&cs, (size_t) 4, (size_t) 1, catd->_fd);
	    fseek(catd->_fd, (long)(cs._n_msgs * sizeof(struct _msgptr)), 1);
	}

	/* after the for loop, ftell returns the byte offset of the */
	/* end of the catalog relative to the begining of the file. */
	/* i.e. i contains the byte offset of the whole catalog.    */
	i = ftell(catd->_fd);

	/* malloc _mem as a temp pointer to hold the entire catalog. */
	catd->_mem = (char *)rmalloc(i);
	if (catd->_mem == NULL)
	    return (CATD_ERR);

	/* reset the file pointer to the begining. */
	fseek(catd->_fd, (long)0, 0);

	/* read in the whole catalog into _mem */
	fread((void *)catd->_mem, (size_t) i, (size_t) 1, catd->_fd);

	/*
	 * If there aren't many holes in the set numbers,
	 * fully expand the compacted set array from the
	 * catalog.  Then in catgets(), we'll be able to use
	 * the set number to index directly into the expanded
	 * array.
	 *
	 * If there are a lot of holes, leave the set array
	 * compacted.  In catgets(), we'll search through it
	 * for the requested set.
	 */

	num_holes = catd->_hd->_setmax - catd->_hd->_n_sets;
	if (!TOO_MANY_HOLES(num_holes, catd->_hd->_n_sets)) {
	    catd->_sets_expanded = TRUE;
	    catd->_n_sets = catd->_hd->_setmax;
	} else {
	    catd->_sets_expanded = FALSE;
	    catd->_n_sets = catd->_hd->_n_sets - 1;
	}

	/* malloc one extra set more than the max. set index */
	catd->_set =
	    (struct _catset *)rmalloc((catd->_n_sets + 1) *
				      sizeof(struct _catset));
	if (catd->_set == NULL)
	    return (CATD_ERR);

	/* save the max. set number in catd->_setmax */
	catd->_setmax = catd->_hd->_setmax;
	/* call make_set to malloc memory for every message */
	if (make_sets(catd) == -1)
	    return (CATD_ERR);
	free(catd->_mem);
	catd->_mem = FALSE;
	add_open_cat(catd);
	return (catd);
    } else {

/*______________________________________________________________________
	Normal mapping has occurred, set a few things up and call make_sets
  ______________________________________________________________________*/

	catd->_hd = (struct _header *)(catd->_mem);
	catd->_setmax = catd->_hd->_setmax;
	catd->_set =
	    (struct _catset *)rmalloc((catd->_hd->_setmax + 1) *
				      sizeof(struct _catset));
	if (catd->_set == NULL)
	    return (CATD_ERR);
	if (make_sets(catd) == -1)
	    return (CATD_ERR);
	add_open_cat(catd);
	return (catd);
    }
}


/*---- catd to be added to the list of catalogs ----*/
static void
add_open_cat(nl_catd catd)
{
    int i = 0;		/*---- Misc counter(s) used for loops ----*/
    while (i < NL_MAXOPEN && catsopen[i]) {
	if (!strcmp(catd->_name, catsopen[i]->_name)
#ifndef	AFS_OSF20_ENV
	    && getpid() == catsopen[i]->_pid)
#else
	    )
#endif
	    return;		/*---- The catalog is already here ----*/
	i++;
    }

    if (i < NL_MAXOPEN) {
	catsopen[i] = catd;
	catpid[i] = getpid();
    }
}


/*
 * 
 * NAME: make_sets
 *
 * FUNCTION: Expands the compacted version of the catalog index table into
 *	the fast access memory version.
 *
 * EXECUTION ENVIRONMENT:
 *
 *	Make_set executes under a process.	
 *
 * RETURNS: int
 */


static int
make_sets(nl_catd catd)
{
    struct _catset *cset;
    char *base = catd->_mem;
    int n_sets = catd->_hd->_n_sets;
    int i;		/*---- Misc counter(s) used for loops ----*/
    int j;		/*---- Misc counter(s) used for loops ----*/
    int msgmax;		/*---- The maximum number of _messages in a set ----*/
    char *cmpct_set_ptr;	/*---- pointer into the index table ----*/
    struct _catset cs;		/*---- used to look at the sets in the table -*/
    int num_holes;

    cmpct_set_ptr = base + sizeof(struct _header);

    for (i = 0; i < n_sets; i++) {
	/* loop through each compacted set */

	cs = *(struct _catset *)cmpct_set_ptr;
	/* set the _catset ptr to the base of the current 
	 * compacted set.        */

	cs._mp =
	    (struct _msgptr *)(cmpct_set_ptr + 2 * sizeof(unsigned short));
	/* set the ms array ptr to the base of
	 * compacted array of _msgptr's     */

	cset =
	    (catd->_sets_expanded) ? &catd->_set[cs._setno] : &catd->_set[i];

	/*
	 * If there aren't many holes in the message numbers,
	 * fully expand the compacted message array from the
	 * catalog.  Then in catgets(), we'll be able to use
	 * the message number to index directly into the
	 * expanded array.
	 *
	 * If there are many holes, leave the message array
	 * compacted.  In catgets(), we'll search through it
	 * for the requested message.
	 */

	msgmax = cs._mp[cs._n_msgs - 1]._msgno;
	num_holes = msgmax - cs._n_msgs;
	if (!TOO_MANY_HOLES(num_holes, cs._n_msgs)) {
	    cset->_msgs_expanded = TRUE;
	    cset->_n_msgs = msgmax;
	} else {
	    cset->_msgs_expanded = FALSE;
	    cset->_n_msgs = cs._n_msgs - 1;
	}

	cset->_mp =
	    (struct _msgptr *)rmalloc((1 + cset->_n_msgs) *
				      sizeof(struct _msgptr));
	if (cset->_mp == NULL)
	    return (-1);

	cset->_msgtxt =
	    (char **)rmalloc((1 + cset->_n_msgs) * sizeof(char *));
	if (cset->_msgtxt == NULL)
	    return (-1);

	if (cset->_msgs_expanded) {
	    for (j = 0; j < cs._n_msgs; j++) {
		cset->_mp[cs._mp[j]._msgno] = cs._mp[j];
	    }
	} else {
	    for (j = 0; j < cs._n_msgs; j++) {
		cset->_mp[j] = cs._mp[j];
	    }
	}

	cset->_setno = cs._setno;
	/* Superfluous but should have the correct data. Increment 
	 * the base of the set pointer.          */

	cmpct_set_ptr +=
	    2 * sizeof(unsigned short) + cs._n_msgs * sizeof(struct _msgptr);
    }
    return (0);
}



/*
 * 
 * NAME: opencatfile
 *
 * FUNCTION: Opens a catalog file, looking in the language path first (if 
 *	there is no slash) and returns a pointer to the file stream.
 *                                                                    
 * EXECUTION ENVIRONMENT:
 *
 * 	Opencatfile executes under a process.	
 *
 * RETURNS:  Returns a pointer to the file stream, and a NULL pointer on
 *	failure.
 */

static FILE *
open1catfile(file)
     char *file;
{
    extern char *getenv();
    char fl[PATH_MAX];		/*---- place to hold full path ----*/
    char *nlspath;		/*---- pointer to the nlspath val ----*/
    FILE *fp;			/*---- file pointer ----*/
    char cpth[PATH_MAX];	/*---- current value of nlspath ----*/
    char *p, *np;
    char *fulllang;		/* %L language value */
    char lang[PATH_MAX];	/* %l language value */
    char *territory;		/* %t language value */
    char *codeset;		/* %c language value */
    char *ptr;			/* for decompose of $LANG */
    char *str;
    char *optr;
    int nchars;
    int lenstr;
    char outptr[PATH_MAX];
    int valid;

    if (strchr(file, '/')) {
	if ((fp = fopen(file, "r"))) {
	    fcntl(fileno(fp), F_SETFD, 1);
	    /* set the close-on-exec flag for
	     * child process                */
	    return (fp);
	}
    } else {
	if (!(nlspath = getenv("NLSPATH")))
	    nlspath = PATH_FORMAT;
	if (!(fulllang = getenv("LANG")))
	    fulllang = DEFAULT_LANG;
	if (fulllang == DEFAULT_LANG)
	    nlspath = PATH_FORMAT;	/* if fullang is C, use the 
					 * the default nlspath:    */

	/*
	 ** LANG is a composite of three fields:
	 ** language_territory.codeset
	 ** and we're going to break it into those
	 ** three fields.
	 */

	strcpy(lang, fulllang);

	territory = "";
	codeset = "";

	ptr = strchr(lang, '_');
	if (ptr != NULL) {
	    territory = ptr + 1;
	    *ptr = '\0';
	    ptr = strchr(territory, '.');
	    if (ptr != NULL) {
		codeset = ptr + 1;
		*ptr = '\0';
	    }
	} else {
	    ptr = strchr(lang, '.');
	    if (ptr != NULL) {
		codeset = ptr + 1;
		*ptr = '\0';
	    }
	}

	np = nlspath;
	while (*np) {
	    p = cpth;
	    while (*np && *np != ':')
		*p++ = *np++;
	    *p = '\0';
	    if (*np)					/*----  iff on a colon then advance --*/
		np++;
	    valid = 0;
	    if (strlen(cpth)) {
		ptr = cpth;
		optr = outptr;

		nchars = 0;
		while (*ptr != '\0') {
		    while ((*ptr != '\0') && (*ptr != '%')
			   && (nchars < PATH_MAX)) {
			*(optr++) = *(ptr++);
			nchars++;
		    }
		    if (*ptr == '%') {
			switch (*(++ptr)) {
			case '%':
			    str = "%";
			    break;
			case 'L':
			    str = fulllang;
			    break;
			case 'N':
			    valid = 1;
			    str = file;
			    break;
			case 'l':
			    str = lang;
			    break;
			case 't':
			    str = territory;
			    break;
			case 'c':
			    str = codeset;
			    break;
			default:
			    str = "";
			    break;
			}
			lenstr = strlen(str);
			nchars += lenstr;
			if (nchars < PATH_MAX) {
			    strcpy(optr, str);
			    optr += lenstr;
			} else {
			    break;
			}
			ptr++;
		    } else {
			if (nchars >= PATH_MAX) {
			    break;
			}
		    }
		}
		*optr = '\0';
		strcpy(cpth, outptr);
	    } else {			/*----  iff leading | trailing | 
                                                adjacent colons ... --*/
		valid = 1;
		strcpy(cpth, file);
	    }
	    if (valid == 1 && (fp = fopen(cpth, "r"))) {
		fcntl(fileno(fp), F_SETFD, 1);
		/* set the close-on-exec flag for
		 * child process                */
		return (fp);
	    }
	}
	if (fp = fopen(file, "r")) {
	    fcntl(fileno(fp), F_SETFD, 1);
	    /* set the close-on-exec flag for
	     * child process                */
	    return (fp);
	}
    }
    return (NULL);
}





/*
 * 
 * NAME: cat_already_open
 *
 * FUNCTION: Checkes to see if a specific cat has already been opened.
 *                                                                    
 * EXECUTION ENVIRONMENT:
 *
 * 	Cat_already_open executes under a process.
 *
 * RETURNS: Returns a pointer to the existing CATD if one exists, and 
 *	a NULL pointer if no CATD exists.
 */

static nl_catd
cat_already_open(cat)
     char *cat;
			/*---- name of the catalog to be opened ----*/

{
    int i;			/*---- Misc counter(s) used for loops ----*/

    for (i = 0; i < NL_MAXOPEN && catsopen[i]; i++) {
#ifndef	AFS_OSF20_ENV
	if (!strcmp(cat, catsopen[i]->_name) && getpid() == catsopen[i]->_pid) {
#else
	if (!strcmp(cat, catsopen[i]->_name)) {
#endif
	    return (catsopen[i]);
	}
    }
    return (0);
}


int
catclose1(catd)					/*---- the catd to be closed ----*/
     nl_catd catd;	/*---- the catd to be closed ----*/

{
    int i;


    if (catd == CATD_ERR)
	return (-1);
    for (i = 0; i < NL_MAXOPEN && catsopen[i]; i++) {
#ifndef	AFS_OSF20_ENV
	if (catd == catsopen[i] && getpid() == catsopen[i]->_pid)
#else
	if (catd == catsopen[i])
#endif
	    break;
    }
    if (i == NL_MAXOPEN || catsopen[i] == NULL)
	return (-1);
    if (catd->_fd == (FILE *) NULL)
				/*----  return if this is an extra open or
					a bad catalog discriptor         ----*/
	return (-1);
    if (cat_already_open(catd->_name)) {
	if (catd->_count == 1) {
	    cat_hard_close(catd);
	    return (0);			/*--- the last legal clsoe ---*/
	} else if (catd->_count > 1) {
	    catd->_count = catd->_count - 1;
	    return (0);			/*--- a legal close ---*/
	} else
	    return (-1);		/*--- an extra illegal close ---*/
    } else {
	return (-1);
    }
}

static void
cat_hard_close(catd)
     nl_catd catd;
		/*---- the catd to be closed ----*/

{
    int i;			/*---- Misc counter(s) used for loops ----*/
    int j;			/*----  Misc counter ----*/

    if (catd == CATD_ERR)
	return;

/*______________________________________________________________________
	remove any entry for the catalog in the catsopen array
  ______________________________________________________________________*/

    for (i = 0; i < NL_MAXOPEN && catsopen[i]; i++) {
	if (catd == catsopen[i]) {
	    for (; i < NL_MAXOPEN - 1; i++) {
		catsopen[i] = catsopen[i + 1];
		catpid[i] = catpid[i + 1];
	    }
	    catsopen[i] = NULL;
	    catpid[i] = 0;
	}
    }

/*______________________________________________________________________
	close the cat and free up the memory
  ______________________________________________________________________*/
    if (catd->_mem == FALSE) {
	for (i = 0; i <= catd->_n_sets; i++) {
	    if (catd->_set[i]._mp)
		free(catd->_set[i]._mp);
			/*---- free the _message pointer arrays ----*/

	    if (catd->_set[i]._msgtxt) {
		for (j = 0; j <= catd->_set[i]._n_msgs; j++) {
		    if (catd->_set[i]._msgtxt[j]) {
/*					free(catd->_set[i]._msgtxt[j]);*/
		    }
		}
		if (catd->_set[i]._msgtxt)
		    free(catd->_set[i]._msgtxt);
	    }
	}
    }

    if (catd->_fd)
	fclose(catd->_fd);		/*---- close the ctatlog ----*/
    if (catd->_set)
	free(catd->_set);		/*---- free the sets ----*/
    if (catd->_name)
	free(catd->_name);		/*---- free the name ----*/
    if (catd->_hd)
	free(catd->_hd);		/*---- free the header ----*/
    if (catd)
	free(catd);			/*---- free the catd ----*/
}

static char *
_do1_read_msg(nl_catd catd, int setno, int msgno)

	/*---- catd: the catd of the catalog to be read from ----*/
	/*---- setno: the set number of the message ----*/
	/*---- msgno: the msgno of the message ----*/
{
    nl_catd catd1;		/*--- catd for different process ----*/
    char *_read1_msg();

#ifndef	AFS_OSF20_ENV
    if (getpid() == catd->_pid)
#else
    if (1)
#endif
	return (_read1_msg(catd, setno, msgno));
    else {
	/*
	 * Since our pid is different from the one in
	 * catd, catd must have come from a catopen()
	 * in our parent.  We need a catd of our own.
	 * The first time through here, the call to
	 * catopen() creates a new catd and we try to
	 * open its message catalog.  After that, the
	 * catopen() just retrieves the catd.
	 */
	if (((catd1 = catopen1(catd->_name, 0)) != CATD_ERR)
	    && ((catd1->_fd == NL_FILE_CLOSED && _do1_open(catd1) != CATD_ERR)
		|| (catd1->_fd != NL_FILE_UNUSED)))
	    return (_read1_msg(catd1, setno, msgno));
	else
	    return (NULL);
    }
}


struct _catset *_cat1_get_catset();
static struct _msgptr *_cat1_get_msgptr();
static char *
_read1_msg(nl_catd catd, int setno, int msgno)
{
    struct _catset *set;	/*--- ptr to set's _catset structure ---*/
    struct _msgptr *msg;	/*--- ptr to msg's _msgptr structure ---*/
    char **msgtxt;		/*--- temporary pointer to the message text
                                      for speed.  ----*/

    set = _cat1_get_catset(catd, setno);
    if (set) {
	msg = _cat1_get_msgptr(set, msgno);
	if (msg) {
	    msgtxt = &set->_msgtxt[msg - set->_mp];
	    if (1 /*!*msgtxt */ ) {
		*msgtxt = (char *)malloc(msg->_msglen + 1);
		if (!*msgtxt)
		    return (NULL);

		fseek(catd->_fd, (long)msg->_offset, 0);
		if (fread
		    ((void *)*msgtxt, (size_t) (msg->_msglen + 1), (size_t) 1,
		     catd->_fd) != 1)
		    return (NULL);
	    }

	    return (*msgtxt);
	}
    }
    return (NULL);
}

/*
 * NAME: compare_sets
 *                                                                    
 * FUNCTION: Compare function used by bsearch() in _cat_get_catset().
 *                                                                    
 * ARGUMENTS:
 *      key             - pointer to set number we're searching for
 *      element         - pointer to current _catset structure
 *
 * RETURNS: Returns -1, 0, or 1, depending on whether the set number
 *          is less than, equal to, or greater than the set number of
 *          the _catset structure.
 *
 */

static int
compare_sets(const void *key, const void *element)
{
    int *setno = (int *)key;
    struct _catset *set = (struct _catset *)element;

    if (*setno < set->_setno)
	return -1;
    if (*setno > set->_setno)
	return 1;

    return 0;
}


/*
 * NAME: _cat_get_catset
 *                                                                    
 * FUNCTION: Find a set in the catd->_set array.  Assumes that the
 *           sets in the array are sorted by increasing set number.
 *                                                                    
 * ARGUMENTS:
 *      catd            - catalog descripter obtained from catopen()
 *      setno           - message catalogue set number
 *
 * RETURNS: Returns a pointer to the set on success.
 *      On any error, returns NULL.
 *
 */

struct _catset *
_cat1_get_catset(nl_catd catd, int setno)
{
    struct _catset *set;

    if ((catd == (nl_catd) NULL) || (catd == CATD_ERR))
	return (struct _catset *)NULL;

    if (catd->_sets_expanded) {
	if ((setno < 0) || (setno > catd->_n_sets))
	    return (struct _catset *)NULL;

	set = &catd->_set[setno];

	/*
	 * Catch empty elements in the array.  They aren't
	 * real sets.
	 */

	if (set->_mp == (struct _msgptr *)NULL)
	    return (struct _catset *)NULL;
    } else {
	set =
	    (struct _catset *)bsearch((void *)&setno, catd->_set,
				      catd->_n_sets + 1,
				      sizeof(struct _catset), compare_sets);

	/*
	 * Since the sets are compacted, there aren't any
	 * empty elements in the array to check for.
	 */
    }

    return set;
}


/*
 * NAME: compare_msgs
 *                                                                    
 * FUNCTION: Compare function used by bsearch() in _cat_get_msgptr().
 *                                                                    
 * ARGUMENTS:
 *      key             - pointer to message number we're searching for
 *      element         - pointer to current _msgptr structure
 *
 * RETURNS: Returns -1, 0, or 1, depending on whether the message
 *          number is less than, equal to, or greater than the message
 *          number of the _msgptr structure.
 *
 */

static int
compare_msgs(const void *key, const void *element)
{
    int *msgno = (int *)key;
    struct _msgptr *msg = (struct _msgptr *)element;

    if (*msgno < msg->_msgno)
	return -1;
    if (*msgno > msg->_msgno)
	return 1;

    return 0;
}

/*
 * NAME: _cat1_get_msgptr
 *                                                                    
 * FUNCTION: Find a message in a set's set->_mp array.  Assumes that
 *           the messages in the array are sorted by increasing
 *           message number.
 *                                                                    
 * ARGUMENTS:
 *      set             - ptr to _catset structure
 *      msgno           - message catalogue message number
 *
 * RETURNS: Returns a pointer to the message on success.
 *      On any error, returns NULL.
 *
 */
static struct _msgptr *
_cat1_get_msgptr(struct _catset *set, int msgno)
{
    struct _msgptr *msg;

    if (set == (struct _catset *)NULL)
	return (struct _msgptr *)NULL;

    if (set->_mp == (struct _msgptr *)NULL)	/* empty set */
	return (struct _msgptr *)NULL;

    if (set->_msgs_expanded) {
	if ((msgno < 0) || (msgno > set->_n_msgs))
	    return (struct _msgptr *)NULL;

	msg = &set->_mp[msgno];

	/*
	 * Catch empty elements in the array.  They aren't
	 * real messages.
	 */

	if (!msg->_offset)
	    return (struct _msgptr *)NULL;
    } else {
	msg =
	    (struct _msgptr *)bsearch((void *)&msgno, set->_mp,
				      set->_n_msgs + 1,
				      sizeof(struct _msgptr), compare_msgs);

	/*
	 * Since the messages are compacted, there aren't any
	 * empty elements in the array to check for.
	 */
    }

    return msg;
}

char *
catgets1(nl_catd catd, int setno, int msgno, char *def)
	/*---- catd: the catd to get the message from ----*/
	/*---- setno: the set number of the message ----*/
	/*---- msgno: the message number of the message ----*/
	/*---- def: the default string to be returned ----*/
{
    int errno_save;
    char *_do_read_msg();
    char *m;
    errno_save = errno;

    if (catd == NULL || catd == CATD_ERR || catd->_magic != CAT_MAGIC
	|| catd->_fd == NL_FILE_UNUSED) {
	return (def);
    }
    if (catd->_fd == NL_FILE_CLOSED) {
	catd = _do1_open(catd);
	if (catd == CATD_ERR)
	    return (def);
    }

    if (catd->_mem) {		/*----  for mapped files ----*/
	if (setno <= catd->_hd->_setmax) {
	    if (msgno < catd->_set[setno]._n_msgs) {
		if (catd->_set[setno]._mp[msgno]._offset) {
		    return (catd->_mem +
			    catd->_set[setno]._mp[msgno]._offset);
		}
	    }
	}
	return (def);
    } else {	/*---- for unmapped files ----*/
	m = _do1_read_msg(catd, setno, msgno);
	if (m == NULL)
	    return (def);
	else
	    return (m);
    }
}

#endif

#define FACILITY_CODE_MASK          0xF0000000
#define FACILITY_CODE_SHIFT         28

#define COMPONENT_CODE_MASK         0x0FFFF000
#define COMPONENT_CODE_SHIFT        12

#define STATUS_CODE_MASK            0x00000FFF
#define STATUS_CODE_SHIFT           0

#define NO_MESSAGE                  "THIS IS NOT A MESSAGE"

/*
 * The system-dependant location for the catalog files is defined in sysconf.h
 * RPC_DEFAULT_NLSPATH should be defined in sysconf.h. Otherwise we use
 * /usr/afs/etc/C/%s.cat 
 */

#ifndef RPC_NLS_FORMAT
#define RPC_NLS_FORMAT "%s.cat"
#endif

dce1_error_inq_text(status_to_convert, error_text, status)
     afs_uint32 status_to_convert;
     unsigned char *error_text;
     int *status;

{
    unsigned short facility_code;
    unsigned short component_code;
    unsigned short status_code;
    unsigned short i, failed = 0;
    nl_catd catd;
    char component_name[4];
    char *facility_name;
    char filename_prefix[7];
    char nls_filename[11];
    char alt_filename[80];
    char *message;
#if defined(AFS_64BITPOINTER_ENV)
    long J;
#else
    int J;
#endif
    static char *facility_names[] = {
	"xxx",
	"afs"
    };

    /*
     * set up output status for future error returns
     */
    if (status != NULL) {
	*status = -1;
    }
    /*
     * check for ok input status
     */
    if (status_to_convert == 0) {
	if (status != NULL) {
	    *status = 0;
	}
	strcpy((char *)error_text, "successful completion");
	return;
    }

    /*
     * extract the component, facility and status codes
     */
    facility_code =
	(status_to_convert & FACILITY_CODE_MASK) >> FACILITY_CODE_SHIFT;
    component_code =
	(status_to_convert & COMPONENT_CODE_MASK) >> COMPONENT_CODE_SHIFT;
    status_code = (status_to_convert & STATUS_CODE_MASK) >> STATUS_CODE_SHIFT;

    /*
     * see if this is a recognized facility
     */
    if (facility_code == 0
	|| facility_code > sizeof(facility_names) / sizeof(char *)) {
	sprintf((char *)error_text, "status %08x (unknown facility)",
		status_to_convert);
	return;
    }
    facility_name = facility_names[facility_code - 1];
    /*
     * Convert component name from RAD-50 component code.  (Mapping is:
     * 0 => 'a', ..., 25 => 'z', 26 => '{', 27 => '0', ..., 36 => '9'.)
     */
    component_name[3] = 0;
    component_name[2] = component_code % 40;
    component_code /= 40;
    component_name[1] = component_code % 40;
    component_name[0] = component_code / 40;
    for (i = 0; i < 3; i++) {
	component_name[i] += (component_name[i] <= 26) ? 'a' : ('0' - 27);
    }
    sprintf(filename_prefix, "%3s%3s", facility_name, component_name);
    sprintf(nls_filename, RPC_NLS_FORMAT, filename_prefix);

    /*
     * Open the message file
     */
#if	defined(AFS_OSF_ENV)
#if	defined(AFS_OSF20_ENV)
    catd = (nl_catd) catopen(nls_filename, 0);
#else
    catd = (nl_catd) catopen1(nls_filename, 0);
#endif
#else
#if defined(AFS_64BITPOINTER_ENV)
    J = (long)catopen(nls_filename, 0);
#else
    J = (int)catopen(nls_filename, 0);
#endif
    catd = (nl_catd) J;
#endif
    if (catd == (nl_catd) - 1) {
	/*
	 * If we did not succeed in opening message file using NLSPATH,
	 * try to open the message file in a well-known default area
	 */
      tryagain:
#ifndef RPC_DEFAULT_NLSPATH
	sprintf(alt_filename, "%s/C/%s.cat", AFSDIR_CLIENT_ETC_DIRPATH,
		filename_prefix);
#else
	sprintf(alt_filename, RPC_DEFAULT_NLSPATH, filename_prefix);
#endif

#if	defined(AFS_OSF_ENV)
#if	defined(AFS_OSF20_ENV)
	catd = (nl_catd) catopen(alt_filename, 0);
#else
	catd = (nl_catd) catopen1(alt_filename, 0);
#endif
#else
#if defined(AFS_64BITPOINTER_ENV)
        J = (long)catopen(alt_filename, 0);
#else
	J = (int)catopen(alt_filename, 0);
#endif
	catd = (nl_catd) J;
#endif
	if (catd == (nl_catd) - 1) {
	    sprintf((char *)error_text, "status %08x (%s / %s)",
		    status_to_convert, facility_name, component_name);
	    return;
	}
    }
    /*
     * try to get the specified message from the file
     */
#if	defined(AFS_OSF_ENV) && !defined(AFS_OSF20_ENV)
    message = (char *)catgets1(catd, 1, status_code, NO_MESSAGE);
#else
    message = (char *)catgets(catd, 1, status_code, NO_MESSAGE);
#endif
    /*
     * if everything went well, return the resulting message
     */
    if (strcmp(message, NO_MESSAGE) != 0) {
	sprintf((char *)error_text, "%s (%s / %s)", message, facility_name,
		component_name);
	if (status != NULL) {
	    *status = 0;
	}
    } else {
	if (!failed) {
	    failed = 1;
#if defined(AFS_OSF_ENV) && !defined(AFS_OSF20_ENV)
	    catclose1(catd);
#else
	    catclose(catd);
#endif
	    goto tryagain;
	}
	sprintf((char *)error_text, "status %08x (%s / %s)",
		status_to_convert, facility_name, component_name);
    }
#if	defined(AFS_OSF_ENV) && !defined(AFS_OSF20_ENV)
    catclose1(catd);
#else
    catclose(catd);
#endif

}


static
DoDump(as, arock)
     register struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 code = 0;
    afs_int32 tcode;
    afs_int32 waitTime = 10 /* seconds */ ;
    int error = 0;
    char *logname;
    char *filename;
    FILE *outfp = stdout;
    time_t startTime;
    struct cmd_item *itemp;

    if (geteuid() != 0) {
	printf("fstrace must be run as root\n");
	exit(1);
    }

    if (as->parms[3].items) {
	if (!as->parms[1].items) {
	    (void)fprintf(stderr, "-sleep can only be used with -follow\n");
	    return 1;
	}
	waitTime = strtol(as->parms[3].items->data, NULL, 0);
    }

    if (as->parms[2].items) {
	/* try to open the specified output file */
	if ((outfp = fopen(as->parms[2].items->data, "w")) == NULL) {
	    (void)fprintf(stderr, "Cannot open file '%s' for writing\n",
			  as->parms[2].items->data);
	    return 1;
	}
    }
#ifdef AFS_SGI64_ENV
    startTime = time((time_t *) 0);
#else
    startTime = time(0);
#endif
    (void)fprintf(outfp, "AFS Trace Dump -\n\n   Date: %s\n",
		  ctime(&startTime));

    if (as->parms[0].items) {
	for (itemp = as->parms[0].items; itemp; itemp = itemp->next) {
	    tcode = icl_DumpKernel(outfp, itemp->data);
	    if (tcode) {
		(void)fprintf(stderr, "Unable to dump set %s (errno = %d)\n",
			      itemp->data, errno);
		code = tcode;
	    }
	}
    } else if (as->parms[1].items) {
	logname = as->parms[1].items->data;
	code = icl_TailKernel(outfp, logname, waitTime);
	if (code) {
	    (void)fprintf(stderr,
			  "Error tailing kernel log '%s' (errno = %d)\n",
			  logname, errno);
	}
    } else
	code = icl_DumpKernel(outfp, NULL);

    (void)fprintf(outfp, "\nAFS Trace Dump - %s\n",
		  code ? "FAILED" : "Completed");

    if (outfp != stdout)
	(void)fclose(outfp);

    return code;
}

static void
SetUpDump()
{
    struct cmd_syndesc *dumpSyntax;

    dumpSyntax =
	cmd_CreateSyntax("dump", DoDump, (char *)NULL, "dump AFS trace logs");
    (void)cmd_AddParm(dumpSyntax, "-set", CMD_LIST, CMD_OPTIONAL, "set_name");
    (void)cmd_AddParm(dumpSyntax, "-follow", CMD_SINGLE, CMD_OPTIONAL,
		      "log_name");
    (void)cmd_AddParm(dumpSyntax, "-file", CMD_SINGLE, CMD_OPTIONAL,
		      "output_filename");
    (void)cmd_AddParm(dumpSyntax, "-sleep", CMD_SINGLE, CMD_OPTIONAL,
		      "seconds_between_reads");
}


static
DoShowSet(as, arock)
     register struct cmd_syndesc *as;
     char *arock;
{
    afs_int32 retVal = 0;
    afs_int32 code = 0;
    afs_int32 state;
    struct cmd_item *itemp;

    if (geteuid() != 0) {
	printf("fstrace must be run as root\n");
	exit(1);
    }
    if (as->parms[0].items) {
	/* print information on the specified sets */
	for (itemp = as->parms[0].items; itemp; itemp = itemp->next) {
	    code = icl_GetSetState(itemp->data, &state);
	    if (code) {
		(void)fprintf(stderr,
			      "Error getting status on set %s (errno = %d)\n",
			      itemp->data, errno);
		retVal = code;
	    } else
		(void)fprintf(stdout, "Set %s: %s%s%s\n", itemp->data,
			      (state & ICL_SETF_ACTIVE) ? "active" :
			      "inactive",
			      (state & ICL_SETF_FREED) ? " (dormant)" : "",
			      (state & ICL_SETF_PERSISTENT) ? " persistent" :
			      "");
	}
    } else {
	/* show all sets */
	(void)fprintf(stdout, "Available sets:\n");
	code = icl_ListSets(stdout);
	if (code) {
	    (void)fprintf(stderr, "Error in listing sets (errno = %d)\n",
			  errno);
	    retVal = code;
	}
    }

    return retVal;
}

static void
SetUpShowSet()
{
    struct cmd_syndesc *showSyntax;

    showSyntax =
	cmd_CreateSyntax("lsset", DoShowSet, (char *)NULL,
			 "list available event sets");
    (void)cmd_AddParm(showSyntax, "-set", CMD_LIST, CMD_OPTIONAL, "set_name");
}


#include "AFS_component_version_number.c"

main(ing argc, char ** argv)
{
    setlocale(LC_ALL, "");

    /* set up user interface then dispatch */

    return (cmd_Dispatch(argc, argv));
}
#else
#include "AFS_component_version_number.c"

main()
{
    printf("fstrace is NOT supported for this OS\n");
}
#endif
