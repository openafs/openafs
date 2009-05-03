PC	[^\"]
AN	[A-Z_a-z0-9]
N	[0-9]
%%

error_table	return ERROR_TABLE;
et		return ERROR_TABLE;
error_code	return ERROR_CODE_ENTRY;
ec		return ERROR_CODE_ENTRY;
end		return END;
index		return INDEX;
prefix		return PREFIX;
id		return ID;
base		return BASE;

[\t\r ]		;
\n		return NL;

\"{PC}*\"	{ register char *p; yylval.dynstr = ds((char *)yytext+1);
		  if ((p=strrchr(yylval.dynstr, '"'))) *p='\0';
		  return QUOTED_STRING;
		}

-*{N}{N}*	{ yylval.dynstr = ds((char *)yytext); return NUMBER; }
{AN}*	{ yylval.dynstr = ds((char *)yytext); return STRING; }

#.*\n		return NL;

.		{ return (*yytext); }
%%
/* 
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */
int yywrap(void) {
  return 1;
}
