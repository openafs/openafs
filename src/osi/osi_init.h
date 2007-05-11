/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_INIT_H
#define _OSI_INIT_H 1


/*
 * osi abstraction
 * modular initialization
 *
 * process initialization function support:
 *
 *  OSI_INIT_FUNC_DECL(sym)
 *    -- declare an initialization function of name $sym$
 *
 *  OSI_INIT_FUNC_PROTOTYPE(sym)
 *    -- emit a prototype for an initialization function of name $sym$
 *
 *  OSI_INIT_FUNC_STATIC_DECL(sym)
 *    -- declare a static initialization function of name $sym$
 *
 *  OSI_INIT_FUNC_STATIC_PROTOTYPE(sym)
 *    -- emit a prototype for a static initialization function of name $sym$
 *
 *  osi_null_init_func();
 *    -- no-op init func; just returns OSI_OK
 *
 *
 * process finalization function support:
 *
 *  OSI_FINI_FUNC_DECL(sym)
 *    -- declare an finalization function of name $sym$
 *
 *  OSI_FINI_FUNC_PROTOTYPE(sym)
 *    -- emit a prototype for an finalization function of name $sym$
 *
 *  OSI_FINI_FUNC_STATIC_DECL(sym)
 *    -- declare a static finalization function of name $sym$
 *
 *  OSI_FINI_FUNC_STATIC_PROTOTYPE(sym)
 *    -- emit a prototype for a static finalization function of name $sym$
 *
 *  osi_null_fini_func();
 *    -- no-op fini func; just returns OSI_OK
 *
 */

typedef osi_result osi_init_func_t(void);
typedef osi_result osi_fini_func_t(void);

struct osi_init_module_linkage {
    osi_init_func_t * init_fp;
    osi_fini_func_t * fini_fp;
    osi_result res;
    struct osi_init_module_linkage ** deps;
};
typedef struct osi_init_module_linkage osi_init_module_linkage_t;

#define OSI_INIT_FUNC_PROTOTYPE(sym) osi_extern osi_result sym (void)
#define OSI_FINI_FUNC_PROTOTYPE(sym) osi_extern osi_result sym (void)
#define OSI_INIT_FUNC_STATIC_PROTOTYPE(sym) osi_static osi_result sym (void)
#define OSI_FINI_FUNC_STATIC_PROTOTYPE(sym) osi_static osi_result sym (void)
#define OSI_INIT_FUNC_DECL(sym) osi_result sym (void)
#define OSI_FINI_FUNC_DECL(sym) osi_result sym (void)
#define OSI_INIT_FUNC_STATIC_DECL(sym) osi_static osi_result sym (void)
#define OSI_FINI_FUNC_STATIC_DECL(sym) osi_static osi_result sym (void)


#endif /* _OSI_INIT_H */
