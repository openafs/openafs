/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_LIB_INIT_H
#define _OSI_OSI_LIB_INIT_H 1

/*
 * osi 
 * library initialization support
 *
 * please note that you may only have one
 * initialization function per compilation unit
 *
 * macro interfaces:
 *
 *  osi_lib_init_prototype(symbol name)
 *    -- provide prototype for a compilation unit's initializer
 *       passed symbol name is merely a hint
 *
 *  osi_lib_init_decl(symbol name)
 *    -- provide a definition for a compilation unit's initializer
 *       passed symbol name is merely a hint
 *
 *
 * example usage:
 *
 *  osi_lib_init_prototype(example_init);
 *
 *  osi_lib_init_decl(example_init)
 *  {
 *       osi_Msg("hello world\n");
 *  }
 */


/*
 * _init support
 */
#if defined(__osi_env_gcc)
#define _osi_lib_init_sym(name) name
#define _osi_lib_init_prototype(name) \
    osi_static void _osi_lib_init_sym(name) (void) __attribute__((constructor))
#define _osi_lib_init_decl(name) \
    osi_static void _osi_lib_init_sym(name) (void)
#else
#define _osi_lib_init_sym(name) __osi_lib_obj_ctor
#define _osi_lib_init_prototype(name) \
    osi_static void _osi_lib_init_sym(name) (void)
#define _osi_lib_init_decl(name) \
    osi_static void _osi_lib_init_sym(name) (void)
#endif


#if defined(__osi_env_sunpro)

#pragma init(__osi_lib_obj_ctor)

#elif defined(__osi_env_hp_c)

#pragma INIT "__osi_lib_obj_ctor"

#endif


/*
 * _init debugging
 */
#if defined(OSI_DEBUG_LIB_INIT)
#define _osi_debug_lib_init_sym(name) __osi_debug_lib_init__##name##_internal
#define osi_lib_init_prototype(name) \
    _osi_lib_init_prototype(name); \
    osi_static void _osi_debug_lib_init_sym(name) (void)
#define osi_lib_init_decl(name) \
    _osi_lib_init_decl(name) \
    { \
        (osi_Msg "osi_lib_init: calling '%s'\n", osi_Macro_ToString(name)); \
        _osi_debug_lib_init_sym(name) (); \
    } \
    osi_static void _osi_debug_lib_init_sym(name) (void)
#else /* !OSI_DEBUG_LIB_INIT */
#define osi_lib_init_prototype(name) _osi_lib_init_prototype(name)
#define osi_lib_init_decl(name) _osi_lib_init_decl(name)
#endif /* !OSI_DEBUG_LIB_INIT */


#endif /* _OSI_OSI_LIB_INIT_H */
