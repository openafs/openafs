/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
        unlog -- Tell the Andrew Cache Manager to either clean up your connection completely
                    or eliminate the caller's PAG.
	February 1986

       Usage:
              unlog [cell ...]

              where:
                  cell is the name the pertinent cell.

	If no cell is provided, unlog destroys all tokens.

	If a cell, for which a token is not held, is provided it is ignored.
*/

#define	VIRTUE	    1
#define	VICE	    1

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <potpourri.h>
#ifdef	AFS_AIX32_ENV
#include <signal.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <afs/vice.h>
#include <sys/file.h>

#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/cmd.h>
#include "afs_token.h"

#undef VIRTUE
#undef VICE


struct tokenInfo {
    struct ktc_token token;
    struct ktc_principal service;
    struct ktc_principal client;
    int deleted;
};


int 
CommandProc(struct cmd_syndesc *as, void *arock)
{
    struct cmd_item *itp;
    afs_int32 code, i;
    char **cells;

    if (as->parms[0].items) {	/* A cell is provided */
	for (i = 0, itp = as->parms[0].items; itp; itp = itp->next)
	    ++i;
	cells = (char **) malloc(i * sizeof *cells);
	if (!cells) {
	    afs_com_err("unlog", ENOMEM, "could not allocate %d cells", i);
	    return ENOMEM;
	}
	for (i = 0, itp = as->parms[0].items; itp; itp = itp->next) {
	    cells[i++] = itp->data;
	}
	code = unlog_ForgetCertainTokens(cells, i);
	free(cells);
    } else
	code = ktc_ForgetAllTokens();
    if (code) {
	afs_com_err("unlog", code, "could not discard tickets");
	return code;
    }
    return 0;
}


#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{				/*Main routine */
    struct cmd_syndesc *ts;
    register afs_int32 code;

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
    initialize_ktc_error_table();
    initialize_rx_error_table();

    ts = cmd_CreateSyntax(NULL, CommandProc, NULL,
			  "Release Kerberos authentication");
    cmd_AddParm(ts, "-cell", CMD_LIST, CMD_OPTIONAL, "cell name");

    code = cmd_Dispatch(argc, argv);
    exit(code != 0);
}				/*Main routine */

struct token_list {
    struct token_list *next;
    pioctl_set_token afst[1];
    int deleted;
};

/*
 * Problem: only the KTC gives you the ability to selectively destroy
 *          a specified token.
 *
 * Solution: Build a list of tokens, delete the bad ones (the ones to
 *           remove from the permissions list,) destroy all tokens, and
 *           then re-register the good ones.  Ugly, but it works.
 */

int
unlog_ForgetCertainTokens(char **list, int listSize)
{
    unsigned count;
    afs_int32 code;
    pioctl_set_token afstoken[1];
    struct token_list *token_list, **tokenp, *p;

    /* normalize all the names in the list */
    unlog_NormalizeCellNames(list, listSize);

    tokenp = &token_list;
    token_list = 0;
    /* get the tokens out of the kernel */
    for (count = 0; ; ++count) {
	memset(afstoken, 0, sizeof *afstoken);
	code = ktc_GetTokenEx(count, 0, afstoken);
	if (code) break;
	p = malloc(sizeof *p);
	if (!p) {
	    afs_com_err("unlog", ENOMEM, "can't allocate token link store");
	    exit(1);
	}
	p->next = 0;
	*p->afst = *afstoken;
	p->deleted = unlog_CheckUnlogList(list, listSize, afstoken->cell);
	*tokenp = p;
	tokenp = &p->next;
    }

    unlog_VerifyUnlog(list, listSize, token_list);
    code = ktc_ForgetAllTokens();

    if (code) {
	printf("unlog: could not discard tickets, code %d\n", code);
	exit(1);
    }

    for (p = token_list; p; p = p->next) {
	if (p->deleted) continue;
	code = ktc_SetTokenEx(p->afst);
	if (code) {
	afs_com_err("unlog", code,
	    "so couldn't re-register token in cell %s", p->afst->cell);
	}
    }
    return 0;
}

/*
 * 0 if not in list, 1 if in list
 */
int
unlog_CheckUnlogList(char **list, int count, char *cell)
{
    do {
	if (strcmp(*list, cell) == 0)
	    return 1;
	list++;
	--count;
    } while (count);

    return 0;
}

/*
 * Caveat: this routine does NOT free up the memory passed (and replaced).
 *         because it assumes it isn't a problem.
 */

int
unlog_NormalizeCellNames(char **list, int size)
{
    char *newCellName, *lcstring();
    unsigned index;
    struct afsconf_dir *conf;
    int code;
    struct afsconf_cell cellinfo;

    if (!(conf = afsconf_Open(AFSDIR_CLIENT_ETC_DIRPATH))) {
	fprintf(stderr, "Cannot get cell configuration info!\n");
	exit(1);
    }

    for (index = 0; index < size; index++, list++) {
	newCellName = malloc(MAXKTCREALMLEN);
	if (!newCellName) {
	    perror("unlog_NormalizeCellNames --- malloc failed");
	    exit(1);
	}

	lcstring(newCellName, *list, MAXKTCREALMLEN);
	code = afsconf_GetCellInfo(conf, newCellName, 0, &cellinfo);
	if (code) {
	    if (code == AFSCONF_NOTFOUND) {
		fprintf(stderr, "Unrecognized cell name %s\n", newCellName);
	    } else {
		fprintf(stderr,
			"unlog_NormalizeCellNames - afsconf_GetCellInfo");
		fprintf(stderr, " failed, code = %d\n", code);
	    }
	    exit(1);
	}


	strcpy(newCellName, cellinfo.name);

	*list = newCellName;
    }
    afsconf_Close(conf);
    return 0;
}

/*
 * check given list to assure tokens were held for specified cells
 * prints warning messages for those cells without such entries.
 */
int
unlog_VerifyUnlog(char **cellList, int cellListSize, struct token_list *tokenList)
{
    struct token_list *p;
    int index;
    for (index = 0; index < cellListSize; index++) {
	int found;

	found = 0;
	for (p = tokenList; p; p = p->next) {
	    if (strcmp(cellList[index], p->afst->cell)) continue;
	    found = 1;
	    break;
	}

	if (!found)
	    fprintf(stderr, "unlog: Warning - no tokens held for cell %s\n",
		    cellList[index]);
    }
    return 0;
}
