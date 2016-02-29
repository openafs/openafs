/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OAFS_HCRYPTO_CONFIG_H
#define OAFS_HCRYPTO_CONFIG_H
#include <afsconfig.h>
#include <afs/param.h>

#if HAVE_STDINT_H
#include <stdint.h>
#endif

#if defined(AFS_NT40_ENV)
# define inline __inline
#elif defined(AFS_HPUX_ENV) || defined(AFS_USR_HPUX_ENV)
# define inline
#elif defined(AFS_AIX_ENV) || defined(AFS_USR_AIX_ENV)
# define inline
#elif defined(AFS_SGI_ENV) || defined(AFS_USR_SGI_ENV)
# define inline
#elif defined(AFS_NBSD_ENV)
# define inline __inline __attribute__((always_inline))
#endif

#define Camellia_DecryptBlock _oafs_h_Camellia_DecryptBlock
#define Camellia_Ekeygen _oafs_h_Camellia_Ekeygen
#define Camellia_EncryptBlock _oafs_h_Camellia_EncryptBlock
#define ENGINE_get_RAND _oafs_h_ENGINE_get_RAND
#define ENGINE_up_ref _oafs_h_ENGINE_up_ref
#define ENGINE_finish _oafs_h_ENGINE_finish

#define RETSIGTYPE void
#define SIGRETURN(x) return

#endif /* OAFS_HCRYPTO_CONFIG_H */
