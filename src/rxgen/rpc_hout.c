/* @(#)rpc_hout.c	1.2 87/11/30 3.9 RPCSRC */
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
 * rpc_hout.c, Header file outputter for the RPC protocol compiler 
 * Copyright (C) 1987, Sun Microsystems, Inc. 
 */
#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "rpc_scan.h"
#include "rpc_parse.h"
#include "rpc_util.h"

/* Static declarations */
static void pconstdef(definition * def);
static void pstructdef(definition * def);
static void puniondef(definition * def);
static void puldefine(char *name, char *num);
static int define_printed(proc_list * stop, version_list * start);
static void pprogramdef(definition * def);
static void psproc1(definition * defp, int callTconnF, char *type,
		    char *prefix, int iomask);
static void psprocdef(definition * defp);
static void penumdef(definition * def);
static void ptypedef(definition * def);
static void pdeclaration(char *name, declaration * dec, int tab);
static int undefined2(char *type, char *stop);

/*
 * Print the C-version of an xdr definition 
 */
void
print_datadef(definition * def)
{
    if (Sflag)
	scan_print = 0;
    if ((def->def_kind != DEF_CONST) && (!IsRxgenDefinition(def))) {
	f_print(fout, "\n");
    }
    switch (def->def_kind) {
    case DEF_CUSTOMIZED:
    case DEF_STRUCT:
	pstructdef(def);
	break;
    case DEF_UNION:
	puniondef(def);
	break;
    case DEF_ENUM:
	penumdef(def);
	break;
    case DEF_TYPEDEF:
	ptypedef(def);
	break;
    case DEF_PROGRAM:
	pprogramdef(def);
	break;
    case DEF_PROC:
	psprocdef(def);
	break;
    case DEF_CONST:
	pconstdef(def);
	break;
    }
    if (def->def_kind != DEF_PROGRAM && def->def_kind != DEF_CONST
	&& (!IsRxgenDefinition(def))) {
	f_print(fout, "bool_t xdr_%s(XDR *xdrs, %s *objp);\n", def->def_name,
		def->def_name);
    }
    if (def->def_kind != DEF_CONST && (!IsRxgenDefinition(def))) {
	f_print(fout, "\n");
    }
    if (Sflag)
	scan_print = 1;
}

static void
pconstdef(definition * def)
{
    pdefine(def->def_name, def->def.co);
}

static void
pstructdef(definition * def)
{
    decl_list *l;
    char *name = def->def_name;

    f_print(fout, "struct %s {\n", name);
    for (l = def->def.st.decls; l != NULL; l = l->next) {
	pdeclaration(name, &l->decl, 1);
    }
    f_print(fout, "};\n");
    f_print(fout, "typedef struct %s %s;\n", name, name);
}

static void
puniondef(definition * def)
{
    case_list *l;
    char *name = def->def_name;
    declaration *decl;

    f_print(fout, "struct %s {\n", name);
    decl = &def->def.un.enum_decl;
    if (streq(decl->type, "bool")) {
	f_print(fout, "\tbool_t %s;\n", decl->name);
    } else {
	f_print(fout, "\t%s %s;\n", decl->type, decl->name);
    }
    f_print(fout, "\tunion {\n");
    for (l = def->def.un.cases; l != NULL; l = l->next) {
	pdeclaration(name, &l->case_decl, 2);
    }
    decl = def->def.un.default_decl;
    if (decl && !streq(decl->type, "void")) {
	pdeclaration(name, decl, 2);
    }
    f_print(fout, "\t} %s_u;\n", name);
    f_print(fout, "};\n");
    f_print(fout, "typedef struct %s %s;\n", name, name);
    STOREVAL(&uniondef_defined, def);
}


void
pdefine(char *name, char *num)
{
    f_print(fout, "#define %s %s\n", name, num);
}

static void
puldefine(char *name, char *num)
{
    f_print(fout, "#define %s ((afs_uint32)%s)\n", name, num);
}

static int
define_printed(proc_list * stop, version_list * start)
{
    version_list *vers;
    proc_list *proc;

    for (vers = start; vers != NULL; vers = vers->next) {
	for (proc = vers->procs; proc != NULL; proc = proc->next) {
	    if (proc == stop) {
		return (0);
	    } else if (streq(proc->proc_name, stop->proc_name)) {
		return (1);
	    }
	}
    }
    abort();
    /* NOTREACHED */
}


static void
pprogramdef(definition * def)
{
    version_list *vers;
    proc_list *proc;

    puldefine(def->def_name, def->def.pr.prog_num);
    for (vers = def->def.pr.versions; vers != NULL; vers = vers->next) {
	puldefine(vers->vers_name, vers->vers_num);
	for (proc = vers->procs; proc != NULL; proc = proc->next) {
	    if (!define_printed(proc, def->def.pr.versions)) {
		puldefine(proc->proc_name, proc->proc_num);
	    }
	    pprocdef(proc, vers);
	}
    }
}

static void
psproc1(definition * defp, int callTconnF, char *type, char *prefix,
	int iomask)
{
    proc1_list *plist;

    f_print(fout, "\nextern %s %s%s%s(\n", type, prefix, defp->pc.proc_prefix,
	    defp->pc.proc_name);

    if (callTconnF) {
	f_print(fout, "\t/*IN */ struct rx_call *z_call");
    } else {
	f_print(fout, "\t/*IN */ struct rx_connection *z_conn");
    }

    for (plist = defp->pc.plists; plist; plist = plist->next) {
	if (plist->component_kind == DEF_PARAM
	    && (iomask & (1 << plist->pl.param_kind))) {
	    switch (plist->pl.param_kind) {
	    case DEF_INPARAM:
		f_print(fout, ",\n\t/*IN */ ");
		break;
	    case DEF_OUTPARAM:
		f_print(fout, ",\n\t/*OUT*/ ");
		break;
	    case DEF_INOUTPARAM:
		f_print(fout, ",\n\t/*I/O*/ ");
		break;
	    }
	    if (plist->pl.param_flag & OUT_STRING) {
		f_print(fout, "%s *%s", plist->pl.param_type,
			plist->pl.param_name);
	    } else {
		f_print(fout, "%s %s", plist->pl.param_type,
			plist->pl.param_name);
	    }
	}
    }
    f_print(fout, ");\n");
}

static void
psprocdef(definition * defp)
{
    int split_flag = defp->pc.split_flag;
    int multi_flag = defp->pc.multi_flag;

    if (split_flag || multi_flag) {
	psproc1(defp, 1, "int", "Start",
		(1 << DEF_INPARAM) | (1 << DEF_INOUTPARAM));
	psproc1(defp, 1, "int", "End",
		(1 << DEF_OUTPARAM) | (1 << DEF_INOUTPARAM));
    } else {
	psproc1(defp, 0, "int", "", 0xFFFFFFFF);
    }

    if (*ServerPrefix)
	psproc1(defp, 1, "afs_int32", ServerPrefix, 0xFFFFFFFF);
}


void
pprocdef(proc_list * proc, version_list * vp)
{
    f_print(fout, "extern ");
    if (proc->res_prefix) {
	if (streq(proc->res_prefix, "enum")) {
	    f_print(fout, "enum ");
	} else {
	    f_print(fout, "struct ");
	}
    }
    if (streq(proc->res_type, "bool")) {
	f_print(fout, "bool_t *");
    } else if (streq(proc->res_type, "string")) {
	f_print(fout, "char **");
    } else {
	f_print(fout, "%s *", fixtype(proc->res_type));
    }
    pvname(proc->proc_name, vp->vers_num);
    f_print(fout, "();\n");
}

static void
penumdef(definition * def)
{
    char *name = def->def_name;
    enumval_list *l;
    char *last = NULL;
    int count = 0;

    f_print(fout, "enum %s {\n", name);
    for (l = def->def.en.vals; l != NULL; l = l->next) {
	f_print(fout, "\t%s", l->name);
	if (l->assignment) {
	    f_print(fout, " = %s", l->assignment);
	    last = l->assignment;
	    count = 1;
	} else {
	    if (last == NULL) {
		f_print(fout, " = %d", count++);
	    } else {
		f_print(fout, " = %s + %d", last, count++);
	    }
	}
	f_print(fout, ",\n");
    }
    f_print(fout, "};\n");
    f_print(fout, "typedef enum %s %s;\n", name, name);
}

static void
ptypedef(definition * def)
{
    char *name = def->def_name;
    char *old = def->def.ty.old_type;
    char prefix[8];		/* enough to contain "struct ", including NUL */
    relation rel = def->def.ty.rel;


    if (!streq(name, old)) {
	if (streq(old, "string")) {
	    old = "char";
	    rel = REL_POINTER;
	} else if (streq(old, "opaque")) {
	    old = "char";
	} else if (streq(old, "bool")) {
	    old = "bool_t";
	}
	if (undefined2(old, name) && def->def.ty.old_prefix) {
	    s_print(prefix, "%s ", def->def.ty.old_prefix);
	} else {
	    prefix[0] = 0;
	}
	f_print(fout, "typedef ");
	switch (rel) {
	case REL_ARRAY:
	    f_print(fout, "struct %s {\n", name);
	    f_print(fout, "\tu_int %s_len;\n", name);
	    f_print(fout, "\t%s%s *%s_val;\n", prefix, old, name);
	    f_print(fout, "} %s", name);
	    break;
	case REL_POINTER:
	    f_print(fout, "%s%s *%s", prefix, old, name);
	    break;
	case REL_VECTOR:
	    f_print(fout, "%s%s %s[%s]", prefix, old, name,
		    def->def.ty.array_max);
	    break;
	case REL_ALIAS:
	    f_print(fout, "%s%s %s", prefix, old, name);
	    break;
	}
	def->pc.rel = rel;
	STOREVAL(&typedef_defined, def);
	f_print(fout, ";\n");
    }
}


static void
pdeclaration(char *name, declaration * dec, int tab)
{
    char buf[8];		/* enough to hold "struct ", include NUL */
    char *prefix;
    char *type;

    if (streq(dec->type, "void")) {
	return;
    }
    tabify(fout, tab);
    if (streq(dec->type, name) && !dec->prefix) {
	f_print(fout, "struct ");
    }
    if (streq(dec->type, "string")) {
	f_print(fout, "char *%s", dec->name);
    } else {
	prefix = "";
	if (streq(dec->type, "bool")) {
	    type = "bool_t";
	} else if (streq(dec->type, "opaque")) {
	    type = "char";
	} else {
	    if (dec->prefix) {
		s_print(buf, "%s ", dec->prefix);
		prefix = buf;
	    }
	    type = dec->type;
	}
	switch (dec->rel) {
	case REL_ALIAS:
	    f_print(fout, "%s%s %s", prefix, type, dec->name);
	    break;
	case REL_VECTOR:
	    f_print(fout, "%s%s %s[%s]", prefix, type, dec->name,
		    dec->array_max);
	    break;
	case REL_POINTER:
	    f_print(fout, "%s%s *%s", prefix, type, dec->name);
	    break;
	case REL_ARRAY:
	    f_print(fout, "struct %s {\n", dec->name);
	    tabify(fout, tab);
	    f_print(fout, "\tu_int %s_len;\n", dec->name);
	    tabify(fout, tab);
	    f_print(fout, "\t%s%s *%s_val;\n", prefix, type, dec->name);
	    tabify(fout, tab);
	    f_print(fout, "} %s", dec->name);
	    break;
	}
    }
    f_print(fout, ";\n");
}



static int
undefined2(char *type, char *stop)
{
    list *l;
    definition *def;

    for (l = defined; l != NULL; l = l->next) {
	def = (definition *) l->val;
	if (def->def_kind != DEF_PROGRAM) {
	    if (streq(def->def_name, stop)) {
		return (1);
	    } else if (streq(def->def_name, type)) {
		return (0);
	    }
	}
    }
    return (1);
}
