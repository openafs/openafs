/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __CMD_INCL__
#define	__CMD_INCL__	    1

/* parmdesc types */
#define	CMD_FLAG	1	/* no parms */
#define	CMD_SINGLE	2	/* one parm */
#define	CMD_LIST	3	/* two parms */
#define CMD_SINGLE_OR_FLAG 4	/* one parm or flag */

/* syndesc flags */
#define	CMD_ALIAS	1	/* this is an alias */
#define CMD_HIDDEN      4	/* A hidden command - similar to CMD_HIDE */
#define CMD_IMPLICIT    8	/* A built-in command - apropos, help, version (and variations) */

#define CMD_HELPPARM	(CMD_MAXPARMS-1)	/* last one is used by -help switch */
#define	CMD_MAXPARMS	64	/* max number of parm types to a cmd line */

/* parse items are here */
struct cmd_item {
    struct cmd_item *next;
    char *data;
};

struct cmd_parmdesc {
    char *name;			/* switch name */
    int type;			/* flag, single or list */
    struct cmd_item *items;	/* list of cmd items */
    afs_int32 flags;		/* flags */
    char *help;			/* optional help descr */
    struct cmd_item *aliases;   /* optional aliases */
};

/* cmd_parmdesc flags */
#define	CMD_REQUIRED	    0
#define	CMD_OPTIONAL	    1
#define	CMD_EXPANDS	    2	/* if list, try to eat tokens through eoline, instead of just 1 */
#define CMD_HIDE            4	/* A hidden option */
#define	CMD_PROCESSED	    8
#define CMD_NOABBRV	   16   /* Abbreviation not supported */

struct cmd_syndesc {
    struct cmd_syndesc *next;	/* next one in system list */
    struct cmd_syndesc *nextAlias;	/* next in alias chain */
    struct cmd_syndesc *aliasOf;	/* back ptr for aliases */
    char *name;			/* subcommand name */
    char *a0name;		/* command name from argv[0] */
    char *help;			/* help description */
    int (*proc) (struct cmd_syndesc * ts, void *arock);
    void *rock;
    int nParms;			/* number of parms */
    afs_uint32 flags;		/* random flags */
    struct cmd_parmdesc parms[CMD_MAXPARMS];	/* parms themselves */
};

extern struct cmd_syndesc *cmd_CreateSyntax(char *namep,
					    int (*aprocp) (struct cmd_syndesc
							   * ts, void *arock),
					    void *rockp, afs_uint32 aflags, char *helpp);
extern int
  cmd_SetBeforeProc(int (*aproc) (struct cmd_syndesc * ts, void *beforeRock),
		    void *arock);
extern int
  cmd_SetAfterProc(int (*aproc) (struct cmd_syndesc * ts, void *afterRock),
		   void *arock);
extern int cmd_CreateAlias(struct cmd_syndesc *as, char *aname);
extern int cmd_Seek(struct cmd_syndesc *as, int apos);
extern int cmd_AddParm(struct cmd_syndesc *as, char *aname, int atype,
		       afs_int32 aflags, char *ahelp);
extern int cmd_AddParmAtOffset(struct cmd_syndesc *as, int ref, char *name,
			       int atype, afs_int32 aflags, char *ahelp);
extern int cmd_AddParmAlias(struct cmd_syndesc *as, int pos, char *alias);
extern int cmd_Dispatch(int argc, char **argv);
extern int cmd_FreeArgv(char **argv);
extern int cmd_ParseLine(char *aline, char **argv, afs_int32 * an,
			 afs_int32 amaxn);
extern void cmd_DisablePositionalCommands(void);
extern void cmd_DisableAbbreviations(void);
extern void PrintSyntax(struct cmd_syndesc *as);
extern void PrintFlagHelp(struct cmd_syndesc *as);

extern int cmd_Parse(int argc, char **argv, struct cmd_syndesc **outsyntax);
extern void cmd_FreeOptions(struct cmd_syndesc **ts);
extern int cmd_OptionAsInt(struct cmd_syndesc *syn, int pos, int *value);
extern int cmd_OptionAsUint(struct cmd_syndesc *, int, unsigned int *);
extern int cmd_OptionAsString(struct cmd_syndesc *syn, int pos, char **value);
extern int cmd_OptionAsList(struct cmd_syndesc *syn, int pos, struct cmd_item **);
extern int cmd_OptionAsFlag(struct cmd_syndesc *syn, int pos, int *value);
extern int cmd_OptionPresent(struct cmd_syndesc *syn, int pos);

/* Config files */

struct cmd_config_binding {
    enum { cmd_config_string, cmd_config_list } type;
    char *name;
    struct cmd_config_binding *next;
    union {
	char *string;
	struct cmd_config_binding *list;
	void *generic;
    } u;
};

/* Raw config file access */
typedef struct cmd_config_binding cmd_config_binding;
typedef struct cmd_config_binding cmd_config_section;

extern int cmd_RawConfigParseFileMulti(const char *, cmd_config_section **);
extern int cmd_RawConfigParseFile(const char *, cmd_config_section **);
extern int cmd_RawConfigFileFree(cmd_config_section *s);
extern const char* cmd_RawConfigGetString(const cmd_config_section *,
					  const char *, ...);
extern int cmd_RawConfigGetBool(const cmd_config_section *, int, ...);
extern int cmd_RawConfigGetInt(const cmd_config_section *, int, ...);
extern const cmd_config_binding *cmd_RawConfigGetList
	(const cmd_config_section *, ...);

extern int cmd_OpenConfigFile(const char *file);
extern void cmd_SetCommandName(const char *command);
extern const cmd_config_section *cmd_RawFile(void);
extern const cmd_config_section *cmd_RawSection(void);

#endif /* __CMD_INCL__ */
