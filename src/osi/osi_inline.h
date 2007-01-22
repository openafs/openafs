/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_INLINE_H
#define _OSI_OSI_INLINE_H 1


/*
 * osi build environment
 * compiler support
 * inline function logic
 */


/*
 * inline function interface
 *
 * Inline definitions must be restricted to special inline definition
 * header files.  This is because of the vastly different ways C99,
 * GCC, and other compilers handle function inlining.
 *
 * the following is a simple inline example:
 *
 * in a common header file:
 *  #if (OSI_ENV_INLINE_INCLUDE)
 *  #include "foo_inline.h"
 *  #endif
 *
 * in foo_inline.h:
 *
 * #ifndef __foo_inline_h
 * #define __foo_inline_h
 * ...
 * osi_inline_define(
 * int
 * foo(int x, int y)
 * {
 *    return x >> y;
 * }
 * )
 * osi_inline_prototype(int foo(int x, int y))
 * ...
 * #endif
 *
 * in foo_inline.c:
 *
 * #define OSI_BUILD_INLINES
 * #include <osi/osi.h>
 * ...
 * #include "foo_inline.h"
 *
 */


/*
 * XXX this api could use some rethinking.
 * problems can arise when one inlined api
 * needs to call into another inlined api.
 */


#if defined(OSI_ENV_INLINE_BUILD)
#define OSI_ENV_INLINE_INCLUDE 0
#else /* !OSI_ENV_INLINE_BUILD */
#define OSI_ENV_INLINE_INCLUDE 1
#endif /* !OSI_ENV_INLINE_BUILD */

#if defined(OSI_ENV_ISO_C99)

#define osi_inline_define(def) osi_inline def
#if defined(OSI_ENV_INLINE_BUILD)
#define osi_inline_prototype(proto) osi_extern proto;
#else /* !OSI_ENV_INLINE_BUILD */
#define osi_inline_prototype(proto)
#endif /* !OSI_ENV_INLINE_BUILD */

#elif defined(__osi_env_gcc)

#define osi_inline_prototype(proto)
#if defined(OSI_ENV_INLINE_BUILD)
#define osi_inline_define(def) def
#else /* !OSI_ENV_INLINE_BUILD */
#define osi_inline_define(def) extern inline def
#endif /* !OSI_ENV_INLINE_BUILD */

#else /* !OSI_ENV_ISO_C99 && ! __osi_env_gcc */

#define osi_inline_prototype(proto)
#if defined(OSI_ENV_INLINE_BUILD)
#define osi_inline_define(def)
#else /* !OSI_ENV_INLINE_BUILD */
#define osi_inline_define(def) osi_static osi_inline def
#endif /* !OSI_ENV_INLINE_BUILD */

#endif /* !OSI_ENV_ISO_C99 && ! __osi_env_gcc */

#endif /* _OSI_OSI_INLINE_H */
