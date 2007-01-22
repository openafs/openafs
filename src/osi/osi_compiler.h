/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_COMPILER_H
#define _OSI_OSI_COMPILER_H 1


/*
 * osi build environment
 * support functionality
 */


/*
 * try to detect common compilers
 */
#if defined(__GCC__) || defined(__GNU_C) || defined(__GNUC__) || defined(__GNU_C__)
#define __osi_env_gcc 1
#define __osi_env_cc_ver   (__GNUC__ * 100 + __GNUC_MINOR__)
#elif defined(__SUNPRO) || defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#define __osi_env_sunpro 1
#define __osi_env_cc_ver   (__SUNPRO_C)
#elif defined(__xlc) || defined(__xlC) || defined(__xlC__) || defined(_AIX)
#define __osi_env_xlc 1
#elif defined(__DECC) || defined(__VAXC) || defined(__osf__) && defined(__LANGUAGE_C__)
#define __osi_env_compaq_c 1
#elif defined(_CRAYC)
#define __osi_env_cray_c 1
#elif defined(__HP_cc)
#define __osi_env_hp_c 1
#elif defined(__sgi)
#define __osi_env_sgi_c 1
#elif defined(__ICC)
#define __osi_env_intel_c 1
#elif defined(_MSC_VER)
#define __osi_env_ms_c 1
#define __osi_env_cc_ver (_MSC_VER)
#endif

/* c99 detection */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define OSI_ENV_ISO_C99 1
#endif


/* common language feature support */
#define __osi_file__ __FILE__
#define __osi_line__ __LINE__

#ifdef OSI_ENV_ISO_C99
#define __osi_func__ __func__
#elif defined(__osi_env_gcc)
#define __osi_func__ __FUNCTION__
#else
#define __osi_func__ __osi_file__ ":" osi_Macro_ToString(__osi_line__)
#endif

#define osi_extern extern
#define osi_static static
#define osi_inline inline
#define osi_volatile volatile

#define osi_NULL NULL


#ifdef MAX
#undef MAX
#endif
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#ifdef MIN
#undef MIN
#endif
#define MIN(a, b) ((a) > (b) ? (b) : (a))


/*
 * linking visibility keywords
 */
#if defined(__osi_env_gcc) && (__osi_env_cc_ver >= 303)
#define osi_sym_global __attribute__((visibility("default")))
#define osi_sym_hidden __attribute__((visibility("hidden")))
#define osi_sym_internal __attribute__((visibility("internal")))
#elif defined(__osi_env_sunpro) && (__osi_env_cc_ver >= 0x550)
#define osi_sym_global __global
#define osi_sym_hidden __hidden
#define osi_sym_internal __hidden
#else
#define osi_sym_global
#define osi_sym_hidden
#define osi_sym_internal
#endif

/*
 * _init/_fini support
 */
#if defined(__osi_env_gcc)
#define osi_lib_init_prototype(name) osi_static void name(void) __attribute__((constructor))
#define osi_lib_fini_prototype(name) osi_static void name(void) __attribute__((destructor))
#define osi_lib_init_decl(name) osi_static void name(void)
#define osi_lib_fini_decl(name) osi_static void name(void)
#else
#define osi_lib_init_prototype(name) osi_static void __osi_lib_obj_ctor(void)
#define osi_lib_fini_prototype(name) osi_static void __osi_lib_obj_dtor(void)
#define osi_lib_init_decl(name) osi_static void __osi_lib_obj_ctor(void)
#define osi_lib_fini_decl(name) osi_static void __osi_lib_obj_dtor(void)
#endif


#define OSI_ILP32_ENV 65537
#define OSI_LP64_ENV  65538
#define OSI_P64_ENV   65539

/* many modern preprocessors support the variadic macro extensions from c99 */
#if defined(OSI_ENV_ISO_C99) || defined(__osi_env_gcc) || defined(__osi_env_sunpro) || defined(__osi_env_xlc)
#define OSI_ENV_VARIADIC_MACROS 1
#endif

/* many modern compilers have int64 arithmetic emulation libraries for ILP32 platforms */
#if defined(__osi_env_gcc) || defined(__osi_env_sunpro) || defined(__osi_env_xlc)
#define OSI_ENV_INT64_TYPE 1
#endif


/*
 * explicit manipulation of branch prediction
 */
#if defined(__osi_env_gcc)
#define osi_compiler_expect_true(x)  __builtin_expect(!(!(x)), 1)  /* !! needed to evaluate x to boolean */
#define osi_compiler_expect_false(x) __builtin_expect(!(!(x)), 0)  /* !! needed to evaluate x to boolean */
#else
#define osi_compiler_expect_true(x)  x
#define osi_compiler_expect_false(x) x
#endif


/*
 * try to detect the target architecture
 */
#if defined(__i386) || defined(__i386__) || defined(__x86) || defined(__x86__) || defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__)

#define __osi_arch_x86 1
#if defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__)
#define __osi_arch_amd64 1
#else /* !__amd64 */
#define __osi_arch_ia32 1
#endif /* !__amd64 */

#elif defined(__sparc) || defined(__sparc__) || defined(__sparc64) || defined(__sparc64__) || defined(__sparcv7) || defined(__sparcv7__) || defined(__sparcv8) || defined(__sparcv8__) || defined(__sparcv9) || defined(__sparcv9__)

#define __osi_arch_sparc 1

#if defined(__sparcv8plus)
#define __osi_arch_sparcv8plus 1
#endif

#if defined(__sparcv9) || defined(__sparcv9__) || defined(__sparc64) || defined(__sparc64__)
#define __osi_arch_sparc64 1
#else /* !__sparcv9 */
#define __osi_arch_sparc32 1
#endif /* !__sparcv9 */

#elif defined(__powerpc) || defined(__powerpc64) || defined(__powerpc__) || defined(__powerpc64__)

#define __osi_arch_ppc 1
#if defined(__powerpc64) || defined(__powerpc64__) || defined(__ppc64)
#define __osi_arch_ppc64 1
#else /* !__powerpc64 */
#define __osi_arch_ppc32 1
#endif /* !__powerpc64 */

#elif defined(__alpha) || defined(__alpha__) || defined(__Alpha_AXP) || defined(__ALPHA)

#define __osi_arch_alpha 1

#elif defined(__s390) || defined(__s390__) || defined(__s390x) || defined(__s390x__)

#define __osi_arch_s390 1
#if defined(__s390x) || defined(__s390x__)
#define __osi_arch_s390x 1
#else /* !__s390x */
#define __osi_arch_s390_31bit 1
#endif /* !__s390x */

#elif defined(__ia64) || defined(__ia64__)

#define __osi_arch_ia64 1

#elif defined(__mips) || defined(__mips__) || defined(__mips64) || defined(__mips64__) || defined(__sgi)

#define __osi_arch_mips 1
#if defined (__mips64) || defined(__mips64__) || defined(__sgi) && (_MIPS_SZPTR == 64)
#define __osi_arch_mips64 1
#else
#define __osi_arch_mips32 1
#endif

#elif defined(__hppa) || defined(__hppa__)

#define __osi_arch_hppa 1

#elif defined(__arm) || defined(__arm__)

#define __osi_arch_arm 1

#else
#error "unknown system architecture; please file a bug"
#endif

#endif /* _OSI_OSI_COMPILER_H */
