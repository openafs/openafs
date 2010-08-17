/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * sym	-	symbol table definitions, VRMIX kernel pseudo-TOC
 */

struct toc_syment {
#ifdef __XCOFF64__
    unsigned long long n_value;
    union {
	struct {
	    int _n_offset;	/* offset into string table */
	} _n_n;
	char *_n_nptr;		/* Allows for overlaying */
    } _n;
#else				/* __XCOFF64__ */
    union {
	char _n_name[8];	/* old COFF version */
	struct {
	    int _n_zeroes;	/* new == 0 */
	    int _n_offset;	/* offset into string table */
	} _n_n;
	char *_n_nptr[2];	/* allows for overlaying */
    } _n;
    int n_value;		/* value of symbol */
#endif				/* __XCOFF64__ */
};
#ifdef __XCOFF64__
#define n_nptr		_n._n_nptr
#else
#define n_name		_n._n_name
#define n_nptr		_n._n_nptr[1]
#define n_zeroes	_n._n_n._n_zeroes
#endif
#define n_offset	_n._n_n._n_offset

typedef struct toc_syment sym_t;

extern struct toc_syment *toc_syms;	/* symbol table         */
extern caddr_t toc_strs;	/* string table         */
extern toc_nsyms;		/* # symbols            */
extern sym_t *sym_flex();
extern sym_t *sym_lookup();
