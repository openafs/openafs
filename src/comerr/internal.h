/*
 * internal include file for afs_com_err package
 *
 */
#include "mit-sipb-cr.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>		/* perror() */

extern void yyerror(const char *s);
extern char *xmalloc(unsigned int size);
