/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*------------------------------------------------------------------------
 * test.c
 *
 * Description:
 *	Check out the parsing of specification files for package,
 *	the AFS workstation configuration tool.
 *
 *------------------------------------------------------------------------*/

#include <afs/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#if defined(AFS_SGI_ENV) || defined(AFS_SUN5_ENV)
#include <sys/mkdev.h>
#endif
#ifdef AFS_LINUX20_ENV
#include <sys/sysmacros.h>
#endif
#include "globals.h"
#include "package.h"
#include "systype.h"

extern int test_linecounter;	/*Line number currently being parsed*/
extern FILE *yyin;

char *emalloc();

CTREEPTR config_root;	/*Top of the config tree*/

int opt_silent  = 0;	/*Silent operation?*/
int opt_verbose = 1;	/*Verbose operation?*/
int opt_debug   = 1;	/*Debugging output enabled?*/

#include "AFS_component_version_number.c"

main(argc, argv)
    int argc;
    char **argv;

{ /*main*/

    int code;	/*Return code*/

    if (argc > 1)
      yyin = efopen(argv[1], "r");
    else
      yyin = stdin;
    code = yyparse();
    if (code)
      printf("** Lexical error in config file, line %d\n", test_linecounter);
    if (yyin != stdin)
      fclose(yyin);

} /*main*/

/*------------------------------------------------------------------------
 * AddRegEntry
 *
 * Description:
 *	Routine called by the lexer when a line from the spec file has
 *	been parsed, passing in its interpretation of the contents of
 *	the (non-directory, `F' type) line.
 *
 * Arguments:
 *	char *f	    : Filename picked up.
 *	PROTOTYPE p : Prototype (target) picked up.
 *	OWNER o	    : Owner information picked up.
 *	MODE m	    : Mode information picked up.
 *	u_short u   : Update options picked up.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Called from the lexer.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int AddRegEntry(f, p, o, m, u)
    char *f;
    PROTOTYPE p;
    OWNER o;
    MODE m;
    u_short u;

{ /*AddRegEntry*/

    printf("F");
    echo_updateoptions(u);
    printf("%s", f);
    echo_prototypeinfo(p);
    echo_ownerinfo(o);
    echo_modeinfo(m);
    return(0);

} /*AddRegEntry*/

/*------------------------------------------------------------------------
 * AddDirEntry
 *
 * Description:
 *	Routine called by the lexer when a line from the spec file has
 *	been parsed, passing in its interpretation of the contents of
 *	the (directory, `D' type) line.
 *
 * Arguments:
 *	char *f	    : Filename picked up.
 *	PROTOTYPE p : Prototype (target) picked up.
 *	OWNER o	    : Owner information picked up.
 *	MODE m	    : Mode information picked up.
 *	u_short u   : Update options picked up.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Called from the lexer.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int AddDirEntry(f, p, o, m, u)
    char *f;
    PROTOTYPE p;
    OWNER o;
    MODE m;
    u_short u;

{ /*AddDirEntry*/

    printf("D");
    echo_updateoptions(u);
    printf("%s", f);
    echo_prototypeinfo(p);
    echo_ownerinfo(o);
    echo_modeinfo(m);
    return(0);

} /*AddDirEntry*/

/*------------------------------------------------------------------------
 * AddLnkEntry
 *
 * Description:
 *	Routine called by the lexer when a line from the spec file has
 *	been parsed, passing in its interpretation of the contents of
 *	the (symlink, `L' type) line.
 *
 * Arguments:
 *	char *f	    : Filename picked up.
 *	PROTOTYPE p : Prototype (target) picked up.
 *	OWNER o	    : Owner information picked up.
 *	MODE m	    : Mode information picked up.
 *	u_short u   : Update options picked up.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Called from the lexer.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int AddLnkEntry(f, p, o, m, u)
    char *f;
    PROTOTYPE p;
    OWNER o;
    MODE m;
    u_short u;

{ /*AddLnkEntry*/

    printf("L");
    echo_updateoptions(u);
    printf("%s", f);
    echo_prototypeinfo(p);
    echo_ownerinfo(o);
    echo_modeinfo(m);
    return(0);

} /*AddLnkEntry*/

/*------------------------------------------------------------------------
 * AddBlkEntry
 *
 * Description:
 *	Routine called by the lexer when a line from the spec file has
 *	been parsed, passing in its interpretation of the contents of
 *	the (block device, `B' type) line.
 *
 * Arguments:
 *	char *f	    : Filename picked up.
 *	PROTOTYPE p : Prototype (target) picked up.
 *	OWNER o	    : Owner information picked up.
 *	MODE m	    : Mode information picked up.
 *	u_short u   : Update options picked up.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Called from the lexer.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int AddBlkEntry(f, p, o, m, u)
    char *f;
    PROTOTYPE p;
    OWNER o;
    MODE m;
    u_short u;

{ /*AddBlkEntry*/

    printf("B");
    echo_updateoptions(u);
    printf("%s", f);
    echo_prototypeinfo(p);
    echo_ownerinfo(o);
    echo_modeinfo(m);
    return(0);

} /*AddBlkEntry*/

/*------------------------------------------------------------------------
 * AddChrEntry
 *
 * Description:
 *	Routine called by the lexer when a line from the spec file has
 *	been parsed, passing in its interpretation of the contents of
 *	the (character device, `C' type) line.
 *
 * Arguments:
 *	char *f	    : Filename picked up.
 *	PROTOTYPE p : Prototype (target) picked up.
 *	OWNER o	    : Owner information picked up.
 *	MODE m	    : Mode information picked up.
 *	u_short u   : Update options picked up.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Called from the lexer.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int AddChrEntry(f, p, o, m, u)
    char *f;
    PROTOTYPE p;
    OWNER o;
    MODE m;
    u_short u;

{ /*AddChrEntry*/

    printf("C");
    echo_updateoptions(u);
    printf("%s", f);
    echo_prototypeinfo(p);
    echo_ownerinfo(o);
    echo_modeinfo(m);
    return(0);

}  /*AddChrEntry*/

/*------------------------------------------------------------------------
 * AddSktEntry
 *
 * Description:
 *	Routine called by the lexer when a line from the spec file has
 *	been parsed, passing in its interpretation of the contents of
 *	the (socket, `S' type) line.
 *
 * Arguments:
 *	char *f : Filename picked up.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Called from the lexer.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int AddSktEntry(f)
    char *f;

{ /*AddSktEntry*/

    printf("S");
    printf("\t%s\n", f);

} /*AddSktEntry*/

/*------------------------------------------------------------------------
 * AddPipeEntry
 *
 * Description:
 *	Routine called by the lexer when a line from the spec file has
 *	been parsed, passing in its interpretation of the contents of
 *	the (named pipe, `P' type) line.
 *
 * Arguments:
 *	char *f	    : Filename picked up.
 *	PROTOTYPE p : Prototype (target) picked up.
 *	OWNER o	    : Owner information picked up.
 *	MODE m	    : Mode information picked up.
 *	u_short u   : Update options picked up.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Called from the lexer.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int AddPipeEntry(f, p, o, m, u)
    char *f;
    PROTOTYPE p;
    OWNER o;
    MODE m;
    u_short u;

{ /*AddPipeEntry*/

    printf("P");
    echo_updateoptions(u);
    printf("\t%s\n", f);
    echo_prototypeinfo(p);
    echo_ownerinfo(o);
    echo_modeinfo(m);
    return(0);

} /*AddPipeEntry*/

/*------------------------------------------------------------------------
 * echo_updateoptions
 *
 * Description:
 *	Print out a readable version of the update options parsed by
 *	the lexer.
 *
 * Arguments:
 *	u_short u : Update options to print.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int echo_updateoptions(u)
    u_short u;

{ /*echo_updateoptions*/

    if (u & U_LOSTFOUND)	printf("X");
    if (u & U_RMEXTRA)		printf("R");
    if (u & U_NOOVERWRITE)	printf("I");
    if (u & U_RENAMEOLD)	printf("O");
    if (u & U_ABSPATH)		printf("A");
    if (u & U_REBOOT)		printf("Q");
    printf("\t");
    return(0);

} /*echo_updateoptions*/

/*------------------------------------------------------------------------
 * echo_prototypeinfo
 *
 * Description:
 *	Print out a readable version of the prototype information parsed
 *	by the lexer.
 *
 * Arguments:
 *	PROTOTYPE p : Prototype info to print.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int echo_prototypeinfo(p)
    PROTOTYPE p;

{ /*echo_prototypeinfo*/

    switch(p.flag) {
	case P_FILE:
	    printf("\t%s", p.info.path);
	    break;

	case P_DEV:
/*	    printf("\t%d\t%d", major(p.info.devno), minor(p.info.devno));*/

#ifndef	AFS_SGI_ENV
	    printf("\t%d\t%d", major(p.info.rdev), minor(p.info.rdev));
#endif

	    break;

	default:
	    printf("\t[Neither file nor device]");
	    break;
    }

} /*echo_prototypeinfo*/

/*------------------------------------------------------------------------
 * echo_ownerinfo
 *
 * Description:
 *	Print out a readable version of the owner information parsed by
 *	the lexer.
 *
 * Arguments:
 *	OWNER o : Owner info to print.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int echo_ownerinfo(o)
    OWNER o;

{ /*echo_ownerinfo*/

    if (o.username != NULL)
      printf("\t%s", o.username);
    else
      printf("\t[No owner info]");
    if (o.groupname != NULL)
	printf("\t%s", o.groupname);
    else
      printf("\t[No group info]");

} /*echo_ownerinfo*/

/*------------------------------------------------------------------------
 * echo_modeinfo
 *
 * Description:
 *	Print out a readable version of the mode info parsed by
 *	the lexer.
 *
 * Arguments:
 *	MODE m : Mode info to print.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Nothing interesting.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int echo_modeinfo(m)
    MODE m;

{ /*echo_modeinfo*/

    if (m.inherit_flag != TRUE)
      printf("\t%o", m.modeval);
    else
      printf("\t[Inherited mode]");
    printf("\n");

} /*echo_modeinfo*/

/*------------------------------------------------------------------------
 * testAddEntry
 *
 * Description:
 *	Version of package's AddEntry() routine called by this test
 *	program.  It looks at the type of spec line that was just
 *	parsed, and it then calls specific routines to display specific
 *	types of lines.
 *
 * Arguments:
 *	u_short a_ftype    : Specifies type of file to be updated (regular
 *				file, directory, device, etc.)
 *	u_short a_updspecs : Specifies actions during update, if necessary.
 *	char *a_filename   : Name of file to be updated.
 *	PROTOTYPE a_proto  : Prototype for filename (e.g., dev numbers,
 *				directory prefix, etc.)
 *	OWNER a_owner	   : Ownership info parsed.
 *	MODE a_mode	   : Protection (mode) bits parsed.
 *
 * Returns:
 *	0 (always).
 *
 * Environment:
 *	Called from the lexer.
 *
 * Side Effects:
 *	None.
 *------------------------------------------------------------------------*/

int testAddEntry(a_ftype, a_updspecs, a_filename, a_proto, a_owner, a_mode)
    u_short a_ftype;
    u_short a_updspecs;
    char *a_filename;
    PROTOTYPE a_proto;
    OWNER a_owner;
    MODE a_mode;

{ /*testAddEntry*/

    switch(a_ftype) {
    case S_IFREG: /*Regular file*/
      AddRegEntry(a_filename, a_proto, a_owner, a_mode, a_updspecs);
      break;

    case S_IFDIR: /*Directory*/
      AddDirEntry(a_filename, a_proto, a_owner, a_mode, a_updspecs);
      break;

    case S_IFLNK: /*Symbolic link*/
      AddLnkEntry(a_filename, a_proto, a_owner, a_mode, a_updspecs);
      break;

#ifndef AFS_AIX_ENV
    case S_IFSOCK: /*Socket*/
      AddSktEntry(a_filename);
      break;
#endif /* AFS_AIX_ENV */

    case S_IFBLK: /*Block device*/
      AddBlkEntry(a_filename, a_proto, a_owner, a_mode, a_updspecs);
      break;

    case S_IFCHR: /*Character device*/
      AddChrEntry(a_filename, a_proto, a_owner, a_mode, a_updspecs);
      break;

#ifdef S_IFIFO
    case S_IFIFO: /*Named pipe*/
      AddPipeEntry(a_filename, a_proto, a_owner, a_mode, a_updspecs);
      break;
#endif /* S_IFIFO */

    default:
      printf("Unknown file type parsed: %d\n", a_ftype);
      break;

    } /*switch a_ftype*/

} /*testAddEntry*/
