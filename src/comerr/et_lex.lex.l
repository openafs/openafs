PC	[^\"]
AN	[A-Z_a-z0-9]
%%

error_table	return ERROR_TABLE;
et		return ERROR_TABLE;
error_code	return ERROR_CODE_ENTRY;
ec		return ERROR_CODE_ENTRY;
end		return END;

[\t\n\r ]		;

\"{PC}*\"	{ register char *p; yylval.dynstr = ds((char *)yytext+1);
		  if ((p=strrchr(yylval.dynstr, '"'))) *p='\0';
		  return QUOTED_STRING;
		}

{AN}*	{ yylval.dynstr = ds((char *)yytext); return STRING; }

#.*\n		;

.		{ return (*yytext); }
%%
/* Copyright (C)  1998  Transarc Corporation.  All rights reserved.
 *
 */
int yywrap(void) {
  return 1;
}
