%{

/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * gram.y:
 *	Grammar defining the set of things recognized by package,
 *	the AFS workstation configuration facility.
 */

#include <afs/param.h>
#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)
#include <sys/mkdev.h>
#endif
#ifdef AFS_LINUX20_ENV
#include <sys/sysmacros.h>
#endif
#include "globals.h"
#include "package.h"

char *emalloc();

#if defined(AFS_AIX41_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_FBSD_ENV)
int test_linecounter;	/*Line number currently being parsed*/
char *ch2str();
char *appendchtostr();
#endif /* AFS_AIX41_ENV */



%}

%union
{
  afs_uint32	 usval;
  int		 ival;
  char		 chval;
  char		*strval;
  PROTOTYPE	 protoval;
  OWNER		 ownval;
  MODE		 modeval;
}

%token WHITESPACE
%token COMMENT
%token NEWLINE
%token BLANKLINE
%token REGTYPE
%token DIRTYPE
%token LNKTYPE
%token BLKTYPE
%token CHRTYPE
%token SOCKTYPE
%token PIPETYPE
%token <chval> LETTER
%token <strval> PATHNAME
%token <usval> DIGIT
%type <ival> input
%type <ival> entry
%type <usval> filetype
%type <usval> devicetype
%type <usval> updatespecs
%type <usval> updateflag
%type <strval> filename
%type <protoval> devprototype
%type <protoval> fileprototype
%type <ival> devmajor
%type <ival> devminor
%type <usval> number
%type <ownval> ownershipinfo
%type <strval> username
%type <strval> groupname
%type <strval> name
%type <modeval> mode
%type <usval> octalnumber


%%	/* rules */

input	:	/* empty */
			{ $$ = InitializeConfigTree();
			  test_linecounter = 1; }
	|	input entry
			{ $$ = ($1 == 0)? $2 : $1; }
	;

entry	:	COMMENT NEWLINE
			{ $$ = 0; test_linecounter++; }
	|	BLANKLINE
			{ $$ = 0; test_linecounter++; }
	|	devicetype	/*Specifies type of device to be updated
				  - block, char, etc*/
		updatespecs	/*Specifies actions during update, if
				  necessary*/
		filename	/*Name of device being updated*/
		devprototype	/*Device numbers for device being updated*/
		ownershipinfo	/*Ownership info for device being updated*/
		mode		/*Protection bits for file being updated*/
		NEWLINE
			{
			  $$ = AddEntry($1, $2, $3, $4, $5, $6);
			  /*Args -
			     filetype,
			     updatespecs,
			     filename,
			     devprototype,
			     ownershipinfo,
			     mode */
			  ReleaseEntry($3, $4, $5);
			  test_linecounter++;
			}

	|	filetype	/*Specifies type of file to be updated;
				  e.g., regular file, directory, special
				  file*/
		updatespecs	/*Specifies actions during update, if
				  necessary*/
		filename	/*Name of file being updated*/
		fileprototype	/*Prototype for file being updated, if
				  necessary - e.g., directory prefix,
				  master copy, device numbers, etc.*/
		ownershipinfo	/*Ownership info for file being updated;
				  should default to info from prototype*/
		mode		/*Protection bits for file being updated;
				  should default to info from prototype*/
		NEWLINE
			{
			  $$ = AddEntry($1, $2, $3, $4, $5, $6);
			  /*Args -
			    filetype,
			    updatespecs,
			    filename,
			    fileprototype,
			    ownershipinfo,
			    mode*/
			  ReleaseEntry($3, $4, $5);
			  test_linecounter++;
			}
	;

devicetype:	BLKTYPE 	/*Block device*/
			{ $$ = S_IFBLK; }
	|	CHRTYPE 	/*Character device*/
			{ $$ = S_IFCHR; }
	|	PIPETYPE 	/*Named pipe*/
			{
#ifdef S_IFIFO
			  $$ = S_IFIFO;
#endif /* S_IFIFO */
			}
	;

filetype:	REGTYPE 	/*Regular file*/
			{ $$ = S_IFREG; }
	|	DIRTYPE	 	/*Directory*/
			{ $$ = S_IFDIR; }
	|	LNKTYPE		/*Symbolic link*/
			{ $$ =S_IFLNK ; }
	|	SOCKTYPE
			{
#ifndef AFS_AIX_ENV
			  $$ = S_IFSOCK;
#endif /* AFS_AIX_ENV */
			}
	;


updatespecs :	/*empty */
			{ $$ = 0; }
	|	/*empty */ WHITESPACE
			{ $$ = 0; }
	|	updatespecs updateflag
			{ $$ = $1 | $2; }
	|	updatespecs updateflag WHITESPACE
			{ $$ = $1 | $2; }
	;

/*
 * Throw this out later.
 */
updateflag:	LETTER
			{
			  switch(yylval.chval)
			    {
			      case 'X':
				/*Make this a lost+found directory*/
				$$ = U_LOSTFOUND;	
				break;

			      case 'R':
				/*Extra unconfigured files to be removed*/
				$$ = U_RMEXTRA;	
				break;

			      case 'I':
				/*Update only if file not present*/
				$$ = U_NOOVERWRITE;
				break;

			      case 'O':
				/*Rename old copy with a suffix of .old*/
				$$ = U_RENAMEOLD;
				break;

			      case 'A':
				/*Prototype specifies an absolute path
				  rather than the prefix*/
				$$ = U_ABSPATH;
				break;

			      case 'Q':
				/*Exit with status=4 if file is updated,
				 indicating that a reboot is needed*/
				$$ = U_REBOOT;
				break;
			    }
			}
	;

filename	:	PATHNAME	/*Pathname of the file to be
					  updated; may not have white-
					  space if at eol*/
			{ $$ = $1; }
	|	PATHNAME WHITESPACE	/*Pathname of the file to be
					  updated*/
			{ $$ = $1; }
	;

fileprototype:	/* empty */
			{ $$.flag = P_NONE; }
	|	filename	/*Pathname of prototype file or directory
				  prefix*/
			{ $$.flag = P_FILE; $$.info.path = $1; }
	;

devprototype:	devmajor WHITESPACE devminor	/*Major & minor device
						  numbers*/
			{ $$.flag = P_DEV; $$.info.rdev = makedev($1, $3); }
	;

devmajor	:	number
			{
			  $$ = $1;
			}
	;

devminor	:	name
			{
			  $$ = strtol($1,0,0);
			}
	|	name WHITESPACE
			{
			  $$ = strtol($1,0,0);
			}
	;

number	:	DIGIT
			{ $$ = $1;
			  dbgprint((stderr, "digit %u\n", $$));
			}
	|	number DIGIT
			{ $$ = 10 * $1 + $2;
			  dbgprint((stderr, "10 * %u + %u = %u\n", $1,$2,$$));
			}
	;

ownershipinfo:	/* empty */
			{ $$.username = NULL; $$.groupname = NULL; }
	|	username groupname
			{ $$.username = $1; $$.groupname = $2; }
	;

username:	name
			{ $$ = $1; }
	|	name WHITESPACE
			{ $$ = $1; }
	;

groupname:	/* empty */
			{$$ = NULL; }
	|	name
			{ $$ = $1; }
	|	name WHITESPACE
			{ $$ = $1; }
	;

name	:	LETTER
			{ $$ = ch2str(yylval.chval);  }
	|	DIGIT
			{ $$ = ch2str('0'+$1);  }
	|	name LETTER
			{ $$ = appendchtostr($1, $2);  }
	|	name DIGIT
			{ $$ = appendchtostr($1, '0'+$2);  }
	;


mode	:	/* empty */
			{ $$.inherit_flag = TRUE; }
	|	COMMENT
			{ $$.inherit_flag = TRUE; }
	|	octalnumber	/*Last field of entry, may not have
				  anything before newline*/
			{ $$.inherit_flag = FALSE; $$.modeval = (afs_uint32)$1; }
	|	octalnumber WHITESPACE	/*Last field of entry, but may have
					  some whitespace before newline*/
			{ $$.inherit_flag = FALSE; $$.modeval = (afs_uint32)$1; }
	|	octalnumber COMMENT	/*Last field of entry, but may have
					  a comment before newline*/
			{ $$.inherit_flag = FALSE; $$.modeval = (afs_uint32)$1; }
	|	octalnumber WHITESPACE COMMENT	/*Last field of entry, but
						  may have some whitespace
						  and a comment before the
						  newline*/
			{ $$.inherit_flag = FALSE; $$.modeval = (afs_uint32)$1; }
	;

octalnumber:	DIGIT
			{ $$ = $1;
			  dbgprint((stderr, "digit %u\n", $$));
			}
	|	octalnumber DIGIT
			{ $$ = 8 * $1 + $2;
			  dbgprint((stderr, "8 * %u + %u = %u\n", $1,$2, $$));
			}
	;


%%

#ifndef AFS_AIX41_ENV
int test_linecounter;	/*Line number currently being parsed*/
#endif /* !AFS_AIX41_ENV */

char *ch2str(c)
    char c;
/*
 * Returns the pointer to a string consisting of the character c
 * terminated by \0
 */
{
  char *s;
  s = (char *)emalloc(2);
  sprintf(s, "%c", c);
  return(s);
}

char *appendchtostr(s, c)
    char c, *s;
/*
 * Returns the pointer to a NULL-terminated string containing the
 * character c appended to the contents of the string s
 */
{
    char *str;
    str = (char *)emalloc((unsigned)(strlen(s) + 2));
    sprintf(str, "%s%c", s, c);
    free(s);
    return(str);
}

ReleaseEntry(f, p, o)
    char *f;
    PROTOTYPE p;
    OWNER o;
/*
 * Release storage allocated for various data structures during the
 * parsing of an entry.
 */
{
    if (f != NULL) free(f);
    if ((p.flag == P_FILE) && (p.info.path != NULL)) free(p.info.path);
    if (o.username != NULL) free(o.username);
    if (o.groupname != NULL) free(o.groupname);
}

