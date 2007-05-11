/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_LIB_FINI_H
#define _OSI_OSI_LIB_FINI_H 1

/*
 * osi 
 * library finalization support
 *
 * please note that you may only have one
 * finalization function per compilation unit
 *
 * macro interfaces:
 *
 *  osi_lib_fini_prototype(symbol name)
 *    -- provide prototype for a compilation unit's finalizer
 *       passed symbol name is merely a hint
 *
 *  osi_lib_fini_decl(symbol name)
 *    -- provide a definition for a compilation unit's finalizer
 *       passed symbol name is merely a hint
 *
 *
 * example usage:
 *
 *  osi_lib_fini_prototype(example_fini);
 *
 *  osi_lib_fini_decl(example_fini)
 *  {
 *       osi_Msg("goodbye world\n");
 *  }
 */

/*
 * _fini support
 */
#if defined(__osi_env_gcc)
#define _osi_lib_fini_sym(name) name
#define _osi_lib_fini_prototype(name) \
    osi_static void _osi_lib_fini_sym(name) (void) __attribute__((destructor))
#define _osi_lib_fini_decl(name) \
    osi_static void _osi_lib_fini_sym(name) (void)
#else
#define _osi_lib_fini_sym(name) __osi_lib_obj_dtor
#define _osi_lib_fini_prototype(name) \
    osi_static void _osi_lib_fini_sym(name) (void)
#define _osi_lib_fini_decl(name) \
    osi_static void _osi_lib_fini_sym(name) (void)
#endif


#if defined(__osi_env_sunpro)

#pragma fini(__osi_lib_obj_dtor)

#elif defined(__osi_env_hp_c)

#pragma FINI "__osi_lib_obj_dtor"

#endif


/*
 * _fini debugging
 */
#if defined(OSI_DEBUG_LIB_FINI)
#define _osi_debug_lib_fini_sym(name) __osi_debug_lib_fini__##name##_internal
#define osi_lib_fini_prototype(name) \
    _osi_lib_fini_prototype(name); \
    osi_static void _osi_debug_lib_fini_sym(name) (void)
#define osi_lib_fini_decl(name) \
    _osi_lib_fini_decl(name) \
    { \
        (osi_Msg "osi_lib_fini: calling '%s'\n", osi_Macro_ToString(name)); \
        _osi_debug_lib_fini_sym(name) (); \
    } \
    osi_static void _osi_debug_lib_fini_sym(name) (void)
#else /* !OSI_DEBUG_LIB_FINI */
#define osi_lib_fini_prototype(name) _osi_lib_fini_prototype(name)
#define osi_lib_fini_decl(name) _osi_lib_fini_decl(name)
#endif /* !OSI_DEBUG_LIB_FINI */


#endif /* _OSI_OSI_LIB_FINI_H */
