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

/* This file provides the message processing function and the support
   routines for implementing our plugin.  Note that some of the
   message processing routines have been moved to other source files
   based on their use.
*/

khm_int32  msg_type_afs = -1;
khm_handle g_credset = NULL;
afs_tk_method tk_method = -1;

/* Handler for system messages.  The only two we handle are
   KMSG_SYSTEM_INIT and KMSG_SYSTEM_EXIT. */
khm_int32 KHMAPI
handle_kmsg_system(khm_int32 msg_type,
                   khm_int32 msg_subtype,
                   khm_ui_4  uparam,
                   void *    vparam) {
    khm_int32 rv = KHM_ERROR_SUCCESS;

    switch (msg_subtype) {

        /* This is the first message that will be received by a
           plugin.  We use it to perform initialization operations
           such as registering any credential types, data types and
           attributes. */
    case KMSG_SYSTEM_INIT:
        {
            wchar_t short_desc[KCDB_MAXCCH_SHORT_DESC];
            wchar_t long_desc[KCDB_MAXCCH_LONG_DESC];
#ifdef USE_CONFIGURATION_PANELS
            khui_config_node_reg creg;
#endif
            afs_msg_announce announce;

            if (KHM_FAILED(kmq_find_type(AFS_MSG_TYPENAME, &msg_type_afs))) {
                return KHM_ERROR_UNKNOWN;
            }

            /* We must first announce our extension plug-in, so that
               the AFS plug-in will know we exist */
            announce.cbsize = sizeof(announce);
            announce.version = AFS_PLUGIN_VERSION;
            announce.name = MYPLUGIN_NAMEW;

            kmq_create_subscription(handle_AFS_MSG, &announce.sub);

            /* Set to TRUE if we are providing a token acquisition
               method */
            announce.provide_token_acq = TRUE;

            LoadString(hResModule, IDS_TKMETHOD_SHORT_DESC,
                       short_desc, ARRAYLENGTH(short_desc));

            announce.token_acq.short_desc = short_desc;

            LoadString(hResModule, IDS_TKMETHOD_LONG_DESC,
                       long_desc, ARRAYLENGTH(long_desc));

            announce.token_acq.long_desc = long_desc;

            if (KHM_FAILED(kmq_send_message(msg_type_afs,
                                            AFS_MSG_ANNOUNCE, 0, &announce))) {
                kmq_delete_subscription(announce.sub);
                announce.sub = NULL;

                return KHM_ERROR_UNKNOWN;
            }

            tk_method = announce.token_acq.method_id;

#ifdef USE_CONFIGURATION_PANELS

            /* Register our configuration panels. */

            /* Registering configuration panels is not required for
               extension plug-in.  As such, this bit of code is
               commented out.  However, if you wish to provide a
               configuration panel, you should uncomment this block
               and fill in the stub functions in config_main.c */

            ZeroMemory(&creg, sizeof(creg));

            short_desc[0] = L'\0';

            LoadString(hResModule, IDS_CFG_SHORT_DESC,
                       short_desc, ARRAYLENGTH(short_desc));

            long_desc[0] = L'\0';

            LoadString(hResModule, IDS_CFG_LONG_DESC,
                       long_desc, ARRAYLENGTH(long_desc));

            creg.name = CONFIGNODE_MAIN;
            creg.short_desc = short_desc;
            creg.long_desc = long_desc;
            creg.h_module = hResModule;
            creg.dlg_template = MAKEINTRESOURCE(IDD_CONFIG);
            creg.dlg_proc = config_dlgproc;
            creg.flags = 0;

            khui_cfg_register(NULL, &creg);
#endif
        }
        break;

        /* This is the last message that will be received by the
           plugin. */
    case KMSG_SYSTEM_EXIT:
        {
            khui_config_node cnode;

            /* It should not be assumed that initialization of the
               plugin went well at this point since we receive a
               KMSG_SYSTEM_EXIT even if the initialization failed. */

            /* Now unregister any configuration nodes we registered. */

            if (KHM_SUCCEEDED(khui_cfg_open(NULL, CONFIGNODE_MAIN, &cnode))) {
                khui_cfg_remove(cnode);
                khui_cfg_release(cnode);
            }

            /* TODO: Perform additional uninitialization
               operations. */
        }
        break;
    }

    return rv;
}

/* This is the main message handler for our plugin.  All the plugin
   messages end up here where we either handle it directly or dispatch
   it to other handlers. */
khm_int32 KHMAPI plugin_msg_proc(khm_int32 msg_type,
                                 khm_int32 msg_subtype,
                                 khm_ui_4  uparam,
                                 void * vparam) {

    switch(msg_type) {
    case KMSG_SYSTEM:
        return handle_kmsg_system(msg_type, msg_subtype, uparam, vparam);
    }

    if (msg_type == msg_type_afs) {
        return handle_AFS_MSG(msg_type, msg_subtype, uparam, vparam);
    }

    return KHM_ERROR_SUCCESS;
}
