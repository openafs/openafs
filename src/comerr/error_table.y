%{
#include <afsconfig.h>
#include <afs/param.h>
#include "compiler.h"

RCSID("$Header$");

/*
 *
 * Copyright 1986, 1987 by the MIT Student Information Processing Board
 *
 * For copyright info, see mit-sipb-cr.h.
 */
#ifndef AFS_NT40_ENV
#include <unistd.h>
#endif
#include <string.h>
#include <assert.h>
#include <ctype.h>
#ifdef AFS_NT40_ENV
#include <sys/types.h>
#include <afs/afsutil.h>
#else
#include <sys/time.h>
#endif
#include <sys/timeb.h>
#include "error_table.h"
#include "mit-sipb-cr.h"

/*
 * If __STDC__ is defined, function prototypes in the SunOS 5.5.1 lex
 * and yacc templates are visible.  We turn this on explicitly on
 * NT because the prototypes help supress certain warning from the
 * Microsoft C compiler.
 */

#ifdef AFS_NT40_ENV
#include <malloc.h>
# ifndef __STDC__
#  define __STDC__ 1
# endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

int char_to_num(char c);
char *gensym(const char *x);
char *current_token = (char *)NULL;
extern char *table_name;

char *ds(const char *string);
char *quote(const char *string);
void set_table_1num(char *string);
int char_to_1num(char c);
void add_ec(const char *name, const char *description);
void add_ec_val(const char *name, const char *val, const char *description);
void put_ecs(void);
void set_table_num(char *string);
void set_table_fun(char *astring);
void set_id(const char *, const char *);
void set_base(const char *);
void set_prefix(const char *);
void add_index(const char *);

%}
%union {
	char *dynstr;
}

%token ERROR_TABLE ERROR_CODE_ENTRY END INDEX PREFIX ID BASE NL
%token <dynstr> STRING QUOTED_STRING NUMBER
%type <dynstr> ec_name description table_id table_fun header
%{
%}
%start error_tables
%%

error_tables	: | error_tables error_table ;

error_table	:	maybe_blank_lines
			ERROR_TABLE header maybe_blank_lines
			error_codes END maybe_blank_lines
			{ table_name = ds($3);
			  current_token = table_name;
			  put_ecs(); }
		;

maybe_blank_lines : | maybe_blank_lines NL ;

header          :       table_fun table_id
                        { current_token = $1;
                          $$ = $2; }
                |       table_id
                        { current_token = $1;
                          set_table_fun(ds("1"));
                          $$ = $1;
                        }
                ;

table_fun       :       NUMBER
                        { current_token = $1;
                          set_table_fun($1);
                          $$ = $1; }
                ;


table_id	:	STRING
			{ current_token = $1;
			  set_table_num($1);
			  $$ = $1; }
		;

error_codes	:	error_codes ec_entry NL
		|	ec_entry NL
		;

ec_entry	:	ERROR_CODE_ENTRY ec_name ',' description
			{ add_ec($2, $4);
			  free($2);
			  free($4); }
		|	ERROR_CODE_ENTRY ec_name '=' STRING ',' description
			{ add_ec_val($2, $4, $6);
			  free($2);
			  free($4);
			  free($6);
			}
		|	INDEX NUMBER
			{ add_index($2);
			  free($2);
			}
		|	PREFIX STRING
			{ set_prefix($2);
			  free($2);
			}
		/*
		|	ID STRING STRING
			{ set_id($2, $3);
			  free($2);
			  free($3);
			}
		*/
		|	BASE NUMBER
			{ set_base($2);
			  free($2);
			}
		|
		;

ec_name		:	STRING
			{ $$ = ds($1);
			  current_token = $$; }
		;

description	:	QUOTED_STRING
			{ $$ = ds($1);
			  current_token = $$; }
		;

%%

/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern FILE *hfile, *cfile, *msfile;
extern int use_msf;
char st_prefix[] = "";
char *prefix = st_prefix;

static afs_int32 gensym_n = 0;

char *gensym(const char *x)
{
	char *symbol;
	if (!gensym_n) {
		struct timeval tv;
		gettimeofday(&tv, (void *)0);
		gensym_n = (tv.tv_sec%10000)*100 + tv.tv_usec/10000;
	}
	symbol = (char *)malloc(32 * sizeof(char));
	gensym_n++;
	sprintf(symbol, "et%ld", (long int) gensym_n);
	return(symbol);
}

char *
ds(const char *string)
{
	char *rv;
	rv = (char *)malloc(strlen(prefix)+strlen(string)+1);
	strcpy(rv, prefix);
	strcat(rv, string);
	return(rv);
}

char *
quote(const char *string)
{
	char *rv;
	rv = (char *)malloc(strlen(string)+3);
	strcpy(rv, "\"");
	strcat(rv, string);
	strcat(rv, "\"");
	return(rv);
}

afs_int32 table_number = 0;
int current = 0;
char **error_codes = (char **)NULL;

void add_ec(const char *name, const char *description)
{
        if (msfile) {
            if (current > 0)
#ifndef sun
                fprintf(msfile, "%d\t%s\n", current, description);
#else
                fprintf(msfile, "%d %s\n", current, description);
#endif /* !sun */
        } else {
	    fprintf(cfile, "\t\"%s\",\n", description);
	}
	if (error_codes == (char **)NULL) {
		error_codes = (char **)malloc(sizeof(char *));
		*error_codes = (char *)NULL;
	}
	error_codes = (char **)realloc((char *)error_codes,
				       (current + 2)*sizeof(char *));
	error_codes[current++] = ds(name);
	error_codes[current] = (char *)NULL;
}

void add_ec_val(const char *name, const char *val, const char *description)
{
	const int ncurrent = atoi(val);
	if (ncurrent < current) {
		fprintf(stderr,"Error code %s (%d) out of order", name,
		       current);
		return;
	}
      
	while (ncurrent > current) {
	     if (!msfile)
		 fputs("\t(char *)0,\n", cfile);
	     current++;
	 }
        if (msfile) {
            if (current > 0)
#ifndef sun
                fprintf(msfile, "%d\t%s\n", current, description);
#else
                fprintf(msfile, "%d %s\n", current, description);
#endif /* ! sun */
        } else {	
	    fprintf(cfile, "\t\"%s\",\n", description);
	}
	if (error_codes == (char **)NULL) {
		error_codes = (char **)malloc(sizeof(char *));
		*error_codes = (char *)NULL;
	}
	error_codes = (char **)realloc((char *)error_codes,
				       (current + 2)*sizeof(char *));
	error_codes[current++] = ds(name);
	error_codes[current] = (char *)NULL;
} 

void add_index(const char *val)
{
	const int ncurrent = atoi(val);
	if (ncurrent < current) {
		fprintf(stderr,"Index (%d) attempt to go backwards",
		       current);
		return;
	}
      
	if (error_codes == (char **)NULL) {
		error_codes = (char **)malloc((ncurrent + 1) * sizeof(char *));
	} else {
		error_codes = (char **)realloc((char *)error_codes,
				       (ncurrent + 1)*sizeof(char *));
	}
	while (ncurrent > current) {
	     if (!msfile)
		 fputs("\t(char *)0,\n", cfile);
	     error_codes[current++] = (char *)NULL;
	 }
}

/*
void set_id(const char *id, const char *text)
{
}
*/

void set_prefix(const char *val)
{
	char *new;

	if (*val) {
		new = malloc(strlen(val)+2);
		strcpy(new, val);
		strcat(new, "_");
	} else {
		new = malloc(1);
		*new = 0;
	}
	if (prefix != st_prefix) {
		free(prefix);
	}
	prefix = new;
}

void set_base(const char *base)
{
	table_number = atoi(base);
}

void put_ecs(void)
{
	int i;
	for (i = 0; i < current; i++) {
	     if (error_codes[i] != (char *)NULL)
		  fprintf(hfile, "#define %-40s (%ldL)\n",
			  error_codes[i], (long int) table_number + i);
	}
	put_ecs_postlog();
}

/*
 * char_to_num -- maps letters and numbers into a small numbering space
 * 	uppercase ->  1-26
 *	lowercase -> 27-52
 *	digits    -> 53-62
 *	underscore-> 63
 */

static const char char_set[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";

int char_to_num(char c)
{
	const char *where;
	int diff;

	where = strchr (char_set, c);
	if (where) {
		diff = where - char_set + 1;
		assert (diff < (1 << ERRCODE_RANGE));
		return diff;
	}
	else if (isprint (c))
		fprintf (stderr,
			 "Illegal character `%c' in error table name\n",
			 c);
	else
		fprintf (stderr,
			 "Illegal character %03o in error table name\n",
			 c);
	exit (1);
}

void set_table_num(char *string)
{
        if (msfile) {
	    set_table_1num(string);
	    return;
	}
	if (strlen(string) > 4) {
		fprintf(stderr, "Table name %s too long, truncated ",
			string);
		string[4] = '\0';
		fprintf(stderr, "to %s\n", string);
	}
	if (char_to_num (string[0]) > char_to_num ('z')) {
		fprintf (stderr, "%s%s%s%s",
			 "First character of error table name must be ",
			 "a letter; name ``",
			 string, "'' rejected\n");
		exit (1);
	}
	while (*string != '\0') {
		table_number = (table_number << BITS_PER_CHAR)
			+ char_to_num(*string);
		string++;
	}
	table_number = table_number << ERRCODE_RANGE;
	current = 0;
	if (error_codes)
		error_codes[current] = (char *)NULL;
        if (!msfile) {
	    fprintf(cfile,
		"static const char * const text%s[] = {\n",
		cot_suffix(count_of_tables));	/* } */
	}
}

void set_table_fun(char *astring)
{
    register char *tp;
    unsigned int tc;

    for(tp=astring; (tc = *tp) != 0; tp++) {
        if (!isdigit(tc)) {
            fprintf(stderr, "Table function '%s' must be a decimal integer.\n",
                    astring);
            exit(1);
        }
    }
    if (msfile) 
	table_number += (atoi(astring)) << 28;
}

/* for compatibility with old comerr's, we truncate package name to 4
 * characters, but only store first 3 in the error code.  Note that this
 * function, as a side effect, truncates the table name down to 4 chars.
 */
void set_table_1num(char *string)
{
        afs_int32 temp;
        int ctr;

        if ((temp = strlen(string)) > 4) {
                fprintf(stderr, "Table name %s too long, truncated ",
                        string);
                string[4] = '\0';
                fprintf(stderr, "to %s\n", string);
        }
        if (temp == 4) {
            fprintf(stderr, "Table name %s too long, only 3 characters fit in error code.\n",
                    string);
        }
        if (char_to_1num (string[0]) > char_to_1num ('z')) {
                fprintf (stderr, "%s%s%s%s",
                         "First character of error table name must be ",
                         "a letter; name ``",
                         string, "'' rejected\n");
                exit (1);
        }
        temp = 0;
        for(ctr=0; ctr < 3; ctr++) {            /* copy at most 3 chars to integer */
            if (*string == '\0') break;         /* and watch for early end */
            temp = (temp * 050)                 /* "radix fifty" is base 050 = 40 */
                + char_to_1num(*string);
            string++;
        }
        table_number += temp << 12;
}

/*
 * char_to_num -- maps letters and numbers into very small space
 *      0-9        -> 0-9
 *      mixed case -> 10-35
 *      _          -> 36
 *      others are reserved
 */

static const char char_1set[] =
        "abcdefghijklmnopqrstuvwxyz_0123456789";

int char_to_1num(char c)
{
        const char *where;
        int diff;

        if (isupper(c)) c = tolower(c);

        where = strchr (char_1set, c);
        if (where) {
                /* start at 1 so we can decode */
                diff = where - char_1set;
                assert (diff < 050);    /* it is radix 50, after all */
                return diff;
        }
        else if (isprint (c))
                fprintf (stderr,
                         "Illegal character `%c' in error table name\n",
                         c);
        else
                fprintf (stderr,
                         "Illegal character %03o in error table name\n",
                         c);
        exit (1);
}

#ifdef AFS_NT40_ENV
#include "et_lex.lex_nt.c"
#else
#include "et_lex.lex.c"
#endif
