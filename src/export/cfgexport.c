/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * cfgexport -	load/configure the EXPORT kernel extension
 */
#include <afsconfig.h>
#include <afs/param.h>


#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/sysconfig.h>
#include <sys/uio.h>
#include <sys/xcoff.h>
#include <sys/ldr.h>
#include <setjmp.h>
#include <signal.h>
#include "export.h"
#include "sym.h"

extern char *malloc(), *optarg;
extern int sysconfig(int cmd, void *arg, int len);

int debug = 0;
char *syms = "/unix";
char *xstrings;

#include "AFS_component_version_number.c"

main(argc, argv)
     char **argv;
{
    int add, del, opts;
    int c;
    char *file;
    mid_t kmid;
    struct cfg_load cload;
    struct cfg_kmod cmod;
    struct k_conf conf;
    FILE *fp;

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a
     * crash (i.e. core is generated) we can include the user's data section
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    add = del = 0;

    while ((c = getopt(argc, argv, "a:s:Z:d:")) != EOF) {
	switch (c) {
	case 'Z':		/* Zdebug option        */
	    ++debug;
	    break;

	case 'a':
	    add = 1;
	    file = optarg;
	    if (!file)
		usage();
	    break;

	case 'd':
	    del = 1;
	    file = optarg;
	    if (!file)
		usage();
	    break;

	case 's':
	    syms = optarg;
	    break;

	default:
	    usage();
	    break;
	}
    }

    if (!add && !del)
	usage();

    if (add) {
	char *buf[1024];
	char PidFile[256];

	buf[0] = "execerror";
	buf[1] = "cfgexport";
	get_syms(&conf, syms);

	cload.path = file;
	if (sysconfig(SYS_KLOAD, &cload, sizeof(cload)) == -1) {
	    loadquery(L_GETMESSAGES, &buf[2], sizeof buf - 8);
	    execvp("/etc/execerror", buf);
	    perror("SYS_KLOAD");
	    exit(1);
	}

	cmod.kmid = cload.kmid;
	cmod.cmd = CFG_INIT;
	cmod.mdiptr = (caddr_t) & conf;
	cmod.mdilen = sizeof(conf);

	if (sysconfig(SYS_CFGKMOD, &cmod, sizeof(cmod)) == -1) {
	    perror("SYS_CFGKMOD");
	    cload.kmid = cload.kmid;
	    sysconfig(SYS_KULOAD, &cload, sizeof(cload));
	    exit(1);
	}
#ifdef	notdef
	printf("cfgexport -d 0x%x # to remove EXPORT\n", cload.kmid);
#endif
	strcpy(PidFile, file);
	strcat(PidFile, ".kmid");
	fp = fopen(PidFile, "w");
	if (fp) {
	    (void)fprintf(fp, "%d\n", cload.kmid);
	    (void)fclose(fp);
	} else {
	    printf("Can't open for write file %s (error=%d); ignored\n",
		   PidFile, errno);
	}
	exit(0);
    } else if (del) {
	char PidFile[256];

	strcpy(PidFile, file);
	strcat(PidFile, ".kmid");
	fp = fopen(PidFile, "r");
	if (!fp) {
	    printf("Can't read %s file (error=%d); aborting\n", PidFile,
		   errno);
	    exit(1);
	}
	(void)fscanf(fp, "%d\n", &kmid);
	(void)fclose(fp);
	unlink(PidFile);
	cmod.kmid = kmid;
	cmod.cmd = CFG_TERM;
	cmod.mdiptr = NULL;
	cmod.mdilen = 0;

	if (sysconfig(SYS_CFGKMOD, &cmod, sizeof(cmod)) == -1) {
	    perror("SYS_CFGKMOD");
	    exit(1);
	}

	cload.kmid = kmid;
	if (sysconfig(SYS_KULOAD, &cload, sizeof(cload)) == -1) {
	    perror("SYS_KULOAD");
	    exit(1);
	}
	exit(0);
    }
}

usage()
{

    error("usage: cfgexport [-a mod_file [-s symbols]] [-d mod_file]\n");
}

/*
 * get_syms -	get kernel symbol table info.
 *
 * Input:
 *	conf	-	^ to EXPORT extension configuration struct
 *	syms	-	^ to name of file containing XCOFF symbols
 */
get_syms(conf, syms)
     struct k_conf *conf;
     char *syms;
{
    sym_t *k_symtab, *ksp;
    struct syment *x_symtab, *xsp, *xsq;
    char *kstrings;
    struct xcoffhdr hdr;	/* XCOFF header from symbol file */
    sym_t k_sym;		/* export version of symbol     */
    struct syment xcoff_sym;	/* xcoff version of symbol      */
    int i, nsyms, nksyms, nxsyms = 0;
    int xstr_size, kstr_size;
    FILE *fp;
    int xsym_compar();

    if (syms == NULL)
      sys_error("syms is NULL");
    fp = fopen(syms, "r");
    if (fp == NULL)
	sys_error(syms);

    if (fread(&hdr, sizeof(hdr), 1, fp) != 1)
	sys_error(syms);

    if (hdr.filehdr.f_nsyms == 0)
	error("%s: no symbols", syms);

    switch (hdr.filehdr.f_magic) {
    case U802WRMAGIC:
    case U802ROMAGIC:
    case U802TOCMAGIC:
    case U800WRMAGIC:
    case U800ROMAGIC:
    case U800TOCMAGIC:
#ifdef __XCOFF64__
    case U64_TOCMAGIC:
#endif
	break;

    default:
	error("%s: funny magic number 0%o", syms, hdr.filehdr.f_magic);
    }

    nsyms = hdr.filehdr.f_nsyms;
    if (debug)
	printf("nsyms = %d\n", nsyms);

    x_symtab = (struct syment *)malloc(nsyms * SYMESZ);
    if (!x_symtab)
	error("no memory for symbol table");

    /*
     * try to snarf the string table: should be just past the
     * symbol table: first 4 bytes is length of rest.
     */
    if (fseek(fp, hdr.filehdr.f_symptr + nsyms * SYMESZ, 0) < 0)
	sys_error("%s: seek to strtab", syms);

    if (fread(&xstr_size, sizeof(xstr_size), 1, fp) != 1)
	error("%s: reading string table size", syms);

    xstrings = malloc(xstr_size + sizeof(xstr_size));
    if (!xstrings)
	error("no memory for string table");

    /*
     * seek back to the start of the strings
     */
    if (fseek(fp, hdr.filehdr.f_symptr + nsyms * SYMESZ, 0) < 0)
	sys_error("%s: seek to strtab", syms);

    if (fread(xstrings, sizeof(*xstrings), xstr_size, fp) != xstr_size)
	error("%s: reading string table");

    /*
     * now seek back to the start of the symbol table, and read it
     * all in.
     */
    if (fseek(fp, hdr.filehdr.f_symptr, 0) < 0)
	sys_error("%s: seek to symtab", syms);

    xsp = &x_symtab[0];

    for (i = nxsyms = 0; i < nsyms; ++i) {
	char name[16], *p;

	if (fread(&xcoff_sym, SYMESZ, 1, fp) != 1)
	    error("%s: reading symbol entry", syms);

#ifdef __XCOFF64__
	p = xstrings + xcoff_sym.n_offset;
#else
	if (xcoff_sym.n_zeroes == 0) {
	    /*
	     * Need to relocate string table offset
	     */
	    p = xcoff_sym.n_nptr = xstrings + xcoff_sym.n_offset;
	} else {
	    strncpy(name, xcoff_sym.n_name, 8);

	    p = name, p[8] = 0;
	}
#endif

	if (debug > 2)
	    dump_xsym(&xcoff_sym);

	switch (xcoff_sym.n_sclass) {
	case C_EXT:		/* external                     */
	case C_HIDEXT:		/* hidden external (sic)        */
	    /*
	     * filtre out the ones with the strange names
	     */
	    if (strchr(p, '@') || strchr(p, '$') || p[0] == 0)
		break;

	    *xsp++ = xcoff_sym;
	    ++nxsyms;

	    if (debug > 1)
		dump_xsym(&xcoff_sym);

	    break;
	}

	if (xcoff_sym.n_numaux) {
	    fseek(fp, xcoff_sym.n_numaux * AUXESZ, 1);
	    i += xcoff_sym.n_numaux;
	}
    }

    fclose(fp);

    /*
     * sort the symbol table
     */
    qsort((char *)x_symtab, nxsyms, sizeof(*x_symtab), xsym_compar);

    /*
     * we will need no more than `nxsyms' symbols.
     */
    k_symtab = (sym_t *) malloc(nxsyms * sizeof(sym_t));
    if (!k_symtab)
	error("no memory for EXPORT symbol table");

    /*
     * uniquify it, and xlate to funny EXPORT format
     */
    xsp = xsq = x_symtab;
    ksp = k_symtab;
    kstrings = 0;
    kstr_size = 0;
    nksyms = 0;

    memset(xsq = &xcoff_sym, 0, sizeof(*xsq));

    for (i = 1; i < nxsyms; ++i, xsq = xsp++) {
#ifdef __XCOFF64__
	if (xsp->n_offset != xsq->n_offset || xsp->n_value != xsq->n_value) {
#else
	if (xsp->n_zeroes != xsq->n_zeroes || xsp->n_offset != xsq->n_offset
	    || xsp->n_value != xsq->n_value) {
#endif
	    xlate_xtok(xsp, ksp++, &kstrings, &kstr_size);
	    ++nksyms;
	}
    }

    /*
     * place the symbol table info into the `conf' data structure
     *
     * XXXXX: for today only, leave the string table the same.
     */
    conf->nsyms = nksyms;
    conf->symt_sz = nksyms * sizeof(sym_t);
    conf->str_sz = kstr_size;
    conf->symtab = (caddr_t) k_symtab;
    conf->strtab = kstrings;
}


/*
 * xlate_xtok	-	xlate XCOFF to EXPORT format
 *
 * Input:
 *	xp	-	^ to XCOFF symbol
 *	kp	-	^ to EXPORT  symbol save area
 *	strp	-	^ to ^ to EXPORT string table
 *	szp	-	^ to EXPORT string table size
 */
#define SYMBUFSIZE 1048576
xlate_xtok(xp, kp, strp, szp)
     struct syment *xp;
     sym_t *kp;
     char **strp;
     uint *szp;
{
    int len;
    static char *export_strings = NULL, *prev = "";
    static left, offset, sz;

    if (!export_strings) {
	export_strings = malloc(sz = SYMBUFSIZE);
	if (!export_strings)
	    error("no memory for EXPORT string table");

	*strp = export_strings;
	*szp = offset = sizeof(uint);
	left = SYMBUFSIZE - offset;

	export_strings += offset;

	*(uint *) export_strings = 0;	/* initial 4 bytes      */
    }
#ifdef __XCOFF64__
    if (strcmp(prev, xstrings + xp->n_offset) == 0) {
	/*
	 * same name as previous entry: just use previous
	 */
	kp->n_offset = offset - strlen(*strp + xp->n_offset) - 1;
    } else
	if (find_suffix
	    (xstrings + xp->n_offset, *strp, offset, &kp->n_offset)) {
	/*
	 * found a string that we are a suffix of
	 */
	;
    } else {
	/*
	 * need to add to our string table
	 */
	len = strlen(xstrings + xp->n_offset) + 1;
	while (len >= left) {
	    fprintf(stderr, "cfgexport: Out of memory. Increase SYMBUFSIZE and recompile\n");
	    exit(1);
#if 0
	    /* Something is broken with this code, after being here
	       cfgexport segfaults */
	    export_strings = (char *)realloc(*strp, sz += SYMBUFSIZE);
	    if (!export_strings)
		error("no memory for EXPORT string table");
	    *strp = export_strings;
	    left += SYMBUFSIZE;
	    prev = "";		/* lazy */
#endif
	}

	strcpy(prev = *strp + offset, xstrings + xp->n_offset);

	kp->n_offset = offset;
	offset += len;
	left -= len;
	*szp += len;
    }
#else
    if (kp->n_zeroes = xp->n_zeroes) {	/* sic  */
	kp->n_zeroes = xp->n_zeroes;
	kp->n_offset = xp->n_offset;
    } else if (strcmp(prev, xp->n_nptr) == 0) {
	/*
	 * same name as previous entry: just use previous
	 */
	kp->n_offset = offset - strlen(xp->n_nptr) - 1;
    } else if (find_suffix(xp->n_nptr, *strp, offset, &kp->n_offset)) {
	/*
	 * found a string that we are a suffix of
	 */
	;
    } else {
	/*
	 * need to add to our string table
	 */
	len = strlen(xp->n_nptr) + 1;
	while (len >= left) {
	    export_strings = (char *)realloc(*strp, sz += SYMBUFSIZE);
	    if (!export_strings)
		error("no memory for EXPORT string table");
	    *strp = export_strings;
	    left += SYMBUFSIZE;
	    prev = "";		/* lazy */
	}

	strcpy(prev = *strp + offset, xp->n_nptr);

	kp->n_offset = offset;
	offset += len;
	left -= len;
	*szp += len;
    }
#endif

    kp->n_value = xp->n_value;

    if (debug)
	dump_ksym(kp, *strp);
}

/*
 * find_suffix	-	look for a string that arg string is suffix of
 *
 * Input:
 *	p	-	^ to string we hope is a suffix of another
 *	strings	-	^ to string table
 *	max	-	max offset of valid string in strings
 *	offp	-	^ to place to store offset, if containing string found
 *
 * Returns:
 *	 0	-	no containing string found
 *	!0	-	string found of which `p' is a suffix
 *
 * NOTE:
 *	This is rather inefficient.
 */
find_suffix(p, strings, max, offp)
     char *p, *strings;
     uint *offp;
{
    char *q, *e;
    int len = strlen(p) - 1;

    strings += sizeof(uint);
    max -= sizeof(uint);

    for (e = strings + max; e > strings;) {
	/*
	 * adjust `e' to point at last non-blank
	 */
	if (*e == 0) {
	    --e;
	    continue;
	}

	for (q = p + len; q > p && *q == *e;)
	    --q, --e;

	if (*q == *e) {
	    if (debug)
		printf("found_suffix: %s\n", p);
	    return *offp = e - strings + sizeof(uint);
	}

	if (*e)
	    while (*e && e > strings)
		--e;
    }

    return 0;
}

/*
 * xsym_compar -	compare two XCOFF symbol table entries
 *
 * If the names are the same, sort by descending storage class, so that
 * C_EXT < C_HIDEXT;
 */
xsym_compar(xp, xq)
     struct syment *xp, *xq;
{
    char *p, *q;
    int compar;

#ifndef __XCOFF64__
    p = (xp->n_zeroes ? xp->n_name : xp->n_nptr);
    q = (xq->n_zeroes ? xq->n_name : xq->n_nptr);

    if (xp->n_zeroes || xq->n_zeroes)
	compar = strncmp(p, q, 8);
    else
#else
    p = xstrings + xp->n_offset;
    q = xstrings + xq->n_offset;
#endif
    compar = strcmp(p, q);

    if (compar == 0)
	compar = xp->n_sclass - xq->n_sclass;

    return compar;
}

/*
 * dump_xsym -	print to XCOFF symbol
 */
dump_xsym(xsp)
     struct syment *xsp;
{

#ifndef __XCOFF64__
    if (xsp->n_zeroes)
	printf
	    ("nptr <%-8.8s  %8.8s> val %8.8x sc# %4.4x type %4.4x sclass %2.2x naux %2.2x\n",
	     xsp->n_name, "", xsp->n_value, xsp->n_scnum & 0xffff,
	     xsp->n_type, xsp->n_sclass, xsp->n_numaux);
    else
#endif
	printf
	    ("nptr <%-17.17s> val %8.8x sc# %4.4x type %4.4x sclass %2.2x naux %2.2x\n"
#ifdef __XCOFF64__
	     , xstrings + xsp->n_offset
#else
	     , xsp->n_nptr
#endif
	     , xsp->n_value, xsp->n_scnum & 0xffff, xsp->n_type,
	     xsp->n_sclass, xsp->n_numaux);
}

dump_ksym(ksp, strings)
     sym_t *ksp;
     char *strings;
{

#ifndef __XCOFF64__
    if (ksp->n_zeroes)
	printf("%8.8x %-8.8s\n", ksp->n_value, ksp->n_name);
    else
#endif
	printf("%8.8x %s\n", ksp->n_value, ksp->n_offset + strings);
}

error(p, a, b, c, d, e)
     char *p;
{

    fprintf(stderr, p, a, b, c, d, e);
    fprintf(stderr, "\n");
    exit(1);
}

sys_error(p, a, b, c, d, e)
     char *p;
{

    fprintf(stderr, p, a, b, c, d, e);
    perror(": ");
    exit(1);
}

warn(p, a, b, c, d, e)
     char *p;
{

    fprintf(stderr, p, a, b, c, d, e);
    fprintf(stderr, "\n");
}
