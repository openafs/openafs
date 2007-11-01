/* @(#)rpc_util.h	1.2 87/11/24 3.9 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * rpc_util.h, Useful definitions for the RPC protocol compiler 
 * Copyright (C) 1987, Sun Microsystems, Inc. 
 */

#include "rxgen_consts.h"
#define alloc(size)		malloc((unsigned)(size))
#define ALLOC(object)   (object *) malloc(sizeof(object))

#define s_print	(void) sprintf
#define	f_print	if (scan_print) (void) fprintf

struct list {
    char *val;
    struct list *next;
};
typedef struct list list;

#define MAXLINESIZE 1024


/* PROTOTYPES */

/* rpc_main.c */
extern char *prefix;
extern int nincludes;
extern char *OutFileFlag;
extern char OutFile[];
extern char Sflag, Cflag, hflag, cflag, kflag, uflag;
extern char ansic_flag;
extern char zflag;
extern char xflag;
extern char yflag;
extern int debug;


/* rpc_util.c */
extern char curline[];
extern char *where;
extern int linenum;
extern char *infilename;
extern char *outfiles[];
extern int nfiles;
extern FILE *fout;
extern FILE *fin;
extern list *defined;

extern void reinitialize(void);
extern int streq(char *a, char *b);
extern char *findval(list * lst, char *val,
		     int (*cmp) (definition * def, char *type));
extern void storeval(list ** lstp, char *val);
extern char *fixtype(char *type);
extern char *stringfix(char *type);
extern void ptype(char *prefix, char *type, int follow);
extern int isvectordef(char *type, relation rel);
extern void pvname(char *pname, char *vnum);
extern void error(char *msg);
extern void crash(void);
extern void record_open(char *file);
extern void expected1(tok_kind exp1);
extern void expected2(tok_kind exp1, tok_kind exp2);
extern void expected3(tok_kind exp1, tok_kind exp2, tok_kind exp3);
extern void expected4(tok_kind exp1, tok_kind exp2, tok_kind exp3,
		      tok_kind exp4);
extern void tabify(FILE * f, int tab);

#define STOREVAL(list,item)	\
	storeval(list,(char *)item)

#define FINDVAL(list,item,finder) \
	findval(list, (char *) item, finder)

/* rpc_clntout.c */
extern void write_stubs(void);

/* rpc_cout.c */
extern void emit(definition * def);
extern void print_param(declaration * dec);

/* rpc_hout.c */
extern void print_datadef(definition * def);
extern void pdefine(char *name, char *num);
extern void pprocdef(proc_list * proc, version_list * vp);

/* rpc_parse.c */
extern list *proc_defined[MAX_PACKAGES], *special_defined, *typedef_defined,
    *uniondef_defined;
extern char *SplitStart;
extern char *SplitEnd;
extern char *MasterPrefix;
extern char *ServerPrefix;
extern char *PackagePrefix[];
extern char *PackageStatIndex[];
extern int no_of_stat_funcs;
extern int no_of_stat_funcs_header[];
extern int no_of_opcodes[], master_no_of_opcodes;
extern int lowest_opcode[], master_lowest_opcode;
extern int highest_opcode[], master_highest_opcode;
extern int master_opcodenumber;
extern int opcodesnotallowed[];
extern int combinepackages;
extern int PackageIndex;
extern int PerProcCounter;
extern int Multi_Init;
extern char function_list[MAX_PACKAGES]
    [MAX_FUNCTIONS_PER_PACKAGE]
    [MAX_FUNCTION_NAME_LEN];
extern int function_list_index;

extern definition *get_definition(void);

extern void er_Proc_CodeGeneration(void);
extern void h_opcode_stats(void);
extern void generate_multi_macros(definition * defp);
extern int IsRxgenToken(token * tokp);
extern int IsRxgenDefinition(definition * def);




extern proc1_list *Proc_list, **Proc_listp;


/* rpc_svcout.c */
extern int nullproc(proc_list * proc);
extern void write_programs(char *storage);
extern void write_rest(void);
extern void write_most(void);
extern void write_register(char *transp);

/* rpc_scan.c */
extern int pushed;
extern token lasttok;
extern int scan_print;

extern void scan(tok_kind expect, token * tokp);
extern void scan2(tok_kind expect1, tok_kind expect2, token * tokp);
extern void scan3(tok_kind expect1, tok_kind expect2, tok_kind expect3,
		  token * tokp);
extern void scan4(tok_kind expect1, tok_kind expect2, tok_kind expect3,
		  tok_kind expect4, token * tokp);
extern void scan_num(token * tokp);
extern void peek(token * tokp);
extern int peekscan(tok_kind expect, token * tokp);
extern void get_token(token * tokp);
extern void unget_token(token * tokp);
extern void findkind(char **mark, token * tokp);
extern void printdirective(char *line);
