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

RCSID
    ("$Header$");

#include <sys/types.h>
#include <ctype.h>
#include "cmd.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* declaration of private token type */
struct cmd_token {
    struct cmd_token *next;
    char *key;
};

static int dummy;		/* non-null ptr used for flag existence */
static struct cmd_syndesc *allSyntax = 0;
static int noOpcodes = 0;
static int (*beforeProc) (struct cmd_syndesc * ts, char *beforeRock) =
    0, (*afterProc) (struct cmd_syndesc * ts, char *afterRock) = 0;
static char *beforeRock, *afterRock;
static char initcmd_opcode[] = "initcmd";	/*Name of initcmd opcode */

/* take name and string, and return null string if name is empty, otherwise return
   the concatenation of the two strings */
static char *
NName(char *a1, char *a2)
{
    static char tbuffer[80];
    if (strlen(a1) == 0) {
	return "";
    } else {
	strcpy(tbuffer, a1);
	strcat(tbuffer, a2);
	return tbuffer;
    }
}

/* return true if asub is a substring of amain */
static int
SubString(register char *amain, register char *asub)
{
    int mlen, slen;
    register int i, j;
    mlen = (int) strlen(amain);
    slen = (int) strlen(asub);
    j = mlen - slen;
    if (j < 0)
	return 0;		/* not a substring */
    for (i = 0; i <= j; i++) {
	if (strncmp(amain, asub, slen) == 0)
	    return 1;
	amain++;
    }
    return 0;			/* didn't find it */
}

static int
FindType(register struct cmd_syndesc *as, register char *aname)
{
    register int i;
    size_t cmdlen;
    int ambig;
    int best;

    cmdlen = strlen(aname);
    ambig = 0;
    best = -1;
    for (i = 0; i < CMD_MAXPARMS; i++) {
	if (as->parms[i].type == 0)
	    continue;		/* this slot not set (seeked over) */
	if (strcmp(as->parms[i].name, aname) == 0)
	    return i;
	if (strlen(as->parms[i].name) < cmdlen)
	    continue;
	/* A hidden option must be a full match (no best matches) */
	if (as->parms[i].flags & CMD_HIDE)
	    continue;

	if (strncmp(as->parms[i].name, aname, cmdlen) == 0) {
	    if (best != -1)
		ambig = 1;
	    else
		best = i;
	}
    }
    return (ambig ? -1 : best);
}

static struct cmd_syndesc *
FindSyntax(char *aname, int *aambig)
{
    register struct cmd_syndesc *ts;
    struct cmd_syndesc *best;
    size_t cmdLen;
    int ambig;

    cmdLen = strlen(aname);
    best = (struct cmd_syndesc *)0;
    ambig = 0;
    if (aambig)
	*aambig = 0;		/* initialize to unambiguous */
    for (ts = allSyntax; ts; ts = ts->next) {
	if (strcmp(aname, ts->name) == 0)
	    return (ts);
	if (strlen(ts->name) < cmdLen)
	    continue;		/* we typed more than item has */
	/* A hidden command must be a full match (no best matches) */
	if (ts->flags & CMD_HIDDEN)
	    continue;

	/* This is just an alias for *best, or *best is just an alias for us.
	 * If we don't make this check explicitly, then an alias which is just a
	 * short prefix of the real command's name might make things ambiguous
	 * for no apparent reason.
	 */
	if (best && ts->aliasOf == best->aliasOf)
	    continue;
	if (strncmp(ts->name, aname, cmdLen) == 0) {
	    if (best)
		ambig = 1;	/* ambiguous name */
	    else
		best = ts;
	}
    }
    if (ambig) {
	if (aambig)
	    *aambig = ambig;	/* if ambiguous and they care, tell them */
	return (struct cmd_syndesc *)0;	/* fails */
    } else
	return best;		/* otherwise its not ambiguous, and they know */
}

/* print the help for a single parameter */
static void
PrintParmHelp(register struct cmd_parmdesc *aparm)
{
    if (aparm->type == CMD_FLAG) {
#ifdef notdef
	/* doc people don't like seeing this information */
	if (aparm->help)
	    printf(" (%s)", aparm->help);
#endif
    } else if (aparm->help) {
	printf(" <%s>", aparm->help);
	if (aparm->type == CMD_LIST)
	    printf("+");
    } else if (aparm->type == CMD_SINGLE)
	printf(" <arg>");
    else if (aparm->type == CMD_LIST)
	printf(" <arg>+");
}

extern char *AFSVersion;

static int
VersionProc(register struct cmd_syndesc *as, char *arock)
{
    printf("%s\n", AFSVersion);
    return 0;
}

void
PrintSyntax(register struct cmd_syndesc *as)
{
    register int i;
    register struct cmd_parmdesc *tp;

    /* now print usage, from syntax table */
    if (noOpcodes)
	printf("Usage: %s", as->a0name);
    else {
	if (!strcmp(as->name, initcmd_opcode))
	    printf("Usage: %s[%s]", NName(as->a0name, " "), as->name);
	else
	    printf("Usage: %s%s", NName(as->a0name, " "), as->name);
    }

    for (i = 0; i < CMD_MAXPARMS; i++) {
	tp = &as->parms[i];
	if (tp->type == 0)
	    continue;		/* seeked over slot */
	if (tp->flags & CMD_HIDE)
	    continue;		/* skip hidden options */
	printf(" ");
	if (tp->flags & CMD_OPTIONAL)
	    printf("[");
	printf("%s", tp->name);
	PrintParmHelp(tp);
	if (tp->flags & CMD_OPTIONAL)
	    printf("]");
    }
    printf("\n");
}

/* must print newline in any case, to terminate preceding line */
static void
PrintAliases(register struct cmd_syndesc *as)
{
    register struct cmd_syndesc *ts;

    if (as->flags & CMD_ALIAS) {
	ts = as->aliasOf;
	printf("(alias for %s)\n", ts->name);
    } else {
	printf("\n");
	if (!as->nextAlias)
	    return;		/* none, print nothing */
	printf("aliases: ");
	for (as = as->nextAlias; as; as = as->nextAlias) {
	    printf("%s ", as->name);
	}
	printf("\n");
    }
}

void
PrintFlagHelp(register struct cmd_syndesc *as)
{
    register int i;
    register struct cmd_parmdesc *tp;
    size_t flag_width;
    char *flag_prefix;

    /* find flag name length */
    flag_width = 0;
    for (i = 0; i < CMD_MAXPARMS; i++) {
	if (i == CMD_HELPPARM)
	    continue;
	tp = &as->parms[i];
	if (tp->type != CMD_FLAG)
	    continue;
	if (tp->flags & CMD_HIDE)
	    continue;		/* skip hidden options */
	if (!tp->help)
	    continue;

	if (strlen(tp->name) > flag_width)
	    flag_width = strlen(tp->name);
    }

    /* print flag help */
    flag_prefix = "Where:";
    for (i = 0; i < CMD_MAXPARMS; i++) {
	if (i == CMD_HELPPARM)
	    continue;
	tp = &as->parms[i];
	if (tp->type != CMD_FLAG)
	    continue;
	if (tp->flags & CMD_HIDE)
	    continue;		/* skip hidden options */
	if (!tp->help)
	    continue;

	printf("%-7s%-*s  %s\n", flag_prefix, flag_width, tp->name, tp->help);
	flag_prefix = "";
    }
}

static int
AproposProc(register struct cmd_syndesc *as, char *arock)
{
    register struct cmd_syndesc *ts;
    char *tsub;
    int didAny;

    didAny = 0;
    tsub = as->parms[0].items->data;
    for (ts = allSyntax; ts; ts = ts->next) {
	if ((ts->flags & CMD_ALIAS) || (ts->flags & CMD_HIDDEN))
	    continue;
	if (SubString(ts->help, tsub)) {
	    printf("%s: %s\n", ts->name, ts->help);
	    didAny = 1;
	} else if (SubString(ts->name, tsub)) {
	    printf("%s: %s\n", ts->name, ts->help);
	    didAny = 1;
	}
    }
    if (!didAny)
	printf("Sorry, no commands found\n");
    return 0;
}

static int
HelpProc(register struct cmd_syndesc *as, char *arock)
{
    register struct cmd_syndesc *ts;
    register struct cmd_item *ti;
    int ambig;
    int code = 0;

    if (as->parms[0].items == 0) {
	printf("%sCommands are:\n", NName(as->a0name, ": "));
	for (ts = allSyntax; ts; ts = ts->next) {
	    if ((ts->flags & CMD_ALIAS) || (ts->flags & CMD_HIDDEN))
		continue;
	    printf("%-15s %s\n", ts->name, (ts->help ? ts->help : ""));
	}
    } else {
	/* print out individual help topics */
	for (ti = as->parms[0].items; ti; ti = ti->next) {
	    code = 0;
	    ts = FindSyntax(ti->data, &ambig);
	    if (ts && (ts->flags & CMD_HIDDEN))
		ts = 0;		/* no hidden commands */
	    if (ts) {
		/* print out command name and help */
		printf("%s%s: %s ", NName(as->a0name, " "), ts->name,
		       (ts->help ? ts->help : ""));
		ts->a0name = as->a0name;
		PrintAliases(ts);
		PrintSyntax(ts);
		PrintFlagHelp(ts);
	    } else {
		if (!ambig)
		    fprintf(stderr, "%sUnknown topic '%s'\n",
			    NName(as->a0name, ": "), ti->data);
		else {
		    /* ambiguous, list 'em all */
		    fprintf(stderr,
			    "%sAmbiguous topic '%s'; use 'apropos' to list\n",
			    NName(as->a0name, ": "), ti->data);
		}
		code = CMD_UNKNOWNCMD;
	    }
	}
    }
    return (code);
}

int
cmd_SetBeforeProc(int (*aproc) (struct cmd_syndesc * ts, char *beforeRock),
		  char *arock)
{
    beforeProc = aproc;
    beforeRock = arock;
    return 0;
}

int
cmd_SetAfterProc(int (*aproc) (struct cmd_syndesc * ts, char *afterRock),
		 char *arock)
{
    afterProc = aproc;
    afterRock = arock;
    return 0;
}

/* thread on list in alphabetical order */
static int
SortSyntax(struct cmd_syndesc *as)
{
    register struct cmd_syndesc **ld, *ud;

    for (ld = &allSyntax, ud = *ld; ud; ld = &ud->next, ud = *ld) {
	if (strcmp(ud->name, as->name) > 0) {	/* next guy is bigger than us */
	    break;
	}
    }
    /* thread us on the list now */
    *ld = as;
    as->next = ud;
    return 0;
}

struct cmd_syndesc *
cmd_CreateSyntax(char *aname,
		 int (*aproc) (struct cmd_syndesc * ts, char *arock),
		 char *arock, char *ahelp)
{
    register struct cmd_syndesc *td;

    /* can't have two cmds in no opcode mode */
    if (noOpcodes)
	return (struct cmd_syndesc *)0;

    td = (struct cmd_syndesc *)calloc(1, sizeof(struct cmd_syndesc));
    assert(td);
    td->aliasOf = td;		/* treat aliasOf as pointer to real command, no matter what */

    /* copy in name, etc */
    if (aname) {
	td->name = (char *)malloc(strlen(aname) + 1);
	assert(td->name);
	strcpy(td->name, aname);
    } else {
	td->name = NULL;
	noOpcodes = 1;
    }
    if (ahelp) {
	/* Piggy-back the hidden option onto the help string */
	if (ahelp == (char *)CMD_HIDDEN) {
	    td->flags |= CMD_HIDDEN;
	} else {
	    td->help = (char *)malloc(strlen(ahelp) + 1);
	    assert(td->help);
	    strcpy(td->help, ahelp);
	}
    } else
	td->help = NULL;
    td->proc = aproc;
    td->rock = arock;

    SortSyntax(td);

    cmd_Seek(td, CMD_HELPPARM);
    cmd_AddParm(td, "-help", CMD_FLAG, CMD_OPTIONAL, "get detailed help");
    cmd_Seek(td, 0);

    return td;
}

int
cmd_CreateAlias(register struct cmd_syndesc *as, char *aname)
{
    register struct cmd_syndesc *td;

    td = (struct cmd_syndesc *)malloc(sizeof(struct cmd_syndesc));
    assert(td);
    memcpy(td, as, sizeof(struct cmd_syndesc));
    td->name = (char *)malloc(strlen(aname) + 1);
    assert(td->name);
    strcpy(td->name, aname);
    td->flags |= CMD_ALIAS;
    /* if ever free things, make copy of help string, too */

    /* thread on list */
    SortSyntax(td);

    /* thread on alias lists */
    td->nextAlias = as->nextAlias;
    as->nextAlias = td;
    td->aliasOf = as;

    return 0;			/* all done */
}

int
cmd_IsAdministratorCommand(register struct cmd_syndesc *as)
{
    as->flags |= CMD_ADMIN;
    return 0;
}

int
cmd_Seek(register struct cmd_syndesc *as, int apos)
{
    if (apos >= CMD_MAXPARMS)
	return CMD_EXCESSPARMS;
    as->nParms = apos;
    return 0;
}

int
cmd_AddParm(register struct cmd_syndesc *as, char *aname, int atype,
	    afs_int32 aflags, char *ahelp)
{
    register struct cmd_parmdesc *tp;

    if (as->nParms >= CMD_MAXPARMS)
	return CMD_EXCESSPARMS;
    tp = &as->parms[as->nParms++];

    tp->name = (char *)malloc(strlen(aname) + 1);
    assert(tp->name);
    strcpy(tp->name, aname);
    tp->type = atype;
    tp->flags = aflags;
    tp->items = (struct cmd_item *)0;
    if (ahelp) {
	tp->help = (char *)malloc(strlen(ahelp) + 1);
	assert(tp->help);
	strcpy(tp->help, ahelp);
    } else
	tp->help = NULL;
    return 0;
}

/* add a text item to the end of the parameter list */
static int
AddItem(register struct cmd_parmdesc *aparm, register char *aval)
{
    register struct cmd_item *ti, *ni;
    ti = (struct cmd_item *)calloc(1, sizeof(struct cmd_item));
    assert(ti);
    ti->data = (char *)malloc(strlen(aval) + 1);
    assert(ti->data);
    strcpy(ti->data, aval);
    /* now put ti at the *end* of the list */
    if ((ni = aparm->items)) {
	for (; ni; ni = ni->next)
	    if (ni->next == 0)
		break;		/* skip to last one */
	ni->next = ti;
    } else
	aparm->items = ti;	/* we're first */
    return 0;
}

/* skip to next non-flag item, if any */
static int
AdvanceType(register struct cmd_syndesc *as, register afs_int32 aval)
{
    register afs_int32 next;
    register struct cmd_parmdesc *tp;

    /* first see if we should try to grab rest of line for this dude */
    if (as->parms[aval].flags & CMD_EXPANDS)
	return aval;

    /* if not, find next non-flag used slot */
    for (next = aval + 1; next < CMD_MAXPARMS; next++) {
	tp = &as->parms[next];
	if (tp->type != 0 && tp->type != CMD_FLAG)
	    return next;
    }
    return aval;
}

/* discard parameters filled in by dispatch */
static void
ResetSyntax(register struct cmd_syndesc *as)
{
    int i;
    register struct cmd_parmdesc *tp;
    register struct cmd_item *ti, *ni;

    tp = as->parms;
    for (i = 0; i < CMD_MAXPARMS; i++, tp++) {
	switch (tp->type) {
	case CMD_SINGLE:
	case CMD_LIST:
	    /* free whole list in both cases, just for fun */
	    for (ti = tp->items; ti; ti = ni) {
		ni = ti->next;
		free(ti->data);
		free(ti);
	    }
	    break;

	default:
	    break;
	}
	tp->items = (struct cmd_item *)0;
    }
}

/* move the expands flag to the last one in the list */
static int
SetupExpandsFlag(register struct cmd_syndesc *as)
{
    register struct cmd_parmdesc *tp;
    register int last, i;

    last = -1;
    /* find last CMD_LIST type parameter, optional or not, and make it expandable
     * if no other dude is expandable */
    for (i = 0; i < CMD_MAXPARMS; i++) {
	tp = &as->parms[i];
	if (tp->type == CMD_LIST) {
	    if (tp->flags & CMD_EXPANDS)
		return 0;	/* done if already specified */
	    last = i;
	}
    }
    if (last >= 0)
	as->parms[last].flags |= CMD_EXPANDS;
    return 0;
}

/*Take the current argv & argc and alter them so that the initialization opcode is made to appear.  This is used in cases where the initialization opcode is implicitly invoked.*/
static char **
InsertInitOpcode(int *aargc, char **aargv)
{
    char **newargv;		/*Ptr to new, expanded argv space */
    char *pinitopcode;		/*Ptr to space for name of init opcode */
    int i;			/*Loop counter */

    /*Allocate the new argv array, plus one for the new opcode, plus one more for the trailing null pointer */
    newargv = (char **)malloc(((*aargc) + 2) * sizeof(char *));
    if (!newargv) {
	fprintf(stderr, "%s: Can't create new argv array with %d+2 slots\n",
		aargv[0], *aargc);
	return (NULL);
    }

    /*Create space for the initial opcode & fill it in */
    pinitopcode = (char *)malloc(sizeof(initcmd_opcode));
    if (!pinitopcode) {
	fprintf(stderr, "%s: Can't malloc initial opcode space\n", aargv[0]);
	free(newargv);
	return (NULL);
    }
    strcpy(pinitopcode, initcmd_opcode);

    /*Move all the items in the old argv into the new argv, in their proper places */
    for (i = *aargc; i > 1; i--)
	newargv[i] = aargv[i - 1];

    /*Slip in the opcode and the trailing null pointer, and bump the argument count up by one for the new opcode */
    newargv[0] = aargv[0];
    newargv[1] = pinitopcode;
    (*aargc)++;
    newargv[*aargc] = NULL;

    /*Return the happy news */
    return (newargv);

}				/*InsertInitOpcode */

static int
NoParmsOK(register struct cmd_syndesc *as)
{
    register int i;
    register struct cmd_parmdesc *td;

    for (i = 0; i < CMD_MAXPARMS; i++) {
	td = &as->parms[i];
	if (td->type != 0 && !(td->flags & CMD_OPTIONAL)) {
	    /* found a non-optional (e.g. required) parm, so NoParmsOK
	     * is false (some parms are required) */
	    return 0;
	}
    }
    return 1;
}

/* Call the appropriate function, or return syntax error code.  Note: if no opcode is specified, an initialization routine exists, and it has NOT been called before, we invoke the special initialization opcode*/
int
cmd_Dispatch(int argc, char **argv)
{
    char *pname;
    struct cmd_syndesc *ts;
    struct cmd_parmdesc *tparm;
    afs_int32 i, j;
    int curType;
    int positional;
    int ambig;
    static int initd = 0;	/*Is this the first time this routine has been called? */
    static int initcmdpossible = 1;	/*Should be consider parsing the initial command? */

    if (!initd) {
	initd = 1;
	/* Add help, apropos commands once */
	if (!noOpcodes) {
	    ts = cmd_CreateSyntax("help", HelpProc, (char *)0,
				  "get help on commands");
	    cmd_AddParm(ts, "-topic", CMD_LIST, CMD_OPTIONAL, "help string");
	    cmd_AddParm(ts, "-admin", CMD_FLAG, CMD_OPTIONAL, NULL);

	    ts = cmd_CreateSyntax("apropos", AproposProc, (char *)0,
				  "search by help text");
	    cmd_AddParm(ts, "-topic", CMD_SINGLE, CMD_REQUIRED,
			"help string");
	    ts = cmd_CreateSyntax("version", VersionProc, (char *)0,
				  (char *)CMD_HIDDEN);
	    ts = cmd_CreateSyntax("-version", VersionProc, (char *)0,
				  (char *)CMD_HIDDEN);
	    ts = cmd_CreateSyntax("-help", HelpProc, (char *)0,
				  (char *)CMD_HIDDEN);
	    ts = cmd_CreateSyntax("--version", VersionProc, (char *)0,
				  (char *)CMD_HIDDEN);
	    ts = cmd_CreateSyntax("--help", HelpProc, (char *)0,
				  (char *)CMD_HIDDEN);
	}
    }

    /*Remember the program name */
    pname = argv[0];

    if (noOpcodes) {
	if (argc == 1) {
	    if (!NoParmsOK(allSyntax)) {
		printf("%s: Type '%s -help' for help\n", pname, pname);
		return (CMD_USAGE);
	    }
	}
    } else {
	if (argc < 2) {
	    /* if there is an initcmd, don't print an error message, just
	     * setup to use the initcmd below. */
	    if (!(initcmdpossible && FindSyntax(initcmd_opcode, (int *)0))) {
		printf("%s: Type '%s help' or '%s help <topic>' for help\n",
		       pname, pname, pname);
		return (CMD_USAGE);
	    }
	}
    }

    /* Find the syntax descriptor for this command, doing prefix matching properly */
    if (noOpcodes) {
	ts = allSyntax;
    } else {
	ts = (argc < 2 ? 0 : FindSyntax(argv[1], &ambig));
	if (!ts) {
	    /*First token doesn't match a syntax descriptor */
	    if (initcmdpossible) {
		/*If initial command line handling hasn't been done yet,
		 * see if there is a descriptor for the initialization opcode.
		 * Only try this once. */
		initcmdpossible = 0;
		ts = FindSyntax(initcmd_opcode, (int *)0);
		if (!ts) {
		    /*There is no initialization opcode available, so we declare
		     * an error */
		    if (ambig) {
			fprintf(stderr, "%s", NName(pname, ": "));
			fprintf(stderr,
				"Ambiguous operation '%s'; type '%shelp' for list\n",
				argv[1], NName(pname, " "));
		    } else {
			fprintf(stderr, "%s", NName(pname, ": "));
			fprintf(stderr,
				"Unrecognized operation '%s'; type '%shelp' for list\n",
				argv[1], NName(pname, " "));
		    }
		    return (CMD_UNKNOWNCMD);
		} else {
		    /*Found syntax structure for an initialization opcode.  Fix
		     * up argv and argc to relect what the user
		     * ``should have'' typed */
		    if (!(argv = InsertInitOpcode(&argc, argv))) {
			fprintf(stderr,
				"%sCan't insert implicit init opcode into command line\n",
				NName(pname, ": "));
			return (CMD_INTERNALERROR);
		    }
		}
	    } /*Initial opcode not yet attempted */
	    else {
		/* init cmd already run and no syntax entry found */
		if (ambig) {
		    fprintf(stderr, "%s", NName(pname, ": "));
		    fprintf(stderr,
			    "Ambiguous operation '%s'; type '%shelp' for list\n",
			    argv[1], NName(pname, " "));
		} else {
		    fprintf(stderr, "%s", NName(pname, ": "));
		    fprintf(stderr,
			    "Unrecognized operation '%s'; type '%shelp' for list\n",
			    argv[1], NName(pname, " "));
		}
		return CMD_UNKNOWNCMD;
	    }
	}			/*Argv[1] is not a valid opcode */
    }				/*Opcodes are defined */

    /* Found the descriptor; start parsing.  curType is the type we're trying to parse */
    curType = 0;

    /* We start off parsing in "positional" mode, where tokens are put in
     * slots positionally.  If we find a name that takes args, we go
     * out of positional mode, and from that point on, expect a switch
     * before any particular token. */
    positional = 1;		/* Are we still in the positional region of the cmd line? */
    i = noOpcodes ? 1 : 2;
    SetupExpandsFlag(ts);
    for (; i < argc; i++) {
	/* Only tokens that start with a hyphen and are not followed by a digit
	 * are considered switches.  This allow negative numbers. */
	if ((argv[i][0] == '-') && !isdigit(argv[i][1])) {
	    /* Find switch */
	    j = FindType(ts, argv[i]);
	    if (j < 0) {
		fprintf(stderr,
			"%sUnrecognized or ambiguous switch '%s'; type ",
			NName(pname, ": "), argv[i]);
		if (noOpcodes)
		    fprintf(stderr, "'%s -help' for detailed help\n",
			    argv[0]);
		else
		    fprintf(stderr, "'%shelp %s' for detailed help\n",
			    NName(argv[0], " "), ts->name);
		ResetSyntax(ts);
		return (CMD_UNKNOWNSWITCH);
	    }
	    if (j >= CMD_MAXPARMS) {
		fprintf(stderr, "%sInternal parsing error\n",
			NName(pname, ": "));
		ResetSyntax(ts);
		return (CMD_INTERNALERROR);
	    }
	    if (ts->parms[j].type == CMD_FLAG) {
		ts->parms[j].items = (struct cmd_item *)&dummy;
	    } else {
		positional = 0;
		curType = j;
		ts->parms[j].flags |= CMD_PROCESSED;
	    }
	} else {
	    /* Try to fit in this descr */
	    if (curType >= CMD_MAXPARMS) {
		fprintf(stderr, "%sToo many arguments\n", NName(pname, ": "));
		ResetSyntax(ts);
		return (CMD_TOOMANY);
	    }
	    tparm = &ts->parms[curType];

	    if ((tparm->type == 0) ||	/* No option in this slot */
		(tparm->type == CMD_FLAG)) {	/* A flag (not an argument */
		/* skipped parm slot */
		curType++;	/* Skip this slot and reprocess this parm */
		i--;
		continue;
	    }

	    if (!(tparm->flags & CMD_PROCESSED) && (tparm->flags & CMD_HIDE)) {
		curType++;	/* Skip this slot and reprocess this parm */
		i--;
		continue;
	    }

	    if (tparm->type == CMD_SINGLE) {
		if (tparm->items) {
		    fprintf(stderr, "%sToo many values after switch %s\n",
			    NName(pname, ": "), tparm->name);
		    ResetSyntax(ts);
		    return (CMD_NOTLIST);
		}
		AddItem(tparm, argv[i]);	/* Add to end of list */
	    } else if (tparm->type == CMD_LIST) {
		AddItem(tparm, argv[i]);	/* Add to end of list */
	    }
	    /* Now, if we're in positional mode, advance to the next item */
	    if (positional)
		curType = AdvanceType(ts, curType);
	}
    }

    /* keep track of this for messages */
    ts->a0name = argv[0];

    /* If we make it here, all the parameters are filled in.  Check to see if this
     * is a -help version.  Must do this before checking for all required parms,
     * otherwise it is a real nuisance */
    if (ts->parms[CMD_HELPPARM].items) {
	PrintSyntax(ts);
	/* Display full help syntax if we don't have subcommands */
	if (noOpcodes)
	    PrintFlagHelp(ts);
	ResetSyntax(ts);
	return 0;
    }

    /* Parsing done, see if we have all of our required parameters */
    for (i = 0; i < CMD_MAXPARMS; i++) {
	tparm = &ts->parms[i];
	if (tparm->type == 0)
	    continue;		/* Skipped parm slot */
	if ((tparm->flags & CMD_PROCESSED) && tparm->items == 0) {
	    fprintf(stderr, "%s The field '%s' isn't completed properly\n",
		    NName(pname, ": "), tparm->name);
	    ResetSyntax(ts);
	    tparm->flags &= ~CMD_PROCESSED;
	    return (CMD_TOOFEW);
	}
	if (!(tparm->flags & CMD_OPTIONAL) && tparm->items == 0) {
	    fprintf(stderr, "%sMissing required parameter '%s'\n",
		    NName(pname, ": "), tparm->name);
	    ResetSyntax(ts);
	    tparm->flags &= ~CMD_PROCESSED;
	    return (CMD_TOOFEW);
	}
	tparm->flags &= ~CMD_PROCESSED;
    }

    /*
     * Before calling the beforeProc and afterProc and all the implications 
     * from those calls, check if the help procedure was called and call it now.
     */
    if ((ts->proc == HelpProc) || (ts->proc == AproposProc)) {
	i = (*ts->proc) (ts, ts->rock);
	ResetSyntax(ts);
	return (i);
    }

    /* Now, we just call the procedure and return */
    if (beforeProc)
	i = (*beforeProc) (ts, beforeRock);
    else
	i = 0;
    if (i) {
	ResetSyntax(ts);
	return (i);
    }
    i = (*ts->proc) (ts, ts->rock);
    if (afterProc)
	(*afterProc) (ts, afterRock);
    ResetSyntax(ts);		/* Reset and free things */
    return (i);
}

/* free token list returned by parseLine */
static int
FreeTokens(register struct cmd_token *alist)
{
    register struct cmd_token *nlist;
    for (; alist; alist = nlist) {
	nlist = alist->next;
	free(alist->key);
	free(alist);
    }
    return 0;
}

/* free an argv list returned by parseline */
int
cmd_FreeArgv(register char **argv)
{
    register char *tp;
    for (tp = *argv; tp; argv++, tp = *argv)
	free(tp);
    return 0;
}

/* copy back the arg list to the argv array, freeing the cmd_tokens as you go; the actual
    data is still malloc'd, and will be freed when the caller calls cmd_FreeArgv
    later on */
#define INITSTR ""
static int
CopyBackArgs(register struct cmd_token *alist, register char **argv,
	     afs_int32 * an, afs_int32 amaxn)
{
    register struct cmd_token *next;
    afs_int32 count;

    count = 0;
    if (amaxn <= 1)
	return CMD_TOOMANY;
    *argv = (char *)malloc(strlen(INITSTR) + 1);
    assert(*argv);
    strcpy(*argv, INITSTR);
    amaxn--;
    argv++;
    count++;
    while (alist) {
	if (amaxn <= 1)
	    return CMD_TOOMANY;	/* argv is too small for his many parms. */
	*argv = alist->key;
	next = alist->next;
	free(alist);
	alist = next;
	amaxn--;
	argv++;
	count++;
    }
    *(argv++) = 0;		/* use last slot for terminating null */
    /* don't count terminating null */
    *an = count;
    return 0;
}

static int
quote(register int x)
{
    if (x == '"' || x == 39 /* single quote */ )
	return 1;
    else
	return 0;
}

static int
space(register int x)
{
    if (x == 0 || x == ' ' || x == '\t' || x == '\n')
	return 1;
    else
	return 0;
}

int
cmd_ParseLine(char *aline, char **argv, afs_int32 * an, afs_int32 amaxn)
{
    char tbuffer[256];
    register char *tptr = 0;
    int inToken, inQuote;
    struct cmd_token *first, *last;
    register struct cmd_token *ttok;
    register int tc;

    inToken = 0;		/* not copying token chars at start */
    first = (struct cmd_token *)0;
    last = (struct cmd_token *)0;
    inQuote = 0;		/* not in a quoted string */
    while (1) {
	tc = *aline++;
	if (tc == 0 || (!inQuote && space(tc))) {	/* terminating null gets us in here, too */
	    if (inToken) {
		inToken = 0;	/* end of this token */
		if (!tptr)
		    return -1;	/* should never get here */
		else
		    *tptr++ = 0;
		ttok = (struct cmd_token *)malloc(sizeof(struct cmd_token));
		assert(ttok);
		ttok->next = (struct cmd_token *)0;
		ttok->key = (char *)malloc(strlen(tbuffer) + 1);
		assert(ttok->key);
		strcpy(ttok->key, tbuffer);
		if (last) {
		    last->next = ttok;
		    last = ttok;
		} else
		    last = ttok;
		if (!first)
		    first = ttok;
	    }
	} else {
	    /* an alpha character */
	    if (!inToken) {
		tptr = tbuffer;
		inToken = 1;
	    }
	    if (tptr - tbuffer >= sizeof(tbuffer)) {
		FreeTokens(first);
		return CMD_TOOBIG;	/* token too long */
	    }
	    if (quote(tc)) {
		/* hit a quote, toggle inQuote flag but don't insert character */
		inQuote = !inQuote;
	    } else {
		/* insert character */
		*tptr++ = tc;
	    }
	}
	if (tc == 0) {
	    /* last token flushed 'cause space(0) --> true */
	    if (last)
		last->next = (struct cmd_token *)0;
	    return CopyBackArgs(first, argv, an, amaxn);
	}
    }
}
