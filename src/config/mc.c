/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define	MAXLINELEN	1024
#define	MAXTOKLEN	100
#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>

#if  defined(__alpha)
extern void *malloc(int size);
#endif

#define TOK_DONTUSE 1 /* Don't copy if match and this flag is set. */
struct token {
    struct token *next;
    char *key;
    int flags;
};

/* free token list returned by parseLine */
static FreeTokens(alist)
    register struct token *alist; {
    register struct token *nlist;
    for(; alist; alist = nlist) {
	nlist = alist->next;
	free(alist->key);
	free(alist);
    }
    return 0;
}

#define	space(x)    ((x) == ' ' || (x) == '\t' || (x) == '<' || (x) == '>')
static ParseLine(aline, alist)
    char *aline;
    struct token **alist; {
    char tbuffer[MAXTOKLEN+1];
    register char *tptr;
    int inToken;
    struct token *first, *last;
    register struct token *ttok;
    register int tc;
    int dontUse = 0;
    
    inToken = 0;	/* not copying token chars at start */
    first = (struct token *) 0;
    last = (struct token *) 0;
    while (1) {
	tc = *aline++;
	if (tc == 0 || space(tc)) {    /* terminating null gets us in here, too */
	    if (inToken) {
		inToken	= 0;	/* end of this token */
		*tptr++ = 0;
		ttok = (struct token *) malloc(sizeof(struct token));
		ttok->next = (struct token *) 0;
		if (dontUse) {
		    ttok->key = (char *) malloc(strlen(tbuffer));
		    strcpy(ttok->key, tbuffer+1);
		    ttok->flags = TOK_DONTUSE;
		    dontUse = 0;
		}
		else {
		    ttok->key = (char *) malloc(strlen(tbuffer)+1);
		    strcpy(ttok->key, tbuffer);
		    ttok->flags = 0;
		}
		if (last) {
		    last->next = ttok;
		    last = ttok;
		}
		else last = ttok;
		if (!first) first = ttok;
	    }
	}
	else {
	    /* an alpha character */
	    if (!inToken) {
		if (tc == '-') {
		    dontUse = 1;
		}
		tptr = tbuffer;
		inToken = 1;
	    }
	    if (tptr - tbuffer >= MAXTOKLEN) return -1;   /* token too long */
	    *tptr++ = tc;
	}
	if (tc == 0) {
	    /* last token flushed 'cause space(0) --> true */
	    if (last) last->next = (struct token *) 0;
	    *alist = first;
	    return 0;
	}
    }
}
/* read a line into a buffer, putting in null termination and stopping on appropriate
    end of line char.  Returns 0 at eof, > 0 at normal line end, and < 0 on error */
static GetLine(afile, abuffer, amax)
    FILE *afile;
    int amax;
    register char *abuffer; {
    register int tc;
    int first;

    first = 1;
    while (1) {
	tc = getc(afile);
	if (first && tc < 0) return 0;
	first = 0;
	if (tc <= 0 || tc == '\012') {
	    if (amax > 0) *abuffer++ = 0;
	    return (amax > 0? 1 : -1);
	}
	if (amax > 0) {
	    /* keep reading to end of line so next one isn't bogus */
	    *abuffer++ = tc;
	    amax--;
	}
    }
}
mc_copy(ain, aout, alist)
    register FILE *ain;
    register FILE *aout;
    char *alist[]; {
    char tbuffer[MAXLINELEN];
    struct token *tokens;
    register char **tp;
    register struct token *tt;
    register int code;
    int copying;
    int done;

    copying = 1;	/* start off copying data */
    while (1) {
	/* copy lines, handling modes appropriately */
	code = GetLine(ain, tbuffer, MAXLINELEN);
	if (code <= 0) break;
	/* otherwise process the line */
	if (tbuffer[0] == '<') {
	    /* interpret the line as a set of options, any one of which will cause us
		to start copying the data again. */
	    code = ParseLine(tbuffer, &tokens);
	    if (code != 0) return -1;
	    copying = 0;
	    done = 0;
	    for(tp = alist; (!done) && (*tp != (char *)0) ; tp++) {
		for(tt = tokens; tt; tt=tt->next) {
		    if (!strcmp(*tp, tt->key)) {
			/* Need to search all tokens in case a dont use
			 * flag is set. But we can stop on the first
			 * don't use.
			 */
			if (tt->flags & TOK_DONTUSE) {
			    copying = 0;
			    done = 1;
			    break;
			}
			else {
			    copying = 1;
			}
		    }
		}
	    }
	    FreeTokens(tokens);
	}
	else {
	    /* just copy the line */
	    if (copying) {
		fwrite(tbuffer, 1, strlen(tbuffer), aout);
		putc('\n', aout);
	    }
	}
    }
    return 0;
}
