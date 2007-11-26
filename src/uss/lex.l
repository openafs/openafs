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

RCSID("$Header$");


#include <string.h>

#include "y.tab.h"
#include "uss_common.h"
int line=1;
#ifdef DEBUG
#define dprint(x)	{fprintf(stderr, x); fflush(stderr);}
#else
#define dprint(x)
#endif
%}

/* definitions */
C	#[^\n]*
W	[ \t]+
L	[A-Za-z]
S	[\.A-Z0-9a-z/$][^ \t\n#=\^\!\|\(\)\{\};]*
Q	\"[^\"\n]*[\"\n]
INVAL	[^ADEFLSVGX# ]
EOL	[\n]

%%
{C}		{dprint(("got a comment\n"));}
^{EOL}		{dprint(("got an empty line\n")); line++;}
^{INVAL}	{uss_procs_PrintErr(line," Invalid command \n");}
^[D]{W}		{dprint(("got a Dir\n"));return(DIR_TKN);}
^[F]{W}		{dprint(("got a File\n"));return(FILE_TKN);}
^[L]{W}		{dprint(("got a Link\n"));return(LINK_TKN);}
^[S]{W}		{dprint(("got a Symlink\n"));return(SYMLINK_TKN);}
^[E]{W}		{dprint(("got an Echo\n"));return(ECHO_TKN);}
^[X]{W}		{dprint(("got an Exec\n"));return(EXEC_TKN);}
^[V]{W}		{dprint(("got a Vol\n"));return(VOL_TKN);}
^[G]{W}		{dprint(("got a Group Declaration\n"));return(GROUP_TKN);}
^[A]{W}		{dprint(("got an Auth\n"));return(AUTH_TKN);}
^[Y]{W}		{dprint(("got a Vol1\n"));return(VOL1_TKN);}
{S}		{dprint(("got a string(%s)\n", yytext));
		 Replace(yytext, yylval.strval);
		 return(STRING_TKN);
		}
{Q}		{dprint(("got a quote: '%s'\n", yytext));
		 Replace(yytext, yylval.strval);
		 return(STRING_TKN);
		}
{EOL}		{line++;
                 return(EOL_TKN);};

%%

/*
 * This routine copies the in buf to out and replaces every known
 * variable, e.g. $user, $1, ... by its value.  This value either
 * comes from main program, or the handling routine will figure it
 * out.  If given a quoted string, it ignores the first double quote
 * and replaces the second with a null.
 */

void
Replace(in, out)
    char *in, *out;

{ /*Replace*/

    char *in_text, *in_var, *out_cp, VarNo;
    int n;
    int isQuotedString;
    char *nullP;
    
    if(in[0] == '"') {
	/*
	 * Strip the opening quote, remember we're handling a
	 * quoted string
	 */
	in_text = in+1;
	isQuotedString = 1;
    }
    else {
	in_text = in;
	isQuotedString = 0;
    }
    out_cp = out;
    
    while ((in_var = strchr(in_text, '$')) != NULL) {
	while(in_text < in_var)
	    *out_cp++ = *in_text++;
	VarNo = *(in_var+1);
	if(VarNo >= '0' && VarNo <= '9') {
	    /*In the 0-9 range*/
	    n = VarNo - '0';
	    if (n == 0) {
		fprintf(stderr,
			"$0 is the program name.  Please start from $1.\n");
		exit(-1);
	    }
	    if (n > uss_VarMax){
		fprintf(stderr,
			"Illegal variable number ($%d is the largest acceptable)\n",
			uss_VarMax);
		exit(-1);
	    }
	    
	    strcpy(out_cp, uss_Var[n]);
	    out_cp += strlen(uss_Var[n]);
	    in_text += 2;
	}
	
	else if (strncmp(in_var, "$USER", 5) == 0) {
	    strcpy(out_cp, uss_User);
	    out_cp += strlen(uss_User);
	    in_text += 5;
	}
	
	else if (strncmp(in_var, "$UID", 4) == 0) {
	    strcpy(out_cp, uss_Uid);
	    out_cp += strlen(uss_Uid);
	    in_text += 4;
	}
	
	else if (strncmp(in_var, "$SERVER", 7) == 0) {
	    strcpy(out_cp, uss_Server);
	    out_cp += strlen(uss_Server);
	    in_text += 7;
	}
	
	else if (strncmp(in_var, "$PART", 5) == 0) {
	    strcpy(out_cp, uss_Partition);
	    out_cp += strlen(uss_Partition);
	    in_text += 5;
	}
	
 	else if (strncmp(in_var, "$MTPT", 5) == 0) {
	    strcpy(out_cp, uss_MountPoint);
	    out_cp += strlen(uss_MountPoint);
	    in_text += 5;
	}
	
	else if (strncmp(in_var, "$NAME", 5) == 0) {
	    strcpy(out_cp, uss_RealName);
	    out_cp += strlen(uss_RealName);
	    in_text += 5;
	}
	
	else if (strncmp(in_var, "$AUTO", 5) == 0) {
	    /*Picks a dir with minimum entries*/
	    uss_procs_PickADir(out, out_cp /*, uss_Auto*/);
	    printf("debug: $AUTO = %s\n", uss_Auto);
	    strcpy(out_cp, uss_Auto);
	    out_cp += strlen(uss_Auto);
	    in_text += 5;
	}
	else if (strncmp(in_var, "$PWEXPIRES", 10) == 0) {
	    sprintf(out_cp, " %d ", uss_Expires);
	    out_cp += strlen(out_cp);
	    in_text += 10;
	}
	
	else{
	    /*Unknown variable*/
	    fprintf(stderr,
		    "Warning: unknown variable in config file: '%s'\n",
		    in_var);
	    *out_cp++ = *in_text++;
	}
    }
    
    /*
     * At this point, we've copied over the in buffer up to the point
     * of the last variable instance, so copy over the rest. If this
     * is a quoted string, we place the terminating null where the
     * ending double quote is.
     */
    while(*in_text != '\0')
	*out_cp++ = *in_text++;
    
    if (isQuotedString) {
	nullP = strchr(out, '"');
	if (nullP == NULL)
	    nullP = out_cp;
    }
    else
	nullP = out_cp;
    *nullP = '\0';

} /*Replace*/

yywrap()
{
return(1);
}
