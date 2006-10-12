/*
 * Copyright (c) 2004 Massachusetts Institute of Technology
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
#include<assert.h>
#include<wchar.h>

/* supported API versions */
#define AFS_PLUGIN_VERSION_MIN 0x00000001
#define AFS_PLUGIN_VERSION_MAX AFS_PLUGIN_VERSION

#define MAX_EXTENSIONS 8

afs_extension extensions[MAX_EXTENSIONS];
khm_size n_extensions = 0;
khm_int32 next_method_id = AFS_TOKEN_USER;

/* not threadsafe.  should only be called from the plugin thread */
khm_int32
afs_add_extension(afs_msg_announce * ann) {
    size_t cbname = 0;
    size_t cbtashort = 0;
    size_t cbtalong = 0;
    afs_extension * ext;
    wchar_t * tmp;

    if (ann->cbsize != sizeof(afs_msg_announce) ||
        FAILED(StringCbLength(ann->name, KHUI_MAXCB_NAME, &cbname)) ||
        ann->sub == NULL ||
        (ann->provide_token_acq &&
         ((FAILED(StringCbLength(ann->token_acq.short_desc,
                                 KHUI_MAXCB_SHORT_DESC,
                                 &cbtashort))) ||
          (ann->token_acq.long_desc &&
           FAILED(StringCbLength(ann->token_acq.long_desc,
                                 KHUI_MAXCB_LONG_DESC,
                                 &cbtalong))))) ||
        ann->version < AFS_PLUGIN_VERSION_MIN ||
        ann->version > AFS_PLUGIN_VERSION_MAX)

        return KHM_ERROR_INVALID_PARAM;

    if (n_extensions == MAX_EXTENSIONS)
        return KHM_ERROR_NO_RESOURCES;

    cbname += sizeof(wchar_t);
    cbtashort += sizeof(wchar_t);
    cbtalong += sizeof(wchar_t);

    ext = &extensions[n_extensions];

    *ext = *ann;

    tmp = PMALLOC(cbname);
#ifdef DEBUG
    assert(tmp);
#endif
    StringCbCopy(tmp, cbname, ann->name);
    ext->name = tmp;

    if (ann->provide_token_acq) {
        tmp = PMALLOC(cbtashort);
#ifdef DEBUG
        assert(tmp);
#endif
        StringCbCopy(tmp, cbtashort, ann->token_acq.short_desc);
        ext->token_acq.short_desc = tmp;

        if (ann->token_acq.long_desc) {
            tmp = PMALLOC(cbtalong);
#ifdef DEBUG
            assert(tmp);
#endif
            StringCbCopy(tmp, cbtalong,
                         ann->token_acq.long_desc);
            ext->token_acq.long_desc = tmp;
        } else {
            ext->token_acq.long_desc = NULL;
        }

        ann->token_acq.method_id = next_method_id++;
        ext->token_acq.method_id = ann->token_acq.method_id;
    } else {
        ZeroMemory(&ext->token_acq, sizeof(ext->token_acq));
    }

    n_extensions++;

    return KHM_ERROR_SUCCESS;
}

void
afs_free_extension(khm_int32 idx) {
    afs_extension * ext;

#ifdef DEBUG
    assert(idx >= 0 && idx < (khm_int32) n_extensions);
#endif

    ext = &extensions[idx];

    if (ext->name)
        PFREE((void *) ext->name);
    if (ext->token_acq.short_desc)
        PFREE((void *) ext->token_acq.short_desc);
    if (ext->token_acq.long_desc)
        PFREE((void *) ext->token_acq.long_desc);
    if (ext->sub)
        kmq_delete_subscription(ext->sub);

    ZeroMemory(ext, sizeof(*ext));
}

/* not thread safe.  only call from plugin thread */
void
afs_remove_extension(khm_int32 idx) {
    if (idx < 0 || idx > (khm_int32) n_extensions)
        return;

    afs_free_extension(idx);

    if (idx == n_extensions-1) {
        n_extensions--;
    } else {
        MoveMemory(&extensions[idx], &extensions[idx + 1],
                   (n_extensions - (idx+1)) * sizeof(*extensions));
    }
}

/* not thread safe. only call from the plugin thread */
afs_extension *
afs_find_extension(const wchar_t * name) {
    khm_size i;

    for (i=0; i < n_extensions; i++) {
        if (extensions[i].name &&
            !wcscmp(extensions[i].name, name))
            return &extensions[i];
    }

    return NULL;
}

/* not thread safe.  only call from the plugin thread */
khm_boolean
afs_is_valid_method_id(afs_tk_method method) {
    khm_size i;

    if (method == AFS_TOKEN_AUTO ||
        method == AFS_TOKEN_KRB5 ||
        method == AFS_TOKEN_KRB524 ||
        method == AFS_TOKEN_KRB4)
        return TRUE;

    for (i=0; i < n_extensions; i++) {
        if (extensions[i].provide_token_acq &&
            extensions[i].token_acq.method_id == method)
            return TRUE;
    }

    return FALSE;
}

khm_boolean
afs_method_describe(afs_tk_method method, khm_int32 flags,
                    wchar_t * wbuf, khm_size cbbuf) {
    khm_size idx;

    switch(method) {
    case AFS_TOKEN_AUTO:
        return LoadString(hResModule,
                          ((flags & KCDB_TS_SHORT)? 
                           IDS_NC_METHOD_AUTO:
                           IDS_NC_METHODL_AUTO),
                          wbuf, (int) cbbuf / sizeof(wchar_t));

    case AFS_TOKEN_KRB5:
        return LoadString(hResModule,
                          ((flags & KCDB_TS_SHORT)?
                           IDS_NC_METHOD_KRB5:
                           IDS_NC_METHODL_KRB5),
                          wbuf, (int) cbbuf / sizeof(wchar_t));

    case AFS_TOKEN_KRB524:
        return LoadString(hResModule,
                          ((flags & KCDB_TS_SHORT)?
                           IDS_NC_METHOD_KRB524:
                           IDS_NC_METHODL_KRB524),
                          wbuf, (int) cbbuf / sizeof(wchar_t));

    case AFS_TOKEN_KRB4:
        return LoadString(hResModule,
                          ((flags & KCDB_TS_SHORT)?
                           IDS_NC_METHOD_KRB4:
                           IDS_NC_METHODL_KRB4),
                          wbuf, (int) cbbuf / sizeof(wchar_t));

    default:
        for (idx = 0; idx < n_extensions; idx++) {
            if(!extensions[idx].provide_token_acq ||
               extensions[idx].token_acq.method_id != method)
                continue;

            if ((flags & KCDB_TS_SHORT) ||
                extensions[idx].token_acq.long_desc == NULL)
                return SUCCEEDED(StringCbCopy(wbuf, cbbuf,
                                              extensions[idx].token_acq.short_desc));
            else
                return SUCCEEDED(StringCbCopy(wbuf, cbbuf,
                                              extensions[idx].token_acq.long_desc));
        }
    }

    return FALSE;
}

afs_tk_method
afs_get_next_method_id(afs_tk_method method) {
    khm_size idx;

    switch(method) {
    case -1:
        return AFS_TOKEN_AUTO;
    case AFS_TOKEN_AUTO:
        return AFS_TOKEN_KRB5;
    case AFS_TOKEN_KRB5:
        return AFS_TOKEN_KRB524;
    case AFS_TOKEN_KRB524:
        return AFS_TOKEN_KRB4;
    case AFS_TOKEN_KRB4:
        idx = 0;
        break;
    default:
        for(idx = 0; idx < n_extensions; idx ++) {
            if (extensions[idx].provide_token_acq &&
                extensions[idx].token_acq.method_id == method)
                break;
        }
        idx++;
    }

    for(; idx < n_extensions; idx++) {
        if (extensions[idx].provide_token_acq)
            return extensions[idx].token_acq.method_id;
    }

    return -1;
}

/* not thread safe.  only call from the plugin thread */
afs_extension *
afs_get_next_token_acq(afs_extension * f) {
    khm_size idx;

    if (f == NULL)
        idx = 0;
    else
        idx = (f - extensions) + 1;

    for(; idx < n_extensions; idx++) {
        if (extensions[idx].provide_token_acq)
            return &extensions[idx];
    }

    return NULL;
}

afs_extension *
afs_get_extension(khm_size i) {
    if (i >= n_extensions)
        return NULL;
    else
        return &extensions[i];
}

afs_tk_method
afs_get_method_id(wchar_t * name) {
    if (!wcscmp(name, AFS_TOKENNAME_AUTO))
        return AFS_TOKEN_AUTO;
    else if (!wcscmp(name, AFS_TOKENNAME_KRB5))
        return AFS_TOKEN_KRB5;
    else if (!wcscmp(name, AFS_TOKENNAME_KRB524))
        return AFS_TOKEN_KRB524;
    else if (!wcscmp(name, AFS_TOKENNAME_KRB4))
        return AFS_TOKEN_KRB4;
    else {
        khm_size i;

        for (i=0; i < n_extensions; i++) {
            if (!extensions[i].provide_token_acq)
                continue;

            if (!wcscmp(extensions[i].name, name))
                return extensions[i].token_acq.method_id;
        }
    }

    return AFS_TOKEN_AUTO;
}

khm_boolean
afs_get_method_name(afs_tk_method method, wchar_t * buf, khm_size cbbuf) {
    if (method == AFS_TOKEN_AUTO)
        return SUCCEEDED(StringCbCopy(buf, cbbuf, AFS_TOKENNAME_AUTO));
    else if (method == AFS_TOKEN_KRB5)
        return SUCCEEDED(StringCbCopy(buf, cbbuf, AFS_TOKENNAME_KRB5));
    else if (method == AFS_TOKEN_KRB524)
        return SUCCEEDED(StringCbCopy(buf, cbbuf, AFS_TOKENNAME_KRB524));
    else if (method == AFS_TOKEN_KRB4)
        return SUCCEEDED(StringCbCopy(buf, cbbuf, AFS_TOKENNAME_KRB4));
    else {
        khm_size i;

        for (i=0; i < n_extensions; i++) {
            if (!extensions[i].provide_token_acq)
                continue;
            if (extensions[i].token_acq.method_id == method)
                return SUCCEEDED(StringCbCopy(buf, cbbuf,
                                              extensions[i].name));
        }
    }

    return FALSE;
}

/* not thread safe.  only call from the plugin thread */
khm_boolean
afs_ext_resolve_token(const wchar_t * cell,
                      const struct ktc_token * token,
                      const struct ktc_principal * serverp,
                      const struct ktc_principal * clientp,
                      khm_handle * pident,
                      afs_tk_method * pmethod) {

    afs_msg_resolve_token rt;
    khm_size idx;
    khm_int32 rv;

    ZeroMemory(&rt, sizeof(rt));

    rt.cbsize = sizeof(rt);

    rt.cell = cell;
    rt.token = token;
    rt.serverp = serverp;
    rt.clientp = clientp;
    rt.method = AFS_TOKEN_AUTO;
    rt.ident = NULL;

    for (idx = 0; idx < n_extensions; idx++) {
        if (!extensions[idx].provide_token_acq)
            continue;

        rv = kmq_send_sub_msg(extensions[idx].sub,
                              afs_msg_type_id,
                              AFS_MSG_RESOLVE_TOKEN,
                              0,
                              (void *) &rt);

        if (KHM_SUCCEEDED(rv)) {
            assert(rt.ident != NULL);

            *pident = rt.ident;
            *pmethod = rt.method;

            return TRUE;
        }
    }

    return FALSE;
}

/* not thread safe. only call from the plugin thread */
khm_boolean
afs_ext_klog(afs_tk_method method,
             khm_handle   identity,
             const char * service,
             const char * cell,
             const char * realm,
             const afs_conf_cell * cell_config,
             khm_int32    lifetime) {

    khm_size idx;
    khm_int32 rv = KHM_ERROR_GENERAL;
    afs_msg_klog msg;
    afs_conf_cell cellconfig;

    ZeroMemory(&msg, sizeof(msg));
    ZeroMemory(&cellconfig, sizeof(cellconfig));

    msg.cbsize = sizeof(msg);

    msg.identity = identity;
    msg.service = service;
    msg.cell = cell;
    msg.realm = realm;
    msg.lifetime = lifetime;

    msg.cell_config = &cellconfig;

    cellconfig = *cell_config;
    cellconfig.cbsize = sizeof(cellconfig);

    for (idx = 0; idx < n_extensions; idx++) {
        if (!extensions[idx].provide_token_acq ||
            (method != AFS_TOKEN_AUTO &&
             extensions[idx].token_acq.method_id != method))
            continue;

        rv = kmq_send_sub_msg(extensions[idx].sub,
                              afs_msg_type_id,
                              AFS_MSG_KLOG,
                              0,
                              (void *) &msg);

        if (KHM_SUCCEEDED(rv))
            return TRUE;
    }

    return FALSE;
}

khm_int32 KHMAPI 
afs_msg_ext(khm_int32 msg_subtype, khm_ui_4 uparam, void * vparam) {
    switch(msg_subtype) {
    case AFS_MSG_ANNOUNCE:
        return afs_add_extension((afs_msg_announce *) vparam);
    }

    return KHM_ERROR_SUCCESS;
}
