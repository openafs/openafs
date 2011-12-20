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

#include <windows.h>
#include "krb5_nt.h"

static krb5_error_code (KRB5_CALLCONV *pkrb5_init_context)(krb5_context *context) = NULL;
static krb5_error_code (KRB5_CALLCONV *pkrb5_free_context)(krb5_context context) = NULL;
static char * (KRB5_CALLCONV *pkrb5_get_error_message)(krb5_context context, krb5_error_code code) = NULL;
static void (KRB5_CALLCONV *pkrb5_free_error_message)(krb5_context context, char *s) = NULL;

# ifndef _WIN64
#  define KRB5LIB "krb5_32.dll"
# else
#  define KRB5LIB "krb5_64.dll"
# endif


void
initialize_krb5(void)
{
    /*
     * On Windows, the error table will be initialized when the
     * krb5 library is loaded into the process.  Since not all
     * versions of krb5 contain krb5_{get,free}_error_message()
     * we load them conditionally by function pointer.
     *
     * On Unix, the MIT krb5 error table will be initialized
     * by the library on first use.
     *
     * initialize_krb5_error_table is a macro substitution to
     * nothing.
     */
    HINSTANCE h = LoadLibrary(KRB5LIB);
    if (h) {
        (FARPROC)pkrb5_init_context = GetProcAddress(h, "krb5_init_context");
        (FARPROC)pkrb5_free_context = GetProcAddress(h, "krb5_free_context");
        (FARPROC)pkrb5_get_error_message = GetProcAddress(h, "krb5_get_error_message");
        (FARPROC)pkrb5_free_error_message = GetProcAddress(h, "krb5_free_error_message");
    }
}

const char *
fetch_krb5_error_message(krb5_context context, krb5_error_code code)
{
    static char errorText[1024];
    char *msg = NULL;
    int free_context = 0;

    if (pkrb5_init_context && pkrb5_get_error_message) {

        if (context == NULL) {
            if (krb5_init_context(&context) != 0)
                goto done;
            free_context = 1;
        }

        msg = pkrb5_get_error_message(context, code);
        if (msg) {
            strncpy(errorText, msg, sizeof(errorText));
            errorText[sizeof(errorText)-1]='\0';
            pkrb5_free_error_message(context, msg);
            msg = errorText;
        }

        if (free_context)
            pkrb5_free_context(context);
    }
  done:
    return msg;
}
