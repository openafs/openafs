/*
 * (C) Copyright Transarc Corporation 1989
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 */

/*------------------------------------------------------------------------
 * messages.c
 *
 * Description:
 *	Getting messages printed to the outside world.
 *
 * Author:
 *	Transarc Corporation & Carnegie Mellon University
 *------------------------------------------------------------------------*/

#ifdef	lint
/* VARARGS1 */fatal(cp) char *cp; /*ARGSUSED*/ {}
/* VARARGS1 */message(cp) char *cp; /*ARGSUSED*/ {}
/* VARARGS1 */loudonly_message(cp) char *cp; /*ARGSUSED*/ {}
/* VARARGS1 */verbose_message(cp) char *cp; /*ARGSUSED*/ {}
#else /* lint */

#include <afs/param.h>
#include <sys/types.h>
#ifdef	AFS_SUN5_ENV
/* XXX Hack for the va_arg decl below XXX */
#define lint
#endif
#include <varargs.h>
#include "package.h"

static char *putnum(dp, n, b)
    register char *dp;
    register unsigned n;
    register int b;

{ /*putnum*/
    register int s;

    for (s = b; n / s; s *= b)
      continue;
    s /= b;
    while (s > 0) {
      *dp++ = '0' + (n / s);
      n %= s;
      s /= b;
    }

    return(dp);

} /*putnum*/

static char *putstr(dp, s)
    register char *dp;
    register char *s;

{ /*putstr*/

    while (*s)
      *dp++ = *s++;

    return(dp);

} /*putstr*/

static char *putformat(dp, fp, ap)
    register char *dp;
    register char *fp;
    register va_list ap;

{ /*putformat*/

    while (*fp) {
      if (*fp == '%') {
	switch (*++fp) {
	case 'c':
	  {
	    char c;

	    c = va_arg(ap,int);
	    *dp++ = c;
	    fp++;
	    break;
	  }
	case 'd':
	  {
	    int	d;

	    d = va_arg(ap,int);
	    if (d < 0) {
	      *dp++ = '-';
	      d = -d;
	    }
	    dp = putnum(dp,(unsigned)d,10);
	    fp++;
	    break;
	  }
	case 'm':
	  {
	    extern int errno;
	    extern int sys_nerr;
	    extern char *sys_errlist[];

	    if (errno >= 0 && errno < sys_nerr)
	      dp = putstr(dp,sys_errlist[errno]);
	    else {
	      dp = putstr(dp,"Unknown error (errorno =");
	      dp = putnum(dp,(unsigned)errno,10);
	      dp = putstr(dp,")");
	    }
	    fp++;
	    break;
	  }
	case 'o':
	  {
	    unsigned o;

	    o = va_arg(ap,int);
	    dp = putnum(dp,o,8);
	    fp++;
	    break;
	  }
	case 's':
	  {
	    char *s;

	    s = va_arg(ap,char *);
	    dp = putstr(dp,s);
	    fp++;
	    break;
	  }
	case 'u':
	  {
	    unsigned u;

	    u = va_arg(ap,int);
	    dp = putnum(dp,u,10);
	    fp++;
	    break;
	  }
	case 'x':
	  {
	    unsigned x;

	    x = va_arg(ap,int);
	    dp = putnum(dp,x,16);
	    fp++;
	    break;
	  }
	}
	continue;
      }
      if (*fp == '\\')
	{
	  switch (*++fp) {
	  case '\\':
	    *dp++ = '\\';
	    fp++;
	    break;

	  case 'f':
	    *dp++ = '\f';
	    fp++;
	    break;

	  case 'n':
	    *dp++ = '\n';
	    fp++;
	    break;

	  case 'r':
	    *dp++ = '\r';
	    fp++;
	    break;

	  case 't':
	    *dp++ = '\t';
	    fp++;
	    break;

	  }
	  continue;
	}
      *dp++ = *fp++;
    }

    return(dp);

} /*putformat*/

#define	maxline	256

fatal(va_alist)
    va_dcl

{ /*fatal*/

    va_list ap;
    char *dp, *fp;
    char line[maxline];

    va_start(ap);
    fp = va_arg(ap,char *);
    dp = putformat(line,fp,ap);
    *dp++ = '\n';
    (void)write(2,line,dp-line);
    exit(status_error);

} /*fatal*/

message(va_alist)
    va_dcl

{ /*message*/

    va_list ap;
    char *dp, *fp;
    char line[maxline];

    va_start(ap);
    fp = va_arg(ap,char *);
    dp = putformat(line,fp,ap);
    *dp++ = '\n';
    (void)write(1,line,dp-line);

} /*message*/

loudonly_message(va_alist)
    va_dcl

{ /*loudonly_message*/

    va_list ap;
    char *dp, *fp;
    char line[maxline];

    if (!opt_silent) {
      va_start(ap);
      fp = va_arg(ap,char *);
      dp = putformat(line,fp,ap);
      *dp++ = '\n';
      (void)write(1,line,dp-line);
    }

} /*loudonly_message*/

verbose_message(va_alist)
    va_dcl

{ /*verbose_message*/

    va_list ap;
    char *dp, *fp;
    char line[maxline];

    if (opt_verbose) {
      va_start(ap);
      fp = va_arg(ap,char *);
      dp = putformat(line,fp,ap);
      *dp++ = '\n';
      (void)write(1,line,dp-line);
    }

}  /*verbose_message*/

debug_message(va_alist)
    va_dcl

{ /*debug_message*/

    va_list ap;
    char *dp, *fp;
    char line[maxline];

    if (opt_debug) {
      va_start(ap);
      fp = va_arg(ap,char *);
      dp = putformat(line,fp,ap);
      *dp++ = '\n';
      (void)write(1,line,dp-line);
    }

}  /*debug_message*/

#endif /* lint */
