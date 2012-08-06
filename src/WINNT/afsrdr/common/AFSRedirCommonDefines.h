/*
 * Copyright (c) 2008-2011 Kernel Drivers, LLC.
 * Copyright (c) 2009-2011 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice,
 *   this list of conditions and the following disclaimer in the
 *   documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Kernel Drivers, LLC nor the names of its
 *   contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission from Kernel Drivers, LLC
 *   and Your File System, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AFS_REDIR_COMMON_DEFINES_H
#define _AFS_REDIR_COMMON_DEFINES_H

//
// Allocation defines
//

#define AFS_GENERIC_MEMORY_1_TAG     '1GFA'
#define AFS_GENERIC_MEMORY_2_TAG     '2GFA'
#define AFS_GENERIC_MEMORY_3_TAG     '3GFA'
#define AFS_GENERIC_MEMORY_4_TAG     '4GFA'
#define AFS_GENERIC_MEMORY_5_TAG     '5GFA'
#define AFS_GENERIC_MEMORY_6_TAG     '6GFA'
#define AFS_GENERIC_MEMORY_7_TAG     '7GFA'
#define AFS_GENERIC_MEMORY_8_TAG     '8GFA'
#define AFS_GENERIC_MEMORY_9_TAG     '9GFA'
#define AFS_GENERIC_MEMORY_10_TAG    'AGFA'
#define AFS_GENERIC_MEMORY_11_TAG    'BGFA'
#define AFS_GENERIC_MEMORY_12_TAG    'CGFA'
#define AFS_GENERIC_MEMORY_13_TAG    'DGFA'
#define AFS_GENERIC_MEMORY_14_TAG    'EGFA'
#define AFS_GENERIC_MEMORY_15_TAG    'FGFA'
#define AFS_GENERIC_MEMORY_16_TAG    'GGFA'
#define AFS_GENERIC_MEMORY_17_TAG    'HGFA'
#define AFS_GENERIC_MEMORY_18_TAG    'IGFA'
#define AFS_GENERIC_MEMORY_19_TAG    'JGFA'
#define AFS_GENERIC_MEMORY_20_TAG    'KGFA'
#define AFS_GENERIC_MEMORY_21_TAG    'LGFA'
#define AFS_GENERIC_MEMORY_22_TAG    'MGFA'
#define AFS_GENERIC_MEMORY_23_TAG    'NGFA'
#define AFS_GENERIC_MEMORY_24_TAG    'OGFA'
#define AFS_GENERIC_MEMORY_25_TAG    'PGFA'
#define AFS_GENERIC_MEMORY_26_TAG    'QGFA'
#define AFS_GENERIC_MEMORY_27_TAG    'RGFA'
#define AFS_GENERIC_MEMORY_28_TAG    'SGFA'
#define AFS_GENERIC_MEMORY_29_TAG    'TGFA'
#define AFS_GENERIC_MEMORY_30_TAG    'UGFA'
#define AFS_GENERIC_MEMORY_31_TAG    'VGFA'
#define AFS_GENERIC_MEMORY_32_TAG    'WGFA'
#define AFS_FCB_ALLOCATION_TAG       'AFFA'
#define AFS_FCB_NP_ALLOCATION_TAG    'NFFA'
#define AFS_VCB_ALLOCATION_TAG       'CVFA'
#define AFS_VCB_NP_ALLOCATION_TAG    'NVFA'
#define AFS_CCB_ALLOCATION_TAG       'CCFA'
#define AFS_CCB_NP_ALLOCATION_TAG    'NCFA'
#define AFS_WORKER_CB_TAG            'CWFA'
#define AFS_WORK_ITEM_TAG            'IWFA'
#define AFS_POOL_ENTRY_TAG           'EPFA'
#define AFS_PROCESS_CB_TAG           'CPFA'
#define AFS_DIR_BUFFER_TAG           'BDFA'
#define AFS_DIR_ENTRY_TAG            'EDFA'
#define AFS_NAME_BUFFER_ONE_TAG      '1NFA'
#define AFS_NAME_BUFFER_TWO_TAG      '2NFA'
#define AFS_NAME_BUFFER_THREE_TAG    '3NFA'
#define AFS_NAME_BUFFER_FOUR_TAG     '4NFA'
#define AFS_NAME_BUFFER_FIVE_TAG     '5NFA'
#define AFS_NAME_BUFFER_SIX_TAG      '6NFA'
#define AFS_NAME_BUFFER_SEVEN_TAG    '7NFA'
#define AFS_NAME_BUFFER_EIGHT_TAG    '8NFA'
#define AFS_NAME_BUFFER_NINE_TAG     '9NFA'
#define AFS_NAME_BUFFER_TEN_TAG      'ANFA'
#define AFS_SUBST_BUFFER_TAG         'SBFA'
#define AFS_FILE_CREATE_BUFFER_TAG   'CFFA'
#define AFS_RENAME_REQUEST_TAG       'RFFA'
#define AFS_DIR_ENTRY_NP_TAG         'NDFA'
#define AFS_PROVIDER_CB              'PNFA'
#define AFS_EXTENT_TAG               'xSFA'
#define AFS_EXTENT_REQUEST_TAG       'XSFA'
#define AFS_EXTENT_RELEASE_TAG       'LSFA'
#define AFS_IO_RUN_TAG               'iSFA'
#define AFS_GATHER_TAG               'gSFA'
#define AFS_UPDATE_RESULT_TAG        'RUFA'
#define AFS_EXTENTS_RESULT_TAG       'XEFA'
#define AFS_SYS_NAME_NODE_TAG        'NSFA'
#define AFS_REPARSE_NAME_TAG         'NRFA'
#define AFS_NAME_ARRAY_TAG           'ANFA'
#define AFS_OBJECT_INFO_TAG          'IOFA'
#define AFS_NP_OBJECT_INFO_TAG       'ONFA'
#define AFS_DIR_SNAPSHOT_TAG         'SSFA'
#define AFS_LIBRARY_QUEUE_TAG        'QLFA'
#define AFS_NETWORK_PROVIDER_1_TAG   '1ZFA'
#define AFS_NETWORK_PROVIDER_2_TAG   '2ZFA'
#define AFS_NETWORK_PROVIDER_3_TAG   '3ZFA'
#define AFS_NETWORK_PROVIDER_4_TAG   '4ZFA'
#define AFS_NETWORK_PROVIDER_5_TAG   '5ZFA'
#define AFS_NETWORK_PROVIDER_6_TAG   '6ZFA'
#define AFS_NETWORK_PROVIDER_7_TAG   '7ZFA'
#define AFS_NETWORK_PROVIDER_8_TAG   '8ZFA'
#define AFS_NETWORK_PROVIDER_9_TAG   '9ZFA'
#define AFS_NETWORK_PROVIDER_10_TAG  'AZFA'
#define AFS_NETWORK_PROVIDER_11_TAG  'BZFA'
#define AFS_AG_ENTRY_CB_TAG          'GAFA'
#define AFS_PROCESS_AG_CB_TAG        'APFA'
#define AFS_BYTERANGE_TAG            '_RBA'
#define __Enter

#define try_return(S) { S; goto try_exit; }

//
// Object types allocated
//

#define AFS_FILE_FCB                            0x0001
#define AFS_DIRECTORY_FCB                       0x0002
#define AFS_NON_PAGED_FCB                       0x0003
#define AFS_CCB                                 0x0004
#define AFS_ROOT_FCB                            0x0006
#define AFS_VCB                                 0x0007
#define AFS_NON_PAGED_VCB                       0x0008
#define AFS_ROOT_ALL                            0x0009
#define AFS_IOCTL_FCB                           0x000A
#define AFS_MOUNT_POINT_FCB                     0x000B
#define AFS_SYMBOLIC_LINK_FCB                   0x000C
#define AFS_SPECIAL_SHARE_FCB                   0x000D
#define AFS_DFS_LINK_FCB                        0x000E
#define AFS_REDIRECTOR_FCB                      0x000F

#define AFS_INVALID_FCB                         0x00FF

//
// Debug information
//

#define AFS_DBG_FLAG_BREAK_ON_ENTRY     0x00000001   // Only enabled in checked build
#define AFS_DBG_TRACE_TO_DEBUGGER       0x00000002
#define AFS_DBG_FLAG_ENABLE_FORCE_CRASH 0x00000004   // Only enabled in checked build
#define AFS_DBG_BUGCHECK_EXCEPTION      0x00000008
#define AFS_DBG_CLEAN_SHUTDOWN          0x00000010
#define AFS_DBG_REQUIRE_CLEAN_SHUTDOWN  0x00000020

//
// Pool state
//

#define POOL_UNKNOWN            0
#define POOL_INACTIVE           1
#define POOL_ACTIVE             2

//
// Volume flags
//

#define AFS_VOLUME_FLAGS_OFFLINE                        0x00000001
#define AFS_VOLUME_PRIVATE_WOKER_INITIALIZED            0x00000002
#define AFS_VOLUME_INSERTED_HASH_TREE                   0x00000004
#define AFS_VOLUME_ACTIVE_GLOBAL_ROOT                   0x00000008

//
// Need this to handle the break point definition
//

typedef
void
(*PAFSDumpTraceFiles)( void);

extern PAFSDumpTraceFiles  AFSDumpTraceFilesFnc;

//
// Debug information
//

static inline void AFS_ASSERT() {
    AFSDumpTraceFilesFnc();
}

#if DBG

//#define AFS_VALIDATE_EXTENTS            0

static inline void AFSBreakPoint() {
#if !defined(KD_DEBUGGER_ENABLED)
#define KD_DEBUGGER_ENABLED DBG
#endif // KD_DEBUGGER_ENABLED

#if (NTDDI_VERSION >= NTDDI_WS03)
    KdRefreshDebuggerNotPresent();
#endif

#if defined(KD_DEBUGGER_NOT_PRESENT)
    if (KD_DEBUGGER_NOT_PRESENT == FALSE)
        DbgBreakPoint();
#endif // KD_DEBUGGER_NOT_PRESENT
}

#define AFSPrint        DbgPrint

#else

static inline void AFSBreakPoint() {
    NOTHING;
}

#define AFSPrint

#endif

//
// Library control device name
//

#define AFS_LIBRARY_CONTROL_DEVICE_NAME     L"\\Device\\AFSLibraryControlDevice"

#define AFS_REDIR_LIBRARY_SERVICE_ENTRY     L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\AFSLibrary"

//
// Common Device flags
//

#define AFS_DEVICE_FLAG_HIDE_DOT_NAMES          0x00000001
#define AFS_DEVICE_FLAG_REDIRECTOR_SHUTDOWN     0x00000002
#define AFS_DEVICE_FLAG_DISABLE_SHORTNAMES      0x00000004

#endif

