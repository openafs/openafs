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

#include <roken.h>
#include <afs/opr.h>

#include <afs/vice.h>

#include <afs/auth.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include <afs/cmd.h>
#include <afs/token.h>
#include <afs/ktc.h>

#undef VIRTUE
#undef VICE


struct tokenInfo {
    struct ktc_setTokenData *tokenSet;
    int deleted;
};

static int unlog_ForgetCertainTokens(char **, int);
static int unlog_NormalizeCellNames(char **, int);
static int unlog_CheckUnlogList(char **, int, char *);
static int unlog_VerifyUnlog(char **, int, struct tokenInfo *, int);

static int
CommandProc(struct cmd_syndesc *as, void *arock)
{
#define	MAXCELLS 20		/* XXX */
    struct cmd_item *itp;
    afs_int32 code, i = 0;
    char *cells[20];

    if (as->parms[0].items) {	/* A cell is provided */
	for (itp = as->parms[0].items; itp; itp = itp->next) {
	    if (i >= MAXCELLS) {
		printf
		    ("The maximum number of cells (%d) is exceeded; the rest are ignored\n",
		     MAXCELLS);
		break;
	    }
	    cells[i++] = itp->data;
	}
	code = unlog_ForgetCertainTokens(cells, i);
    } else
	code = ktc_ForgetAllTokens();
    if (code) {
	printf("unlog: could not discard tickets, code %d\n", code);
	exit(1);
    }
    return 0;
}


#include "AFS_component_version_number.c"

int
main(int argc, char *argv[])
{				/*Main routine */
    struct cmd_syndesc *ts;
    afs_int32 code;

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

    ts = cmd_CreateSyntax(NULL, CommandProc, NULL, 0,
			  "Release Kerberos authentication");
    cmd_AddParm(ts, "-cell", CMD_LIST, CMD_OPTIONAL, "cell name");

    code = cmd_Dispatch(argc, argv);
    exit(code != 0);
}				/*Main routine */


/*
 * Problem: only the KTC gives you the ability to selectively destroy
 *          a specified token.
 *
 * Solution: Build a list of tokens, delete the bad ones (the ones to
 *           remove from the permissions list,) destroy all tokens, and
 *           then re-register the good ones.  Ugly, but it works.
 */

static int
unlog_ForgetCertainTokens(char **list, int listSize)
{
    int index, index2;
    int count;
    afs_int32 code;
    char *cellName = NULL;
    struct tokenInfo *tokenInfoP;

    /* normalize all the names in the list */
    unlog_NormalizeCellNames(list, listSize);

    /* figure out how many tokens exist */
    count = 0;
    do {
	code = ktc_ListTokensEx(count, &count, &cellName);
	free(cellName);
	cellName = NULL;
    } while (!code);

    tokenInfoP = calloc(count, sizeof(tokenInfoP[0]));
    if (!tokenInfoP) {
	perror("unlog_ForgetCertainTokens -- osi_Alloc failed");
	exit(1);
    }

    for (code = index = index2 = 0; (!code) && (index < count); index++) {
	code =
	    ktc_ListTokensEx(index2, &index2, &cellName);

	if (!code) {
	    code =
		ktc_GetTokenEx(cellName, &tokenInfoP[index].tokenSet);

	    if (!code)
		tokenInfoP[index].deleted =
		    unlog_CheckUnlogList(list, listSize,
					 tokenInfoP[index].tokenSet->cell);
	}
	free(cellName);
	cellName = NULL;
    }

    unlog_VerifyUnlog(list, listSize, tokenInfoP, count);
    code = ktc_ForgetAllTokens();

    if (code) {
	printf("unlog: could not discard tickets, code %d\n", code);
	exit(1);
    }

    for (code = index = 0; index < count; index++) {
	if (!tokenInfoP[index].deleted) {
	    code =
		ktc_SetTokenEx(tokenInfoP[index].tokenSet);
	    if (code) {
		fprintf(stderr, "Couldn't re-register token, code = %d\n",
			code);
	    }
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
    char *newCellName;
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
unlog_VerifyUnlog(char **cellList, int cellListSize, struct tokenInfo *tokenList, int tokenListSize)
{
    int index;

    for (index = 0; index < cellListSize; index++) {
	int index2;
	int found;

	for (found = index2 = 0; !found && index2 < tokenListSize; index2++)
	    found =
		strcmp(cellList[index],
		       tokenList[index2].tokenSet->cell) == 0;

	if (!found)
	    fprintf(stderr, "unlog: Warning - no tokens held for cell %s\n",
		    cellList[index]);
    }
    return 0;
}
