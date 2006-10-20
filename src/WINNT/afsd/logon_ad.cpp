/*

Copyright 2004 by the Massachusetts Institute of Technology

All rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the Massachusetts
Institute of Technology (M.I.T.) not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


//#pragma keyword("interface",on)
#define interface struct
#define SECURITY_WIN32
#include "afslogon.h"

/**/
#include <security.h>
#include <ntsecapi.h>
#include <sddl.h>
#include <unknwn.h>
#include <oaidl.h>
#include <Iads.h>
#include <adshlp.h>
/**/

#define SEC_ERR_VALUE(v) if (status==v) return #v

char * _get_sec_err_text(SECURITY_STATUS status) {
	SEC_ERR_VALUE(SEC_E_OK);
	SEC_ERR_VALUE(SEC_I_CONTINUE_NEEDED);
	SEC_ERR_VALUE(SEC_I_COMPLETE_NEEDED);
	SEC_ERR_VALUE(SEC_I_COMPLETE_AND_CONTINUE);
	SEC_ERR_VALUE(SEC_E_INCOMPLETE_MESSAGE);
	SEC_ERR_VALUE(SEC_I_INCOMPLETE_CREDENTIALS);
	SEC_ERR_VALUE(SEC_E_INVALID_HANDLE);
	SEC_ERR_VALUE(SEC_E_TARGET_UNKNOWN);
	SEC_ERR_VALUE(SEC_E_LOGON_DENIED);
	SEC_ERR_VALUE(SEC_E_INTERNAL_ERROR);
	SEC_ERR_VALUE(SEC_E_NO_CREDENTIALS);
	SEC_ERR_VALUE(SEC_E_NO_AUTHENTICATING_AUTHORITY);
	SEC_ERR_VALUE(SEC_E_INSUFFICIENT_MEMORY);
	SEC_ERR_VALUE(SEC_E_INVALID_TOKEN);
	SEC_ERR_VALUE(SEC_E_UNSUPPORTED_FUNCTION);
	SEC_ERR_VALUE(SEC_E_WRONG_PRINCIPAL);
	return "Unknown";
}

#undef SEC_ERR_VALUE

DWORD LogonSSP(PLUID lpLogonId, PCtxtHandle outCtx) {
	DWORD code = 1;
    SECURITY_STATUS status;
	CredHandle creds;
	CtxtHandle ctxclient,ctxserver;
	TimeStamp expiry;
	BOOL cont = TRUE;
	BOOL first = TRUE;
	SecBufferDesc sdescc,sdescs;
	SecBuffer stokc,stoks;
	ULONG cattrs,sattrs;
	int iters = 10;

	outCtx->dwLower = 0;
	outCtx->dwUpper = 0;

	cattrs = 0;
	sattrs = 0;

	status = AcquireCredentialsHandle(
		NULL,
		"Negotiate",
		SECPKG_CRED_BOTH,
		lpLogonId,
		NULL,
		NULL,
		NULL,
		&creds,
		&expiry);

	if (status != SEC_E_OK) {
		DebugEvent("AcquireCredentialsHandle failed: %lX", status);
		goto ghp_0;
	}

	sdescc.cBuffers = 1;
	sdescc.pBuffers = &stokc;
	sdescc.ulVersion = SECBUFFER_VERSION;

	stokc.BufferType = SECBUFFER_TOKEN;
	stokc.cbBuffer = 0;
	stokc.pvBuffer = NULL;

	sdescs.cBuffers = 1;
	sdescs.pBuffers = &stoks;
	sdescs.ulVersion = SECBUFFER_VERSION;

	stoks.BufferType = SECBUFFER_TOKEN;
	stoks.cbBuffer = 0;
	stoks.pvBuffer = NULL;

	do {
		status = InitializeSecurityContext(
			&creds,
			((first)? NULL:&ctxclient),
            NULL,
			ISC_REQ_DELEGATE | ISC_REQ_ALLOCATE_MEMORY,
			0,
			SECURITY_NATIVE_DREP,
			((first)?NULL:&sdescs),
			0,
			&ctxclient,
			&sdescc,
			&cattrs,
			&expiry
			);

		DebugEvent("InitializeSecurityContext returns status[%lX](%s)",status,_get_sec_err_text(status));

		if (!first) FreeContextBuffer(stoks.pvBuffer);
        
		if (status == SEC_I_COMPLETE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
			CompleteAuthToken(&ctxclient, &sdescc);
		}

		if (status != SEC_I_CONTINUE_NEEDED && status != SEC_I_COMPLETE_AND_CONTINUE) {
			cont = FALSE;
		}

		if (!stokc.cbBuffer && !cont) {
			DebugEvent("Breaking out after InitializeSecurityContext");
			break;
		}

		status = AcceptSecurityContext(
			&creds,
			((first)?NULL:&ctxserver),
			&sdescc,
			ASC_REQ_DELEGATE | ASC_REQ_ALLOCATE_MEMORY,
			SECURITY_NATIVE_DREP,
			&ctxserver,
			&sdescs,
			&sattrs,
			&expiry);

		DebugEvent("AcceptSecurityContext returns status[%lX](%s)", status, _get_sec_err_text(status));

		FreeContextBuffer(stokc.pvBuffer);

		if (status == SEC_I_COMPLETE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
			CompleteAuthToken(&ctxserver,&sdescs);
		}

		if (status == SEC_I_CONTINUE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
			cont = TRUE;
		}

		if (!cont)
			FreeContextBuffer(stoks.pvBuffer);

		first = FALSE;
		iters--; /* just in case, hard limit on loop */
	} while (cont && iters);

	if (sattrs & ASC_RET_DELEGATE) {
		DebugEvent("Received delegate context");
		*outCtx = ctxserver;
		code = 0;
	} else {
		DebugEvent("Didn't receive delegate context");
		outCtx->dwLower = 0;
		outCtx->dwUpper = 0;
		DeleteSecurityContext(&ctxserver);
	}

	DeleteSecurityContext(&ctxclient);
    FreeCredentialsHandle(&creds);
ghp_0:
	return code;
}

DWORD QueryAdHomePathFromSid(char * homePath, size_t homePathLen, PSID psid, PWSTR domain) {
	DWORD code = 1; /* default is failure */
	NTSTATUS rv = 0;
	HRESULT hr = S_OK;
	LPWSTR p = NULL;
	WCHAR adsPath[MAX_PATH] = L"";
	BOOL coInitialized = FALSE;
    CHAR ansidomain[256], *a;

	homePath[0] = '\0';
    
    /* I trust this is an ASCII domain name */
    for ( p=domain, a=ansidomain; *a = (CHAR)*p; p++, a++);
    DebugEvent("Domain: %s", ansidomain);

    if (ConvertSidToStringSidW(psid,&p)) {
        IADsNameTranslate *pNto;

        DebugEvent("Got SID string [%S]", p);

        hr = CoInitialize(NULL);
        if (SUCCEEDED(hr))
            coInitialized = TRUE;

        hr = CoCreateInstance( CLSID_NameTranslate,
                               NULL,
                               CLSCTX_INPROC_SERVER,
                               IID_IADsNameTranslate,
                               (void**)&pNto);

        if (FAILED(hr)) { DebugEvent("Can't create nametranslate object"); }
        else {
            hr = pNto->Init(ADS_NAME_INITTYPE_GC,L"");
            if (FAILED(hr)) { 
                DebugEvent("NameTranslate Init GC failed [%ld]", hr);
                if ( domain ) {
                    hr = pNto->Init(ADS_NAME_INITTYPE_DOMAIN,domain);
                    if (FAILED(hr)) { 
                        DebugEvent("NameTranslate Init Domain failed [%ld]", hr);
                    }
                }
            }

            if (!FAILED(hr)) {
                hr = pNto->Set(ADS_NAME_TYPE_SID_OR_SID_HISTORY_NAME, p);
                if (FAILED(hr)) { DebugEvent("Can't set sid string"); }
                else {
                    BSTR bstr;

                    hr = pNto->Get(ADS_NAME_TYPE_1779, &bstr);
                    if(SUCCEEDED(hr)) {
                        hr = StringCchCopyW(adsPath, MAX_PATH, bstr);
                        if(FAILED(hr)) {
                            DebugEvent("Overflow while copying ADS path");
                            adsPath[0] = L'\0';
                        }

                        SysFreeString(bstr);
                    }
                }
            }
            pNto->Release();
        }

        LocalFree(p);

    } else {
        DebugEvent("Can't convert sid to string");
    }

	if (adsPath[0]) {
		WCHAR fAdsPath[MAX_PATH];
		IADsUser *pAdsUser;
		BSTR bstHomeDir = NULL;

		hr = StringCchPrintfW(fAdsPath, MAX_PATH, L"LDAP://%s", adsPath);
		if (hr != S_OK) {
			DebugEvent("Can't format full adspath");
			goto cleanup;
		}

		DebugEvent("Trying adsPath=[%S]", fAdsPath);

		hr = ADsGetObject( fAdsPath, IID_IADsUser, (LPVOID *) &pAdsUser);
		if (hr != S_OK) {
			DebugEvent("Can't open IADs object");
			goto cleanup;
		}

        hr = pAdsUser->get_Profile(&bstHomeDir);
		if (hr != S_OK) {
			DebugEvent("Can't get profile directory");
			goto cleanup_homedir_section;
		}

		wcstombs(homePath, bstHomeDir, homePathLen);

		DebugEvent("Got homepath [%s]", homePath);

		SysFreeString(bstHomeDir);

		code = 0;

cleanup_homedir_section:
		pAdsUser->Release();
	}

cleanup:
	if (coInitialized)
		CoUninitialize();

	return code;               
}

/* Try to determine the user's AD home path.  *homePath is assumed to be at least MAXPATH bytes. 
   If successful, opt.flags is updated with LOGON_FLAG_AD_REALM to indicate that we are dealing with
   an AD realm. */
DWORD GetAdHomePath(char * homePath, size_t homePathLen, PLUID lpLogonId, LogonOptions_t * opt) {
	CtxtHandle ctx;
	DWORD code = 0;
	SECURITY_STATUS status;

	homePath[0] = '\0';

	if (LogonSSP(lpLogonId,&ctx)) {
        DebugEvent("Failed LogonSSP");
		return 1;
    } else {
		status = ImpersonateSecurityContext(&ctx);
		if (status == SEC_E_OK) {
		    PSECURITY_LOGON_SESSION_DATA plsd;
            NTSTATUS rv;

            rv = LsaGetLogonSessionData(lpLogonId, &plsd);
            if (rv == 0) {
                PWSTR domain;

                domain = (PWSTR)malloc(sizeof(WCHAR) * (plsd->LogonDomain.Length+1));
                memcpy(domain, plsd->LogonDomain.Buffer, sizeof(WCHAR) * (plsd->LogonDomain.Length));
                domain[plsd->LogonDomain.Length] = 0;

                if (!QueryAdHomePathFromSid(homePath,homePathLen,plsd->Sid,domain)) {
                    DebugEvent("Returned home path [%s]",homePath);
                    opt->flags |= LOGON_FLAG_AD_REALM;
                }
                free(domain);
                LsaFreeReturnBuffer(plsd);
            } else {
                DebugEvent("LsaGetLogonSessionData failed [%lX]", rv);
            }
            RevertSecurityContext(&ctx);
		} else {
			DebugEvent("Can't impersonate context [%lX]",status);
			code = 1;
		}

        DeleteSecurityContext(&ctx);
		return code;
	}
}

BOOL GetLocalShortDomain(PWSTR Domain, DWORD cbDomain)
{
    HRESULT hr;
    IADsADSystemInfo *pADsys;
    BOOL coInitialized = FALSE;
    BOOL retval = FALSE;

    hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
        coInitialized = TRUE;

    hr = CoCreateInstance(CLSID_ADSystemInfo,
                           NULL,
                           CLSCTX_INPROC_SERVER,
                           IID_IADsADSystemInfo,
                           (void**)&pADsys);
    if ( !FAILED(hr) ) {
        BSTR bstr;

        hr = pADsys->get_DomainShortName(&bstr);
        if ( !FAILED(hr) ) {
            hr = StringCbCopyW( Domain, cbDomain, bstr );
            if(SUCCEEDED(hr)) {
                retval = TRUE;
            }
            SysFreeString(bstr);
        }
        pADsys->Release();
    }

	if (coInitialized)
		CoUninitialize();

    return retval;
}
