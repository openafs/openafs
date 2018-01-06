/*
 * Copyright (c) 2010 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Neither the name of Your File System, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission from Your File System, Inc.
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

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>

#include <windows.h>
#include "krb5_nt.h"

# include <krb5\krb5.h>
# include <com_err.h>

static int krb5_initialized = 0;

void
initialize_krb5(void)
{
    /*
     * On Windows, the error table will be initialized when the
     * krb5 library is loaded into the process for MIT KFW but
     * for Heimdal the error table is initialized when the
     * krb5_init_context() call is issued.  We always build
     * against the Kerberos Compat SDK now so we do not have
     * load by function pointer.  Use DelayLoadHeimd
     *
     * On Unix, the MIT krb5 error table will be initialized
     * by the library on first use.
     *
     * initialize_krb5_error_table is a macro substitution to
     * nothing.
     */
    if (!DelayLoadHeimdal()) {
        fprintf(stderr, "Kerberos for Windows or Heimdal is not available.\n");
    } else {
        krb5_initialized = 1;
    }
}

const char *
fetch_krb5_error_message(afs_uint32 code)
{
    static char errorText[1024];
    char *msg = NULL;
    krb5_context context;

    if (krb5_initialized && krb5_init_context(&context) == 0) {
        msg = krb5_get_error_message(context, code);
        if (msg) {
            strncpy(errorText, msg, sizeof(errorText));
            errorText[sizeof(errorText)-1]='\0';
            krb5_free_error_message(context, msg);
            msg = errorText;
        }
        krb5_free_context(context);
    }
    return msg;
}
