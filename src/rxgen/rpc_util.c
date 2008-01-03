/* @(#)rpc_util.c	1.2 87/11/24 3.9 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * rpc_util.c, Utility routines for the RPC protocol compiler 
 * Copyright (C) 1987, Sun Microsystems, Inc. 
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "rpc_scan.h"
#include "rpc_parse.h"
#include "rpc_util.h"

char curline[MAXLINESIZE];	/* current read line */
char *where = curline;		/* current point in line */
int linenum = 0;		/* current line number */

char *infilename;		/* input filename */

#define NFILES 6
char *outfiles[NFILES];		/* output file names */
int nfiles;

FILE *fout;			/* file pointer of current output */
FILE *fin;			/* file pointer of current input */

list *defined;			/* list of defined things */

/* static prototypes */
static int findit(definition * def, char *type);
static char *fixit(char *type, char *orig);
static int typedefed(definition * def, char *type);
static char *locase(char *str);
static char *toktostr(tok_kind kind);
static void printbuf(void);
static void printwhere(void);


/*
 * Reinitialize the world 
 */
void
reinitialize(void)
{
    int i;
    memset(curline, 0, MAXLINESIZE);
    where = curline;
    linenum = 0;
    defined = NULL;
    special_defined = typedef_defined = uniondef_defined = NULL;
    PackageIndex = -1;
    master_opcodenumber = 99999;
    master_highest_opcode = 0;
    no_of_stat_funcs = 0;
    for (i = 0; i < MAX_PACKAGES; i++) {
	no_of_stat_funcs_header[i] = 0;
    }
}

/*
 * string equality 
 */
int
streq(char *a, char *b)
{
    return (strcmp(a, b) == 0);
}

/*
 * find a value in a list 
 */
char *
findval(list * lst, char *val, int (*cmp) (definition * def, char *type))
{
    for (; lst != NULL; lst = lst->next) {
	if ((*cmp) ((definition *) lst->val, val)) {
	    return (lst->val);
	}
    }
    return (NULL);
}

/*
 * store a value in a list 
 */
void
storeval(list ** lstp, char *val)
{
    list **l;
    list *lst;

    for (l = lstp; *l != NULL; l = (list **) & (*l)->next);
    lst = ALLOC(list);
    lst->val = val;
    lst->next = NULL;
    *l = lst;
}


static int
findit(definition * def, char *type)
{
    return (streq(def->def_name, type));
}


static char *
fixit(char *type, char *orig)
{
    definition *def;

    def = (definition *) FINDVAL(defined, type, findit);
    if (def == NULL || def->def_kind != DEF_TYPEDEF) {
	return (orig);
    }
    switch (def->def.ty.rel) {
    case REL_VECTOR:
	return (def->def.ty.old_type);
    case REL_ALIAS:
	return (fixit(def->def.ty.old_type, orig));
    default:
	return (orig);
    }
}

char *
fixtype(char *type)
{
    return (fixit(type, type));
}

char *
stringfix(char *type)
{
    if (streq(type, "string")) {
	return ("wrapstring");
    } else {
	return (type);
    }
}

void
ptype(char *prefix, char *type, int follow)
{
    if (prefix != NULL) {
	if (streq(prefix, "enum")) {
	    f_print(fout, "enum ");
	} else {
	    f_print(fout, "struct ");
	}
    }
    if (streq(type, "bool")) {
	f_print(fout, "bool_t ");
    } else if (streq(type, "string")) {
	f_print(fout, "char *");
    } else {
	f_print(fout, "%s ", follow ? fixtype(type) : type);
    }
}


static int
typedefed(definition * def, char *type)
{
    if (def->def_kind != DEF_TYPEDEF || def->def.ty.old_prefix != NULL) {
	return (0);
    } else {
	return (streq(def->def_name, type));
    }
}

int
isvectordef(char *type, relation rel)
{
    definition *def;

    for (;;) {
	switch (rel) {
	case REL_VECTOR:
	    return (!streq(type, "string"));
	case REL_ARRAY:
	    return (0);
	case REL_POINTER:
	    return (0);
	case REL_ALIAS:
	    def = (definition *) FINDVAL(defined, type, typedefed);
	    if (def == NULL) {
		return (0);
	    }
	    type = def->def.ty.old_type;
	    rel = def->def.ty.rel;
	}
    }
}


static char *
locase(char *str)
{
    char c;
    static char buf[100];
    char *p = buf;

    while ((c = *str++)) {
	*p++ = (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
    }
    *p = 0;
    return (buf);
}


void
pvname(char *pname, char *vnum)
{
    f_print(fout, "%s_%s", locase(pname), vnum);
}


/*
 * print a useful (?) error message, and then die 
 */
void
error(char *msg)
{
    printwhere();
    f_print(stderr, "%s, line %d: ", infilename, linenum);
    f_print(stderr, "%s\n", msg);
    crash();
}

/*
 * Something went wrong, unlink any files that we may have created and then
 * die. 
 */
void
crash(void)
{
    int i;

    for (i = 0; i < nfiles; i++) {
	(void)unlink(outfiles[i]);
    }
    exit(1);
}


void
record_open(char *file)
{
    if (nfiles < NFILES) {
	outfiles[nfiles++] = file;
    } else {
	f_print(stderr, "too many files!\n");
	crash();
    }
}

/* buffer shared for all of the expected* routines */
static char expectbuf[100];

/*
 * error, token encountered was not the expected one 
 */
void
expected1(tok_kind exp1)
{
    s_print(expectbuf, "expected '%s'", toktostr(exp1));
    error(expectbuf);
}

/*
 * error, token encountered was not one of two expected ones 
 */
void
expected2(tok_kind exp1, tok_kind exp2)
{
    s_print(expectbuf, "expected '%s' or '%s'", toktostr(exp1),
	    toktostr(exp2));
    error(expectbuf);
}

/*
 * error, token encountered was not one of 3 expected ones 
 */
void
expected3(tok_kind exp1, tok_kind exp2, tok_kind exp3)
{
    s_print(expectbuf, "expected '%s', '%s' or '%s'", toktostr(exp1),
	    toktostr(exp2), toktostr(exp3));
    error(expectbuf);
}


/*
 * error, token encountered was not one of 4 expected ones
 */
void
expected4(tok_kind exp1, tok_kind exp2, tok_kind exp3, tok_kind exp4)
{
    sprintf(expectbuf, "expected '%s', '%s', '%s', or '%s'", toktostr(exp1),
	    toktostr(exp2), toktostr(exp3), toktostr(exp4));
    error(expectbuf);
}

void
tabify(FILE * f, int tab)
{
    if (scan_print)
	while (tab--) {
	    (void)fputc('\t', f);
	}
}

static token tokstrings[] = {
    {TOK_IDENT, "identifier"},
    {TOK_CONST, "const"},
    {TOK_RPAREN, ")"},
    {TOK_LPAREN, "("},
    {TOK_RBRACE, "}"},
    {TOK_LBRACE, "{"},
    {TOK_LBRACKET, "["},
    {TOK_RBRACKET, "]"},
    {TOK_STAR, "*"},
    {TOK_COMMA, ","},
    {TOK_EQUAL, "="},
    {TOK_COLON, ":"},
    {TOK_SEMICOLON, ";"},
    {TOK_UNION, "union"},
    {TOK_STRUCT, "struct"},
    {TOK_SWITCH, "switch"},
    {TOK_CASE, "case"},
    {TOK_DEFAULT, "default"},
    {TOK_ENUM, "enum"},
    {TOK_TYPEDEF, "typedef"},
    {TOK_INT, "int"},
    {TOK_SHORT, "short"},
    {TOK_INT32, "afs_int32"},	/* XXX */
    {TOK_UNSIGNED, "unsigned"},
    {TOK_DOUBLE, "double"},
    {TOK_FLOAT, "float"},
    {TOK_CHAR, "char"},
    {TOK_STRING, "string"},
    {TOK_OPAQUE, "opaque"},
    {TOK_BOOL, "bool"},
    {TOK_VOID, "void"},
    {TOK_PROGRAM, "program"},
    {TOK_VERSION, "version"},
    {TOK_PACKAGE, "package"},
    {TOK_PREFIX, "prefix"},
    {TOK_STATINDEX, "statindex"},
    {TOK_SPECIAL, "special"},
    {TOK_STARTINGOPCODE, "startingopcode"},
    {TOK_CUSTOMIZED, "customized"},
    {TOK_PROC, "proc"},
    {TOK_SPLITPREFIX, "splitprefix"},
    {TOK_SPLIT, "split"},
    {TOK_MULTI, "multi"},
    {TOK_IN, "IN"},
    {TOK_OUT, "OUT"},
    {TOK_INOUT, "INOUT"},
    {TOK_AFSUUID, "afsUUID"},
    {TOK_EOF, "??????"}
};

static char *
toktostr(tok_kind kind)
{
    token *sp;

    for (sp = tokstrings; sp->kind != TOK_EOF && sp->kind != kind; sp++);
    return (sp->str);
}



static void
printbuf(void)
{
    char c;
    int i;
    int cnt;

#	define TABSIZE 4

    for (i = 0; (c = curline[i]); i++) {
	if (c == '\t') {
	    cnt = 8 - (i % TABSIZE);
	    c = ' ';
	} else {
	    cnt = 1;
	}
	while (cnt--) {
	    fputc(c, stderr);
	}
    }
}


static void
printwhere(void)
{
    int i;
    char c;
    int cnt;

    printbuf();
    for (i = 0; i < where - curline; i++) {
	c = curline[i];
	if (c == '\t') {
	    cnt = 8 - (i % TABSIZE);
	} else {
	    cnt = 1;
	}
	while (cnt--) {
	    fputc('^', stderr);
	}
    }
    fputc('\n', stderr);
}
