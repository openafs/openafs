/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
testcellconfig.c:

    Test of the routines used by the FileServer to manipulate the cell/server database and
    determine the local cell name:
        1) Reading in the local cell name from file.
        2) Reading in the cell/server database from disk.
        3) Reporting the set of servers associated with a given cell name.
        4) Printing out the contents of the cell/server database.
        5) Reclaiming the space used by an in-memory database.

Creation date:
    17 August 1987

--------------------------------------------------------------------------------------------------------------*/
#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <afs/afsutil.h>
#ifdef AFS_NT40_ENV
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <afs/cellconfig.h>
#include <afs/cmd.h>

int _afsconf_Touch(struct afsconf_dir *adir);

enum optionsList {
    OPT_confdir,
    OPT_cell,
    OPT_reload
};

static int
PrintOneCell(struct afsconf_cell *ainfo, void *arock, struct afsconf_dir *adir)
{
    int i;
    long temp;

    printf("Cell %s:\n", ainfo->name);
    for (i = 0; i < ainfo->numServers; i++) {
	memcpy(&temp, &ainfo->hostAddr[i].sin_addr, sizeof(long));
	printf(" %d host %s at %lx port %x\n", i, ainfo->hostName[i], temp,
	       ainfo->hostAddr[i].sin_port);
    }
    return 0;
}

static void
PrintClones(char *clones, int numServers)
{
    int i;

    printf("Clones:\n");
    for (i = 0; i < numServers; i++) {
	printf(" %d clone %s\n", i, (clones[i] ? "yes" : "no"));
    }
}

static int
TestCellConfig(struct cmd_syndesc *as, void *arock)
{
    struct afsconf_dir *theDir;
    char tbuffer[1024];
    struct afsconf_cell theCell;
    int code;
    char *dirName = NULL;
    char clones[MAXHOSTSPERCELL];
    int reload = 0;
    struct cmd_item *cells = NULL;

    memset(clones, 0, sizeof(clones));

    cmd_OptionAsString(as, OPT_confdir, &dirName);
    reload = cmd_OptionPresent(as, OPT_reload);

    if (!dirName)
	dirName = strdup(AFSDIR_SERVER_ETC_DIRPATH);
    theDir = afsconf_Open(dirName);
    if (!theDir) {
	printf("could not open configuration files in '%s'\n", dirName);
	free(dirName);
	return 1;
    }

    /* get the cell */
    code = afsconf_GetLocalCell(theDir, tbuffer, sizeof(tbuffer));
    if (code != 0) {
	printf("get local cell failed, code %d\n", code);
	afsconf_Close(theDir);
	free(dirName);
	return 1;
    }
    printf("Local cell is '%s'\n\n", tbuffer);

    if (cmd_OptionAsList(as, OPT_cell, &cells) != 0) {
	printf("About to print cell database contents:\n");
	afsconf_CellApply(theDir, PrintOneCell, 0);
	printf("Done.\n\n");
	/* do this junk once */
	printf("start of special test\n");
	code = afsconf_GetCellInfo(theDir, NULL, "afsprot", &theCell);
	if (code)
	    printf("failed to find afsprot service (%d)\n", code);
	else {
	    printf("AFSPROT service:\n");
	    PrintOneCell(&theCell, NULL, theDir);
	}
	code = afsconf_GetExtendedCellInfo(theDir, NULL, "afsprot", &theCell, clones);
	if (code) {
	    printf("failed to find extended cell info (%d)\n", code);
	} else {
	    printf("AFSPROT service extended info:\n");
	    PrintOneCell(&theCell, NULL, theDir);
	    PrintClones(clones, theCell.numServers);
	}
	code = afsconf_GetCellInfo(theDir, 0, "bozotheclown", &theCell);
	if (code == 0)
	    printf("unexpectedly found service 'bozotheclown'\n");
	code = afsconf_GetCellInfo(theDir, NULL, "telnet", &theCell);
	printf("Here's the telnet service:\n");
	PrintOneCell(&theCell, NULL, theDir);
	printf("done with special test\n");
    } else {
	/* Now print out specified cell info. */
	for (; cells != NULL; cells = cells->next) {
	    char *cell = cells->data;
	    code = afsconf_GetCellInfo(theDir, cell, 0, &theCell);
	    if (code) {
		printf("Could not find info for cell '%s', code %d\n",
		       cell, code);
	    } else
		PrintOneCell(&theCell, NULL, theDir);

	    /* And print extended cell info. */
	    memset(clones, 0, sizeof(clones));
	    code = afsconf_GetExtendedCellInfo(theDir, cell, "afsprot", &theCell, clones);
	    if (code) {
		printf("Could not find extended info for cell '%s', code %d\n",
		       cell, code);
	    } else {
		PrintOneCell(&theCell, NULL, theDir);
		PrintClones(clones, theCell.numServers);
	    }
	}
    }

    if (reload) {
	printf("Forcing reload\n");
	code = _afsconf_Touch(theDir);
	if (code) {
	    printf("Unable to touch cellservdb file (%d)\n", code);
	} else {
	    sleep(2);
	    code = afsconf_GetCellInfo(theDir, NULL, "afsprot", &theCell);
	    if (code)
		printf("failed to find afsprot service (%d)\n", code);
	    else
		PrintOneCell(&theCell, NULL, theDir);
	}
    }

    /* all done */
    afsconf_Close(theDir);
    free(dirName);
    return 0;
}


int
main(int argc, char *argv[])
{
    afs_int32 code;
    struct cmd_syndesc *ts;

#ifdef AFS_NT40_ENV
    WSADATA WSAjunk;
    /* Start up sockets */
    WSAStartup(0x0101, &WSAjunk);
#endif /* AFS_NT40_ENV */

    ts = cmd_CreateSyntax("initcmd", TestCellConfig, NULL, 0, "Test cell configuration");
    cmd_AddParmAtOffset(ts, OPT_confdir, "-confdir", CMD_SINGLE, CMD_OPTIONAL,
		"Configuration directory pathname");
    cmd_AddParmAtOffset(ts, OPT_cell, "-cell", CMD_LIST, CMD_OPTIONAL, "Cell to display");
    cmd_AddParmAtOffset(ts, OPT_reload, "-reload", CMD_FLAG, CMD_OPTIONAL, "Perform reload test");

    code = cmd_Dispatch(argc, argv);
    return code;
}
