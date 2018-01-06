/*
 * Copyright 2015, Sine Nomine Associates
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef OPENAFS_PROCMGMT_SOFTSIG_H
#define OPENAFS_PROCMGMT_SOFTSIG_H

#ifdef AFS_NT40_ENV

/*
 * Windows implementation of the pthread soft signal processing.
 *
 * Similar to the replacment of signal, kill, etc, in procmgmt.h,
 * this header replaces the opr softsig function names with the
 * process management library names, to avoid sprinkling ifdefs
 * in the code base.  This header should been included after
 * opr/softsig.h.
 *
 */

#define opr_softsig_Init()  pmgt_SignalInit()
extern int pmgt_SignalInit(void);

#define opr_softsig_Register(sig, handler)  pmgt_SignalRegister(sig, handler)
extern int pmgt_SignalRegister(int sig, void (__cdecl *handler)(int));

#endif /* AFS_NT40_ENV */
#endif /* OPENAFS_PROCMGMT_SOFTSIG_H */
