/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*------------------------------------------------------------------------
 * package.c
 *
 * Description:
 *	AFS workstation configuration tool.
 *
 *------------------------------------------------------------------------*/
#include <afs/param.h>
#include <stdio.h>		/*I/O stuff */
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif
#include <sys/types.h>		/*Low-level type definitions */
#include <sys/stat.h>		/*Stat buffer defs */
#include <sys/param.h>		/*Machine-type dependent params */
#if	defined(AFS_SUN_ENV)
#include <sys/ioccom.h>		/* for _IOW macro */
#endif
#include <afs/com_err.h>	/*Error compiler package */
#include <afs/cmd.h>		/*Command interpretation library */

#include <netinet/in.h>
#include <afs/vice.h>
#include <afs/venus.h>

#include "globals.h"		/*Our own set of global defs */
#include "package.h"		/*Generally-used definitions */
#include "systype.h"		/*Systype string */

/*
 * Command line parameter indices.
 */
#define P_CONFIG	0
#define P_FULLCONFIG	1
#define P_OVERWRITE	2
#define P_NOACTION	3
#define P_VERBOSE	4
#define P_SILENT	5
#define P_REBOOTFILES	6
#define P_DEBUG		7

extern int test_linecounter;	/*Line number currently being parsed */
char *emalloc();
int check();
int update();

/*
 * Set up default configuration: Perform all actions, moderate output
 * level, handle files causing reboot, don't overwrite protected files,
 * and turn debugging output off.
 */
int status = status_noerror;	/*Start off with no errors */

int opt_lazy = FALSE;		/*Perform all actions */
int opt_silent = FALSE;		/*Don't be overly silent */
int opt_verbose = FALSE;	/*Don't be overly verbose */
int opt_stdin = FALSE;		/*Read configuration from stdin */
int opt_reboot = TRUE;		/*Update boot-critical files (e.g., vmunix) */
#ifdef KFLAG
int opt_kflag = TRUE;		/*Why was this KFLAG stuff ifdefed? */
#endif /* KFLAG */
int opt_debug = FALSE;		/*Turn off debugging output */

CTREEPTR config_root;		/*Top of the config tree */

char *conffile = "/etc/package";	/*Base configuration file */
char filename[MAXPATHLEN];	/*Chosen configuration file name */

/*------------------------------------------------------------------------
 * PRIVATE packageExecute
 *
 * Description:
 *	Actually perform that for which the program was invoked,
 *	namely bringing the machine's local disk into agreement
 *	with the given goal.
 *
 * Arguments:
 *	None.
 *
 * Returns:
 *	Nothing.
 *
 * Environment:
 *	Everything we need to know is in globals.
 *
 * Side Effects:
 *	As described.
 *------------------------------------------------------------------------*/

static void
packageExecute()
{				/*packageExecute */

    FILE *fp;			/*Descriptor for configuration filename */
    int code;			/*Return value from functions */
    static char parse_err[80];	/*Parsing error string */

    /*
     * If we're getting our configuration info from stdin, go straight
     * for it.  Otherwise, open the config file, build the tree, and
     * then close the config file.
     */
    if (opt_stdin) {
	code = BuildConfigTree(stdin);
	if (code) {
	    sprintf(parse_err,
		    "** Lexical error in configuration file, line %d",
		    test_linecounter);
	    fatal(parse_err);
	}
    } else {
	verbose_message("Loading configuration file '%s'", filename);
	fp = efopen(filename, "r");
	verbose_message("Building configuration tree");
	code = BuildConfigTree(fp);
	if (code) {
	    sprintf(parse_err,
		    "** Lexical error in configuration file, line %d",
		    test_linecounter);
	    fatal(parse_err);
	}
	(void)fclose(fp);
    }

    /*
     * Now that the configuration tree is built, ``apply'' the check
     * function on that tree.
     */
    verbose_message("Checking integrity of configuration tree");
    ApplyConfigTree(check);

    /*
     * The config tree seems fine, so ``apply'' the update function
     * on the tree.
     */
    verbose_message("Updating local disk to match configuration tree");
    ApplyConfigTree(update);

    loudonly_message("Sync");
    if (!opt_lazy && (sync() < 0))
	message("Sync; %m");

    /*
     * We're all done!  Print out a parting message (if we've been asked
     * to be verbose) and return to our caller.  The overall status has
     * been recorded in the global status variable.
     */
    verbose_message("Done");

}				/*packageExecute */

#define MAXSIZE 2048
struct output {
    afs_int32 found;
    char name[MAXSIZE];
} sysoutput;

char *
getsystype()
{
    afs_int32 code;
    struct ViceIoctl data;

    data.in = (char *)&sysoutput;
    data.out = (char *)&sysoutput;
    data.out_size = MAXSIZE;
    data.in_size = sizeof(afs_int32);

    code = pioctl(0, VIOC_AFS_SYSNAME, &data, 1);
    if (!code && sysoutput.found)
	return (sysoutput.name);
    return (SYS_NAME);
}

/*------------------------------------------------------------------------
 * PRIVATE packageInit
 *
 * Description:
 *	Routine called when package is invoked, responsible for basic
 *	initialization, command line parsing, and calling the
 *	routine that does all the work.
 *
 * Arguments:
 *	as	: Command syntax descriptor.
 *	arock	: Associated rock (not used here).
 *
 * Returns:
 *	Zero (but may exit the entire program on error!)
 *
 * Environment:
 *	Puts everything it finds into global variables.
 *
 * Side Effects:
 *	Initializes this program.
 *------------------------------------------------------------------------*/

static int
packageInit(struct cmd_syndesc *as, char *arock)
{				/*packageInit */
    systype = getsystype();

    /*
     * Set up the default configuration file to use.
     */
    sprintf(filename, "%s.%s", conffile, systype);

    if (as->parms[P_DEBUG].items != 0) {
	opt_debug = TRUE;
	debug_message("Debugging output enabled");
    }

    if (as->parms[P_CONFIG].items != 0) {
	/*
	 * Pull out the configuration file name, tack on the sysname.
	 */
	sprintf(filename, "%s.%s", as->parms[P_CONFIG].items->data, systype);
    }

    if (as->parms[P_FULLCONFIG].items != 0) {
	/*
	 * Make sure we use only one of -config and -fullconfig.
	 */
	if (as->parms[P_CONFIG].items != 0) {
	    message
		("Can't use the -config and -fullconfig switches together");
	    status = -1;
	    exit(status);
	}

	/*Switch conflict */
	/*
	 * If ``stdin'' is the configuration file to use, make sure we
	 * remember that fact.
	 */
	if (strcmp(as->parms[P_FULLCONFIG].items->data, "stdin") == 0) {
	    opt_stdin = TRUE;
	    filename[0] = '\0';
	} else
	    sprintf(filename, "%s", as->parms[P_FULLCONFIG].items->data);
    }
    /*Full config file name given */
    if (as->parms[P_OVERWRITE].items != 0)
	opt_kflag = FALSE;
    else
	opt_kflag = TRUE;

    if (as->parms[P_NOACTION].items != 0)
	opt_lazy = TRUE;

    if (as->parms[P_VERBOSE].items != 0)
	opt_verbose = TRUE;

    if (as->parms[P_SILENT].items != 0) {
	if (opt_verbose) {
	    message("Can't use the -silent and -verbose flags together");
	    status = -1;
	    exit(status);
	} else
	    opt_silent = TRUE;
    }

    if (as->parms[P_REBOOTFILES].items != 0)
	opt_reboot = FALSE;

    /*
     * Now that we've finished parsing, execute the package function.
     * This sets the global status variable, so we simply exit with
     * its value.
     */
    packageExecute();
    exit(status);

}				/*packageInit */

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{				/*main */

    register afs_int32 code;	/*Return code */
    register struct cmd_syndesc *ts;	/*Ptr to cmd line syntax descriptor */

#ifdef	AFS_AIX32_ENV
    /*
     * The following signal action for AIX is necessary so that in case of a 
     * crash (i.e. core is generated) we can include the user's data section 
     * in the core dump. Unfortunately, by default, only a partial core is
     * generated which, in many cases, isn't too useful.
     */
    struct sigaction nsa;

    sigemptyset(&nsa.sa_mask);
    nsa.sa_handler = SIG_DFL;
    nsa.sa_flags = SA_FULLDUMP;
    sigaction(SIGSEGV, &nsa, NULL);
#endif
    /*
     * Set file mode creation mask.
     */
    (void)umask(0);

    /*
     * Set up to parse the command line.
     */
    ts = cmd_CreateSyntax("initcmd", packageInit, 0,
			  "initialize the program");
    cmd_AddParm(ts, "-config", CMD_SINGLE, CMD_OPTIONAL,
		"base name of configuration file");
    cmd_AddParm(ts, "-fullconfig", CMD_SINGLE, CMD_OPTIONAL,
		"full name of configuration file, or stdin for standard input");
    cmd_AddParm(ts, "-overwrite", CMD_FLAG, CMD_OPTIONAL,
		"overwrite write-protected files");
    cmd_AddParm(ts, "-noaction", CMD_FLAG, CMD_OPTIONAL,
		"show what would be done, but don't do it");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL,
		"give more details about what's happening");
    cmd_AddParm(ts, "-silent", CMD_FLAG, CMD_OPTIONAL, "supress all output");
    cmd_AddParm(ts, "-rebootfiles", CMD_FLAG, CMD_OPTIONAL,
		"don't handle boot-critical files");
    cmd_AddParm(ts, "-debug", CMD_FLAG, CMD_OPTIONAL,
		"enable debugging input");

    /*
     * Set up the appropriate error tables.
     */
    initialize_CMD_error_table();
    initialize_rx_error_table();

    /*
     * Parse command line switches & execute the command, then get the
     * heck out of here.
     */
    code = cmd_Dispatch(argc, argv);
    if (code) {
	afs_com_err(argv[0], code, "while dispatching command line");
	exit(-1);
    }

}				/*main */
