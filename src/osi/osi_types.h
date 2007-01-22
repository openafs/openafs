/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_TYPES_H
#define _OSI_OSI_TYPES_H 1

/*
 * platform-independent osi types
 *
 *  osi_datamodel_t -- context datamodel type
 *  osi_proc_id_t -- process id type
 *  osi_user_id_t -- user id type
 *  osi_group_id_t -- group id type
 *  osi_ProgramType_t -- program type
 *  osi_result -- result code type
 */

typedef enum {
    OSI_DATAMODEL_ILP32,
    OSI_DATAMODEL_LP64,
    OSI_DATAMODEL_P64      /* apparently win64 is P64 */
} osi_datamodel_t;

#include <osi/osi_datamodel.h>


/*
 * globally useful constants
 */
#define OSI_BITS_PER_BYTE 8
#define OSI_LOG2_BITS_PER_BYTE 3



typedef char osi_int8;
typedef unsigned char osi_uint8;
typedef afs_int16 osi_int16;
typedef afs_uint16 osi_uint16;
typedef afs_int32 osi_int32;
typedef afs_uint32 osi_uint32;

#define OSI_TYPE_INT8_BITS   8
#define OSI_TYPE_INT16_BITS  16
#define OSI_TYPE_INT32_BITS  32
#define OSI_TYPE_INT64_BITS  64
#define OSI_TYPE_INT8_LOG2_BITS  3
#define OSI_TYPE_INT16_LOG2_BITS 4
#define OSI_TYPE_INT32_LOG2_BITS 5
#define OSI_TYPE_INT64_LOG2_BITS 6

#define OSI_TYPE_INT8_MIN   0x80
#define OSI_TYPE_INT8_MAX   0x7f
#define OSI_TYPE_UINT8_MIN  0
#define OSI_TYPE_UINT8_MAX  0xff
#define OSI_TYPE_INT16_MIN  0x8000
#define OSI_TYPE_INT16_MAX  0x7fff
#define OSI_TYPE_UINT16_MIN 0
#define OSI_TYPE_UINT16_MAX 0xffff
#define OSI_TYPE_INT32_MIN  0x80000000
#define OSI_TYPE_INT32_MAX  0x7fffffff
#define OSI_TYPE_UINT32_MIN 0
#define OSI_TYPE_UINT32_MAX 0xffffffff
#define OSI_TYPE_INT64_MIN  0x8000000000000000
#define OSI_TYPE_INT64_MAX  0x7fffffffffffffff
#define OSI_TYPE_UINT64_MIN 0
#define OSI_TYPE_UINT64_MAX 0xffffffffffffffff

typedef size_t osi_size_t;
typedef ssize_t osi_ssize_t;

typedef enum {
    OSI_FALSE,
    OSI_TRUE
} osi_bool_t;

/*
 * 64-bit int support
 */

#if OSI_DATAMODEL_IS(OSI_LP64_ENV)
#define OSI_ENV_NATIVE_INT64_TYPE 1
typedef long osi_int64;
typedef unsigned long osi_uint64;
#elif defined(OSI_ENV_INT64_TYPE)
#define OSI_ENV_NATIVE_INT64_TYPE 1
#if defined(OSI_NT40_ENV)
typedef longlong_t osi_int64;
typedef ulonglong_t osi_uint64;
#else /* !OSI_NT40_ENV */
typedef long long osi_int64;
typedef unsigned long long osi_uint64;
#endif /* !OSI_NT40_ENV */
#else /* !OSI_LP64_ENV */
typedef afs_int64 osi_int64;
typedef afs_uint64 osi_uint64;
#endif /* !OSI_LP64_ENV */

/* 
 * 64-bit type conversion macro package 
 *
 * osi_convert_afs64_to_osi64(afs_int64 in, osi_int64 out)
 * osi_convert_osi64_to_afs64(osi_int64 in, afs_int64 out)
 * osi_convert_afsU64_to_osiU64(afs_uint64 in, osi_uint64 out)
 * osi_convert_osiU64_to_afsU64(osi_uint64 in, afs_uint64 out)
 * osi_convert_hyper_to_osiU64(afs_hyper_t in, osi_uint64 out)
 * osi_convert_osiU64_to_hyper(osi_uint64 in, afs_hyper_t out)
 */
#if defined(OSI_ENV_NATIVE_INT64_TYPE) && defined(AFS_64BIT_ENV)
#define osi_convert_afs64_to_osi64(in, out) ((out) = (in))
#define osi_convert_osi64_to_afs64(in, out) ((out) = (in))
#define osi_convert_afsU64_to_osiU64(in, out) ((out) = (in))
#define osi_convert_osiU64_to_afsU64(in, out) ((out) = (in))
#elif defined(OSI_ENV_NATIVE_INT64_TYPE)
#define osi_convert_afs64_to_osi64(in, out) ((out) = (((osi_int64)((in).high)) << 32) | (in).low)
#define osi_convert_osi64_to_afs64(in, out) ((out).high = ((in)>>32), (out).low = (in)&0xffffffff)
#define osi_convert_afsU64_to_osiU64(in, out) ((out) = (((osi_uint64)((in).high)) << 32) | (in).low)
#define osi_convert_osiU64_to_afsU64(in, out) ((out).high = ((in)>>32), (out).low = (in)&0xffffffff)
#else /* !OSI_ENV_NATIVE_INT64_TYPE */
#define osi_convert_afs64_to_osi64(in, out) ((out) = (in))
#define osi_convert_osi64_to_afs64(in, out) ((out) = (in))
#define osi_convert_afsU64_to_osiU64(in, out) ((out) = (in))
#define osi_convert_osiU64_to_afsU64(in, out) ((out) = (in))
#endif /* !OSI_ENV_NATIVE_INT64_TYPE */

#if defined(OSI_ENV_NATIVE_INT64_TYPE)
#define osi_convert_hyper_to_osiU64(in, out) ((out) = (((osi_uint64)((in).high)) << 32) | (in).low)
#define osi_convert_osiU64_to_hyper(in, out) ((out).high = ((in)>>32), (out).low = (in)&0xffffffff)
#else /* !OSI_ENV_NATIVE_INT64_TYPE */
#define osi_convert_hyper_to_osiU64(in, out) (FillInt64(out, (in).high, (in).low))
#define osi_convert_osiU64_to_hyper(in, out) (SplitInt64(in, (out).high, (out).low))
#endif /* !OSI_ENV_NATIVE_INT64_TYPE */


/*
 * unified osi return type
 */
typedef osi_int32 osi_result;
#define OSI_OK 0
#define OSI_FAIL           OSI_ERROR_UNSPECIFIED
#define OSI_UNIMPLEMENTED  OSI_ERROR_UNIMPLEMENTED

/* 
 * basic return type checks 
 *
 * for compatibility with com_err: 
 *   positive codes are errors
 *   zero is success
 *   negative codes are success with caveats
 */
#define OSI_RESULT_OK(x)  (x<=0)
#define OSI_RESULT_FAIL(x) (x>0)

/* generate a partial success code from a com_err error code */
#define OSI_RESULT_SUCCESS_CODE(x) (-(x))

/* generate an rpc return code from an osi_result */
#define OSI_RESULT_RPC_CODE(x) ((afs_int32)x)

/* return type checks with compiler branch prediction hints */
#define OSI_RESULT_OK_LIKELY(x) \
    (osi_compiler_expect_true(OSI_RESULT_OK(x)))
#define OSI_RESULT_OK_UNLIKELY(x) \
    (osi_compiler_expect_false(OSI_RESULT_OK(x)))
#define OSI_RESULT_FAIL_LIKELY(x) \
    (osi_compiler_expect_true(OSI_RESULT_FAIL(x)))
#define OSI_RESULT_FAIL_UNLIKELY(x) \
    (osi_compiler_expect_false(OSI_RESULT_FAIL(x)))


/*
 * unified osi program type
 *
 * this enumeration is used in ipc and wire protocols
 * please treat it as append-only
 *
 * !!! whenever this enumeration is updated, please
 * !!! remember to make the necessary changes in: 
 *   src/osi/COMMON/libosi_options.c
 *   src/osi/COMMON/util.c
 */
typedef enum {
    osi_ProgramType_Library,
    osi_ProgramType_Bosserver,
    osi_ProgramType_CacheManager,
    osi_ProgramType_Fileserver,
    osi_ProgramType_Volserver,
    osi_ProgramType_Salvager,
    osi_ProgramType_Salvageserver,
    osi_ProgramType_SalvageserverWorker,
    osi_ProgramType_Ptserver,
    osi_ProgramType_Vlserver,
    osi_ProgramType_Kaserver,
    osi_ProgramType_Buserver,
    osi_ProgramType_Utility,
    osi_ProgramType_EphemeralUtility,
    osi_ProgramType_TraceCollector,
    osi_ProgramType_TestSuite,
    osi_ProgramType_TraceKernel,
    osi_ProgramType_Backup,
    osi_ProgramType_BuTC,
    osi_ProgramType_Max_Id
} osi_ProgramType_t;
#define osi_ProgramType_Min_Id osi_ProgramType_Library

typedef osi_result osi_init_func_t(void);
typedef osi_result osi_fini_func_t(void);

/* XXX the following type is a hack */
typedef afs_int64 osi_time64;

/* until we flesh out an osi_file api, just do 
 * this hack for file descriptor abstraction */
#if defined(OSI_USERSPACE_ENV)
typedef int osi_fd_t;
#endif


/* on many hardware architectures, certain sizes of loads and
 * stores occur siginificantly faster (e.g. alpha is optimized for 
 * 8-byte loads and stores).  These data types are meant for
 * applications where we are more concerned with memory transfer
 * speed than actual data type size (e.g. bit vectors):
 *
 *  osi_fast_int
 *  osi_fast_uint
 */
#if defined(__osi_arch_alpha) || defined(__osi_arch_sparc64) || defined(__osi_arch_sparcv8plus) || defined(__osi_arch_ppc64)
#define OSI_TYPE_FAST_BITS 64
#define OSI_TYPE_FAST_LOG2_BITS 6
#define OSI_TYPE_FAST_INT_MIN OSI_TYPE_INT64_MIN
#define OSI_TYPE_FAST_INT_MAX OSI_TYPE_INT64_MAX
#define OSI_TYPE_FAST_UINT_MIN OSI_TYPE_UINT64_MIN
#define OSI_TYPE_FAST_UINT_MAX OSI_TYPE_UINT64_MAX
typedef osi_int64 osi_fast_int;
typedef osi_uint64 osi_fast_uint;
#else
#define OSI_TYPE_FAST_BITS 32
#define OSI_TYPE_FAST_LOG2_BITS 5
#define OSI_TYPE_FAST_INT_MIN OSI_TYPE_INT32_MIN
#define OSI_TYPE_FAST_INT_MAX OSI_TYPE_INT32_MAX
#define OSI_TYPE_FAST_UINT_MIN OSI_TYPE_UINT32_MIN
#define OSI_TYPE_FAST_UINT_MAX OSI_TYPE_UINT32_MAX
typedef osi_int32 osi_fast_int;
typedef osi_uint32 osi_fast_uint;
#endif


/*
 * largest natively defined integer type
 */
#if defined(OSI_ENV_NATIVE_INT64_TYPE)
#define OSI_TYPE_LARGE_BITS 64
#define OSI_TYPE_LARGE_LOG2_BITS 6
#define OSI_TYPE_LARGE_INT_MIN OSI_TYPE_INT64_MIN
#define OSI_TYPE_LARGE_INT_MAX OSI_TYPE_INT64_MAX
#define OSI_TYPE_LARGE_UINT_MIN OSI_TYPE_UINT64_MIN
#define OSI_TYPE_LARGE_UINT_MAX OSI_TYPE_UINT64_MAX
typedef osi_int64 osi_large_int;
typedef osi_uint64 osi_large_uint;
#else
#define OSI_TYPE_LARGE_BITS 32
#define OSI_TYPE_LARGE_LOG2_BITS 5
#define OSI_TYPE_LARGE_INT_MIN OSI_TYPE_INT32_MIN
#define OSI_TYPE_LARGE_INT_MAX OSI_TYPE_INT32_MAX
#define OSI_TYPE_LARGE_UINT_MIN OSI_TYPE_UINT32_MIN
#define OSI_TYPE_LARGE_UINT_MAX OSI_TYPE_UINT32_MAX
typedef osi_int32 osi_large_int;
typedef osi_uint32 osi_large_uint;
#endif


/*
 * integer types to match register size
 */
#if OSI_DATAMODEL_IS(OSI_ILP32_ENV)
#define OSI_TYPE_REGISTER_BITS 32
#define OSI_TYPE_REGISTER_LOG2_BITS 5
#define OSI_TYPE_REGISTER_INT_MIN OSI_TYPE_INT32_MIN
#define OSI_TYPE_REGISTER_INT_MAX OSI_TYPE_INT32_MAX
#define OSI_TYPE_REGISTER_UINT_MIN OSI_TYPE_UINT32_MIN
#define OSI_TYPE_REGISTER_UINT_MAX OSI_TYPE_UINT32_MAX
typedef osi_int32 osi_register_int;
typedef osi_uint32 osi_register_uint;
#elif OSI_DATAMODEL_IS(OSI_LP64_ENV) || OSI_DATAMODEL_IS(OSI_P64_ENV)
#define OSI_TYPE_REGISTER_BITS 64
#define OSI_TYPE_REGISTER_LOG2_BITS 6
#define OSI_TYPE_REGISTER_INT_MIN OSI_TYPE_INT64_MIN
#define OSI_TYPE_REGISTER_INT_MAX OSI_TYPE_INT64_MAX
#define OSI_TYPE_REGISTER_UINT_MIN OSI_TYPE_UINT64_MIN
#define OSI_TYPE_REGISTER_UINT_MAX OSI_TYPE_UINT64_MAX
typedef osi_int64 osi_register_int;
typedef osi_uint64 osi_register_uint;
#else
#error "platform has unknown datamodel; please file a bug"
#endif


/*
 * integer types to match pointer size
 */
#if OSI_DATAMODEL_IS(OSI_ILP32_ENV)
#define OSI_TYPE_INTPTR_BITS 32
#define OSI_TYPE_INTPTR_LOG2_BITS 5
#define OSI_TYPE_INTPTR_MIN OSI_TYPE_INT32_MIN
#define OSI_TYPE_INTPTR_MAX OSI_TYPE_INT32_MAX
#define OSI_TYPE_UINTPTR_MIN OSI_TYPE_UINT32_MIN
#define OSI_TYPE_UINTPTR_MAX OSI_TYPE_UINT32_MAX
typedef osi_int32 osi_intptr_t;
typedef osi_uint32 osi_uintptr_t;
#elif OSI_DATAMODEL_IS(OSI_LP64_ENV) || OSI_DATAMODEL_IS(OSI_P64_ENV)
#define OSI_TYPE_INTPTR_BITS 64
#define OSI_TYPE_INTPTR_LOG2_BITS 6
#define OSI_TYPE_INTPTR_MIN OSI_TYPE_INT64_MIN
#define OSI_TYPE_INTPTR_MAX OSI_TYPE_INT64_MAX
#define OSI_TYPE_UINTPTR_MIN OSI_TYPE_UINT64_MIN
#define OSI_TYPE_UINTPTR_MAX OSI_TYPE_UINT64_MAX
typedef osi_int64 osi_intptr_t;
typedef osi_uint64 osi_uintptr_t;
#else
#error "platform has unknown datamodel; please file a bug"
#endif

#endif /* _OSI_OSI_TYPES_H */
