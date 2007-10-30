%{
/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <stdio.h>
#include <string.h>

extern int line;
extern int uss_perr;
%}

%union
{
   int ival;
  char chval;
  char strval[1000];
}

%token  EOL_TKN
%token  DIR_TKN
%token FILE_TKN
%token ECHO_TKN
%token EXEC_TKN
%token LINK_TKN
%token SYMLINK_TKN
%token VOL_TKN
%token GROUP_TKN
%token AUTH_TKN
%token <strval> STRING_TKN
%token VOL1_TKN
%type <ival> input
%type <ival> entry
%type <strval> accesslist

%%	/* rules */

input	:	/* empty */
			{ $$ = 0;}
	|	input entry
			{ $$ = ($1 == 0)? $2 : $1;}
	;

entry	:	DIR_TKN
		STRING_TKN	/*2-directory name*/
		STRING_TKN	/*3-mode*/
		STRING_TKN	/*4-owner*/
		accesslist	/*5-access list*/
			{$$ = uss_perr = uss_procs_BuildDir($2,$3,$4,$5);}
	|	FILE_TKN
		STRING_TKN	/*2-filename*/
		STRING_TKN	/*3-mode*/
		STRING_TKN	/*4-owner*/
		STRING_TKN	/*5-rototype*/
			{$$ = uss_perr = uss_procs_CpFile($2, $3, $4, $5);}
	|	ECHO_TKN
		STRING_TKN	/*2-filename*/
		STRING_TKN	/*3-mode*/
		STRING_TKN	/*4-owner*/
		STRING_TKN	/*5-file content*/
			{$$ = uss_perr = uss_procs_EchoToFile($2, $3, $4, $5);}
	|	EXEC_TKN
		STRING_TKN	/*2-command string*/
			{$$ = uss_perr = uss_procs_Exec($2);}
	|	LINK_TKN
		STRING_TKN	/*2-filename1*/
		STRING_TKN	/*3-filename2*/
			{$$ = uss_perr = uss_procs_SetLink($2, $3,'h');}

	|	SYMLINK_TKN
		STRING_TKN	/*2-filename1*/
		STRING_TKN	/*3-filename2*/
			{$$ = uss_perr = uss_procs_SetLink($2, $3,'s');}
	|	VOL_TKN
		STRING_TKN	/*2-vol name*/
		STRING_TKN	/*3-server*/
		STRING_TKN	/*4-partition*/
		STRING_TKN	/*5-quota*/
		STRING_TKN	/*6-Mount point*/
		STRING_TKN	/*7-Owner*/
		accesslist	/*8-access list*/
			{$$ = uss_perr = uss_vol_CreateVol($2, $3, $4, $5, $6, $7, $8);}
	|	GROUP_TKN
		STRING_TKN	/*2-declared dir*/
			{$$ = uss_perr = uss_procs_AddToDirPool($2);}
	|	AUTH_TKN
		STRING_TKN	/*2-user name*/
		STRING_TKN	/*3-password lifetime (days<255)*/
                STRING_TKN      /*4-reuse/noreuse */
                STRING_TKN      /*5-failed login attempts */
                STRING_TKN      /*6-lockout time */
			{$$ = uss_perr = uss_kauth_SetFields($2, $3, $4, $5, $6);}
	|	VOL1_TKN
		STRING_TKN	/*2-vol name*/
		STRING_TKN	/*3-server*/
		STRING_TKN	/*4-partition*/
		STRING_TKN	/*5-quota*/
		STRING_TKN	/*6-Mount point*/
		STRING_TKN	/*7-Owner*/
		STRING_TKN	/*8-access list*/
			{$$ = uss_perr = uss_vol_CreateVol($2, $3, $4, $5, $6, $7, $8);}
	|	EOL_TKN	/*End of line */
			{$$=0;}
	|	error entry
		    {uss_procs_PrintErr(line-1, " near \"%s\"\n",yylval.strval);}
	;


accesslist :	/* empty */
			{strcpy($$," ");}
	|	STRING_TKN
		STRING_TKN
		accesslist
			{strcat($1," "); strcat($2," ");strcat($1,strcat($2,$3));strcpy($$,$1);}
		
	;

%%
yyerror(s)
char *s;
{
fprintf(stderr,"%s. ",s);
}
