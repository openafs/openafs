
#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "globals.h"
#include "package.h"

char *emalloc();


yyerror()
{
}

yywrap()
{
return 1;
}

sprint(s)
char *s;
{
    printf("sprint - %s", s);
}

allprint(i)
int i;
{
    printf("allprint - %d\n",i);
}

