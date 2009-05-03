/*
 * Copyright (c) 2006
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

/*
[	start a top level section name
{	start contents of a section
=	define a relation
"	start a quoted string
\b \n \t
*	finalize a section or subsection

[name]
	foo=bar
	test={
		try1=x
		try2="\tstring"
	}
[another]*
	x=6
	y={
		asd=5
	}*

; or # == start a comment.

numbers: strtol conversion (0x or 0...)

boolean:	y yes true t 1 on
		n no false nil 0 off
heimdal only understands yes true, or numbers that are not 0 (using atoi).

name
subname
subsubname
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define afs_osi_Alloc(n)	malloc(n)
#define afs_osi_Free(p,n)	free(p)
#define afs_strcasecmp(p,q)	strcasecmp(p,q)
#include <sys/types.h>
#include <errno.h>
#include "k5ssl.h"
#include "k5s_im.h"

struct _krb5_profile_element {
    struct _krb5_profile_element *next, *list;
    char *value, name[1];
};

int
krb5i_config_lookup(krb5_profile_element **root, const char *const *names, int (*f)(), void *a)
{
    int i, r;
    krb5_profile_element **pp, *np;

    pp = root;
    np = 0;
    for (i = 0; names[i]; ++i) {
Next:
	for ( ; (np = *pp); pp = &np->next)
	    if (strcmp(np->name, names[i]))
		continue;
	    else if (names[i+1]) {
		if (!np->list) continue;
		break;
	    } else {
		if (!f || np->list) continue;
		if ((r = (*f)(np->value, a))) {
		    if (r != PROFILE_GO_AWAY) return r;
		    *pp = np->next;
		    np->next = 0;
		    krb5i_free_profile(np);
		    goto Next;
		}
		continue;
	    }
	if (np)
	    ;
	else if (f)
	    break;
	else {
	    np = (krb5_profile_element *) malloc(sizeof *np
		+ (r = strlen(names[i])));
	    if (!np) break;
	    memset(np, 0, sizeof *np);
	    memcpy(np->name, names[i], (r+1));
	    np->next = *pp;
	    *pp = np;
	}
	pp = &np->list;
    }
    if (!f && np)
	np->value = strdup((char *) a);
    return 0;
}

struct find_names_arg {
    char **values;
    int i;
    int max;
};

static int
find_names_helper(char *value, void *arg)
{
    struct find_names_arg *fnsa = (struct find_names_arg *) arg;
    char **x;
    if (fnsa->i+1 >= fnsa->max) {
	x = malloc(sizeof *x * (fnsa->max + 5));
	if (!x) return ENOMEM;
	fnsa->max += 5;
	memcpy(x, fnsa->values, sizeof *x * fnsa->i);
	free(fnsa->values);
	fnsa->values = x;
    }
    fnsa->values[fnsa->i++] = value;
    return 0;
}

int
krb5i_find_names(krb5_profile_element *root, const char *const *names, char ***values)
{
    struct find_names_arg fnsa[1];
    int r;
    memset(fnsa, 0, sizeof *fnsa);
    fnsa->values = malloc(sizeof(char *) * (fnsa->max = 5));
    r = ENOMEM; if (!fnsa->values) goto Done;
    r = krb5i_config_lookup(&root, names, find_names_helper, fnsa);
    fnsa->values[fnsa->i] = 0;
Done:
    if (r) {
	if (fnsa->values) free(fnsa->values);
    } else {
	*values = fnsa->values;
    }
    return r;
}

int
krb5i_add_name_value(krb5_profile_element **root,
    const char *value,
    const char *const *names)
{
    return krb5i_config_lookup(root, names, 0, value);
}

int
krb5i_make_final(krb5_profile_element **root, const char *const *names)
{
#if 0
    int i;
    if (vflag) {
	for (i = 0; names[i]; ++i)
	    printf (":%s"+!i, names[i]);
	printf (" FINAL\n");
    }
#endif
    return 0;
}

static int
visit_helper(char **names, char **npp,
    krb5_profile_element *np,
    int (*f)(), void *arg)
{
    int r;

    if (!np) return 0;
    for (;np; np = np->next) {
	npp[0] = np->name;
	npp[1] = 0;
	if (!np->value)
	    ;
	else if ((r = (*f)(names, np->value, arg)))
	    return r;
	r = visit_helper(names, npp+1, np->list, f, arg);
	if (r) return r;
    }
    return 0;
}

int
krb5i_visit_nodes(krb5_profile_element *root, int (*f)(), void *arg)
{
    char *names[50];
    return visit_helper(names, names, root, f, arg);
}

void
krb5i_free_profile(krb5_profile_element *profile)
{
	krb5_profile_element *e, *f, **g, *list;

	list = profile;
	while ((e = list)) {
		list = e->next;
		if ((f = e->list)) {
			for (g = &f->next; *g; g = &(*g)->next)
				;
			*g = list;
			list = f;
		}
		if (e->value) free(e->value);
		free(e);
	}
}

int
krb5i_parse_profile(char *fn, krb5_profile_element **out)
{
    FILE *fp;
    int c, delim, state, a;
#define WSIZE 256
#define SSIZE 8
    char temp[WSIZE], *cp, *tp, *name, *value, *ep;
    const char *names[SSIZE];
    int si;
    int code;
    krb5_profile_element *root = 0;

    tp = temp;
    state = si = 0;
    ep = cp = value = name = 0;
    c = 0;
    delim = '\n';
#define FIRST 1
#define COMMENT 2
#define QUOTE 4

    *out = 0;
    fp = fopen(fn, "r");

    if (!fp) return errno ? errno : EDOM;
    temp[WSIZE-1] = 0;

    for (a = 0; a != EOF; ) {
	if (!c) c = getc(fp);
	a = 0;
	if (c == EOF)
	    ;
	else if (!(state & FIRST)) {
	    if (c != '[' || delim != '\n') {
		delim = c;
		c = 0;
		continue;
	    }
	    delim = 0;
	    state |= FIRST;
	} else if (state & COMMENT) {
	    if (c == '\n')
		state &= ~COMMENT;
	    c = 0;
	    continue;
	}
	else if (delim) {
	    if (state & QUOTE) switch(state &= ~QUOTE, c) {
	    case '\n': c = 0; break;
	    case 'b': c = '\b'; break;
	    case 't': c = '\t'; break;
	    case 'n': c = '\n'; break;
	    } else switch(c) {
	    case '\\':
		if (delim != '\"') break;
		state |= QUOTE;
		c = 0;
		continue;
	    default:
		if (c != delim) break;
		/* fall through */
	    case '\n':
		while (cp-1 > tp && isspace(cp[-1]))
		    --cp;
		c = 0;
		break;
	    }
	    if (c) {
		if (cp < temp+WSIZE-1) *cp++ = c;
		c = 0;
		continue;
	    }
	}
	switch(c) {
	case '[' /* ] */: case '\"':
	case '*': case '=': case '{': case '}':
	case '#': case ';':
	    if (name && cp) break;
	case EOF:
	    a = c;
	    break;
	default:
	    if (!isspace(c)) break;
	    if (name && cp) {
		if (cp < temp+WSIZE-1) *cp++ = c;
		c = 0;
		continue;
	    }
	    /* fall through */
	case 0:
	case '\n':
	    a = ' ';
	}
	if (a) c = 0;
	if (cp && a) {
	    if (cp < temp+WSIZE-1)
		*cp++ = 0;
	    if (delim == /* [ */ ']') {
		names[si++] = temp;
		tp = cp;
	    }
	    else if (name) {
		if (ep) *ep = 0;
		names[si] = name;
		names[si+1] = 0;
		krb5i_add_name_value(&root, value, names);
		tp = name;
		name = 0;
	    }
	    cp = 0;
	    delim = 0;
	}
	switch(a) {
	case '#': case ';':
	    state |= COMMENT;
	    break;
	case '[' /* ] */:
	    si = 0;
	    cp = tp = temp;
	    delim = /* [ */ ']';
	    break;
	case '\"':
	    if (!cp) value = cp = tp;
	    delim = '\"';
	    break;
	case '*':
	    if (si) {
		names[si] = 0;
		krb5i_make_final(&root, names);
	    }
	    break;
	case ' ':
	    break;
	case '=':
	    if (value) {
		name = value;
		tp = value + strlen(value) + 1;
	    }
	    break;
	case '{':
	    if (name && si < SSIZE-1) {
		names[si++] = name;
		tp = name + strlen(name)+1;
		name = 0;
	    }
	    c = 0;
	    break;
	case '}':
	    if (si > 1)
		tp = (char *) names[--si];
	    break;
	case 0:
	    if (!cp) value = cp = tp;
	    if (cp < temp+WSIZE-1) *cp++ = c;
	    ep = cp;
	}
	c = 0;
    }
    code = 0;
    fclose(fp);
    if (code) {
	krb5i_free_profile(root);
    } else {
	*out = root;
    }
    return code;
}
