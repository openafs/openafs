
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

void 
sprint(char *s)
{
    printf("sprint - %s", s);
}

void 
allprint(int i)
{
    printf("allprint - %d\n", i);
}
