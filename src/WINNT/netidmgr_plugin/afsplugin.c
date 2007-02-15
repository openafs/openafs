/*
 * Copyright (c) 2005,2006 Secure Endpoints Inc.
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

#include<afscred.h>
#include<kcreddb.h>
#include<khmsgtypes.h>
#include<kherror.h>
#include<khuidefs.h>
#include<commctrl.h>
#include<htmlhelp.h>
#include<assert.h>

static BOOL initialized = FALSE;
khm_int32 afs_credtype_id = -1;
khm_int32 krb5_credtype_id = -1;
khm_int32 krb4_credtype_id = -1;
khm_int32 afs_msg_type_id = -1;
khm_int32 afs_type_principal = -1;
khm_int32 afs_type_method = -1;
khm_int32 afs_attr_client_princ = -1;
khm_int32 afs_attr_server_princ = -1;
khm_int32 afs_attr_cell = -1;
khm_int32 afs_attr_method = -1;
khm_int32 afs_attr_realm = -1;
khm_handle afs_credset = NULL;
khm_handle afs_sub = NULL;      /* AFS message subscription */

khm_int32 action_id_afs_help = 0;

/* forward dcls */
khm_int32 KHMAPI 
afs_msg_system(khm_int32 msg_subtype, khm_ui_4 uparam, void * vparam);

khm_int32 KHMAPI 
afs_msg_kcdb(khm_int32 msg_subtype, khm_ui_4 uparam, void * vparam);

khm_int32 KHMAPI 
afs_msg_cred(khm_int32 msg_subtype, khm_ui_4 uparam, void * vparam);

khm_int32 KHMAPI
afs_msg_act(khm_int32 msg_subtype, khm_ui_4 uparam, void * vparam);

khm_int32 KHMAPI 
afs_msg_ext(khm_int32 msg_subtype, khm_ui_4 uparam, void * vparam);

/* AFS help menu extensions */

/* some of the functions we will be calling are only available on API
   version 7 and above of Network Identity Manager.  Since these
   aren't critical, we allow building using API version 5 or above,
   but conditionally use the newer functionality if the plug-in is
   loaded by a newer version of Network Identity Manager. */

#if KH_VERSION_API < 7

HMODULE hm_netidmgr;

/* declarations from version 7 of the API */
KHMEXP void
(KHMAPI * pkhui_action_lock)(void);

KHMEXP void
(KHMAPI * pkhui_action_unlock)(void);

KHMEXP void
(KHMAPI * pkhui_refresh_actions)(void);

typedef khm_int32
(KHMAPI * khm_ui_callback)(HWND hwnd_main_wnd, void * rock);

KHMEXP khm_int32
(KHMAPI * pkhui_request_UI_callback)(khm_ui_callback cb,
                                     void * rock);

#define khui_action_lock         (*pkhui_action_lock)
#define khui_action_unlock       (*pkhui_action_unlock)
#define khui_refresh_actions     (*pkhui_refresh_actions)
#define khui_request_UI_callback (*pkhui_request_UI_callback)

#endif

/* AFS plugin callback */
khm_int32 KHMAPI 
afs_plugin_cb(khm_int32 msg_type,
              khm_int32 msg_subtype,
              khm_ui_4 uparam,
              void * vparam)
{
    if (msg_type == KMSG_SYSTEM)
        return afs_msg_system(msg_subtype, uparam, vparam);
    if (msg_type == KMSG_KCDB)
        return afs_msg_kcdb(msg_subtype, uparam, vparam);
    if (msg_type == KMSG_CRED)
        return afs_msg_cred(msg_subtype, uparam, vparam);
    if (msg_type == KMSG_ACT)
        return afs_msg_act(msg_subtype, uparam, vparam);
    if (msg_type == afs_msg_type_id)
        return afs_msg_ext(msg_subtype, uparam, vparam);

    return KHM_ERROR_SUCCESS;
}

/* ktc_principal attribute type */
/* String */

khm_int32 KHMAPI 
afs_type_principal_toString(const void * d, 
                            khm_size cbd, 
                            wchar_t * buffer, 
                            khm_size * cb_buf, 
                            khm_int32 flags)
{
    size_t cbsize;
    struct ktc_principal * p;
    wchar_t sprinc[512] = L"";

    if(!cb_buf)
        return KHM_ERROR_INVALID_PARAM;

    p = (struct ktc_principal *) d;

    // assume this works.
    afs_princ_to_string(p, sprinc, sizeof(sprinc));
    StringCbLength(sprinc, sizeof(sprinc), &cbsize);
    cbsize += sizeof(wchar_t);

    if(!buffer || *cb_buf < cbsize) {
        *cb_buf = cbsize;
        return KHM_ERROR_TOO_LONG;
    }

    StringCbCopy(buffer, *cb_buf, sprinc);

    *cb_buf = cbsize;

    return KHM_ERROR_SUCCESS;
}

khm_boolean KHMAPI 
afs_type_principal_isValid(const void * d,
                           khm_size cbd)
{
    /*TODO: check for more inconsistencies */
    if(cbd != sizeof(struct ktc_principal))
        return FALSE;
    return TRUE;
}

khm_int32 KHMAPI 
afs_type_principal_comp(const void * d1,
                        khm_size cbd1,
                        const void * d2,
                        khm_size cbd2)
{
    struct ktc_principal * p1 = (struct ktc_principal *) d1;
    struct ktc_principal * p2 = (struct ktc_principal *) d2;
    int r;

    r = strcmp(p1->name, p2->name);
    if(r != 0)
        return r;
    r = strcmp(p1->instance, p2->instance);
    if(r != 0)
        return r;
    r = strcmp(p1->cell, p2->cell);
    return r;
}

khm_int32 KHMAPI 
afs_type_principal_dup(const void * d_src,
                       khm_size cbd_src,
                       void * d_dst,
                       khm_size * cbd_dst)
{
    if(!d_dst || *cbd_dst < sizeof(struct ktc_principal)) {
        *cbd_dst = sizeof(struct ktc_principal);
        return KHM_ERROR_TOO_LONG;
    }

    memcpy(d_dst, d_src, sizeof(struct ktc_principal));
    *cbd_dst = sizeof(struct ktc_principal);

    return KHM_ERROR_SUCCESS;
}

khm_int32 KHMAPI
afs_type_method_toString(const void * data,
                         khm_size     cb_data,
                         wchar_t *    s_buf,
                         khm_size *   pcb_s_buf,
                         khm_int32    flags) {
    khm_int32 * pmethod = (khm_int32 *) data;
    wchar_t wbuf[KHUI_MAXCCH_LONG_DESC];
    khm_size cb;

    if (!data || cb_data != sizeof(khm_int32))
        return KHM_ERROR_INVALID_PARAM;

    wbuf[0] = L'\0';
    if (!afs_method_describe(*pmethod, flags, wbuf, sizeof(wbuf))) {
        LoadString(hResModule,
                   IDS_NC_METHOD_INVALID,
                   wbuf,
                   ARRAYLENGTH(wbuf));
    }

    StringCbLength(wbuf, sizeof(wbuf), &cb);
    cb += sizeof(wchar_t);

    if (!s_buf || *pcb_s_buf < cb) {
        *pcb_s_buf = cb;
        return KHM_ERROR_TOO_LONG;
    } else {
        StringCbCopy(s_buf, *pcb_s_buf, wbuf);
        *pcb_s_buf = cb;
        return KHM_ERROR_SUCCESS;
    }
}

/* process KMSG_SYSTEM messages */
khm_int32 KHMAPI 
afs_msg_system(khm_int32 msg_subtype, 
               khm_ui_4 uparam, 
               void * vparam)
{
    khm_int32 rv = KHM_ERROR_UNKNOWN;

    switch(msg_subtype) {
    case KMSG_SYSTEM_INIT:
        /* Perform critical registrations and data structure
           initalization */
        {
            kcdb_credtype ct;
            wchar_t buf[KCDB_MAXCCH_LONG_DESC];
            size_t cbsize;
            kcdb_attrib att;
            khm_handle csp_afscred = NULL;
            khm_int32 disable_afscreds = FALSE;

            ZeroMemory(&ct, sizeof(ct));
            /* first of all, register the AFS token credential type */
            ct.id = KCDB_CREDTYPE_AUTO;
            ct.name = AFS_CREDTYPE_NAME;

            if(LoadString(hResModule, 
                          IDS_AFS_SHORT_DESC, 
                          buf, 
                          ARRAYLENGTH(buf)) != 0) {
                StringCbLength(buf, sizeof(buf), &cbsize);
                cbsize += sizeof(wchar_t);
                ct.short_desc = PMALLOC(cbsize);
                StringCbCopy(ct.short_desc, cbsize, buf);
            } else
                ct.short_desc = NULL;

            if(LoadString(hResModule, 
                          IDS_AFS_LONG_DESC, 
                          buf, 
                          ARRAYLENGTH(buf)) != 0) {
                StringCbLength(buf, sizeof(buf), &cbsize);
                cbsize += sizeof(wchar_t);
                ct.long_desc = PMALLOC(cbsize);
                StringCbCopy(ct.long_desc, cbsize, buf);
            } else
                ct.long_desc = NULL;

            ct.icon = LoadImage(hResModule, 
                                MAKEINTRESOURCE(IDI_AFSTOKEN), 
                                IMAGE_ICON, 
                                0, 0, LR_DEFAULTSIZE);

            kmq_create_subscription(afs_plugin_cb, &afs_sub);
            ct.sub = afs_sub;

            kcdb_credtype_register(&ct, &afs_credtype_id);

            /* register the attribute types */
            {
                kcdb_type type;

                ZeroMemory(&type, sizeof(type));
                type.comp = afs_type_principal_comp;
                type.dup = afs_type_principal_dup;
                type.isValid = afs_type_principal_isValid;
                type.toString = afs_type_principal_toString;
                type.name = AFS_TYPENAME_PRINCIPAL;
                type.id = KCDB_TYPE_INVALID;
                type.cb_max = sizeof(struct ktc_principal);
                type.cb_min = sizeof(struct ktc_principal);
                type.flags = KCDB_TYPE_FLAG_CB_FIXED;

                if(KHM_FAILED(kcdb_type_register(&type, 
                                                 &afs_type_principal)))
                    goto _exit_init;
            }

            {
                kcdb_type type;
                kcdb_type *ti32 = NULL;

                kcdb_type_get_info(KCDB_TYPE_INT32, &ti32);

                ZeroMemory(&type, sizeof(type));
                type.comp = ti32->comp;
                type.dup = ti32->dup;
                type.isValid = ti32->isValid;
                type.toString = afs_type_method_toString;
                type.name = AFS_TYPENAME_METHOD;
                type.id = KCDB_TYPE_INVALID;
                type.cb_max = sizeof(khm_int32);
                type.cb_min = sizeof(khm_int32);
                type.flags = KCDB_TYPE_FLAG_CB_FIXED;

                if(KHM_FAILED(kcdb_type_register(&type, 
                                                 &afs_type_method))) {
                    kcdb_type_release_info(ti32);
                    goto _exit_init;
                }

                kcdb_type_release_info(ti32);
            }

            /* now register the attributes */
            {
                wchar_t short_desc[KCDB_MAXCCH_SHORT_DESC];
                    
                ZeroMemory(&att, sizeof(att));

                att.type = KCDB_TYPE_STRING;
                att.name = AFS_ATTRNAME_CELL;
                LoadString(hResModule, 
                           IDS_ATTR_CELL_SHORT_DESC, 
                           short_desc, 
                           ARRAYLENGTH(short_desc));
                att.short_desc = short_desc;
                att.long_desc = NULL;
                att.id = KCDB_ATTR_INVALID;
                att.flags = KCDB_ATTR_FLAG_TRANSIENT;
                    
                if(KHM_FAILED(rv = kcdb_attrib_register(&att, 
                                                        &afs_attr_cell)))
                    goto _exit_init;
            }

            {
                wchar_t short_desc[KCDB_MAXCCH_SHORT_DESC];
                    
                ZeroMemory(&att, sizeof(att));

                att.type = KCDB_TYPE_STRING;
                att.name = AFS_ATTRNAME_REALM;
                LoadString(hResModule, 
                           IDS_ATTR_REALM_SHORT_DESC, 
                           short_desc, 
                           ARRAYLENGTH(short_desc));
                att.short_desc = short_desc;
                att.long_desc = NULL;
                att.id = KCDB_ATTR_INVALID;
                att.flags = KCDB_ATTR_FLAG_TRANSIENT;
                    
                if(KHM_FAILED(rv = kcdb_attrib_register(&att, 
                                                        &afs_attr_realm)))
                    goto _exit_init;
            }

            {
                wchar_t short_desc[KCDB_MAXCCH_SHORT_DESC];
                    
                ZeroMemory(&att, sizeof(att));

                att.type = afs_type_method;
                att.name = AFS_ATTRNAME_METHOD;
                LoadString(hResModule, 
                           IDS_ATTR_METHOD_SHORT_DESC, 
                           short_desc, 
                           ARRAYLENGTH(short_desc));
                att.short_desc = short_desc;
                att.long_desc = NULL;
                att.id = KCDB_ATTR_INVALID;
                att.flags = KCDB_ATTR_FLAG_TRANSIENT;
                    
                if(KHM_FAILED(rv = kcdb_attrib_register(&att, 
                                                        &afs_attr_method)))
                    goto _exit_init;
            }

            {
                wchar_t short_desc[KCDB_MAXCCH_SHORT_DESC];

                ZeroMemory(&att, sizeof(att));

                att.type = afs_type_principal;
                att.name = AFS_ATTRNAME_CLIENT_PRINC;
                LoadString(hResModule, 
                           IDS_ATTR_CLIENT_PRINC_SHORT_DESC, 
                           short_desc, 
                           ARRAYLENGTH(short_desc));
                att.short_desc = short_desc;
                att.long_desc = NULL;
                att.id = KCDB_ATTR_INVALID;
                att.flags = KCDB_ATTR_FLAG_TRANSIENT;
                    
                if(KHM_FAILED(rv = kcdb_attrib_register(&att, &afs_attr_client_princ)))
                    goto _exit_init;
            }

            {
                wchar_t short_desc[KCDB_MAXCCH_SHORT_DESC];

                ZeroMemory(&att, sizeof(att));

                att.type = afs_type_principal;
                att.name = AFS_ATTRNAME_SERVER_PRINC;
                LoadString(hResModule, 
                           IDS_ATTR_SERVER_PRINC_SHORT_DESC, 
                           short_desc, ARRAYLENGTH(short_desc));
                att.short_desc = short_desc;
                att.long_desc = NULL;
                att.id = KCDB_ATTR_INVALID;
                att.flags = KCDB_ATTR_FLAG_TRANSIENT;
                    
                if(KHM_FAILED(rv = kcdb_attrib_register(&att, &afs_attr_server_princ)))
                    goto _exit_init;
            }

            /* afs_credset is our stock credentials set that we
               use for all our credset needs (instead of creating
               a new one every time) */

            if(KHM_FAILED(rv = kcdb_credset_create(&afs_credset)))
                goto _exit_init;

            if(KHM_FAILED(rv = kcdb_credtype_get_id(KRB5_CREDTYPE_NAME,
                                                    &krb5_credtype_id)))
                goto _exit_init;

            /* register the configuration nodes */
            {
                khui_config_node node_ident;
                khui_config_node_reg reg;
                wchar_t wshort_desc[KHUI_MAXCCH_SHORT_DESC];
                wchar_t wlong_desc[KHUI_MAXCCH_LONG_DESC];

                if (KHM_FAILED(rv = khui_cfg_open(NULL,
                                                  L"KhmIdentities",
                                                  &node_ident)))
                    goto _exit_init;

                ZeroMemory(&reg, sizeof(reg));
                reg.name = AFS_CONFIG_NODE_MAIN;
                reg.short_desc = wshort_desc;
                reg.long_desc = wlong_desc;
                reg.h_module = hResModule;
                reg.dlg_template = MAKEINTRESOURCE(IDD_CFG_AFS);
                reg.dlg_proc = afs_cfg_main_proc;
                reg.flags = 0;
                LoadString(hResModule, IDS_CFG_MAIN_LONG,
                           wlong_desc, ARRAYLENGTH(wlong_desc));
                LoadString(hResModule, IDS_CFG_MAIN_SHORT,
                           wshort_desc, ARRAYLENGTH(wshort_desc));

                khui_cfg_register(NULL, &reg);

                ZeroMemory(&reg, sizeof(reg));
                reg.name = AFS_CONFIG_NODE_IDS;
                reg.short_desc = wshort_desc;
                reg.long_desc = wshort_desc;
                reg.h_module = hResModule;
                reg.dlg_template = MAKEINTRESOURCE(IDD_CFG_IDS_TAB);
                reg.dlg_proc = afs_cfg_ids_proc;
                reg.flags = KHUI_CNFLAG_SUBPANEL;
                LoadString(hResModule, IDS_CFG_IDS_TAB,
                           wshort_desc, ARRAYLENGTH(wshort_desc));

                khui_cfg_register(node_ident, &reg);

                ZeroMemory(&reg, sizeof(reg));
                reg.name = AFS_CONFIG_NODE_ID;
                reg.short_desc = wshort_desc;
                reg.long_desc = wshort_desc;
                reg.h_module = hResModule;
                reg.dlg_template = MAKEINTRESOURCE(IDD_CFG_ID_TAB);
                reg.dlg_proc = afs_cfg_id_proc;
                reg.flags = KHUI_CNFLAG_SUBPANEL | KHUI_CNFLAG_PLURAL;
                LoadString(hResModule, IDS_CFG_ID_TAB,
                           wshort_desc, ARRAYLENGTH(wshort_desc));

                khui_cfg_register(node_ident, &reg);
            }

            /* and register the AFS message type */
            rv = kmq_register_type(AFS_MSG_TYPENAME, &afs_msg_type_id);

            if (KHM_SUCCEEDED(rv))
                kmq_subscribe(afs_msg_type_id, afs_plugin_cb);

            /* if the configuration is set to disable afscreds.exe,
               then we look for the shortcut and remove it if
               found. */
            if (KHM_SUCCEEDED(kmm_get_plugin_config(AFS_PLUGIN_NAME,
                                                    0,
                                                    &csp_afscred))) {
                wchar_t wpath[MAX_PATH];

                khc_read_int32(csp_afscred, L"Disableafscreds",
                               &disable_afscreds);

                if (disable_afscreds &&
                    afs_cfg_get_afscreds_shortcut(wpath)) {

                    DeleteFile(wpath);

                }

                khc_close_space(csp_afscred);
            }

            /* try to register the "AFS Help" menu item, if
               possible */
            {
                khm_handle h_sub = NULL;
                wchar_t short_desc[KHUI_MAXCCH_SHORT_DESC];
                wchar_t long_desc[KHUI_MAXCCH_LONG_DESC];

#if KH_VERSION_API < 7

                khm_version libver;
                khm_ui_4 apiver;

                khm_get_lib_version(&libver, &apiver);

                if (apiver < 7)
                    goto no_custom_help;

                hm_netidmgr = LoadLibrary(L"nidmgr32.dll");

                if (hm_netidmgr == NULL)
                    goto no_custom_help;

                pkhui_action_lock = (void (KHMAPI *)(void))
                    GetProcAddress(hm_netidmgr, "_khui_action_lock@0");
                pkhui_action_unlock = (void (KHMAPI *)(void))
                    GetProcAddress(hm_netidmgr, "_khui_action_unlock@0");
                pkhui_refresh_actions = (void (KHMAPI *)(void))
                    GetProcAddress(hm_netidmgr, "_khui_refresh_actions@0");
                pkhui_request_UI_callback = (khm_int32 (KHMAPI *)(khm_ui_callback, void *))
                    GetProcAddress(hm_netidmgr, "_khui_request_UI_callback@8");

                if (pkhui_action_lock == NULL ||
                    pkhui_action_unlock == NULL ||
                    pkhui_refresh_actions == NULL ||
                    pkhui_request_UI_callback == NULL)

                    goto no_custom_help;

#endif
                kmq_create_subscription(afs_plugin_cb, &h_sub);

                LoadString(hResModule, IDS_ACTION_AFS_HELP,
                           short_desc, ARRAYLENGTH(short_desc));
                LoadString(hResModule, IDS_ACTION_AFS_HELP_TT,
                           long_desc, ARRAYLENGTH(long_desc));

                action_id_afs_help = khui_action_create(NULL,
                                                        short_desc,
                                                        long_desc,
                                                        NULL,
                                                        KHUI_ACTIONTYPE_TRIGGER,
                                                        h_sub);

                if (action_id_afs_help != 0) {
                    khm_size s;
                    khm_size i;
                    khui_menu_def * help_menu;
                    khm_boolean refresh = FALSE;

                    khui_action_lock();

                    help_menu = khui_find_menu(KHUI_MENU_HELP);
                    if (help_menu) {
                        s = khui_menu_get_size(help_menu);

                        for (i=0; i < s; i++) {
                            khui_action_ref * aref;

                            aref = khui_menu_get_action(help_menu, i);

                            if (aref && !(aref->flags & KHUI_ACTIONREF_PACTION) &&
                                aref->action == KHUI_ACTION_HELP_INDEX) {

                                khui_menu_insert_action(help_menu,
                                                        i + 1,
                                                        action_id_afs_help,
                                                        0);
                                refresh = TRUE;
                                break;
                            }
                        }
                    }

                    khui_action_unlock();

                    if (refresh)
                        khui_refresh_actions();
                }

#if KH_VERSION_API < 7
            no_custom_help:
                ;
#endif
            }

        _exit_init:
            if(ct.short_desc)
                PFREE(ct.short_desc);
            if(ct.long_desc)
                PFREE(ct.long_desc);
        }
        /* now that the critical stuff is done, we move on to the
           non-critical stuff */
        if(KHM_SUCCEEDED(rv)) {
            initialized = TRUE;

            /* obtain existing tokens */
            afs_list_tokens();
        }

        /* define this so that if there are no TGT's, we don't
           deadlock trying to open a new creds dialog from within the
           new creds dialog. */
        SetEnvironmentVariable(L"KERBEROSLOGIN_NEVER_PROMPT", L"1");

        break;
        /* end of KMSG_SYSTEM_INIT */

    case KMSG_SYSTEM_EXIT:

        /* Try to remove the AFS plug-in action from Help menu if it
           was successfully registered.  Also, delete the action. */
        if (action_id_afs_help != 0) {

            khui_menu_def * help_menu;
            khm_boolean menu_changed = FALSE;

            khui_action_lock();

            help_menu = khui_find_menu(KHUI_MENU_HELP);
            if (help_menu) {
                khm_size s;
                khm_size i;

                s = khui_menu_get_size(help_menu);
                for (i=0; i < s; i++) {
                    khui_action_ref * aref = khui_menu_get_action(help_menu, i);

                    if (aref && !(aref->flags & KHUI_ACTIONREF_PACTION) &&
                        aref->action == action_id_afs_help) {

                        khui_menu_remove_action(help_menu, i);
                        menu_changed = TRUE;
                        break;
                    }
                }
            }

            khui_action_delete(action_id_afs_help);

            khui_action_unlock();

            if (menu_changed)
                khui_refresh_actions();

            action_id_afs_help = 0;
        }

        if (afs_msg_type_id != -1) {
            kmq_unsubscribe(afs_msg_type_id, afs_plugin_cb);
            kmq_unregister_type(afs_msg_type_id);
        }
        if(afs_credtype_id >= 0) {
            kcdb_credtype_unregister(afs_credtype_id);
        }
#if 0
        if(afs_attr_client >= 0) {
            kcdb_attrib_unregister(afs_attr_client);
        }
#endif
        if(afs_attr_cell >= 0) {
            kcdb_attrib_unregister(afs_attr_cell);
        }
        if(afs_attr_realm >= 0) {
            kcdb_attrib_unregister(afs_attr_realm);
        }
        if(afs_attr_method >= 0) {
            kcdb_attrib_unregister(afs_attr_method);
        }
        if(afs_attr_client_princ >= 0) {
            kcdb_attrib_unregister(afs_attr_client_princ);
        }
        if(afs_attr_server_princ >= 0) {
            kcdb_attrib_unregister(afs_attr_server_princ);
        }
        if(afs_type_principal >= 0) {
            kcdb_type_unregister(afs_type_principal);
        }
        if(afs_type_method >= 0) {
            kcdb_type_unregister(afs_type_method);
        }
        initialized = FALSE;
        if(afs_credset)
            kcdb_credset_delete(afs_credset);

        /* afs_sub doesn't need to be deleted.  That is taken care
           of when unregistering the afs cred type */
        afs_sub = NULL;

#if KH_VERSION_API < 7
        if (hm_netidmgr)
            FreeLibrary(hm_netidmgr);

        pkhui_action_lock = NULL;
        pkhui_action_unlock = NULL;
        pkhui_refresh_actions = NULL;
        pkhui_request_UI_callback = NULL;
#endif

        rv = KHM_ERROR_SUCCESS;
        break;
        /* end of KMSG_SYSTEM_EXIT */
    }
    return rv;
}

/* process KMSG_KCDB messages */
khm_int32 KHMAPI 
afs_msg_kcdb(khm_int32 msg_subtype, 
             khm_ui_4 uparam, 
             void * vparam)
{
    khm_int32 rv = KHM_ERROR_SUCCESS;

    /* we don't really do anything with this yet */
#if 0
    switch(msg_subtype) {
    }
#endif

    return rv;
}



static khm_int32 KHMAPI
afs_cred_destroy_proc(khm_handle cred, void * rock) {
    khm_int32 t;

    if (KHM_FAILED(kcdb_cred_get_type(cred, &t)) ||
        t != afs_credtype_id)
        return KHM_ERROR_SUCCESS;

    afs_unlog_cred(cred);

    return KHM_ERROR_SUCCESS;
}

/* process KMSG_CRED messages */
khm_int32 KHMAPI 
afs_msg_cred(khm_int32 msg_subtype, 
             khm_ui_4 uparam, 
             void * vparam)
{
    khm_int32 rv = KHM_ERROR_SUCCESS;

    switch(msg_subtype) {
    case KMSG_CRED_REFRESH:
        afs_list_tokens();
        break;

    case KMSG_CRED_DESTROY_CREDS:
        {
            khui_action_context * ctx;

            ctx = (khui_action_context *) vparam;

            if (ctx->credset) {
                _begin_task(0);
                _report_cs0(KHERR_INFO, L"Destroying AFS Tokens");
                _describe();

                kcdb_credset_apply(ctx->credset,
                                   afs_cred_destroy_proc,
                                   NULL);

                _end_task();
            }
        }
        break;

    default:

        if (IS_CRED_ACQ_MSG(msg_subtype))
            return afs_msg_newcred(msg_subtype, uparam, vparam);
    }

    return rv;
}


khm_int32 KHMAPI
help_launcher(HWND hwnd_main, void * rock) {
    afs_html_help(hwnd_main, NULL, HH_DISPLAY_TOC, 0);

    return KHM_ERROR_SUCCESS;
}

khm_int32 KHMAPI 
afs_msg_act(khm_int32 msg_subtype, 
            khm_ui_4 uparam, 
            void * vparam)
{
    khm_int32 rv = KHM_ERROR_SUCCESS;

    if (msg_subtype == KMSG_ACT_ACTIVATE &&
        uparam == action_id_afs_help) {

        khui_request_UI_callback(help_launcher, NULL);

    }

    return rv;
}
