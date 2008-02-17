/*
 * Copyright (c) 2006 Secure Endpoints Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* $Id$ */

#include "credprov.h"
#include "afspext.h"
#include<assert.h>

/* Resolve a token

   This message is typically sent when the AFS plug-in can't determine
   the authentication method that was used to obtain a specific token.

   When the AFS plug-in starts, it attempts to list the AFS tokens
   that belong to the user. During this process, the plug-in has to
   determine the authentication method and identity corresponding to
   each token for the purpose of handling token renewals and
   acquisitions.  If the plug-in is unable to do so, it will query any
   registered extensions using this message.  If you handle this
   message, and are able to determine the authentication method used
   to obtain the token specified in prt->token, then you should set
   prt->ident and prt->method members to the identity and method for
   the token and return KHM_ERROR_SUCCESS.
 */
khm_int32
handle_AFS_MSG_RESOLVE_TOKEN(afs_msg_resolve_token * prt)
{
    /* TODO: Implement this */
    return KHM_ERROR_NOT_IMPLEMENTED;
}

/* Handle a klog message

   This message is sent when the AFS plug-in is attempting to obtain a
   token using this authentication method, or if it is attempting to
   obtain a token using the 'AUTO' method and none of the stock
   authentication methods were successful.

   If you handle this message and successfully obtain a token, you
   should return KHM_ERROR_SUCCESS.  Any other return value will cause
   the AFS plug-in to either report a failure or continue down the
   list of authentication method extensions.

*/
khm_int32
handle_AFS_MSG_KLOG(afs_msg_klog * pklog)
{

    /* TODO: Implement this */

    return KHM_ERROR_NOT_IMPLEMENTED;
}

/* Handle AFS_MSG messages

   These are the messages that the AFS plug-in uses to communicate
   with its extensions.  This function just dispatches the messages to
   the appropriate handler defined above.
 */
khm_int32 KHMAPI
handle_AFS_MSG(khm_int32 msg_type,
               khm_int32 msg_subtype,
               khm_ui_4  uparam,
               void *    vparam)
{
    switch (msg_subtype) {
    case AFS_MSG_RESOLVE_TOKEN:
        return handle_AFS_MSG_RESOLVE_TOKEN((afs_msg_resolve_token *) vparam);

    case AFS_MSG_KLOG:
        return handle_AFS_MSG_KLOG((afs_msg_klog *) vparam);
    }

    return KHM_ERROR_NOT_IMPLEMENTED;
}

