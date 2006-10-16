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

/* Disable the 'macro redefinition' warning which is getting
   triggerred by a redefinition of the ENCRYPT and DECRYPT macros. */
#pragma warning (push)
#pragma warning (disable: 4005)

#include<afscred.h>
#include<kmm.h>
#include<dynimport.h>
#ifdef DEBUG
#include<assert.h>
#endif

#pragma warning (pop)

kmm_module h_khModule; /* KMM's handle to this module */
HINSTANCE hInstance;
HMODULE hResModule;    /* HMODULE to the resource library */

khm_handle csp_plugins      = NULL;
khm_handle csp_afscred      = NULL;
khm_handle csp_params       = NULL;

kmm_module_locale locales[] = {
    LOCALE_DEF(MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US), L"afscred_en_us.dll", KMM_MLOC_FLAG_DEFAULT)
};
int n_locales = ARRAYLENGTH(locales);

/* These two probably should not do anything */
void init_afs() {
}

void exit_afs() {
}

/* called by the NetIDMgr module manager */
KHMEXP khm_int32 KHMAPI init_module(kmm_module h_module) {
    khm_int32 rv = KHM_ERROR_SUCCESS;
    kmm_plugin_reg pi;
    wchar_t buf[256];

    h_khModule = h_module;

    rv = kmm_set_locale_info(h_module, locales, n_locales);
    if(KHM_SUCCEEDED(rv)) {
        hResModule = kmm_get_resource_hmodule(h_module);
    } else {
        goto _exit;
    }

    ZeroMemory(&pi,sizeof(pi));

    pi.msg_proc = afs_plugin_cb;
    pi.name = AFS_PLUGIN_NAME;
    pi.type = KHM_PITYPE_CRED;
    pi.icon = LoadImage(hResModule, MAKEINTRESOURCE(IDI_AFSPLUGIN), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    pi.dependencies = AFS_PLUGIN_DEPS;
    pi.description = buf;

    LoadString(hResModule, IDS_PLUGIN_DESC,
               buf, ARRAYLENGTH(buf));

    kmm_provide_plugin(h_module, &pi);

    if(KHM_FAILED(rv = init_imports()))
        goto _exit;

    rv = kmm_get_plugins_config(0, &csp_plugins);
    if(KHM_FAILED(rv)) goto _exit;

    rv = khc_load_schema(csp_plugins, schema_afsconfig);
    if(KHM_FAILED(rv)) goto _exit;

    rv = khc_open_space(csp_plugins, CSNAME_AFSCRED, 0, &csp_afscred);
    if(KHM_FAILED(rv)) goto _exit;

    rv = khc_open_space(csp_afscred, CSNAME_PARAMS, 0, &csp_params);
    if(KHM_FAILED(rv)) goto _exit;

_exit:
    return rv;
}

/* called by the NetIDMgr module manager */
KHMEXP khm_int32 KHMAPI exit_module(kmm_module h_module) {
    exit_imports();

    if(csp_params) {
        khc_close_space(csp_params);
        csp_params = NULL;
    }
    if(csp_afscred) {
        khc_close_space(csp_afscred);
        csp_afscred = NULL;
    }
    if(csp_plugins) {
        khc_unload_schema(csp_plugins, schema_afsconfig);
        khc_close_space(csp_plugins);
        csp_plugins = NULL;
    }

    return KHM_ERROR_SUCCESS; /* the return code is ignored */
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL,
                    DWORD fdwReason,
                    LPVOID lpvReserved)
{
    switch(fdwReason) {
        case DLL_PROCESS_ATTACH:
            hInstance = hinstDLL;
            init_afs();
            break;
        case DLL_PROCESS_DETACH:
            exit_afs();
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}
