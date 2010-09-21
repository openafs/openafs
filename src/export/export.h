/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * export.h -	definitions for the EXPORT kernel extension
 */

/*
 * EXPORT kernel extension configuration parameters
 */
struct k_conf {
    u_int nsyms;		/* # of symbols                 */
    u_int symt_sz;		/* size of symbol table         */
    u_int str_sz;		/* size of string table         */
    caddr_t symtab;		/* user address of symtab       */
    caddr_t strtab;		/* user address of string table */
};

/*
 * kernel function import
 */
struct k_func {
    void *(**fpp) ();		/* ^ to ^ to function we import */
    char *name;			/* ^ to symbol name             */
#if defined(__XCOFF64__) || defined(AFS_64BIT_KERNEL)
    u_int64 fdesc[3];		/* function descriptor storage  */
#else
    u_int fdesc[3];		/* function descriptor storage  */
#endif
};

/*
 * kernel variable import
 */
struct k_var {
    void *varp;			/* ^ to surrogate variable      */
    char *name;			/* ^ to symbol name             */
};
