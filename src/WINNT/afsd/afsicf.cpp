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

#define _WIN32_DCOM
#include <windows.h>
#include <netfw.h>
#include <objbase.h>
#include <oleauto.h>
#include "afsicf.h"

#ifdef TESTMAIN
#include<stdio.h>
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"oleaut32.lib")
#define DEBUGOUT(x) printf(x)
#define DEBUGOUTW(x) wprintf(x)
#else
#define DEBUGOUT(x) OutputDebugString(x)
#define DEBUGOUTW(x) OutputDebugStringW(x)
#endif

/* an IPv4, enabled port with global scope */
struct global_afs_port_type {
    LPWSTR	name;
    LONG	port;
    NET_FW_IP_PROTOCOL protocol;
};

typedef struct global_afs_port_type global_afs_port_t;

global_afs_port_t afs_clientPorts[] = {
    { L"AFS CacheManager Callback (UDP)", 7001, NET_FW_IP_PROTOCOL_UDP }
#ifdef AFS_TCP
,   { L"AFS CacheManager Callback (TCP)", 7001, NET_FW_IP_PROTOCOL_TCP }
#endif
};

global_afs_port_t afs_serverPorts[] = {
    { L"AFS File Server (UDP)", 7000, NET_FW_IP_PROTOCOL_UDP },
#ifdef AFS_TCP
    { L"AFS File Server (TCP)", 7000, NET_FW_IP_PROTOCOL_TCP },
#endif
    { L"AFS User & Group Database (UDP)", 7002, NET_FW_IP_PROTOCOL_UDP },
#ifdef AFS_TCP
    { L"AFS User & Group Database (TCP)", 7002, NET_FW_IP_PROTOCOL_TCP },
#endif
    { L"AFS Volume Location Database (UDP)", 7003, NET_FW_IP_PROTOCOL_UDP },
#ifdef AFS_TCP
    { L"AFS Volume Location Database (TCP)", 7003, NET_FW_IP_PROTOCOL_TCP },
#endif
    { L"AFS/Kerberos Authentication (UDP)", 7004, NET_FW_IP_PROTOCOL_UDP },
#ifdef AFS_TCP
    { L"AFS/Kerberos Authentication (TCP)", 7004, NET_FW_IP_PROTOCOL_TCP },
#endif
    { L"AFS Volume Mangement (UDP)", 7005, NET_FW_IP_PROTOCOL_UDP },
#ifdef AFS_TCP
    { L"AFS Volume Mangement (TCP)", 7005, NET_FW_IP_PROTOCOL_TCP },
#endif
    { L"AFS Error Interpretation (UDP)", 7006, NET_FW_IP_PROTOCOL_UDP },
#ifdef AFS_TCP
    { L"AFS Error Interpretation (TCP)", 7006, NET_FW_IP_PROTOCOL_TCP },
#endif
    { L"AFS Basic Overseer (UDP)", 7007, NET_FW_IP_PROTOCOL_UDP },
#ifdef AFS_TCP
    { L"AFS Basic Overseer (TCP)", 7007, NET_FW_IP_PROTOCOL_TCP },
#endif
    { L"AFS Server-to-server Updater (UDP)", 7008, NET_FW_IP_PROTOCOL_UDP },
#ifdef AFS_TCP
    { L"AFS Server-to-server Updater (TCP)", 7008, NET_FW_IP_PROTOCOL_TCP },
#endif
    { L"AFS Remote Cache Manager (UDP)", 7009, NET_FW_IP_PROTOCOL_UDP }
#ifdef AFS_TCP
,    { L"AFS Remote Cache Manager (TCP)", 7009, NET_FW_IP_PROTOCOL_TCP }
#endif
};

HRESULT icf_OpenFirewallProfile(INetFwProfile ** fwProfile) {
    HRESULT hr = S_OK;
    INetFwMgr* fwMgr = NULL;
    INetFwPolicy* fwPolicy = NULL;

    *fwProfile = NULL;

    // Create an instance of the firewall settings manager.
    hr = CoCreateInstance(
            __uuidof(NetFwMgr),
            NULL,
            CLSCTX_INPROC_SERVER,
            __uuidof(INetFwMgr),
            reinterpret_cast<void**>(static_cast<INetFwMgr**>(&fwMgr))
            );
    if (FAILED(hr))
    {
	DEBUGOUT(("Can't create fwMgr\n"));
        goto error;
    }

    // Retrieve the local firewall policy.
    hr = fwMgr->get_LocalPolicy(&fwPolicy);
    if (FAILED(hr))
    {
	DEBUGOUT(("Cant get local policy\n"));
        goto error;
    }

    // Retrieve the firewall profile currently in effect.
    hr = fwPolicy->get_CurrentProfile(fwProfile);
    if (FAILED(hr))
    {
	DEBUGOUT(("Can't get current profile\n"));
        goto error;
    }

  error:

    // Release the local firewall policy.
    if (fwPolicy != NULL)
    {
        fwPolicy->Release();
    }

    // Release the firewall settings manager.
    if (fwMgr != NULL)
    {
        fwMgr->Release();
    }

    return hr;
}

HRESULT icf_CheckAndAddPorts(INetFwProfile * fwProfile, global_afs_port_t * ports, int nPorts) {
    INetFwOpenPorts * fwPorts = NULL;
    INetFwOpenPort * fwPort = NULL;
    HRESULT hr;
    HRESULT rhr = S_OK; /* return value */
    int i = 0;

    hr = fwProfile->get_GloballyOpenPorts(&fwPorts);
    if (FAILED(hr)) {
	// Abort!
	DEBUGOUT(("Can't get globallyOpenPorts\n"));
	rhr = hr;
	goto cleanup;
    }

    // go through the supplied ports
    for (i=0; i<nPorts; i++) {
	VARIANT_BOOL vbEnabled;
	BSTR bstName = NULL;
	BOOL bCreate = FALSE;
	fwPort = NULL;

	hr = fwPorts->Item(ports[i].port, ports[i].protocol, &fwPort);
	if (SUCCEEDED(hr)) {
	    DEBUGOUTW((L"Found port for %S\n",ports[i].name));
            hr = fwPort->get_Enabled(&vbEnabled);
	    if (SUCCEEDED(hr)) {
		if ( vbEnabled == VARIANT_FALSE ) {
		    hr = fwPort->put_Enabled(VARIANT_TRUE);
		    if (FAILED(hr)) {
			// failed. Mark as failure. Don't try to create the port either.
			rhr = hr;
		    }
		} // else we are fine
	    } else {
                // Something is wrong with the port.
		// We try to create a new one thus overriding this faulty one.
		bCreate = TRUE;
	    }
	    fwPort->Release();
	    fwPort = NULL;
	} else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
	    DEBUGOUTW((L"Port not found for %S\n", ports[i].name));
	    bCreate = TRUE;
	}

	if (bCreate) {
	    DEBUGOUTW((L"Trying to create port %S\n",ports[i].name));
	    hr = CoCreateInstance( __uuidof(NetFwOpenPort),
				   NULL,
				   CLSCTX_INPROC_SERVER,
				   __uuidof(INetFwOpenPort),
				   reinterpret_cast<void**>
				   (static_cast<INetFwOpenPort**>(&fwPort))
				   );

	    if (FAILED(hr)) {
		DEBUGOUT(("Can't create port\n"));
                rhr = hr;
	    } else {
		DEBUGOUT(("Created port\n"));
		hr = fwPort->put_IpVersion( NET_FW_IP_VERSION_ANY );
		if (FAILED(hr)) {
		    DEBUGOUT(("Can't set IpVersion\n"));
		    rhr = hr;
		    goto abandon_port;
		}

		hr = fwPort->put_Port( ports[i].port );
		if (FAILED(hr)) {
		    DEBUGOUT(("Can't set Port\n"));
		    rhr = hr;
		    goto abandon_port;
		}

		hr = fwPort->put_Protocol( ports[i].protocol );
		if (FAILED(hr)) {
		    DEBUGOUT(("Can't set Protocol\n"));
		    rhr = hr;
		    goto abandon_port;
		}

		hr = fwPort->put_Scope( NET_FW_SCOPE_ALL );
		if (FAILED(hr)) {
		    DEBUGOUT(("Can't set Scope\n"));
		    rhr = hr;
		    goto abandon_port;
		}

		bstName = SysAllocString( ports[i].name );

		if (SysStringLen(bstName) == 0) {
		    rhr = E_OUTOFMEMORY;
		} else {
		    hr = fwPort->put_Name( bstName );
		    if (FAILED(hr)) {
			DEBUGOUT(("Can't set Name\n"));
			rhr = hr;
			SysFreeString( bstName );
			goto abandon_port;
		    }
		}

		SysFreeString( bstName );

		hr = fwPorts->Add( fwPort );
		if (FAILED(hr)) {
		    DEBUGOUT(("Can't add port\n"));
		    rhr = hr;
		} else
		    DEBUGOUT(("Added port\n"));

	      abandon_port:	 	
		fwPort->Release();
	    }
	}
    } // loop through ports

    fwPorts->Release();

  cleanup:

    if (fwPorts != NULL)
	fwPorts->Release();

    return rhr;
}	

long icf_CheckAndAddAFSPorts(int portset) {
    HRESULT hr;
    BOOL coInitialized = FALSE;
    INetFwProfile * fwProfile = NULL;
    global_afs_port_t * ports;
    int nports;
    long code = 0;

    if (portset == AFS_PORTSET_CLIENT) {
	ports = afs_clientPorts;
	nports = sizeof(afs_clientPorts) / sizeof(*afs_clientPorts);
    } else if (portset == AFS_PORTSET_SERVER) {
	ports = afs_serverPorts;
	nports = sizeof(afs_serverPorts) / sizeof(*afs_serverPorts);
    } else
	return 1; /* Invalid port set */

    hr = CoInitializeEx( NULL,
			 COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE
			 );

    if (SUCCEEDED(hr) || RPC_E_CHANGED_MODE == hr)
    {
       coInitialized = TRUE;
    }
    // not necessarily catastrophic if the call failed.  We'll try to
    // continue as if it succeeded.

    hr = icf_OpenFirewallProfile(&fwProfile);
    if (FAILED(hr)) {
	// Ok. That didn't work.  This could be because the machine we
	// are running on doesn't have Windows Firewall.  We'll return
	// a failure to the caller, which shouldn't be taken to mean
	// it's catastrophic.
	DEBUGOUT(("Can't open Firewall profile\n"));
	code = 2;
	goto cleanup;
    }

    // Now that we have a firewall profile, we can start checking
    // and adding the ports that we want.
    hr = icf_CheckAndAddPorts(fwProfile, ports, nports);
    if (FAILED(hr))
	code = 3;

  cleanup:
    if (coInitialized) {
	CoUninitialize();
    }

    return code;
}


#ifdef TESTMAIN
int main(int argc, char **argv) {
    printf("Starting...\n");
    if (icf_CheckAndAddAFSPorts(AFS_PORTSET_CLIENT))
	printf("Failed\n");
    else
	printf("Succeeded\n");
    printf("Done\n");
    return 0;
}	
#endif