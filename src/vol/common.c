
#ifndef lint
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*

	System:		VICE-TWO
	Module:		common.c
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

#include <sys/types.h>
#include <stdio.h>
#include <afs/param.h>
#include <afs/afsutil.h>

int Statistics = 0;

/* VARARGS */
Log (a,b,c,d,e,f,g,h,i,j,k)
    char *a, *b, *c, *d, *e, *f, *g, *h, *i, *j, *k;
{
    int level;

    if (Statistics)
	level = -1;
    else
	level = 0;
    ViceLog(level,(a,b,c,d,e,f,g,h,i,j,k));
}

Abort(s,a,b,c,d,e,f,g,h,i,j) 
    char *s, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    ViceLog(0, ("Program aborted: "));
    ViceLog(0, (s,a,b,c,d,e,f,g,h,i,j));
    abort();
}

Quit(s,a,b,c,d,e,f,g,h,i,j) 
    char *s, *a, *b, *c, *d, *e, *f, *g, *h, *i, *j;
{
    ViceLog(0, (s,a,b,c,d,e,f,g,h,i,j));
    exit(1);
}
