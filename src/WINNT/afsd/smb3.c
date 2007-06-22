/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#ifndef DJGPP
#include <windows.h>
#include <ntstatus.h>
#define SECURITY_WIN32
#include <security.h>
#include <lmaccess.h>
#endif /* !DJGPP */
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <osi.h>

#include "afsd.h"
#include <WINNT\afsreg.h>

#include "smb.h"

extern osi_hyper_t hzero;

smb_packet_t *smb_Directory_Watches = NULL;
osi_mutex_t smb_Dir_Watch_Lock;

smb_tran2Dispatch_t smb_tran2DispatchTable[SMB_TRAN2_NOPCODES];

smb_tran2Dispatch_t smb_rapDispatchTable[SMB_RAP_NOPCODES];

/* protected by the smb_globalLock */
smb_tran2Packet_t *smb_tran2AssemblyQueuep;

/* retrieve a held reference to a user structure corresponding to an incoming
 * request */
cm_user_t *smb_GetTran2User(smb_vc_t *vcp, smb_tran2Packet_t *inp)
{
    smb_user_t *uidp;
    cm_user_t *up = NULL;
        
    uidp = smb_FindUID(vcp, inp->uid, 0);
    if (!uidp) 
	return NULL;
        
    up = smb_GetUserFromUID(uidp);

    smb_ReleaseUID(uidp);

    return up;
}

/*
 * Return extended attributes.
 * Right now, we aren't using any of the "new" bits, so this looks exactly
 * like smb_Attributes() (see smb.c).
 */
unsigned long smb_ExtAttributes(cm_scache_t *scp)
{
    unsigned long attrs;

    if (scp->fileType == CM_SCACHETYPE_DIRECTORY ||
        scp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
        scp->fileType == CM_SCACHETYPE_INVALID)
    {
        attrs = SMB_ATTR_DIRECTORY;
#ifdef SPECIAL_FOLDERS
        attrs |= SMB_ATTR_SYSTEM;		/* FILE_ATTRIBUTE_SYSTEM */
#endif /* SPECIAL_FOLDERS */
    } else if (scp->fileType == CM_SCACHETYPE_DFSLINK) {
        attrs = SMB_ATTR_DIRECTORY | SMB_ATTR_SPARSE_FILE;
    } else
        attrs = 0;
    /*
     * We used to mark a file RO if it was in an RO volume, but that
     * turns out to be impolitic in NT.  See defect 10007.
     */
#ifdef notdef
    if ((scp->unixModeBits & 0222) == 0 || (scp->flags & CM_SCACHEFLAG_RO))
        attrs |= SMB_ATTR_READONLY;		/* Read-only */
#else
    if ((scp->unixModeBits & 0222) == 0)
        attrs |= SMB_ATTR_READONLY;		/* Read-only */
#endif

    if (attrs == 0)
        attrs = SMB_ATTR_NORMAL;		/* FILE_ATTRIBUTE_NORMAL */

    return attrs;
}

int smb_V3IsStarMask(char *maskp)
{
    char tc;

    while (tc = *maskp++)
        if (tc == '?' || tc == '*' || tc == '<' || tc == '>') 
            return 1;
    return 0;
}

unsigned char *smb_ParseString(unsigned char *inp, char **chainpp)
{
    if (chainpp) {
        /* skip over null-terminated string */
        *chainpp = inp + strlen(inp) + 1;
    }
    return inp;
}   

void OutputDebugF(char * format, ...) {
    va_list args;
    int len;
    char * buffer;

    va_start( args, format );
    len = _vscprintf( format, args ) // _vscprintf doesn't count
                               + 3; // terminating '\0' + '\n'
    buffer = malloc( len * sizeof(char) );
    vsprintf( buffer, format, args );
    osi_Log0(smb_logp, osi_LogSaveString(smb_logp, buffer));
    strcat(buffer, "\n");
    OutputDebugString(buffer);
    free( buffer );
}

void OutputDebugHexDump(unsigned char * buffer, int len) {
    int i,j,k,pcts=0;
    char buf[256];
    static char tr[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

    OutputDebugF("Hexdump length [%d]",len);

    for (i=0;i<len;i++) {
        if(!(i%16)) {
            if(i) {
                osi_Log0(smb_logp, osi_LogSaveString(smb_logp, buf));
                strcat(buf,"\n");
                OutputDebugString(buf);
            }
            sprintf(buf,"%5x",i);
            memset(buf+5,' ',80);
            buf[85] = 0;
        }

        j = (i%16);
        j = j*3 + 7 + ((j>7)?1:0);
        k = buffer[i];

        buf[j] = tr[k / 16]; buf[j+1] = tr[k % 16];

        j = (i%16);
        j = j + 56 + ((j>7)?1:0) + pcts;

        buf[j] = (k>32 && k<127)?k:'.';
		if (k == '%') {
			buf[++j] = k;
			pcts++;
		}
    }    
    if(i) {
        osi_Log0(smb_logp, osi_LogSaveString(smb_logp, buf));
        strcat(buf,"\n");
        OutputDebugString(buf);
    }   
}

#define SMB_EXT_SEC_PACKAGE_NAME "Negotiate"

void smb_NegotiateExtendedSecurity(void ** secBlob, int * secBlobLength) {
    SECURITY_STATUS status, istatus;
    CredHandle creds = {0,0};
    TimeStamp expiry;
    SecBufferDesc secOut;
    SecBuffer secTok;
    CtxtHandle ctx;
    ULONG flags;

    *secBlob = NULL;
    *secBlobLength = 0;

    OutputDebugF("Negotiating Extended Security");

    status = AcquireCredentialsHandle( NULL,
                                       SMB_EXT_SEC_PACKAGE_NAME,
                                       SECPKG_CRED_INBOUND,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       &creds,
                                       &expiry);

    if (status != SEC_E_OK) {
        /* Really bad. We return an empty security blob */
        OutputDebugF("AcquireCredentialsHandle failed with %lX", status);
        goto nes_0;
    }

    secOut.cBuffers = 1;
    secOut.pBuffers = &secTok;
    secOut.ulVersion = SECBUFFER_VERSION;

    secTok.BufferType = SECBUFFER_TOKEN;
    secTok.cbBuffer = 0;
    secTok.pvBuffer = NULL;

    ctx.dwLower = ctx.dwUpper = 0;

    status = AcceptSecurityContext( &creds,
                                    NULL,
                                    NULL,
                                    ASC_REQ_CONNECTION | ASC_REQ_EXTENDED_ERROR | ASC_REQ_ALLOCATE_MEMORY,
                                    SECURITY_NETWORK_DREP,
                                    &ctx,
                                    &secOut,
                                    &flags,
                                    &expiry
                                    );

    if (status == SEC_I_COMPLETE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
        OutputDebugF("Completing token...");
        istatus = CompleteAuthToken(&ctx, &secOut);
        if ( istatus != SEC_E_OK )
            OutputDebugF("Token completion failed: %x", istatus);
    }

    if (status == SEC_I_COMPLETE_AND_CONTINUE || status == SEC_I_CONTINUE_NEEDED) {
        if (secTok.pvBuffer) {
            *secBlobLength = secTok.cbBuffer;
            *secBlob = malloc( secTok.cbBuffer );
            memcpy(*secBlob, secTok.pvBuffer, secTok.cbBuffer );
        }
    } else {
        if ( status != SEC_E_OK )
            OutputDebugF("AcceptSecurityContext status != CONTINUE  %lX", status);
    }

    /* Discard partial security context */
    DeleteSecurityContext(&ctx);

    if (secTok.pvBuffer) FreeContextBuffer( secTok.pvBuffer );

    /* Discard credentials handle.  We'll reacquire one when we get the session setup X */
    FreeCredentialsHandle(&creds);

  nes_0:
    return;
}

struct smb_ext_context {
    CredHandle creds;
    CtxtHandle ctx;
    int partialTokenLen;
    void * partialToken;
};      

long smb_AuthenticateUserExt(smb_vc_t * vcp, char * usern, char * secBlobIn, int secBlobInLength, char ** secBlobOut, int * secBlobOutLength) {
    SECURITY_STATUS status, istatus;
    CredHandle creds;
    TimeStamp expiry;
    long code = 0;
    SecBufferDesc secBufIn;
    SecBuffer secTokIn;
    SecBufferDesc secBufOut;
    SecBuffer secTokOut;
    CtxtHandle ctx;
    struct smb_ext_context * secCtx = NULL;
    struct smb_ext_context * newSecCtx = NULL;
    void * assembledBlob = NULL;
    int assembledBlobLength = 0;
    ULONG flags;

    OutputDebugF("In smb_AuthenticateUserExt");

    *secBlobOut = NULL;
    *secBlobOutLength = 0;

    if (vcp->flags & SMB_VCFLAG_AUTH_IN_PROGRESS) {
        secCtx = vcp->secCtx;
        lock_ObtainMutex(&vcp->mx);
        vcp->flags &= ~SMB_VCFLAG_AUTH_IN_PROGRESS;
        vcp->secCtx = NULL;
        lock_ReleaseMutex(&vcp->mx);
    }

    if (secBlobIn) {
        OutputDebugF("Received incoming token:");
        OutputDebugHexDump(secBlobIn,secBlobInLength);
    }
    
    if (secCtx) {
        OutputDebugF("Continuing with existing context.");		
        creds = secCtx->creds;
        ctx = secCtx->ctx;

        if (secCtx->partialToken) {
            assembledBlobLength = secCtx->partialTokenLen + secBlobInLength;
            assembledBlob = malloc(assembledBlobLength);
            memcpy(assembledBlob,secCtx->partialToken, secCtx->partialTokenLen);
            memcpy(((BYTE *)assembledBlob) + secCtx->partialTokenLen, secBlobIn, secBlobInLength);
        }
    } else {
        status = AcquireCredentialsHandle( NULL,
                                           SMB_EXT_SEC_PACKAGE_NAME,
                                           SECPKG_CRED_INBOUND,
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL,
                                           &creds,
                                           &expiry);

        if (status != SEC_E_OK) {
            OutputDebugF("Can't acquire Credentials handle [%lX]", status);
            code = CM_ERROR_BADPASSWORD; /* means "try again when I'm sober" */
            goto aue_0;
        }

        ctx.dwLower = 0;
        ctx.dwUpper = 0;
    }

    secBufIn.cBuffers = 1;
    secBufIn.pBuffers = &secTokIn;
    secBufIn.ulVersion = SECBUFFER_VERSION;

    secTokIn.BufferType = SECBUFFER_TOKEN;
    if (assembledBlob) {
        secTokIn.cbBuffer = assembledBlobLength;
        secTokIn.pvBuffer = assembledBlob;
    } else {
        secTokIn.cbBuffer = secBlobInLength;
        secTokIn.pvBuffer = secBlobIn;
    }

    secBufOut.cBuffers = 1;
    secBufOut.pBuffers = &secTokOut;
    secBufOut.ulVersion = SECBUFFER_VERSION;

    secTokOut.BufferType = SECBUFFER_TOKEN;
    secTokOut.cbBuffer = 0;
    secTokOut.pvBuffer = NULL;

    status = AcceptSecurityContext( &creds,
                                    ((secCtx)?&ctx:NULL),
                                    &secBufIn,
                                    ASC_REQ_CONNECTION | ASC_REQ_EXTENDED_ERROR	| ASC_REQ_ALLOCATE_MEMORY,
                                    SECURITY_NETWORK_DREP,
                                    &ctx,
                                    &secBufOut,
                                    &flags,
                                    &expiry
                                    );

    if (status == SEC_I_COMPLETE_NEEDED || status == SEC_I_COMPLETE_AND_CONTINUE) {
        OutputDebugF("Completing token...");
        istatus = CompleteAuthToken(&ctx, &secBufOut);
        if ( istatus != SEC_E_OK )
            OutputDebugF("Token completion failed: %lX", istatus);
    }

    if (status == SEC_I_COMPLETE_AND_CONTINUE || status == SEC_I_CONTINUE_NEEDED) {
        OutputDebugF("Continue needed");

        newSecCtx = malloc(sizeof(*newSecCtx));

        newSecCtx->creds = creds;
        newSecCtx->ctx = ctx;
        newSecCtx->partialToken = NULL;
        newSecCtx->partialTokenLen = 0;

        lock_ObtainMutex( &vcp->mx );
        vcp->flags |= SMB_VCFLAG_AUTH_IN_PROGRESS;
        vcp->secCtx = newSecCtx;
        lock_ReleaseMutex( &vcp->mx );

        code = CM_ERROR_GSSCONTINUE;
    }

    if ((status == SEC_I_COMPLETE_NEEDED || status == SEC_E_OK || 
          status == SEC_I_COMPLETE_AND_CONTINUE || status == SEC_I_CONTINUE_NEEDED) && 
         secTokOut.pvBuffer) {
        OutputDebugF("Need to send token back to client");

        *secBlobOutLength = secTokOut.cbBuffer;
        *secBlobOut = malloc(secTokOut.cbBuffer);
        memcpy(*secBlobOut, secTokOut.pvBuffer, secTokOut.cbBuffer);

        OutputDebugF("Outgoing token:");
        OutputDebugHexDump(*secBlobOut,*secBlobOutLength);
    } else if (status == SEC_E_INCOMPLETE_MESSAGE) {
        OutputDebugF("Incomplete message");

        newSecCtx = malloc(sizeof(*newSecCtx));

        newSecCtx->creds = creds;
        newSecCtx->ctx = ctx;
        newSecCtx->partialToken = malloc(secTokOut.cbBuffer);
        memcpy(newSecCtx->partialToken, secTokOut.pvBuffer, secTokOut.cbBuffer);
        newSecCtx->partialTokenLen = secTokOut.cbBuffer;

        lock_ObtainMutex( &vcp->mx );
        vcp->flags |= SMB_VCFLAG_AUTH_IN_PROGRESS;
        vcp->secCtx = newSecCtx;
        lock_ReleaseMutex( &vcp->mx );

        code = CM_ERROR_GSSCONTINUE;
    }

    if (status == SEC_E_OK || status == SEC_I_COMPLETE_NEEDED) {
        /* woo hoo! */
        SecPkgContext_Names names;

        OutputDebugF("Authentication completed");
        OutputDebugF("Returned flags : [%lX]", flags);

        if (!QueryContextAttributes(&ctx, SECPKG_ATTR_NAMES, &names)) {
            OutputDebugF("Received name [%s]", names.sUserName);
            strcpy(usern, names.sUserName);
            strlwr(usern); /* in tandem with smb_GetNormalizedUsername */
            FreeContextBuffer(names.sUserName);
        } else {
            /* Force the user to retry if the context is invalid */
            OutputDebugF("QueryContextAttributes Names failed [%x]", GetLastError());
            code = CM_ERROR_BADPASSWORD; 
        }
    } else if (!code) {
        switch ( status ) {
        case SEC_E_INVALID_TOKEN:
            OutputDebugF("Returning bad password :: INVALID_TOKEN");
            break;
        case SEC_E_INVALID_HANDLE:
            OutputDebugF("Returning bad password :: INVALID_HANDLE");
            break;
        case SEC_E_LOGON_DENIED:
            OutputDebugF("Returning bad password :: LOGON_DENIED");
            break;
        case SEC_E_UNKNOWN_CREDENTIALS:
            OutputDebugF("Returning bad password :: UNKNOWN_CREDENTIALS");
            break;
        case SEC_E_NO_CREDENTIALS:
            OutputDebugF("Returning bad password :: NO_CREDENTIALS");
            break;
        case SEC_E_CONTEXT_EXPIRED:
            OutputDebugF("Returning bad password :: CONTEXT_EXPIRED");
            break;
        case SEC_E_INCOMPLETE_CREDENTIALS:
            OutputDebugF("Returning bad password :: INCOMPLETE_CREDENTIALS");
            break;
        case SEC_E_WRONG_PRINCIPAL:
            OutputDebugF("Returning bad password :: WRONG_PRINCIPAL");
            break;
        case SEC_E_TIME_SKEW:
            OutputDebugF("Returning bad password :: TIME_SKEW");
            break;
        default:
            OutputDebugF("Returning bad password :: Status == %lX", status);
        }
        code = CM_ERROR_BADPASSWORD;
    }

    if (secCtx) {
        if (secCtx->partialToken) free(secCtx->partialToken);
        free(secCtx);
    }

    if (assembledBlob) {
        free(assembledBlob);
    }

    if (secTokOut.pvBuffer)
        FreeContextBuffer(secTokOut.pvBuffer);

    if (code != CM_ERROR_GSSCONTINUE) {
        DeleteSecurityContext(&ctx);
        FreeCredentialsHandle(&creds);
    }

  aue_0:
    return code;
}

#define P_LEN 256
#define P_RESP_LEN 128

/* LsaLogonUser expects input parameters to be in a contiguous block of memory. 
   So put stuff in a struct. */
struct Lm20AuthBlob {
    MSV1_0_LM20_LOGON lmlogon;
    BYTE ciResponse[P_RESP_LEN];    /* Unicode representation */
    BYTE csResponse[P_RESP_LEN];    /* ANSI representation */
    WCHAR accountNameW[P_LEN];
    WCHAR primaryDomainW[P_LEN];
    WCHAR workstationW[MAX_COMPUTERNAME_LENGTH + 1];
    TOKEN_GROUPS tgroups;
    TOKEN_SOURCE tsource;
};

long smb_AuthenticateUserLM(smb_vc_t *vcp, char * accountName, char * primaryDomain, char * ciPwd, unsigned ciPwdLength, char * csPwd, unsigned csPwdLength) 
{
    NTSTATUS nts, ntsEx;
    struct Lm20AuthBlob lmAuth;
    PMSV1_0_LM20_LOGON_PROFILE lmprofilep;
    QUOTA_LIMITS quotaLimits;
    DWORD size;
    ULONG lmprofilepSize;
    LUID lmSession;
    HANDLE lmToken;

    OutputDebugF("In smb_AuthenticateUser for user [%s] domain [%s]", accountName, primaryDomain);
    OutputDebugF("ciPwdLength is %d and csPwdLength is %d", ciPwdLength, csPwdLength);

    if (ciPwdLength > P_RESP_LEN || csPwdLength > P_RESP_LEN) {
        OutputDebugF("ciPwdLength or csPwdLength is too long");
        return CM_ERROR_BADPASSWORD;
    }

    memset(&lmAuth,0,sizeof(lmAuth));

    lmAuth.lmlogon.MessageType = MsV1_0NetworkLogon;
	
    lmAuth.lmlogon.LogonDomainName.Buffer = lmAuth.primaryDomainW;
    mbstowcs(lmAuth.primaryDomainW, primaryDomain, P_LEN);
    lmAuth.lmlogon.LogonDomainName.Length = (USHORT)(wcslen(lmAuth.primaryDomainW) * sizeof(WCHAR));
    lmAuth.lmlogon.LogonDomainName.MaximumLength = P_LEN * sizeof(WCHAR);

    lmAuth.lmlogon.UserName.Buffer = lmAuth.accountNameW;
    mbstowcs(lmAuth.accountNameW, accountName, P_LEN);
    lmAuth.lmlogon.UserName.Length = (USHORT)(wcslen(lmAuth.accountNameW) * sizeof(WCHAR));
    lmAuth.lmlogon.UserName.MaximumLength = P_LEN * sizeof(WCHAR);

    lmAuth.lmlogon.Workstation.Buffer = lmAuth.workstationW;
    lmAuth.lmlogon.Workstation.MaximumLength = (MAX_COMPUTERNAME_LENGTH + 1) * sizeof(WCHAR);
    size = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(lmAuth.workstationW, &size);
    lmAuth.lmlogon.Workstation.Length = (USHORT)(wcslen(lmAuth.workstationW) * sizeof(WCHAR));

    memcpy(lmAuth.lmlogon.ChallengeToClient, vcp->encKey, MSV1_0_CHALLENGE_LENGTH);

    lmAuth.lmlogon.CaseInsensitiveChallengeResponse.Buffer = lmAuth.ciResponse;
    lmAuth.lmlogon.CaseInsensitiveChallengeResponse.Length = ciPwdLength;
    lmAuth.lmlogon.CaseInsensitiveChallengeResponse.MaximumLength = P_RESP_LEN;
    memcpy(lmAuth.ciResponse, ciPwd, ciPwdLength);

    lmAuth.lmlogon.CaseSensitiveChallengeResponse.Buffer = lmAuth.csResponse;
    lmAuth.lmlogon.CaseSensitiveChallengeResponse.Length = csPwdLength;
    lmAuth.lmlogon.CaseSensitiveChallengeResponse.MaximumLength = P_RESP_LEN;
    memcpy(lmAuth.csResponse, csPwd, csPwdLength);

    lmAuth.lmlogon.ParameterControl = 0;

    lmAuth.tgroups.GroupCount = 0;
    lmAuth.tgroups.Groups[0].Sid = NULL;
    lmAuth.tgroups.Groups[0].Attributes = 0;

    lmAuth.tsource.SourceIdentifier.HighPart = (DWORD)((LONG_PTR)vcp << 32);
    lmAuth.tsource.SourceIdentifier.LowPart = (DWORD)((LONG_PTR)vcp & _UI32_MAX);
    strcpy(lmAuth.tsource.SourceName,"OpenAFS"); /* 8 char limit */

    nts = LsaLogonUser( smb_lsaHandle,
                        &smb_lsaLogonOrigin,
                        Network, /*3*/
                        smb_lsaSecPackage,
                        &lmAuth,
                        sizeof(lmAuth),
                        &lmAuth.tgroups,
                        &lmAuth.tsource,
                        &lmprofilep,
                        &lmprofilepSize,
                        &lmSession,
                        &lmToken,
                        &quotaLimits,
                        &ntsEx);

    if (nts != STATUS_SUCCESS || ntsEx != STATUS_SUCCESS)
        osi_Log2(smb_logp,"LsaLogonUser failure: nts %u ntsEx %u",
                  nts, ntsEx);

    OutputDebugF("Return from LsaLogonUser is 0x%lX", nts);
    OutputDebugF("Extended status is 0x%lX", ntsEx);

    if (nts == ERROR_SUCCESS) {
        /* free the token */
        LsaFreeReturnBuffer(lmprofilep);
        CloseHandle(lmToken);
        return 0;
    } else {
        /* No AFS for you */
        if (nts == 0xC000015BL)
            return CM_ERROR_BADLOGONTYPE;
        else /* our catchall is a bad password though we could be more specific */
            return CM_ERROR_BADPASSWORD;
    }       
}

/* The buffer pointed to by usern is assumed to be at least SMB_MAX_USERNAME_LENGTH bytes */
long smb_GetNormalizedUsername(char * usern, const char * accountName, const char * domainName) 
{
    char * atsign;
    const char * domain;

    /* check if we have sane input */
    if ((strlen(accountName) + strlen(domainName) + 1) > SMB_MAX_USERNAME_LENGTH)
        return 1;

    /* we could get : [accountName][domainName]
       [user][domain]
       [user@domain][]
       [user][]/[user][?]
       [][]/[][?] */

    atsign = strchr(accountName, '@');

    if (atsign) /* [user@domain][] -> [user@domain][domain] */
        domain = atsign + 1;
    else
        domain = domainName;

    /* if for some reason the client doesn't know what domain to use,
       it will either return an empty string or a '?' */
    if (!domain[0] || domain[0] == '?')
        /* Empty domains and empty usernames are usually sent from tokenless contexts.
           This way such logins will get an empty username (easy to check).  I don't know 
           when a non-empty username would be supplied with an anonymous domain, but *shrug* */
        strcpy(usern,accountName);
    else {
        /* TODO: what about WIN.MIT.EDU\user vs. WIN\user? */
        strcpy(usern,domain);
        strcat(usern,"\\");
        if (atsign)
            strncat(usern,accountName,atsign - accountName);
        else
            strcat(usern,accountName);
    }       

    strlwr(usern);

    return 0;
}

/* When using SMB auth, all SMB sessions have to pass through here
 * first to authenticate the user.  
 *
 * Caveat: If not using SMB auth, the protocol does not require
 * sending a session setup packet, which means that we can't rely on a
 * UID in subsequent packets.  Though in practice we get one anyway.
 */
long smb_ReceiveV3SessionSetupX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    char *tp;
    smb_user_t *uidp;
    unsigned short newUid;
    unsigned long caps = 0;
    smb_username_t *unp;
    char *s1 = " ";
    long code = 0; 
    char usern[SMB_MAX_USERNAME_LENGTH];
    char *secBlobOut = NULL;
    int  secBlobOutLength = 0;

    /* Check for bad conns */
    if (vcp->flags & SMB_VCFLAG_REMOTECONN)
        return CM_ERROR_REMOTECONN;

    if (vcp->flags & SMB_VCFLAG_USENT) {
        if (smb_authType == SMB_AUTH_EXTENDED) {
            /* extended authentication */
            char *secBlobIn;
            int secBlobInLength;

            OutputDebugF("NT Session Setup: Extended");
        
            if (!(vcp->flags & SMB_VCFLAG_SESSX_RCVD)) {
                caps = smb_GetSMBParm(inp,10) | (((unsigned long) smb_GetSMBParm(inp,11)) << 16);
            }

            secBlobInLength = smb_GetSMBParm(inp, 7);
            secBlobIn = smb_GetSMBData(inp, NULL);

            code = smb_AuthenticateUserExt(vcp, usern, secBlobIn, secBlobInLength, &secBlobOut, &secBlobOutLength);

            if (code == CM_ERROR_GSSCONTINUE) {
                smb_SetSMBParm(outp, 2, 0);
                smb_SetSMBParm(outp, 3, secBlobOutLength);
                smb_SetSMBDataLength(outp, secBlobOutLength + smb_ServerOSLength + smb_ServerLanManagerLength + smb_ServerDomainNameLength);
                tp = smb_GetSMBData(outp, NULL);
                if (secBlobOutLength) {
                    memcpy(tp, secBlobOut, secBlobOutLength);
                    free(secBlobOut);
                    tp += secBlobOutLength;
                }	
                memcpy(tp,smb_ServerOS,smb_ServerOSLength);
                tp += smb_ServerOSLength;
                memcpy(tp,smb_ServerLanManager,smb_ServerLanManagerLength);
                tp += smb_ServerLanManagerLength;
                memcpy(tp,smb_ServerDomainName,smb_ServerDomainNameLength);
                tp += smb_ServerDomainNameLength;
            }

            /* TODO: handle return code and continue auth. Also free secBlobOut if applicable. */
        } else {
            unsigned ciPwdLength, csPwdLength;
            char *ciPwd, *csPwd;
            char *accountName;
            char *primaryDomain;
            int  datalen;

            if (smb_authType == SMB_AUTH_NTLM)
                OutputDebugF("NT Session Setup: NTLM");
            else
                OutputDebugF("NT Session Setup: None");

            /* TODO: parse for extended auth as well */
            ciPwdLength = smb_GetSMBParm(inp, 7); /* case insensitive password length */
            csPwdLength = smb_GetSMBParm(inp, 8); /* case sensitive password length */

            tp = smb_GetSMBData(inp, &datalen);

            OutputDebugF("Session packet data size [%d]",datalen);

            ciPwd = tp;
            tp += ciPwdLength;
            csPwd = tp;
            tp += csPwdLength;

            accountName = smb_ParseString(tp, &tp);
            primaryDomain = smb_ParseString(tp, NULL);

            OutputDebugF("Account Name: %s",accountName);
            OutputDebugF("Primary Domain: %s", primaryDomain);
            OutputDebugF("Case Sensitive Password: %s", csPwd && csPwd[0] ? "yes" : "no");
            OutputDebugF("Case Insensitive Password: %s", ciPwd && ciPwd[0] ? "yes" : "no");

            if (smb_GetNormalizedUsername(usern, accountName, primaryDomain)) {
                /* shouldn't happen */
                code = CM_ERROR_BADSMB;
                goto after_read_packet;
            }

            /* capabilities are only valid for first session packet */
            if (!(vcp->flags & SMB_VCFLAG_SESSX_RCVD)) {
                caps = smb_GetSMBParm(inp, 11) | (((unsigned long)smb_GetSMBParm(inp, 12)) << 16);
            }

            if (smb_authType == SMB_AUTH_NTLM) {
                code = smb_AuthenticateUserLM(vcp, accountName, primaryDomain, ciPwd, ciPwdLength, csPwd, csPwdLength);
                if ( code )
                    OutputDebugF("LM authentication failed [%d]", code);
                else
                    OutputDebugF("LM authentication succeeded");
            }
        }
    }  else { /* V3 */
        unsigned ciPwdLength;
        char *ciPwd;
        char *accountName;
        char *primaryDomain;

        switch ( smb_authType ) {
        case SMB_AUTH_EXTENDED:
            OutputDebugF("V3 Session Setup: Extended");
            break;
        case SMB_AUTH_NTLM:
            OutputDebugF("V3 Session Setup: NTLM");
            break;
        default:
            OutputDebugF("V3 Session Setup: None");
        }
        ciPwdLength = smb_GetSMBParm(inp, 7);
        tp = smb_GetSMBData(inp, NULL);
        ciPwd = tp;
        tp += ciPwdLength;

        accountName = smb_ParseString(tp, &tp);
        primaryDomain = smb_ParseString(tp, NULL);

        OutputDebugF("Account Name: %s",accountName);
        OutputDebugF("Primary Domain: %s", primaryDomain);
        OutputDebugF("Case Insensitive Password: %s", ciPwd && ciPwd[0] ? "yes" : "no");

        if ( smb_GetNormalizedUsername(usern, accountName, primaryDomain)) {
            /* shouldn't happen */
            code = CM_ERROR_BADSMB;
            goto after_read_packet;
        }

        /* even if we wanted extended auth, if we only negotiated V3, we have to fallback
         * to NTLM.
         */
        if (smb_authType == SMB_AUTH_NTLM || smb_authType == SMB_AUTH_EXTENDED) {
            code = smb_AuthenticateUserLM(vcp,accountName,primaryDomain,ciPwd,ciPwdLength,"",0);
            if ( code )
                OutputDebugF("LM authentication failed [%d]", code);
            else
                OutputDebugF("LM authentication succeeded");
        }
    }

  after_read_packet:
    /* note down that we received a session setup X and set the capabilities flag */
    if (!(vcp->flags & SMB_VCFLAG_SESSX_RCVD)) {
        lock_ObtainMutex(&vcp->mx);
        vcp->flags |= SMB_VCFLAG_SESSX_RCVD;
        /* for the moment we can only deal with NTSTATUS */
        if (caps & NTNEGOTIATE_CAPABILITY_NTSTATUS) {
            vcp->flags |= SMB_VCFLAG_STATUS32;
        }       
        lock_ReleaseMutex(&vcp->mx);
    }

    /* code would be non-zero if there was an authentication failure.
       Ideally we would like to invalidate the uid for this session or break
       early to avoid accidently stealing someone else's tokens. */

    if (code) {
        return code;
    }

    OutputDebugF("Received username=[%s]", usern);

    /* On Windows 2000, this function appears to be called more often than
       it is expected to be called. This resulted in multiple smb_user_t
       records existing all for the same user session which results in all
       of the users tokens disappearing.

       To avoid this problem, we look for an existing smb_user_t record
       based on the users name, and use that one if we find it.
    */

    uidp = smb_FindUserByNameThisSession(vcp, usern);
    if (uidp) {   /* already there, so don't create a new one */
        unp = uidp->unp;
        newUid = uidp->userID;
        osi_Log3(smb_logp,"smb_ReceiveV3SessionSetupX FindUserByName:Lana[%d],lsn[%d],userid[%d]",
		 vcp->lana,vcp->lsn,newUid);
        smb_ReleaseUID(uidp);
    }
    else {
	cm_user_t *userp;

	/* do a global search for the username/machine name pair */
        unp = smb_FindUserByName(usern, vcp->rname, SMB_FLAG_CREATE);
 	lock_ObtainMutex(&unp->mx);
 	if (unp->flags & SMB_USERNAMEFLAG_AFSLOGON) {
 	    /* clear the afslogon flag so that the tickets can now 
 	     * be freed when the refCount returns to zero.
 	     */
 	    unp->flags &= ~SMB_USERNAMEFLAG_AFSLOGON;
 	}
 	lock_ReleaseMutex(&unp->mx);

        /* Create a new UID and cm_user_t structure */
        userp = unp->userp;
        if (!userp)
            userp = cm_NewUser();
 	cm_HoldUserVCRef(userp);
	lock_ObtainMutex(&vcp->mx);
        if (!vcp->uidCounter)
            vcp->uidCounter++; /* handle unlikely wraparounds */
        newUid = (strlen(usern)==0)?0:vcp->uidCounter++;
        lock_ReleaseMutex(&vcp->mx);

        /* Create a new smb_user_t structure and connect them up */
        lock_ObtainMutex(&unp->mx);
        unp->userp = userp;
        lock_ReleaseMutex(&unp->mx);

        uidp = smb_FindUID(vcp, newUid, SMB_FLAG_CREATE);
	if (uidp) {
	    lock_ObtainMutex(&uidp->mx);
	    uidp->unp = unp;
	    osi_Log4(smb_logp,"smb_ReceiveV3SessionSetupX MakeNewUser:VCP[%p],Lana[%d],lsn[%d],userid[%d]",vcp,vcp->lana,vcp->lsn,newUid);
	    lock_ReleaseMutex(&uidp->mx);
	    smb_ReleaseUID(uidp);
	}
    }

    /* Return UID to the client */
    ((smb_t *)outp)->uid = newUid;
    /* Also to the next chained message */
    ((smb_t *)inp)->uid = newUid;

    osi_Log3(smb_logp, "SMB3 session setup name %s creating ID %d%s",
             osi_LogSaveString(smb_logp, usern), newUid, osi_LogSaveString(smb_logp, s1));

    smb_SetSMBParm(outp, 2, 0);

    if (vcp->flags & SMB_VCFLAG_USENT) {
        if (smb_authType == SMB_AUTH_EXTENDED) {
            smb_SetSMBParm(outp, 3, secBlobOutLength);
            smb_SetSMBDataLength(outp, secBlobOutLength + smb_ServerOSLength + smb_ServerLanManagerLength + smb_ServerDomainNameLength);
            tp = smb_GetSMBData(outp, NULL);
            if (secBlobOutLength) {
                memcpy(tp, secBlobOut, secBlobOutLength);
                free(secBlobOut);
                tp += secBlobOutLength;
            }	
            memcpy(tp,smb_ServerOS,smb_ServerOSLength);
            tp += smb_ServerOSLength;
            memcpy(tp,smb_ServerLanManager,smb_ServerLanManagerLength);
            tp += smb_ServerLanManagerLength;
            memcpy(tp,smb_ServerDomainName,smb_ServerDomainNameLength);
            tp += smb_ServerDomainNameLength;
        } else {
            smb_SetSMBDataLength(outp, 0);
        }
    } else {
        if (smb_authType == SMB_AUTH_EXTENDED) {
            smb_SetSMBDataLength(outp, smb_ServerOSLength + smb_ServerLanManagerLength + smb_ServerDomainNameLength);
            tp = smb_GetSMBData(outp, NULL);
            memcpy(tp,smb_ServerOS,smb_ServerOSLength);
            tp += smb_ServerOSLength;
            memcpy(tp,smb_ServerLanManager,smb_ServerLanManagerLength);
            tp += smb_ServerLanManagerLength;
            memcpy(tp,smb_ServerDomainName,smb_ServerDomainNameLength);
            tp += smb_ServerDomainNameLength;
        } else {
            smb_SetSMBDataLength(outp, 0);
        }
    }

    return 0;
}

long smb_ReceiveV3UserLogoffX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_user_t *uidp;

    /* find the tree and free it */
    uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
    if (uidp) {
	smb_username_t * unp;

        osi_Log2(smb_logp, "SMB3 user logoffX uid %d name %s", uidp->userID,
                  osi_LogSaveString(smb_logp, (uidp->unp) ? uidp->unp->name: " "));

        lock_ObtainMutex(&uidp->mx);
        uidp->flags |= SMB_USERFLAG_DELETE;
	/*
         * it doesn't get deleted right away
         * because the vcp points to it
         */
	unp = uidp->unp;
        lock_ReleaseMutex(&uidp->mx);

#ifdef COMMENT
	/* we can't do this.  we get logoff messages prior to a session
	 * disconnect even though it doesn't mean the user is logging out.
	 * we need to create a new pioctl and EventLogoff handler to set
	 * SMB_USERNAMEFLAG_LOGOFF.
	 */
	if (unp && smb_LogoffTokenTransfer) {
	    lock_ObtainMutex(&unp->mx);
	    unp->flags |= SMB_USERNAMEFLAG_LOGOFF;
	    unp->last_logoff_t = osi_Time() + smb_LogoffTransferTimeout;
	    lock_ReleaseMutex(&unp->mx);
	}
#endif

	smb_ReleaseUID(uidp);
    }
    else    
        osi_Log0(smb_logp, "SMB3 user logoffX");

    smb_SetSMBDataLength(outp, 0);
    return 0;
}

#define SMB_SUPPORT_SEARCH_BITS        0x0001
#define SMB_SHARE_IS_IN_DFS            0x0002

long smb_ReceiveV3TreeConnectX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_tid_t *tidp;
    smb_user_t *uidp = NULL;
    unsigned short newTid;
    char shareName[256];
    char *sharePath;
    int shareFound;
    char *tp;
    char *pathp;
    char *passwordp;
    char *servicep;
    cm_user_t *userp = NULL;
    int ipc = 0;
        
    osi_Log0(smb_logp, "SMB3 receive tree connect");

    /* parse input parameters */
    tp = smb_GetSMBData(inp, NULL);
    passwordp = smb_ParseString(tp, &tp);
    pathp = smb_ParseString(tp, &tp);
    if (smb_StoreAnsiFilenames)
        OemToChar(pathp,pathp);
    servicep = smb_ParseString(tp, &tp);

    tp = strrchr(pathp, '\\');
    if (!tp) {
        return CM_ERROR_BADSMB;
    }
    strcpy(shareName, tp+1);

    osi_Log2(smb_logp, "Tree connect pathp[%s] shareName[%s]",
             osi_LogSaveString(smb_logp, pathp),
             osi_LogSaveString(smb_logp, shareName));

    if (strcmp(servicep, "IPC") == 0 || strcmp(shareName, "IPC$") == 0) {
#ifndef NO_IPC
        osi_Log0(smb_logp, "TreeConnectX connecting to IPC$");
        ipc = 1;
#else
        return CM_ERROR_NOIPC;
#endif
    }

    uidp = smb_FindUID(vcp, ((smb_t *)inp)->uid, 0);
    if (uidp)
	userp = smb_GetUserFromUID(uidp);

    lock_ObtainMutex(&vcp->mx);
    newTid = vcp->tidCounter++;
    lock_ReleaseMutex(&vcp->mx);
        
    tidp = smb_FindTID(vcp, newTid, SMB_FLAG_CREATE);

    if (!ipc) {
	if (!strcmp(shareName, "*."))
	    strcpy(shareName, "all");
	shareFound = smb_FindShare(vcp, uidp, shareName, &sharePath);
	if (!shareFound) {
	    if (uidp)
		smb_ReleaseUID(uidp);
            smb_ReleaseTID(tidp);
            return CM_ERROR_BADSHARENAME;
	}

	if (vcp->flags & SMB_VCFLAG_USENT)
        {
            int policy = smb_FindShareCSCPolicy(shareName);
            smb_SetSMBParm(outp, 2, SMB_SUPPORT_SEARCH_BITS | 
#ifdef DFS_SUPPORT
                            SMB_SHARE_IS_IN_DFS |
#endif
                            (policy << 2));
        }
    } else {
        smb_SetSMBParm(outp, 2, 0);
        sharePath = NULL;
    }
    if (uidp)
	smb_ReleaseUID(uidp);

    lock_ObtainMutex(&tidp->mx);
    tidp->userp = userp;
    tidp->pathname = sharePath;
    if (ipc) 
        tidp->flags |= SMB_TIDFLAG_IPC;
    lock_ReleaseMutex(&tidp->mx);
    smb_ReleaseTID(tidp);

    ((smb_t *)outp)->tid = newTid;
    ((smb_t *)inp)->tid = newTid;
    tp = smb_GetSMBData(outp, NULL);
    if (!ipc) {
        /* XXX - why is this a drive letter? */
        *tp++ = 'A';
        *tp++ = ':';
        *tp++ = 0;
        *tp++ = 'A';
        *tp++ = 'F';
        *tp++ = 'S';
        *tp++ = 0;
        smb_SetSMBDataLength(outp, 7);
    } else {
        strcpy(tp, "IPC");
        smb_SetSMBDataLength(outp, 4);
    }

    osi_Log1(smb_logp, "SMB3 tree connect created ID %d", newTid);
    return 0;
}

/* must be called with global tran lock held */
smb_tran2Packet_t *smb_FindTran2Packet(smb_vc_t *vcp, smb_packet_t *inp)
{
    smb_tran2Packet_t *tp;
    smb_t *smbp;
        
    smbp = (smb_t *) inp->data;
    for (tp = smb_tran2AssemblyQueuep; tp; tp = (smb_tran2Packet_t *) osi_QNext(&tp->q)) {
        if (tp->vcp == vcp && tp->mid == smbp->mid && tp->tid == smbp->tid)
            return tp;
    }
    return NULL;
}

smb_tran2Packet_t *smb_NewTran2Packet(smb_vc_t *vcp, smb_packet_t *inp,
	int totalParms, int totalData)
{
    smb_tran2Packet_t *tp;
    smb_t *smbp;
        
    smbp = (smb_t *) inp->data;
    tp = malloc(sizeof(*tp));
    memset(tp, 0, sizeof(*tp));
    tp->vcp = vcp;
    smb_HoldVC(vcp);
    tp->curData = tp->curParms = 0;
    tp->totalData = totalData;
    tp->totalParms = totalParms;
    tp->tid = smbp->tid;
    tp->mid = smbp->mid;
    tp->uid = smbp->uid;
    tp->pid = smbp->pid;
    tp->res[0] = smbp->res[0];
    osi_QAdd((osi_queue_t **)&smb_tran2AssemblyQueuep, &tp->q);
    if (totalParms != 0)
        tp->parmsp = malloc(totalParms);
    if (totalData != 0)
        tp->datap = malloc(totalData);
    if (smbp->com == 0x25 || smbp->com == 0x26)
        tp->com = 0x25;
    else {
        tp->opcode = smb_GetSMBParm(inp, 14);
        tp->com = 0x32;
    }
    tp->flags |= SMB_TRAN2PFLAG_ALLOC;
    return tp;
}

smb_tran2Packet_t *smb_GetTran2ResponsePacket(smb_vc_t *vcp,
                                               smb_tran2Packet_t *inp, smb_packet_t *outp,
                                               int totalParms, int totalData)  
{
    smb_tran2Packet_t *tp;
    unsigned short parmOffset;
    unsigned short dataOffset;
    unsigned short dataAlign;
        
    tp = malloc(sizeof(*tp));
    memset(tp, 0, sizeof(*tp));
    smb_HoldVC(vcp);
    tp->vcp = vcp;
    tp->curData = tp->curParms = 0;
    tp->totalData = totalData;
    tp->totalParms = totalParms;
    tp->oldTotalParms = totalParms;
    tp->tid = inp->tid;
    tp->mid = inp->mid;
    tp->uid = inp->uid;
    tp->pid = inp->pid;
    tp->res[0] = inp->res[0];
    tp->opcode = inp->opcode;
    tp->com = inp->com;

    /*
     * We calculate where the parameters and data will start.
     * This calculation must parallel the calculation in
     * smb_SendTran2Packet.
     */

    parmOffset = 10*2 + 35;
    parmOffset++;			/* round to even */
    tp->parmsp = (unsigned short *) (outp->data + parmOffset);

    dataOffset = parmOffset + totalParms;
    dataAlign = dataOffset & 2;	/* quad-align */
    dataOffset += dataAlign;
    tp->datap = outp->data + dataOffset;

    return tp;
}       

/* free a tran2 packet */
void smb_FreeTran2Packet(smb_tran2Packet_t *t2p)
{
    if (t2p->vcp) {
        smb_ReleaseVC(t2p->vcp);
	t2p->vcp = NULL;
    }
    if (t2p->flags & SMB_TRAN2PFLAG_ALLOC) {
        if (t2p->parmsp)
            free(t2p->parmsp);
        if (t2p->datap)
            free(t2p->datap);
    }       
    free(t2p);
}

/* called with a VC, an input packet to respond to, and an error code.
 * sends an error response.
 */
void smb_SendTran2Error(smb_vc_t *vcp, smb_tran2Packet_t *t2p,
	smb_packet_t *tp, long code)
{
    smb_t *smbp;
    unsigned short errCode;
    unsigned char errClass;
    unsigned long NTStatus;

    if (vcp->flags & SMB_VCFLAG_STATUS32)
        smb_MapNTError(code, &NTStatus);
    else
        smb_MapCoreError(code, vcp, &errCode, &errClass);

    smb_FormatResponsePacket(vcp, NULL, tp);
    smbp = (smb_t *) tp;

    /* We can handle long names */
    if (vcp->flags & SMB_VCFLAG_USENT)
        smbp->flg2 |= SMB_FLAGS2_IS_LONG_NAME;
        
    /* now copy important fields from the tran 2 packet */
    smbp->com = t2p->com;
    smbp->tid = t2p->tid;
    smbp->mid = t2p->mid;
    smbp->pid = t2p->pid;
    smbp->uid = t2p->uid;
    smbp->res[0] = t2p->res[0];
    if (vcp->flags & SMB_VCFLAG_STATUS32) {
        smbp->rcls = (unsigned char) (NTStatus & 0xff);
        smbp->reh = (unsigned char) ((NTStatus >> 8) & 0xff);
        smbp->errLow = (unsigned char) ((NTStatus >> 16) & 0xff);
        smbp->errHigh = (unsigned char) ((NTStatus >> 24) & 0xff);
        smbp->flg2 |= SMB_FLAGS2_32BIT_STATUS;
    }
    else {
        smbp->rcls = errClass;
        smbp->errLow = (unsigned char) (errCode & 0xff);
        smbp->errHigh = (unsigned char) ((errCode >> 8) & 0xff);
    }
        
    /* send packet */
    smb_SendPacket(vcp, tp);
}        

void smb_SendTran2Packet(smb_vc_t *vcp, smb_tran2Packet_t *t2p, smb_packet_t *tp)
{
    smb_t *smbp;
    unsigned short parmOffset;
    unsigned short dataOffset;
    unsigned short totalLength;
    unsigned short dataAlign;
    char *datap;

    smb_FormatResponsePacket(vcp, NULL, tp);
    smbp = (smb_t *) tp;

    /* We can handle long names */
    if (vcp->flags & SMB_VCFLAG_USENT)
        smbp->flg2 |= SMB_FLAGS2_IS_LONG_NAME;

    /* now copy important fields from the tran 2 packet */
    smbp->com = t2p->com;
    smbp->tid = t2p->tid;
    smbp->mid = t2p->mid;
    smbp->pid = t2p->pid;
    smbp->uid = t2p->uid;
    smbp->res[0] = t2p->res[0];

    totalLength = 1 + t2p->totalData + t2p->totalParms;

    /* now add the core parameters (tran2 info) to the packet */
    smb_SetSMBParm(tp, 0, t2p->totalParms);	/* parm bytes */
    smb_SetSMBParm(tp, 1, t2p->totalData);	/* data bytes */
    smb_SetSMBParm(tp, 2, 0);		/* reserved */
    smb_SetSMBParm(tp, 3, t2p->totalParms);	/* parm bytes in this packet */
    parmOffset = 10*2 + 35;			/* parm offset in packet */
    parmOffset++;				/* round to even */
    smb_SetSMBParm(tp, 4, parmOffset);	/* 11 parm words plus *
    * hdr, bcc and wct */
    smb_SetSMBParm(tp, 5, 0);		/* parm displacement */
    smb_SetSMBParm(tp, 6, t2p->totalData);	/* data in this packet */
    dataOffset = parmOffset + t2p->oldTotalParms;
    dataAlign = dataOffset & 2;		/* quad-align */
    dataOffset += dataAlign;
    smb_SetSMBParm(tp, 7, dataOffset);	/* offset of data */
    smb_SetSMBParm(tp, 8, 0);		/* data displacement */
    smb_SetSMBParm(tp, 9, 0);		/* low: setup word count *
                                         * high: resvd */

    datap = smb_GetSMBData(tp, NULL);
    *datap++ = 0;				/* we rounded to even */

    totalLength += dataAlign;
    smb_SetSMBDataLength(tp, totalLength);
        
    /* next, send the datagram */
    smb_SendPacket(vcp, tp);
}   

long smb_ReceiveV3Trans(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_tran2Packet_t *asp;
    int totalParms;
    int totalData;
    int parmDisp;
    int dataDisp;
    int parmOffset;
    int dataOffset;
    int parmCount;
    int dataCount;
    int firstPacket;
    int rapOp;
    long code = 0;

    /* We sometimes see 0 word count.  What to do? */
    if (*inp->wctp == 0) {
        osi_Log0(smb_logp, "Transaction2 word count = 0"); 
#ifndef DJGPP
	LogEvent(EVENTLOG_WARNING_TYPE, MSG_SMB_ZERO_TRANSACTION_COUNT);
#endif /* !DJGPP */

        smb_SetSMBDataLength(outp, 0);
        smb_SendPacket(vcp, outp);
        return 0;
    }

    totalParms = smb_GetSMBParm(inp, 0);
    totalData = smb_GetSMBParm(inp, 1);
        
    firstPacket = (inp->inCom == 0x25);
        
    /* find the packet we're reassembling */
    lock_ObtainWrite(&smb_globalLock);
    asp = smb_FindTran2Packet(vcp, inp);
    if (!asp) {
        asp = smb_NewTran2Packet(vcp, inp, totalParms, totalData);
    }
    lock_ReleaseWrite(&smb_globalLock);
        
    /* now merge in this latest packet; start by looking up offsets */
    if (firstPacket) {
        parmDisp = dataDisp = 0;
        parmOffset = smb_GetSMBParm(inp, 10);
        dataOffset = smb_GetSMBParm(inp, 12);
        parmCount = smb_GetSMBParm(inp, 9);
        dataCount = smb_GetSMBParm(inp, 11);
        asp->maxReturnParms = smb_GetSMBParm(inp, 2);
        asp->maxReturnData = smb_GetSMBParm(inp, 3);

        osi_Log3(smb_logp, "SMB3 received Trans init packet total data %d, cur data %d, max return data %d",
                  totalData, dataCount, asp->maxReturnData);
    }
    else {
        parmDisp = smb_GetSMBParm(inp, 4);
        parmOffset = smb_GetSMBParm(inp, 3);
        dataDisp = smb_GetSMBParm(inp, 7);
        dataOffset = smb_GetSMBParm(inp, 6);
        parmCount = smb_GetSMBParm(inp, 2);
        dataCount = smb_GetSMBParm(inp, 5);

        osi_Log2(smb_logp, "SMB3 received Trans aux packet parms %d, data %d",
                 parmCount, dataCount);
    }   

    /* now copy the parms and data */
    if ( asp->totalParms > 0 && parmCount != 0 )
    {
        memcpy(((char *)asp->parmsp) + parmDisp, inp->data + parmOffset, parmCount);
    }
    if ( asp->totalData > 0 && dataCount != 0 ) {
        memcpy(asp->datap + dataDisp, inp->data + dataOffset, dataCount);
    }

    /* account for new bytes */
    asp->curData += dataCount;
    asp->curParms += parmCount;

    /* finally, if we're done, remove the packet from the queue and dispatch it */
    if (asp->totalParms > 0 &&
        asp->curParms > 0 &&
        asp->totalData <= asp->curData &&
        asp->totalParms <= asp->curParms) {
        /* we've received it all */
        lock_ObtainWrite(&smb_globalLock);
        osi_QRemove((osi_queue_t **) &smb_tran2AssemblyQueuep, &asp->q);
        lock_ReleaseWrite(&smb_globalLock);

        /* now dispatch it */
        rapOp = asp->parmsp[0];

        if ( rapOp >= 0 && rapOp < SMB_RAP_NOPCODES && smb_rapDispatchTable[rapOp].procp) {
            osi_Log4(smb_logp,"AFS Server - Dispatch-RAP %s vcp[%p] lana[%d] lsn[%d]",myCrt_RapDispatch(rapOp),vcp,vcp->lana,vcp->lsn);
            code = (*smb_rapDispatchTable[rapOp].procp)(vcp, asp, outp);
            osi_Log4(smb_logp,"AFS Server - Dispatch-RAP return  code 0x%x vcp[%x] lana[%d] lsn[%d]",code,vcp,vcp->lana,vcp->lsn);
        }
        else {
            osi_Log4(smb_logp,"AFS Server - Dispatch-RAP [INVALID] op[%x] vcp[%p] lana[%d] lsn[%d]", rapOp, vcp, vcp->lana, vcp->lsn);
            code = CM_ERROR_BADOP;
        }

        /* if an error is returned, we're supposed to send an error packet,
         * otherwise the dispatched function already did the data sending.
         * We give dispatched proc the responsibility since it knows how much
         * space to allocate.
         */
        if (code != 0) {
            smb_SendTran2Error(vcp, asp, outp, code);
        }

        /* free the input tran 2 packet */
        smb_FreeTran2Packet(asp);
    }
    else if (firstPacket) {
        /* the first packet in a multi-packet request, we need to send an
         * ack to get more data.
         */
        smb_SetSMBDataLength(outp, 0);
        smb_SendPacket(vcp, outp);
    }

    return 0;
}

/* ANSI versions.  The unicode versions support arbitrary length
   share names, but we don't support unicode yet. */

typedef struct smb_rap_share_info_0 {
    char        shi0_netname[13];
} smb_rap_share_info_0_t;

typedef struct smb_rap_share_info_1 {
    char			shi1_netname[13];
    char			shi1_pad;
    WORD			shi1_type;
    DWORD			shi1_remark; /* char *shi1_remark; data offset */
} smb_rap_share_info_1_t;

typedef struct smb_rap_share_info_2 {
    char				shi2_netname[13];
    char				shi2_pad;
    unsigned short		shi2_type;
    DWORD				shi2_remark; /* char *shi2_remark; data offset */
    unsigned short		shi2_permissions;
    unsigned short		shi2_max_uses;
    unsigned short		shi2_current_uses;
    DWORD				shi2_path;  /* char *shi2_path; data offset */
    unsigned short		shi2_passwd[9];
    unsigned short		shi2_pad2;
} smb_rap_share_info_2_t;

#define SMB_RAP_MAX_SHARES 512

typedef struct smb_rap_share_list {
    int cShare;
    int maxShares;
    smb_rap_share_info_0_t * shares;
} smb_rap_share_list_t;

int smb_rapCollectSharesProc(cm_scache_t *dscp, cm_dirEntry_t *dep, void *vrockp, osi_hyper_t *offp) {
    smb_rap_share_list_t * sp;
    char * name;

    name = dep->name;

    if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2])))
        return 0; /* skip over '.' and '..' */

    sp = (smb_rap_share_list_t *) vrockp;

    strncpy(sp->shares[sp->cShare].shi0_netname, name, 12);
    sp->shares[sp->cShare].shi0_netname[12] = 0;

    sp->cShare++;

    if (sp->cShare >= sp->maxShares)
        return CM_ERROR_STOPNOW;
    else
        return 0;
}       

long smb_ReceiveRAPNetShareEnum(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    smb_tran2Packet_t *outp;
    unsigned short * tp;
    int len;
    int infoLevel;
    int bufsize;
    int outParmsTotal;	/* total parameter bytes */
    int outDataTotal;	/* total data bytes */
    int code = 0;
    DWORD rv;
    DWORD allSubmount = 0;
    USHORT nShares = 0;
    DWORD nRegShares = 0;
    DWORD nSharesRet = 0;
    HKEY hkParam;
    HKEY hkSubmount = NULL;
    smb_rap_share_info_1_t * shares;
    USHORT cshare = 0;
    char * cstrp;
    char thisShare[256];
    int i,j;
    DWORD dw;
    int nonrootShares;
    smb_rap_share_list_t rootShares;
    cm_req_t req;
    cm_user_t * userp;
    osi_hyper_t thyper;

    tp = p->parmsp + 1; /* skip over function number (always 0) */
    (void) smb_ParseString((char *) tp, (char **) &tp); /* skip over parm descriptor */
    (void) smb_ParseString((char *) tp, (char **) &tp); /* skip over data descriptor */
    infoLevel = tp[0];
    bufsize = tp[1];

    if (infoLevel != 1) {
        return CM_ERROR_INVAL;
    }

    /* first figure out how many shares there are */
    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY, 0,
                      KEY_QUERY_VALUE, &hkParam);
    if (rv == ERROR_SUCCESS) {
        len = sizeof(allSubmount);
        rv = RegQueryValueEx(hkParam, "AllSubmount", NULL, NULL,
                             (BYTE *) &allSubmount, &len);
        if (rv != ERROR_SUCCESS || allSubmount != 0) {
            allSubmount = 1;
        }
        RegCloseKey (hkParam);
    }

    rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_OPENAFS_SUBKEY "\\Submounts",
                      0, KEY_QUERY_VALUE, &hkSubmount);
    if (rv == ERROR_SUCCESS) {
        rv = RegQueryInfoKey(hkSubmount, NULL, NULL, NULL, NULL,
                             NULL, NULL, &nRegShares, NULL, NULL, NULL, NULL);
        if (rv != ERROR_SUCCESS)
            nRegShares = 0;
    } else {
        hkSubmount = NULL;
    }

    /* fetch the root shares */
    rootShares.maxShares = SMB_RAP_MAX_SHARES;
    rootShares.cShare = 0;
    rootShares.shares = malloc( sizeof(smb_rap_share_info_0_t) * SMB_RAP_MAX_SHARES );

    cm_InitReq(&req);

    userp = smb_GetTran2User(vcp,p);

    thyper.HighPart = 0;
    thyper.LowPart = 0;

    cm_HoldSCache(cm_data.rootSCachep);
    cm_ApplyDir(cm_data.rootSCachep, smb_rapCollectSharesProc, &rootShares, &thyper, userp, &req, NULL);
    cm_ReleaseSCache(cm_data.rootSCachep);

    cm_ReleaseUser(userp);

    nShares = (USHORT)(rootShares.cShare + nRegShares + allSubmount);

#define REMARK_LEN 1
    outParmsTotal = 8; /* 4 dwords */
    outDataTotal = (sizeof(smb_rap_share_info_1_t) + REMARK_LEN) * nShares ;
    if(outDataTotal > bufsize) {
        nSharesRet = bufsize / (sizeof(smb_rap_share_info_1_t) + REMARK_LEN);
        outDataTotal = (sizeof(smb_rap_share_info_1_t) + REMARK_LEN) * nSharesRet;
    }
    else {
        nSharesRet = nShares;
    }
    
    outp = smb_GetTran2ResponsePacket(vcp, p, op, outParmsTotal, outDataTotal);

    /* now for the submounts */
    shares = (smb_rap_share_info_1_t *) outp->datap;
    cstrp = outp->datap + sizeof(smb_rap_share_info_1_t) * nSharesRet;

    memset(outp->datap, 0, (sizeof(smb_rap_share_info_1_t) + REMARK_LEN) * nSharesRet);

    if (allSubmount) {
        strcpy( shares[cshare].shi1_netname, "all" );
        shares[cshare].shi1_remark = (DWORD)(cstrp - outp->datap);
        /* type and pad are zero already */
        cshare++;
        cstrp+=REMARK_LEN;
    }

    if (hkSubmount) {
        for (dw=0; dw < nRegShares && cshare < nSharesRet; dw++) {
            len = sizeof(thisShare);
            rv = RegEnumValue(hkSubmount, dw, thisShare, &len, NULL, NULL, NULL, NULL);
            if (rv == ERROR_SUCCESS && strlen(thisShare) && (!allSubmount || stricmp(thisShare,"all"))) {
                strncpy(shares[cshare].shi1_netname, thisShare, sizeof(shares->shi1_netname)-1);
                shares[cshare].shi1_netname[sizeof(shares->shi1_netname)-1] = 0; /* unfortunate truncation */
                shares[cshare].shi1_remark = (DWORD)(cstrp - outp->datap);
                cshare++;
                cstrp+=REMARK_LEN;
            }
            else
                nShares--; /* uncount key */
        }

        RegCloseKey(hkSubmount);
    }

    nonrootShares = cshare;

    for (i=0; i < rootShares.cShare && cshare < nSharesRet; i++) {
        /* in case there are collisions with submounts, submounts have higher priority */		
        for (j=0; j < nonrootShares; j++)
            if (!stricmp(shares[j].shi1_netname, rootShares.shares[i].shi0_netname))
                break;
		
        if (j < nonrootShares) {
            nShares--; /* uncount */
            continue;
        }

        strcpy(shares[cshare].shi1_netname, rootShares.shares[i].shi0_netname);
        shares[cshare].shi1_remark = (DWORD)(cstrp - outp->datap);
        cshare++;
        cstrp+=REMARK_LEN;
    }

    outp->parmsp[0] = ((cshare == nShares)? ERROR_SUCCESS : ERROR_MORE_DATA);
    outp->parmsp[1] = 0;
    outp->parmsp[2] = cshare;
    outp->parmsp[3] = nShares;

    outp->totalData = (int)(cstrp - outp->datap);
    outp->totalParms = outParmsTotal;

    smb_SendTran2Packet(vcp, outp, op);
    smb_FreeTran2Packet(outp);

    free(rootShares.shares);

    return code;
}

long smb_ReceiveRAPNetShareGetInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    smb_tran2Packet_t *outp;
    unsigned short * tp;
    char * shareName;
    BOOL shareFound = FALSE;
    unsigned short infoLevel;
    unsigned short bufsize;
    int totalData;
    int totalParam;
    DWORD len;
    HKEY hkParam;
    HKEY hkSubmount;
    DWORD allSubmount;
    LONG rv;
    long code = 0;

    tp = p->parmsp + 1; /* skip over function number (always 1) */
    (void) smb_ParseString( (char *) tp, (char **) &tp); /* skip over param descriptor */
    (void) smb_ParseString( (char *) tp, (char **) &tp); /* skip over data descriptor */
    shareName = smb_ParseString( (char *) tp, (char **) &tp);
    infoLevel = *tp++;
    bufsize = *tp++;
    
    totalParam = 6;

    if (infoLevel == 0)
        totalData = sizeof(smb_rap_share_info_0_t);
    else if(infoLevel == SMB_INFO_STANDARD)
        totalData = sizeof(smb_rap_share_info_1_t) + 1; /* + empty string */
    else if(infoLevel == SMB_INFO_QUERY_EA_SIZE)
        totalData = sizeof(smb_rap_share_info_2_t) + 2; /* + two empty strings */
    else
        return CM_ERROR_INVAL;

    outp = smb_GetTran2ResponsePacket(vcp, p, op, totalParam, totalData);

    if(!stricmp(shareName,"all") || !strcmp(shareName,"*.")) {
        rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY, 0,
                          KEY_QUERY_VALUE, &hkParam);
        if (rv == ERROR_SUCCESS) {
            len = sizeof(allSubmount);
            rv = RegQueryValueEx(hkParam, "AllSubmount", NULL, NULL,
                                  (BYTE *) &allSubmount, &len);
            if (rv != ERROR_SUCCESS || allSubmount != 0) {
                allSubmount = 1;
            }
            RegCloseKey (hkParam);
        }

        if (allSubmount)
            shareFound = TRUE;

    } else {
        rv = RegOpenKeyEx(HKEY_LOCAL_MACHINE, AFSREG_CLT_SVC_PARAM_SUBKEY "\\Submounts", 0,
                          KEY_QUERY_VALUE, &hkSubmount);
        if (rv == ERROR_SUCCESS) {
            rv = RegQueryValueEx(hkSubmount, shareName, NULL, NULL, NULL, NULL);
            if (rv == ERROR_SUCCESS) {
                shareFound = TRUE;
            }
            RegCloseKey(hkSubmount);
        }
    }

    if (!shareFound) {
        smb_FreeTran2Packet(outp);
        return CM_ERROR_BADSHARENAME;
    }

    memset(outp->datap, 0, totalData);

    outp->parmsp[0] = 0;
    outp->parmsp[1] = 0;
    outp->parmsp[2] = totalData;

    if (infoLevel == 0) {
        smb_rap_share_info_0_t * info = (smb_rap_share_info_0_t *) outp->datap;
        strncpy(info->shi0_netname, shareName, sizeof(info->shi0_netname)-1);
        info->shi0_netname[sizeof(info->shi0_netname)-1] = 0;
    } else if(infoLevel == SMB_INFO_STANDARD) {
        smb_rap_share_info_1_t * info = (smb_rap_share_info_1_t *) outp->datap;
        strncpy(info->shi1_netname, shareName, sizeof(info->shi1_netname)-1);
        info->shi1_netname[sizeof(info->shi1_netname)-1] = 0;
        info->shi1_remark = (DWORD)(((unsigned char *) (info + 1)) - outp->datap);
        /* type and pad are already zero */
    } else { /* infoLevel==2 */
        smb_rap_share_info_2_t * info = (smb_rap_share_info_2_t *) outp->datap;
        strncpy(info->shi2_netname, shareName, sizeof(info->shi2_netname)-1);
        info->shi2_netname[sizeof(info->shi2_netname)-1] = 0;
        info->shi2_remark = (DWORD)(((unsigned char *) (info + 1)) - outp->datap);
        info->shi2_permissions = ACCESS_ALL;
        info->shi2_max_uses = (unsigned short) -1;
        info->shi2_path = (DWORD)(1 + (((unsigned char *) (info + 1)) - outp->datap));
    }

    outp->totalData = totalData;
    outp->totalParms = totalParam;

    smb_SendTran2Packet(vcp, outp, op);
    smb_FreeTran2Packet(outp);

    return code;
}

typedef struct smb_rap_wksta_info_10 {
    DWORD	wki10_computername;	/*char *wki10_computername;*/
    DWORD	wki10_username; /* char *wki10_username; */
    DWORD  	wki10_langroup;	/* char *wki10_langroup;*/
    unsigned char  	wki10_ver_major;
    unsigned char	wki10_ver_minor;
    DWORD	wki10_logon_domain;	/*char *wki10_logon_domain;*/
    DWORD	wki10_oth_domains; /* char *wki10_oth_domains;*/
} smb_rap_wksta_info_10_t;


long smb_ReceiveRAPNetWkstaGetInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    smb_tran2Packet_t *outp;
    long code = 0;
    int infoLevel;
    int bufsize;
    unsigned short * tp;
    int totalData;
    int totalParams;
    smb_rap_wksta_info_10_t * info;
    char * cstrp;
    smb_user_t *uidp;

    tp = p->parmsp + 1; /* Skip over function number */
    (void) smb_ParseString((unsigned char*) tp, (char **) &tp); /* skip over param descriptor */
    (void) smb_ParseString((unsigned char*) tp, (char **) &tp); /* skip over data descriptor */
    infoLevel = *tp++;
    bufsize = *tp++;

    if (infoLevel != 10) {
        return CM_ERROR_INVAL;
    }

    totalParams = 6;
	
    /* infolevel 10 */
    totalData = sizeof(*info) +		/* info */
        MAX_COMPUTERNAME_LENGTH +	/* wki10_computername */
        SMB_MAX_USERNAME_LENGTH +	/* wki10_username */
        MAX_COMPUTERNAME_LENGTH +	/* wki10_langroup */
        MAX_COMPUTERNAME_LENGTH +	/* wki10_logon_domain */
        1;				/* wki10_oth_domains (null)*/

    outp = smb_GetTran2ResponsePacket(vcp, p, op, totalParams, totalData);

    memset(outp->parmsp,0,totalParams);
    memset(outp->datap,0,totalData);

    info = (smb_rap_wksta_info_10_t *) outp->datap;
    cstrp = (char *) (info + 1);

    info->wki10_computername = (DWORD) (cstrp - outp->datap);
    strcpy(cstrp, smb_localNamep);
    cstrp += strlen(cstrp) + 1;

    info->wki10_username = (DWORD) (cstrp - outp->datap);
    uidp = smb_FindUID(vcp, p->uid, 0);
    if (uidp) {
        lock_ObtainMutex(&uidp->mx);
        if(uidp->unp && uidp->unp->name)
            strcpy(cstrp, uidp->unp->name);
        lock_ReleaseMutex(&uidp->mx);
        smb_ReleaseUID(uidp);
    }
    cstrp += strlen(cstrp) + 1;

    info->wki10_langroup = (DWORD) (cstrp - outp->datap);
    strcpy(cstrp, "WORKGROUP");
    cstrp += strlen(cstrp) + 1;

    /* TODO: Not sure what values these should take, but these work */
    info->wki10_ver_major = 5;
    info->wki10_ver_minor = 1;

    info->wki10_logon_domain = (DWORD) (cstrp - outp->datap);
    strcpy(cstrp, smb_ServerDomainName);
    cstrp += strlen(cstrp) + 1;

    info->wki10_oth_domains = (DWORD) (cstrp - outp->datap);
    cstrp ++; /* no other domains */

    outp->totalData = (unsigned short) (cstrp - outp->datap); /* actual data size */
    outp->parmsp[2] = outp->totalData;
    outp->totalParms = totalParams;

    smb_SendTran2Packet(vcp,outp,op);
    smb_FreeTran2Packet(outp);

    return code;
}

typedef struct smb_rap_server_info_0 {
    char    sv0_name[16];
} smb_rap_server_info_0_t;

typedef struct smb_rap_server_info_1 {
    char            sv1_name[16];
    char            sv1_version_major;
    char            sv1_version_minor;
    unsigned long   sv1_type;
    DWORD           *sv1_comment_or_master_browser; /* char *sv1_comment_or_master_browser;*/
} smb_rap_server_info_1_t;

char smb_ServerComment[] = "OpenAFS Client";
int smb_ServerCommentLen = sizeof(smb_ServerComment);

#define SMB_SV_TYPE_SERVER      	0x00000002L
#define SMB_SV_TYPE_NT              0x00001000L
#define SMB_SV_TYPE_SERVER_NT       0x00008000L

long smb_ReceiveRAPNetServerGetInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    smb_tran2Packet_t *outp;
    long code = 0;
    int infoLevel;
    int bufsize;
    unsigned short * tp;
    int totalData;
    int totalParams;
    smb_rap_server_info_0_t * info0;
    smb_rap_server_info_1_t * info1;
    char * cstrp;

    tp = p->parmsp + 1; /* Skip over function number */
    (void) smb_ParseString((unsigned char*) tp, (char **) &tp); /* skip over param descriptor */
    (void) smb_ParseString((unsigned char*) tp, (char **) &tp); /* skip over data descriptor */
    infoLevel = *tp++;
    bufsize = *tp++;

    if (infoLevel != 0 && infoLevel != 1) {
        return CM_ERROR_INVAL;
    }

    totalParams = 6;

    totalData = 
        (infoLevel == 0) ? sizeof(smb_rap_server_info_0_t)
        : (sizeof(smb_rap_server_info_1_t) + smb_ServerCommentLen);

    outp = smb_GetTran2ResponsePacket(vcp, p, op, totalParams, totalData);

    memset(outp->parmsp,0,totalParams);
    memset(outp->datap,0,totalData);

    if (infoLevel == 0) {
        info0 = (smb_rap_server_info_0_t *) outp->datap;
        cstrp = (char *) (info0 + 1);
        strcpy(info0->sv0_name, "AFS");
    } else { /* infoLevel == SMB_INFO_STANDARD */
        info1 = (smb_rap_server_info_1_t *) outp->datap;
        cstrp = (char *) (info1 + 1);
        strcpy(info1->sv1_name, "AFS");

        info1->sv1_type = 
            SMB_SV_TYPE_SERVER |
            SMB_SV_TYPE_NT |
            SMB_SV_TYPE_SERVER_NT;

        info1->sv1_version_major = 5;
        info1->sv1_version_minor = 1;
        info1->sv1_comment_or_master_browser = (DWORD *) (cstrp - outp->datap);

        strcpy(cstrp, smb_ServerComment);

        cstrp += smb_ServerCommentLen;
    }

    totalData = (DWORD)(cstrp - outp->datap);
    outp->totalData = min(bufsize,totalData); /* actual data size */
    outp->parmsp[0] = (outp->totalData == totalData)? 0 : ERROR_MORE_DATA;
    outp->parmsp[2] = totalData;
    outp->totalParms = totalParams;

    smb_SendTran2Packet(vcp,outp,op);
    smb_FreeTran2Packet(outp);

    return code;
}

long smb_ReceiveV3Tran2A(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_tran2Packet_t *asp;
    int totalParms;
    int totalData;
    int parmDisp;
    int dataDisp;
    int parmOffset;
    int dataOffset;
    int parmCount;
    int dataCount;
    int firstPacket;
    long code = 0;

    /* We sometimes see 0 word count.  What to do? */
    if (*inp->wctp == 0) {
        osi_Log0(smb_logp, "Transaction2 word count = 0"); 
#ifndef DJGPP
	LogEvent(EVENTLOG_WARNING_TYPE, MSG_SMB_ZERO_TRANSACTION_COUNT);
#endif /* !DJGPP */

        smb_SetSMBDataLength(outp, 0);
        smb_SendPacket(vcp, outp);
        return 0;
    }

    totalParms = smb_GetSMBParm(inp, 0);
    totalData = smb_GetSMBParm(inp, 1);
        
    firstPacket = (inp->inCom == 0x32);
        
    /* find the packet we're reassembling */
    lock_ObtainWrite(&smb_globalLock);
    asp = smb_FindTran2Packet(vcp, inp);
    if (!asp) {
        asp = smb_NewTran2Packet(vcp, inp, totalParms, totalData);
    }
    lock_ReleaseWrite(&smb_globalLock);
        
    /* now merge in this latest packet; start by looking up offsets */
    if (firstPacket) {
        parmDisp = dataDisp = 0;
        parmOffset = smb_GetSMBParm(inp, 10);
        dataOffset = smb_GetSMBParm(inp, 12);
        parmCount = smb_GetSMBParm(inp, 9);
        dataCount = smb_GetSMBParm(inp, 11);
        asp->maxReturnParms = smb_GetSMBParm(inp, 2);
        asp->maxReturnData = smb_GetSMBParm(inp, 3);

        osi_Log3(smb_logp, "SMB3 received T2 init packet total data %d, cur data %d, max return data %d",
                 totalData, dataCount, asp->maxReturnData);
    }
    else {
        parmDisp = smb_GetSMBParm(inp, 4);
        parmOffset = smb_GetSMBParm(inp, 3);
        dataDisp = smb_GetSMBParm(inp, 7);
        dataOffset = smb_GetSMBParm(inp, 6);
        parmCount = smb_GetSMBParm(inp, 2);
        dataCount = smb_GetSMBParm(inp, 5);

        osi_Log2(smb_logp, "SMB3 received T2 aux packet parms %d, data %d",
                 parmCount, dataCount);
    }   

    /* now copy the parms and data */
    if ( asp->totalParms > 0 && parmCount != 0 )
    {
        memcpy(((char *)asp->parmsp) + parmDisp, inp->data + parmOffset, parmCount);
    }
    if ( asp->totalData > 0 && dataCount != 0 ) {
        memcpy(asp->datap + dataDisp, inp->data + dataOffset, dataCount);
    }

    /* account for new bytes */
    asp->curData += dataCount;
    asp->curParms += parmCount;

    /* finally, if we're done, remove the packet from the queue and dispatch it */
    if (asp->totalParms > 0 &&
        asp->curParms > 0 &&
        asp->totalData <= asp->curData &&
        asp->totalParms <= asp->curParms) {
        /* we've received it all */
        lock_ObtainWrite(&smb_globalLock);
        osi_QRemove((osi_queue_t **) &smb_tran2AssemblyQueuep, &asp->q);
        lock_ReleaseWrite(&smb_globalLock);

        /* now dispatch it */
        if ( asp->opcode >= 0 && asp->opcode < 20 && smb_tran2DispatchTable[asp->opcode].procp) {
            osi_Log4(smb_logp,"AFS Server - Dispatch-2 %s vcp[%p] lana[%d] lsn[%d]",myCrt_2Dispatch(asp->opcode),vcp,vcp->lana,vcp->lsn);
            code = (*smb_tran2DispatchTable[asp->opcode].procp)(vcp, asp, outp);
        }
        else {
            osi_Log4(smb_logp,"AFS Server - Dispatch-2 [INVALID] op[%x] vcp[%p] lana[%d] lsn[%d]", asp->opcode, vcp, vcp->lana, vcp->lsn);
            code = CM_ERROR_BADOP;
        }

        /* if an error is returned, we're supposed to send an error packet,
         * otherwise the dispatched function already did the data sending.
         * We give dispatched proc the responsibility since it knows how much
         * space to allocate.
         */
        if (code != 0) {
            smb_SendTran2Error(vcp, asp, outp, code);
        }

        /* free the input tran 2 packet */
        smb_FreeTran2Packet(asp);
    }
    else if (firstPacket) {
        /* the first packet in a multi-packet request, we need to send an
         * ack to get more data.
         */
        smb_SetSMBDataLength(outp, 0);
        smb_SendPacket(vcp, outp);
    }

    return 0;
}

long smb_ReceiveTran2Open(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    char *pathp;
    smb_tran2Packet_t *outp;
    long code = 0;
    cm_space_t *spacep;
    int excl;
    cm_user_t *userp;
    cm_scache_t *dscp;		/* dir we're dealing with */
    cm_scache_t *scp;		/* file we're creating */
    cm_attr_t setAttr;
    int initialModeBits;
    smb_fid_t *fidp;
    int attributes;
    char *lastNamep;
    afs_uint32 dosTime;
    int openFun;
    int trunc;
    int openMode;
    int extraInfo;
    int openAction;
    int parmSlot;			/* which parm we're dealing with */
    long returnEALength;
    char *tidPathp;
    cm_req_t req;
    int created = 0;

    cm_InitReq(&req);

    scp = NULL;
        
    extraInfo = (p->parmsp[0] & 1);	/* return extra info */
    returnEALength = (p->parmsp[0] & 8);	/* return extended attr length */

    openFun = p->parmsp[6];		/* open function */
    excl = ((openFun & 3) == 0);
    trunc = ((openFun & 3) == 2);	/* truncate it */
    openMode = (p->parmsp[1] & 0x7);
    openAction = 0;			/* tracks what we did */

    attributes = p->parmsp[3];
    dosTime = p->parmsp[4] | (p->parmsp[5] << 16);
        
    /* compute initial mode bits based on read-only flag in attributes */
    initialModeBits = 0666;
    if (attributes & SMB_ATTR_READONLY) 
        initialModeBits &= ~0222;
        
    pathp = (char *) (&p->parmsp[14]);
    if (smb_StoreAnsiFilenames)
        OemToChar(pathp,pathp);
    
    outp = smb_GetTran2ResponsePacket(vcp, p, op, 40, 0);

    spacep = cm_GetSpace();
    smb_StripLastComponent(spacep->data, &lastNamep, pathp);

    if (lastNamep && strcmp(lastNamep, SMB_IOCTL_FILENAME) == 0) {
        /* special case magic file name for receiving IOCTL requests
         * (since IOCTL calls themselves aren't getting through).
         */
        fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
        smb_SetupIoctlFid(fidp, spacep);

        /* copy out remainder of the parms */
        parmSlot = 0;
        outp->parmsp[parmSlot++] = fidp->fid;
        if (extraInfo) {
            outp->parmsp[parmSlot++] = 0;       /* attrs */
            outp->parmsp[parmSlot++] = 0;       /* mod time */
            outp->parmsp[parmSlot++] = 0; 
            outp->parmsp[parmSlot++] = 0;       /* len */
            outp->parmsp[parmSlot++] = 0x7fff;
            outp->parmsp[parmSlot++] = openMode;
            outp->parmsp[parmSlot++] = 0;       /* file type 0 ==> normal file or dir */
            outp->parmsp[parmSlot++] = 0;       /* IPC junk */
        }   
        /* and the final "always present" stuff */
        outp->parmsp[parmSlot++] = 1;           /* openAction found existing file */
        /* next write out the "unique" ID */
        outp->parmsp[parmSlot++] = 0x1234;
        outp->parmsp[parmSlot++] = 0x5678;
        outp->parmsp[parmSlot++] = 0;
        if (returnEALength) {
            outp->parmsp[parmSlot++] = 0;
            outp->parmsp[parmSlot++] = 0;
        }       
                
        outp->totalData = 0;
        outp->totalParms = parmSlot * 2;
                
        smb_SendTran2Packet(vcp, outp, op);
                
        smb_FreeTran2Packet(outp);

        /* and clean up fid reference */
        smb_ReleaseFID(fidp);
        return 0;
    }

#ifdef DEBUG_VERBOSE
    {
        char *hexp, *asciip;
        asciip = (lastNamep ? lastNamep : pathp);
        hexp = osi_HexifyString( asciip );
        DEBUG_EVENT2("AFS","T2Open H[%s] A[%s]", hexp, asciip);
        free(hexp);
    }       
#endif

    userp = smb_GetTran2User(vcp, p);
    /* In the off chance that userp is NULL, we log and abandon */
    if (!userp) {
        osi_Log1(smb_logp, "ReceiveTran2Open user [%d] not resolvable", p->uid);
        smb_FreeTran2Packet(outp);
        return CM_ERROR_BADSMB;
    }

    code = smb_LookupTIDPath(vcp, p->tid, &tidPathp);
    if (code == CM_ERROR_TIDIPC) {
        /* Attempt to use a TID allocated for IPC.  The client
         * is probably looking for DCE RPC end points which we
         * don't support OR it could be looking to make a DFS
         * referral request. 
         */
        osi_Log0(smb_logp, "Tran2Open received IPC TID");
#ifndef DFS_SUPPORT
        cm_ReleaseUser(userp);
        smb_FreeTran2Packet(outp);
        return CM_ERROR_NOSUCHPATH;
#endif
    }

    dscp = NULL;
    code = cm_NameI(cm_data.rootSCachep, pathp,
                     CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                     userp, tidPathp, &req, &scp);
    if (code != 0) {
        code = cm_NameI(cm_data.rootSCachep, spacep->data,
                         CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                         userp, tidPathp, &req, &dscp);
        cm_FreeSpace(spacep);

        if (code) {
            cm_ReleaseUser(userp);
            smb_FreeTran2Packet(outp);
            return code;
        }
        
#ifdef DFS_SUPPORT
        if (dscp->fileType == CM_SCACHETYPE_DFSLINK) {
            cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            smb_FreeTran2Packet(outp);
            if ( WANTS_DFS_PATHNAMES(p) )
                return CM_ERROR_PATH_NOT_COVERED;
            else
                return CM_ERROR_BADSHARENAME;
        }
#endif /* DFS_SUPPORT */

        /* otherwise, scp points to the parent directory.  Do a lookup,
         * and truncate the file if we find it, otherwise we create the
         * file.
         */
        if (!lastNamep) 
            lastNamep = pathp;
        else 
            lastNamep++;
        code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD, userp,
                         &req, &scp);
        if (code && code != CM_ERROR_NOSUCHFILE) {
            cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            smb_FreeTran2Packet(outp);
            return code;
        }
    } else {
#ifdef DFS_SUPPORT
        if (scp->fileType == CM_SCACHETYPE_DFSLINK) {
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            smb_FreeTran2Packet(outp);
            if ( WANTS_DFS_PATHNAMES(p) )
                return CM_ERROR_PATH_NOT_COVERED;
            else
                return CM_ERROR_BADSHARENAME;
        }
#endif /* DFS_SUPPORT */

        /* macintosh is expensive to program for it */
        cm_FreeSpace(spacep);
    }
        
    /* if we get here, if code is 0, the file exists and is represented by
     * scp.  Otherwise, we have to create it.
     */
    if (code == 0) {
        code = cm_CheckOpen(scp, openMode, trunc, userp, &req);
        if (code) {
            if (dscp) 
                cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            smb_FreeTran2Packet(outp);
            return code;
        }

        if (excl) {
            /* oops, file shouldn't be there */
            if (dscp) 
                cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            smb_FreeTran2Packet(outp);
            return CM_ERROR_EXISTS;
        }

        if (trunc) {
            setAttr.mask = CM_ATTRMASK_LENGTH;
            setAttr.length.LowPart = 0;
            setAttr.length.HighPart = 0;
            code = cm_SetAttr(scp, &setAttr, userp, &req);
            openAction = 3;	/* truncated existing file */
        }   
        else 
            openAction = 1;	/* found existing file */
    }
    else if (!(openFun & 0x10)) {
        /* don't create if not found */
        if (dscp) 
            cm_ReleaseSCache(dscp);
        osi_assert(scp == NULL);
        cm_ReleaseUser(userp);
        smb_FreeTran2Packet(outp);
        return CM_ERROR_NOSUCHFILE;
    }
    else {
        osi_assert(dscp != NULL && scp == NULL);
        openAction = 2;	/* created file */
        setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
        smb_UnixTimeFromSearchTime(&setAttr.clientModTime, dosTime);
        code = cm_Create(dscp, lastNamep, 0, &setAttr, &scp, userp,
                          &req);
        if (code == 0) {
	    created = 1;
	    if (dscp->flags & CM_SCACHEFLAG_ANYWATCH)
		smb_NotifyChange(FILE_ACTION_ADDED,
				 FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,  
				  dscp, lastNamep, NULL, TRUE);
	} else if (!excl && code == CM_ERROR_EXISTS) {
            /* not an exclusive create, and someone else tried
             * creating it already, then we open it anyway.  We
             * don't bother retrying after this, since if this next
             * fails, that means that the file was deleted after we
             * started this call.
             */
            code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
                              userp, &req, &scp);
            if (code == 0) {
                if (trunc) {
                    setAttr.mask = CM_ATTRMASK_LENGTH;
                    setAttr.length.LowPart = 0;
                    setAttr.length.HighPart = 0;
                    code = cm_SetAttr(scp, &setAttr, userp,
                                       &req);
                }   
            }	/* lookup succeeded */
        }
    }
        
    /* we don't need this any longer */
    if (dscp) 
        cm_ReleaseSCache(dscp);

    if (code) {
        /* something went wrong creating or truncating the file */
        if (scp) 
            cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        smb_FreeTran2Packet(outp);
        return code;
    }
        
    /* make sure we're about to open a file */
    if (scp->fileType != CM_SCACHETYPE_FILE) {
        code = 0;
        while (code == 0 && scp->fileType == CM_SCACHETYPE_SYMLINK) {
            cm_scache_t * targetScp = 0;
            code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
            if (code == 0) {
                /* we have a more accurate file to use (the
                 * target of the symbolic link).  Otherwise,
                 * we'll just use the symlink anyway.
                 */
                osi_Log2(smb_logp, "symlink vp %x to vp %x",
                          scp, targetScp);
                cm_ReleaseSCache(scp);
                scp = targetScp;
            }
        }
        if (scp->fileType != CM_SCACHETYPE_FILE) {
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            smb_FreeTran2Packet(outp);
            return CM_ERROR_ISDIR;
        }
    }

    /* now all we have to do is open the file itself */
    fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
    osi_assert(fidp);
	
    cm_HoldUser(userp);
    lock_ObtainMutex(&fidp->mx);
    /* save a pointer to the vnode */
    osi_Log2(smb_logp,"smb_ReceiveTran2Open fidp 0x%p scp 0x%p", fidp, scp);
    fidp->scp = scp;
    lock_ObtainMutex(&scp->mx);
    scp->flags |= CM_SCACHEFLAG_SMB_FID;
    lock_ReleaseMutex(&scp->mx);
    
    /* and the user */
    fidp->userp = userp;
        
    /* compute open mode */
    if (openMode != 1) 
	fidp->flags |= SMB_FID_OPENREAD_LISTDIR;
    if (openMode == 1 || openMode == 2)
        fidp->flags |= SMB_FID_OPENWRITE;

    /* remember that the file was newly created */
    if (created)
	fidp->flags |= SMB_FID_CREATED;

    lock_ReleaseMutex(&fidp->mx);

    smb_ReleaseFID(fidp);
        
    cm_Open(scp, 0, userp);

    /* copy out remainder of the parms */
    parmSlot = 0;
    outp->parmsp[parmSlot++] = fidp->fid;
    lock_ObtainMutex(&scp->mx);
    if (extraInfo) {
        outp->parmsp[parmSlot++] = smb_Attributes(scp);
        smb_SearchTimeFromUnixTime(&dosTime, scp->clientModTime);
        outp->parmsp[parmSlot++] = (unsigned short)(dosTime & 0xffff);
        outp->parmsp[parmSlot++] = (unsigned short)((dosTime>>16) & 0xffff);
        outp->parmsp[parmSlot++] = (unsigned short) (scp->length.LowPart & 0xffff);
        outp->parmsp[parmSlot++] = (unsigned short) ((scp->length.LowPart >> 16) & 0xffff);
        outp->parmsp[parmSlot++] = openMode;
        outp->parmsp[parmSlot++] = 0;   /* file type 0 ==> normal file or dir */
        outp->parmsp[parmSlot++] = 0;   /* IPC junk */
    }   
    /* and the final "always present" stuff */
    outp->parmsp[parmSlot++] = openAction;
    /* next write out the "unique" ID */
    outp->parmsp[parmSlot++] = (unsigned short) (scp->fid.vnode & 0xffff); 
    outp->parmsp[parmSlot++] = (unsigned short) (scp->fid.volume & 0xffff); 
    outp->parmsp[parmSlot++] = 0; 
    if (returnEALength) {
        outp->parmsp[parmSlot++] = 0; 
        outp->parmsp[parmSlot++] = 0; 
    }   
    lock_ReleaseMutex(&scp->mx);
    outp->totalData = 0;		/* total # of data bytes */
    outp->totalParms = parmSlot * 2;	/* shorts are two bytes */

    smb_SendTran2Packet(vcp, outp, op);

    smb_FreeTran2Packet(outp);

    cm_ReleaseUser(userp);
    /* leave scp held since we put it in fidp->scp */
    return 0;
}   

long smb_ReceiveTran2QFSInfoFid(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    unsigned short fid;
    unsigned short infolevel;

    infolevel = p->parmsp[0];
    fid = p->parmsp[1];
    osi_Log2(smb_logp, "T2 QFSInfoFid InfoLevel 0x%x fid 0x%x - NOT_SUPPORTED", infolevel, fid);
    
    return CM_ERROR_BAD_LEVEL;
}

long smb_ReceiveTran2QFSInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    smb_tran2Packet_t *outp;
    smb_tran2QFSInfo_t qi;
    int responseSize;
    static char FSname[8] = {'A', 0, 'F', 0, 'S', 0, 0, 0};
        
    osi_Log1(smb_logp, "T2 QFSInfo type 0x%x", p->parmsp[0]);

    switch (p->parmsp[0]) {
    case SMB_INFO_ALLOCATION: 
	responseSize = sizeof(qi.u.allocInfo); 
	break;
    case SMB_INFO_VOLUME: 
	responseSize = sizeof(qi.u.volumeInfo); 
	break;
    case SMB_QUERY_FS_VOLUME_INFO: 
	responseSize = sizeof(qi.u.FSvolumeInfo); 
	break;
    case SMB_QUERY_FS_SIZE_INFO: 
	responseSize = sizeof(qi.u.FSsizeInfo); 
	break;
    case SMB_QUERY_FS_DEVICE_INFO: 
	responseSize = sizeof(qi.u.FSdeviceInfo); 
	break;
    case SMB_QUERY_FS_ATTRIBUTE_INFO: 
	responseSize = sizeof(qi.u.FSattributeInfo); 
	break;
    case SMB_INFO_UNIX: 	/* CIFS Unix Info */
    case SMB_INFO_MACOS: 	/* Mac FS Info */
    default: 
	return CM_ERROR_BADOP;
    }

    outp = smb_GetTran2ResponsePacket(vcp, p, op, 0, responseSize);
    switch (p->parmsp[0]) {
    case SMB_INFO_ALLOCATION: 
        /* alloc info */
        qi.u.allocInfo.FSID = 0;
        qi.u.allocInfo.sectorsPerAllocUnit = 1;
        qi.u.allocInfo.totalAllocUnits = 0x7fffffff;
        qi.u.allocInfo.availAllocUnits = 0x3fffffff;
        qi.u.allocInfo.bytesPerSector = 1024;
        break;

    case SMB_INFO_VOLUME: 
        /* volume info */
        qi.u.volumeInfo.vsn = 1234;
        qi.u.volumeInfo.vnCount = 4;
        /* we're supposed to pad it out with zeroes to the end */
        memset(&qi.u.volumeInfo.label, 0, sizeof(qi.u.volumeInfo.label));
        memcpy(qi.u.volumeInfo.label, "AFS", 4);
        break;

    case SMB_QUERY_FS_VOLUME_INFO: 
        /* FS volume info */
        memset((char *)&qi.u.FSvolumeInfo.vct, 0, 4);
        qi.u.FSvolumeInfo.vsn = 1234;
        qi.u.FSvolumeInfo.vnCount = 8;
        memcpy(qi.u.FSvolumeInfo.label, "A\0F\0S\0\0\0", 8);
        break;

    case SMB_QUERY_FS_SIZE_INFO: 
        /* FS size info */
        qi.u.FSsizeInfo.totalAllocUnits.HighPart = 0;
	qi.u.FSsizeInfo.totalAllocUnits.LowPart= 0x7fffffff;
        qi.u.FSsizeInfo.availAllocUnits.HighPart = 0;
	qi.u.FSsizeInfo.availAllocUnits.LowPart= 0x3fffffff;
        qi.u.FSsizeInfo.sectorsPerAllocUnit = 1;
        qi.u.FSsizeInfo.bytesPerSector = 1024;
        break;

    case SMB_QUERY_FS_DEVICE_INFO: 
        /* FS device info */
        qi.u.FSdeviceInfo.devType = 0x14; /* network file system */
        qi.u.FSdeviceInfo.characteristics = 0x50; /* remote, virtual */
        break;

    case SMB_QUERY_FS_ATTRIBUTE_INFO: 
        /* FS attribute info */
        /* attributes, defined in WINNT.H:
         *	FILE_CASE_SENSITIVE_SEARCH	0x1
         *	FILE_CASE_PRESERVED_NAMES	0x2
	 *      FILE_VOLUME_QUOTAS              0x10
         *	<no name defined>		0x4000
         *	   If bit 0x4000 is not set, Windows 95 thinks
         *	   we can't handle long (non-8.3) names,
         *	   despite our protestations to the contrary.
         */
        qi.u.FSattributeInfo.attributes = 0x4003;
        qi.u.FSattributeInfo.maxCompLength = MAX_PATH;
        qi.u.FSattributeInfo.FSnameLength = sizeof(FSname);
        memcpy(qi.u.FSattributeInfo.FSname, FSname, sizeof(FSname));
        break;
    }   
        
    /* copy out return data, and set corresponding sizes */
    outp->totalParms = 0;
    outp->totalData = responseSize;
    memcpy(outp->datap, &qi, responseSize);

    /* send and free the packets */
    smb_SendTran2Packet(vcp, outp, op);
    smb_FreeTran2Packet(outp);

    return 0;
}

long smb_ReceiveTran2SetFSInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
    osi_Log0(smb_logp,"ReceiveTran2SetFSInfo - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
}

struct smb_ShortNameRock {
    char *maskp;
    unsigned int vnode;
    char *shortName;
    size_t shortNameLen;
};      

int cm_GetShortNameProc(cm_scache_t *scp, cm_dirEntry_t *dep, void *vrockp,
                         osi_hyper_t *offp)
{       
    struct smb_ShortNameRock *rockp;
    char *shortNameEnd;

    rockp = vrockp;
    /* compare both names and vnodes, though probably just comparing vnodes
     * would be safe enough.
     */
    if (cm_stricmp(dep->name, rockp->maskp) != 0)
        return 0;
    if (ntohl(dep->fid.vnode) != rockp->vnode)
        return 0;
    /* This is the entry */
    cm_Gen8Dot3Name(dep, rockp->shortName, &shortNameEnd);
    rockp->shortNameLen = shortNameEnd - rockp->shortName;
    return CM_ERROR_STOPNOW;
}       

long cm_GetShortName(char *pathp, cm_user_t *userp, cm_req_t *reqp,
	char *tidPathp, int vnode, char *shortName, size_t *shortNameLenp)
{
    struct smb_ShortNameRock rock;
    char *lastNamep;
    cm_space_t *spacep;
    cm_scache_t *dscp;
    int caseFold = CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD;
    long code = 0;
    osi_hyper_t thyper;

    spacep = cm_GetSpace();
    smb_StripLastComponent(spacep->data, &lastNamep, pathp);

    code = cm_NameI(cm_data.rootSCachep, spacep->data, caseFold, userp, tidPathp,
                     reqp, &dscp);
    cm_FreeSpace(spacep);
    if (code) 
        return code;

#ifdef DFS_SUPPORT
    if (dscp->fileType == CM_SCACHETYPE_DFSLINK) {
        cm_ReleaseSCache(dscp);
        cm_ReleaseUser(userp);
        return CM_ERROR_PATH_NOT_COVERED;
    }
#endif /* DFS_SUPPORT */

    if (!lastNamep) lastNamep = pathp;
    else lastNamep++;
    thyper.LowPart = 0;
    thyper.HighPart = 0;
    rock.shortName = shortName;
    rock.vnode = vnode;
    rock.maskp = lastNamep;
    code = cm_ApplyDir(dscp, cm_GetShortNameProc, &rock, &thyper, userp, reqp, NULL);

    cm_ReleaseSCache(dscp);

    if (code == 0)
        return CM_ERROR_NOSUCHFILE;
    if (code == CM_ERROR_STOPNOW) {
        *shortNameLenp = rock.shortNameLen;
        return 0;
    }
    return code;
}

long smb_ReceiveTran2QPathInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
    smb_tran2Packet_t *outp;
    afs_uint32 dosTime;
    FILETIME ft;
    unsigned short infoLevel;
    smb_tran2QPathInfo_t qpi;
    int responseSize;
    unsigned short attributes;
    unsigned long extAttributes;
    char shortName[13];
    unsigned int len;
    cm_user_t *userp;
    cm_space_t *spacep;
    cm_scache_t *scp, *dscp;
    int scp_mx_held = 0;
    int delonclose = 0;
    long code = 0;
    char *pathp;
    char *tidPathp;
    char *lastComp;
    cm_req_t req;

    cm_InitReq(&req);

    infoLevel = p->parmsp[0];
    if (infoLevel == SMB_INFO_IS_NAME_VALID) 
        responseSize = 0;
    else if (infoLevel == SMB_INFO_STANDARD) 
        responseSize = sizeof(qpi.u.QPstandardInfo);
    else if (infoLevel == SMB_INFO_QUERY_EA_SIZE) 
        responseSize = sizeof(qpi.u.QPeaSizeInfo);
    else if (infoLevel == SMB_QUERY_FILE_BASIC_INFO)
        responseSize = sizeof(qpi.u.QPfileBasicInfo);
    else if (infoLevel == SMB_QUERY_FILE_STANDARD_INFO)
	responseSize = sizeof(qpi.u.QPfileStandardInfo);
    else if (infoLevel == SMB_QUERY_FILE_EA_INFO)
        responseSize = sizeof(qpi.u.QPfileEaInfo);
    else if (infoLevel == SMB_QUERY_FILE_NAME_INFO) 
        responseSize = sizeof(qpi.u.QPfileNameInfo);
    else if (infoLevel == SMB_QUERY_FILE_ALL_INFO) 
        responseSize = sizeof(qpi.u.QPfileAllInfo);
    else if (infoLevel == SMB_QUERY_FILE_ALT_NAME_INFO) 
        responseSize = sizeof(qpi.u.QPfileAltNameInfo);
    else {
        osi_Log2(smb_logp, "Bad Tran2 op 0x%x infolevel 0x%x",
                  p->opcode, infoLevel);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_BAD_LEVEL);
        return 0;
    }

    pathp = (char *)(&p->parmsp[3]);
    if (smb_StoreAnsiFilenames)
	OemToChar(pathp,pathp);
    osi_Log2(smb_logp, "T2 QPathInfo type 0x%x path %s", infoLevel,
              osi_LogSaveString(smb_logp, pathp));

    outp = smb_GetTran2ResponsePacket(vcp, p, opx, 2, responseSize);

    if (infoLevel > 0x100)
        outp->totalParms = 2;
    else
        outp->totalParms = 0;
    outp->totalData = responseSize;
        
    /* now, if we're at infoLevel 6, we're only being asked to check
     * the syntax, so we just OK things now.  In particular, we're *not*
     * being asked to verify anything about the state of any parent dirs.
     */
    if (infoLevel == SMB_INFO_IS_NAME_VALID) {
        smb_SendTran2Packet(vcp, outp, opx);
        smb_FreeTran2Packet(outp);
        return 0;
    }   
        
    userp = smb_GetTran2User(vcp, p);
    if (!userp) {
        osi_Log1(smb_logp, "ReceiveTran2QPathInfo unable to resolve user [%d]", p->uid);
        smb_FreeTran2Packet(outp);
        return CM_ERROR_BADSMB;
    }

    code = smb_LookupTIDPath(vcp, p->tid, &tidPathp);
    if(code) {
        cm_ReleaseUser(userp);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_NOSUCHPATH);
        smb_FreeTran2Packet(outp);
        return 0;
    }

    /*
     * XXX Strange hack XXX
     *
     * As of Patch 7 (13 January 98), we are having the following problem:
     * In NT Explorer 4.0, whenever we click on a directory, AFS gets
     * requests to look up "desktop.ini" in all the subdirectories.
     * This can cause zillions of timeouts looking up non-existent cells
     * and volumes, especially in the top-level directory.
     *
     * We have not found any way to avoid this or work around it except
     * to explicitly ignore the requests for mount points that haven't
     * yet been evaluated and for directories that haven't yet been
     * fetched.
     */
    if (infoLevel == SMB_QUERY_FILE_BASIC_INFO) {
        spacep = cm_GetSpace();
        smb_StripLastComponent(spacep->data, &lastComp, pathp);
#ifndef SPECIAL_FOLDERS
        /* Make sure that lastComp is not NULL */
        if (lastComp) {
            if (stricmp(lastComp, "\\desktop.ini") == 0) {
                code = cm_NameI(cm_data.rootSCachep, spacep->data,
                                 CM_FLAG_CASEFOLD
                                 | CM_FLAG_DIRSEARCH
                                 | CM_FLAG_FOLLOW,
                                 userp, tidPathp, &req, &dscp);
                if (code == 0) {
#ifdef DFS_SUPPORT
                    if (dscp->fileType == CM_SCACHETYPE_DFSLINK) {
                        if ( WANTS_DFS_PATHNAMES(p) )
                            code = CM_ERROR_PATH_NOT_COVERED;
                        else
                            code = CM_ERROR_BADSHARENAME;
                    } else
#endif /* DFS_SUPPORT */
                    if (dscp->fileType == CM_SCACHETYPE_MOUNTPOINT && !dscp->mountRootFid.volume)
                        code = CM_ERROR_NOSUCHFILE;
                    else if (dscp->fileType == CM_SCACHETYPE_DIRECTORY) {
                        cm_buf_t *bp = buf_Find(dscp, &hzero);
                        if (bp)
                            buf_Release(bp);
                        else
                            code = CM_ERROR_NOSUCHFILE;
                    }
                    cm_ReleaseSCache(dscp);
                    if (code) {
                        cm_FreeSpace(spacep);
                        cm_ReleaseUser(userp);
                        smb_SendTran2Error(vcp, p, opx, code);
                        smb_FreeTran2Packet(outp);
                        return 0;
                    }
                }
            }
        }
#endif /* SPECIAL_FOLDERS */

        cm_FreeSpace(spacep);
    }

    /* now do namei and stat, and copy out the info */
    code = cm_NameI(cm_data.rootSCachep, pathp,
                     CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD, userp, tidPathp, &req, &scp);

    if (code) {
        cm_ReleaseUser(userp);
        smb_SendTran2Error(vcp, p, opx, code);
        smb_FreeTran2Packet(outp);
        return 0;
    }

#ifdef DFS_SUPPORT
    if (scp->fileType == CM_SCACHETYPE_DFSLINK) {
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        if ( WANTS_DFS_PATHNAMES(p) )
            code = CM_ERROR_PATH_NOT_COVERED;
        else
            code = CM_ERROR_BADSHARENAME;
        smb_SendTran2Error(vcp, p, opx, code);
        smb_FreeTran2Packet(outp);
        return 0;
    }
#endif /* DFS_SUPPORT */

    lock_ObtainMutex(&scp->mx);
    scp_mx_held = 1;
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) goto done;

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        
    /* now we have the status in the cache entry, and everything is locked.
     * Marshall the output data.
     */
    /* for info level 108, figure out short name */
    if (infoLevel == SMB_QUERY_FILE_ALT_NAME_INFO) {
        code = cm_GetShortName(pathp, userp, &req,
                                tidPathp, scp->fid.vnode, shortName,
                                (size_t *) &len);
        if (code) {
            goto done;
        }

	qpi.u.QPfileAltNameInfo.fileNameLength = (len + 1) * 2;
        mbstowcs((unsigned short *)qpi.u.QPfileAltNameInfo.fileName, shortName, len + 1);

        goto done;
    }
    else if (infoLevel == SMB_QUERY_FILE_NAME_INFO) {
	len = strlen(lastComp);
	qpi.u.QPfileNameInfo.fileNameLength = (len + 1) * 2;
        mbstowcs((unsigned short *)qpi.u.QPfileNameInfo.fileName, lastComp, len + 1);

        goto done;
    }
    else if (infoLevel == SMB_INFO_STANDARD || infoLevel == SMB_INFO_QUERY_EA_SIZE) {
        smb_SearchTimeFromUnixTime(&dosTime, scp->clientModTime);
	qpi.u.QPstandardInfo.creationDateTime = dosTime;
	qpi.u.QPstandardInfo.lastAccessDateTime = dosTime;
	qpi.u.QPstandardInfo.lastWriteDateTime = dosTime;
        qpi.u.QPstandardInfo.dataSize = scp->length.LowPart;
        qpi.u.QPstandardInfo.allocationSize = scp->length.LowPart;
        attributes = smb_Attributes(scp);
        qpi.u.QPstandardInfo.attributes = attributes;
	qpi.u.QPstandardInfo.eaSize = 0;
    }
    else if (infoLevel == SMB_QUERY_FILE_BASIC_INFO) {
        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        qpi.u.QPfileBasicInfo.creationTime = ft;
        qpi.u.QPfileBasicInfo.lastAccessTime = ft;
        qpi.u.QPfileBasicInfo.lastWriteTime = ft;
        qpi.u.QPfileBasicInfo.changeTime = ft;
        extAttributes = smb_ExtAttributes(scp);
	qpi.u.QPfileBasicInfo.attributes = extAttributes;
	qpi.u.QPfileBasicInfo.reserved = 0;
    }
    else if (infoLevel == SMB_QUERY_FILE_STANDARD_INFO) {
	smb_fid_t *fidp = smb_FindFIDByScache(vcp, scp);

        qpi.u.QPfileStandardInfo.allocationSize = scp->length;
        qpi.u.QPfileStandardInfo.endOfFile = scp->length;
        qpi.u.QPfileStandardInfo.numberOfLinks = scp->linkCount;
        qpi.u.QPfileStandardInfo.directory = 
	    ((scp->fileType == CM_SCACHETYPE_DIRECTORY ||
	      scp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
	      scp->fileType == CM_SCACHETYPE_INVALID) ? 1 : 0);
        qpi.u.QPfileStandardInfo.reserved = 0;

    	if (fidp) {
	    lock_ReleaseMutex(&scp->mx);
	    scp_mx_held = 0;
	    lock_ObtainMutex(&fidp->mx);
	    delonclose = fidp->flags & SMB_FID_DELONCLOSE;
	    lock_ReleaseMutex(&fidp->mx);
	    smb_ReleaseFID(fidp);
	}
        qpi.u.QPfileStandardInfo.deletePending = (delonclose ? 1 : 0);
    }
    else if (infoLevel == SMB_QUERY_FILE_EA_INFO) {
        qpi.u.QPfileEaInfo.eaSize = 0;
    }
    else if (infoLevel == SMB_QUERY_FILE_ALL_INFO) {
        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        qpi.u.QPfileAllInfo.creationTime = ft;
        qpi.u.QPfileAllInfo.lastAccessTime = ft;
        qpi.u.QPfileAllInfo.lastWriteTime = ft;
        qpi.u.QPfileAllInfo.changeTime = ft;
        extAttributes = smb_ExtAttributes(scp);
	qpi.u.QPfileAllInfo.attributes = extAttributes;
        qpi.u.QPfileAllInfo.allocationSize = scp->length;
        qpi.u.QPfileAllInfo.endOfFile = scp->length;
        qpi.u.QPfileAllInfo.numberOfLinks = scp->linkCount;
        qpi.u.QPfileAllInfo.deletePending = 0;
        qpi.u.QPfileAllInfo.directory = 
	    ((scp->fileType == CM_SCACHETYPE_DIRECTORY ||
	      scp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
	      scp->fileType == CM_SCACHETYPE_INVALID) ? 1 : 0);
	qpi.u.QPfileAllInfo.indexNumber.HighPart = scp->fid.cell;
	qpi.u.QPfileAllInfo.indexNumber.LowPart  = scp->fid.volume;
	qpi.u.QPfileAllInfo.eaSize = 0;
	qpi.u.QPfileAllInfo.accessFlags = 0;
	qpi.u.QPfileAllInfo.indexNumber2.HighPart = scp->fid.vnode;
	qpi.u.QPfileAllInfo.indexNumber2.LowPart  = scp->fid.unique;
	qpi.u.QPfileAllInfo.currentByteOffset.HighPart = 0;
	qpi.u.QPfileAllInfo.currentByteOffset.LowPart = 0;
	qpi.u.QPfileAllInfo.mode = 0;
	qpi.u.QPfileAllInfo.alignmentRequirement = 0;
	len = strlen(lastComp);
	qpi.u.QPfileAllInfo.fileNameLength = (len + 1) * 2;
        mbstowcs((unsigned short *)qpi.u.QPfileAllInfo.fileName, lastComp, len + 1);
    }

    /* send and free the packets */
  done:
    if (scp_mx_held)
	lock_ReleaseMutex(&scp->mx);
    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    if (code == 0) {
	memcpy(outp->datap, &qpi, responseSize);
	smb_SendTran2Packet(vcp, outp, opx);
    } else {
        smb_SendTran2Error(vcp, p, opx, code);
    }
    smb_FreeTran2Packet(outp);

    return 0;
}

long smb_ReceiveTran2SetPathInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
#if 0
    osi_Log0(smb_logp,"ReceiveTran2SetPathInfo - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
#else
    long code = 0;
    smb_fid_t *fidp;
    unsigned short infoLevel;
    char * pathp;
    smb_tran2Packet_t *outp;
    smb_tran2QPathInfo_t *spi;
    cm_user_t *userp;
    cm_scache_t *scp, *dscp;
    cm_req_t req;
    cm_space_t *spacep;
    char *tidPathp;
    char *lastComp;

    cm_InitReq(&req);

    infoLevel = p->parmsp[0];
    osi_Log1(smb_logp,"ReceiveTran2SetPathInfo type 0x%x", infoLevel);
    if (infoLevel != SMB_INFO_STANDARD && 
	infoLevel != SMB_INFO_QUERY_EA_SIZE &&
	infoLevel != SMB_INFO_QUERY_ALL_EAS) {
        osi_Log2(smb_logp, "Bad Tran2 op 0x%x infolevel 0x%x",
                  p->opcode, infoLevel);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_BAD_LEVEL);
        return 0;
    }

    pathp = (char *)(&p->parmsp[3]);
    if (smb_StoreAnsiFilenames)
	OemToChar(pathp,pathp);
    osi_Log2(smb_logp, "T2 SetPathInfo infolevel 0x%x path %s", infoLevel,
              osi_LogSaveString(smb_logp, pathp));

    userp = smb_GetTran2User(vcp, p);
    if (!userp) {
    	osi_Log1(smb_logp,"ReceiveTran2SetPathInfo unable to resolve user [%d]", p->uid);
    	code = CM_ERROR_BADSMB;
    	goto done;
    }   

    code = smb_LookupTIDPath(vcp, p->tid, &tidPathp);
    if (code == CM_ERROR_TIDIPC) {
        /* Attempt to use a TID allocated for IPC.  The client
         * is probably looking for DCE RPC end points which we
         * don't support OR it could be looking to make a DFS
         * referral request. 
         */
        osi_Log0(smb_logp, "Tran2Open received IPC TID");
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHPATH;
    }

    /*
    * XXX Strange hack XXX
    *
    * As of Patch 7 (13 January 98), we are having the following problem:
    * In NT Explorer 4.0, whenever we click on a directory, AFS gets
    * requests to look up "desktop.ini" in all the subdirectories.
    * This can cause zillions of timeouts looking up non-existent cells
    * and volumes, especially in the top-level directory.
    *
    * We have not found any way to avoid this or work around it except
    * to explicitly ignore the requests for mount points that haven't
    * yet been evaluated and for directories that haven't yet been
    * fetched.
    */
    if (infoLevel == SMB_QUERY_FILE_BASIC_INFO) {
        spacep = cm_GetSpace();
        smb_StripLastComponent(spacep->data, &lastComp, pathp);
#ifndef SPECIAL_FOLDERS
        /* Make sure that lastComp is not NULL */
        if (lastComp) {
            if (stricmp(lastComp, "\\desktop.ini") == 0) {
                code = cm_NameI(cm_data.rootSCachep, spacep->data,
                                 CM_FLAG_CASEFOLD
                                 | CM_FLAG_DIRSEARCH
                                 | CM_FLAG_FOLLOW,
                                 userp, tidPathp, &req, &dscp);
                if (code == 0) {
#ifdef DFS_SUPPORT
                    if (dscp->fileType == CM_SCACHETYPE_DFSLINK) {
                        if ( WANTS_DFS_PATHNAMES(p) )
                            code = CM_ERROR_PATH_NOT_COVERED;
                        else
                            code = CM_ERROR_BADSHARENAME;
                    } else
#endif /* DFS_SUPPORT */
                    if (dscp->fileType == CM_SCACHETYPE_MOUNTPOINT && !dscp->mountRootFid.volume)
                        code = CM_ERROR_NOSUCHFILE;
                    else if (dscp->fileType == CM_SCACHETYPE_DIRECTORY) {
                        cm_buf_t *bp = buf_Find(dscp, &hzero);
                        if (bp)
                            buf_Release(bp);
                        else
                            code = CM_ERROR_NOSUCHFILE;
                    }
                    cm_ReleaseSCache(dscp);
                    if (code) {
                        cm_FreeSpace(spacep);
                        cm_ReleaseUser(userp);
                        smb_SendTran2Error(vcp, p, opx, code);
                        return 0;
                    }
                }
            }
        }
#endif /* SPECIAL_FOLDERS */

        cm_FreeSpace(spacep);
    }

    /* now do namei and stat, and copy out the info */
    code = cm_NameI(cm_data.rootSCachep, pathp,
                     CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD, userp, tidPathp, &req, &scp);
    if (code) {
        cm_ReleaseUser(userp);
        smb_SendTran2Error(vcp, p, opx, code);
        return 0;
    }

    fidp = smb_FindFIDByScache(vcp, scp);
    if (!fidp) {
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
	smb_SendTran2Error(vcp, p, opx, code);
        return 0;
    }

    lock_ObtainMutex(&fidp->mx);
    if (!(fidp->flags & SMB_FID_OPENWRITE)) {
	lock_ReleaseMutex(&fidp->mx);
        cm_ReleaseSCache(scp);
        smb_ReleaseFID(fidp);
        cm_ReleaseUser(userp);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_NOACCESS);
        return 0;
    }
    lock_ReleaseMutex(&fidp->mx);

    outp = smb_GetTran2ResponsePacket(vcp, p, opx, 2, 0);

    outp->totalParms = 2;
    outp->totalData = 0;

    spi = (smb_tran2QPathInfo_t *)p->datap;
    if (infoLevel == SMB_INFO_STANDARD || infoLevel == SMB_INFO_QUERY_EA_SIZE) {
        cm_attr_t attr;

        /* lock the vnode with a callback; we need the current status
         * to determine what the new status is, in some cases.
         */
        lock_ObtainMutex(&scp->mx);
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
                          CM_SCACHESYNC_GETSTATUS
                         | CM_SCACHESYNC_NEEDCALLBACK);
        if (code) {
	    lock_ReleaseMutex(&scp->mx);
            goto done;
        }
	cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

	lock_ReleaseMutex(&scp->mx);
	lock_ObtainMutex(&fidp->mx);
	lock_ObtainMutex(&scp->mx);

        /* prepare for setattr call */
        attr.mask = CM_ATTRMASK_LENGTH;
        attr.length.LowPart = spi->u.QPstandardInfo.dataSize;
        attr.length.HighPart = 0;

	if (spi->u.QPstandardInfo.lastWriteDateTime != 0) {
	    smb_UnixTimeFromSearchTime(&attr.clientModTime, spi->u.QPstandardInfo.lastWriteDateTime);
            attr.mask |= CM_ATTRMASK_CLIENTMODTIME;
            fidp->flags |= SMB_FID_MTIMESETDONE;
        }
		
        if (spi->u.QPstandardInfo.attributes != 0) {
            if ((scp->unixModeBits & 0222)
                 && (spi->u.QPstandardInfo.attributes & SMB_ATTR_READONLY) != 0) {
                /* make a writable file read-only */
                attr.mask |= CM_ATTRMASK_UNIXMODEBITS;
                attr.unixModeBits = scp->unixModeBits & ~0222;
            }
            else if ((scp->unixModeBits & 0222) == 0
                      && (spi->u.QPstandardInfo.attributes & SMB_ATTR_READONLY) == 0) {
                /* make a read-only file writable */
                attr.mask |= CM_ATTRMASK_UNIXMODEBITS;
                attr.unixModeBits = scp->unixModeBits | 0222;
            }
        }
        lock_ReleaseMutex(&scp->mx);
	lock_ReleaseMutex(&fidp->mx);

        /* call setattr */
        if (attr.mask)
            code = cm_SetAttr(scp, &attr, userp, &req);
        else
            code = 0;
    }               
    else if (infoLevel == SMB_INFO_QUERY_ALL_EAS) {
	/* we don't support EAs */
	code = CM_ERROR_INVAL;
    }       

  done:
    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);
    if (code == 0) 
        smb_SendTran2Packet(vcp, outp, opx);
    else 
        smb_SendTran2Error(vcp, p, opx, code);
    smb_FreeTran2Packet(outp);

    return 0;
#endif
}

long smb_ReceiveTran2QFileInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
    smb_tran2Packet_t *outp;
    FILETIME ft;
    unsigned long attributes;
    unsigned short infoLevel;
    int responseSize;
    unsigned short fid;
    int delonclose = 0;
    cm_user_t *userp;
    smb_fid_t *fidp;
    cm_scache_t *scp;
    smb_tran2QFileInfo_t qfi;
    long code = 0;
    cm_req_t req;

    cm_InitReq(&req);

    fid = p->parmsp[0];
    fidp = smb_FindFID(vcp, fid, 0);

    if (fidp == NULL) {
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_BADFD);
        return 0;
    }

    infoLevel = p->parmsp[1];
    if (infoLevel == SMB_QUERY_FILE_BASIC_INFO) 
        responseSize = sizeof(qfi.u.QFbasicInfo);
    else if (infoLevel == SMB_QUERY_FILE_STANDARD_INFO) 
        responseSize = sizeof(qfi.u.QFstandardInfo);
    else if (infoLevel == SMB_QUERY_FILE_EA_INFO)
        responseSize = sizeof(qfi.u.QFeaInfo);
    else if (infoLevel == SMB_QUERY_FILE_NAME_INFO) 
        responseSize = sizeof(qfi.u.QFfileNameInfo);
    else {
        osi_Log2(smb_logp, "Bad Tran2 op 0x%x infolevel 0x%x",
                  p->opcode, infoLevel);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_BAD_LEVEL);
        smb_ReleaseFID(fidp);
        return 0;
    }
    osi_Log2(smb_logp, "T2 QFileInfo type 0x%x fid %d", infoLevel, fid);

    outp = smb_GetTran2ResponsePacket(vcp, p, opx, 2, responseSize);

    if (infoLevel > 0x100)
        outp->totalParms = 2;
    else
        outp->totalParms = 0;
    outp->totalData = responseSize;

    userp = smb_GetTran2User(vcp, p);
    if (!userp) {
    	osi_Log1(smb_logp, "ReceiveTran2QFileInfo unable to resolve user [%d]", p->uid);
    	code = CM_ERROR_BADSMB;
    	goto done;
    }   

    lock_ObtainMutex(&fidp->mx);
    delonclose = fidp->flags & SMB_FID_DELONCLOSE;
    scp = fidp->scp;
    osi_Log2(smb_logp,"smb_ReleaseTran2QFileInfo fidp 0x%p scp 0x%p", fidp, scp);
    cm_HoldSCache(scp);
    lock_ReleaseMutex(&fidp->mx);
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) 
        goto done;

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    /* now we have the status in the cache entry, and everything is locked.
     * Marshall the output data.
     */
    if (infoLevel == SMB_QUERY_FILE_BASIC_INFO) {
        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        qfi.u.QFbasicInfo.creationTime = ft;
        qfi.u.QFbasicInfo.lastAccessTime = ft;
        qfi.u.QFbasicInfo.lastWriteTime = ft;
        qfi.u.QFbasicInfo.lastChangeTime = ft;
        attributes = smb_ExtAttributes(scp);
        qfi.u.QFbasicInfo.attributes = attributes;
    }
    else if (infoLevel == SMB_QUERY_FILE_STANDARD_INFO) {
	qfi.u.QFstandardInfo.allocationSize = scp->length;
	qfi.u.QFstandardInfo.endOfFile = scp->length;
        qfi.u.QFstandardInfo.numberOfLinks = scp->linkCount;
        qfi.u.QFstandardInfo.deletePending = (delonclose ? 1 : 0);
        qfi.u.QFstandardInfo.directory = 
	    ((scp->fileType == CM_SCACHETYPE_DIRECTORY ||
	      scp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
	      scp->fileType == CM_SCACHETYPE_INVALID)? 1 : 0);
    }
    else if (infoLevel == SMB_QUERY_FILE_EA_INFO) {
        qfi.u.QFeaInfo.eaSize = 0;
    }
    else if (infoLevel == SMB_QUERY_FILE_NAME_INFO) {
        unsigned long len;
        char *name;

	lock_ReleaseMutex(&scp->mx);
	lock_ObtainMutex(&fidp->mx);
	lock_ObtainMutex(&scp->mx);
        if (fidp->NTopen_wholepathp)
            name = fidp->NTopen_wholepathp;
        else
            name = "\\";	/* probably can't happen */
	lock_ReleaseMutex(&fidp->mx);
        len = (unsigned long)strlen(name);
        outp->totalData = ((len+1)*2) + 4;	/* this is actually what we want to return */
        qfi.u.QFfileNameInfo.fileNameLength = (len + 1) * 2;
        mbstowcs((unsigned short *)qfi.u.QFfileNameInfo.fileName, name, len + 1);
    }

    /* send and free the packets */
  done:
    lock_ReleaseMutex(&scp->mx);
    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);
    if (code == 0) {
	memcpy(outp->datap, &qfi, responseSize);
        smb_SendTran2Packet(vcp, outp, opx);
    } else {
        smb_SendTran2Error(vcp, p, opx, code);
    }
    smb_FreeTran2Packet(outp);

    return 0;
}       

long smb_ReceiveTran2SetFileInfo(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
    long code = 0;
    unsigned short fid;
    smb_fid_t *fidp;
    unsigned short infoLevel;
    smb_tran2Packet_t *outp;
    cm_user_t *userp = NULL;
    cm_scache_t *scp = NULL;
    cm_req_t req;

    cm_InitReq(&req);

    fid = p->parmsp[0];
    fidp = smb_FindFID(vcp, fid, 0);

    if (fidp == NULL) {
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_BADFD);
        return 0;
    }

    infoLevel = p->parmsp[1];
    osi_Log2(smb_logp,"ReceiveTran2SetFileInfo type 0x%x fid %d", infoLevel, fid);
    if (infoLevel > SMB_SET_FILE_END_OF_FILE_INFO || infoLevel < SMB_SET_FILE_BASIC_INFO) {
        osi_Log2(smb_logp, "Bad Tran2 op 0x%x infolevel 0x%x",
                  p->opcode, infoLevel);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_BAD_LEVEL);
        smb_ReleaseFID(fidp);
        return 0;
    }

    lock_ObtainMutex(&fidp->mx);
    if (infoLevel == SMB_SET_FILE_DISPOSITION_INFO && 
	!(fidp->flags & SMB_FID_OPENDELETE)) {
	osi_Log3(smb_logp,"smb_ReceiveTran2SetFileInfo !SMB_FID_OPENDELETE fidp 0x%p scp 0x%p fidp->flags 0x%x", 
		  fidp, fidp->scp, fidp->flags);
	lock_ReleaseMutex(&fidp->mx);
        smb_ReleaseFID(fidp);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_NOACCESS);
        return 0;
    }
    if ((infoLevel == SMB_SET_FILE_ALLOCATION_INFO || 
	 infoLevel == SMB_SET_FILE_END_OF_FILE_INFO)
         && !(fidp->flags & SMB_FID_OPENWRITE)) {
	osi_Log3(smb_logp,"smb_ReceiveTran2SetFileInfo !SMB_FID_OPENWRITE fidp 0x%p scp 0x%p fidp->flags 0x%x", 
		  fidp, fidp->scp, fidp->flags);
	lock_ReleaseMutex(&fidp->mx);
        smb_ReleaseFID(fidp);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_NOACCESS);
        return 0;
    }

    scp = fidp->scp;
    osi_Log2(smb_logp,"smb_ReceiveTran2SetFileInfo fidp 0x%p scp 0x%p", fidp, scp);
    cm_HoldSCache(scp);
    lock_ReleaseMutex(&fidp->mx);

    outp = smb_GetTran2ResponsePacket(vcp, p, opx, 2, 0);

    outp->totalParms = 2;
    outp->totalData = 0;

    userp = smb_GetTran2User(vcp, p);
    if (!userp) {
    	osi_Log1(smb_logp,"ReceiveTran2SetFileInfo unable to resolve user [%d]", p->uid);
    	code = CM_ERROR_BADSMB;
    	goto done;
    }   

    if (infoLevel == SMB_SET_FILE_BASIC_INFO) {
        FILETIME lastMod;
        unsigned int attribute;
        cm_attr_t attr;
	smb_tran2QFileInfo_t *sfi;

	sfi = (smb_tran2QFileInfo_t *)p->datap;

	/* lock the vnode with a callback; we need the current status
         * to determine what the new status is, in some cases.
         */
        lock_ObtainMutex(&scp->mx);
        code = cm_SyncOp(scp, NULL, userp, &req, 0,
                          CM_SCACHESYNC_GETSTATUS
                         | CM_SCACHESYNC_NEEDCALLBACK);
        if (code) {
	    lock_ReleaseMutex(&scp->mx);
            goto done;
	}

	cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

	lock_ReleaseMutex(&scp->mx);
	lock_ObtainMutex(&fidp->mx);
	lock_ObtainMutex(&scp->mx);

        /* prepare for setattr call */
        attr.mask = 0;

        lastMod = sfi->u.QFbasicInfo.lastWriteTime;
        /* when called as result of move a b, lastMod is (-1, -1). 
         * If the check for -1 is not present, timestamp
         * of the resulting file will be 1969 (-1)
         */
        if (LargeIntegerNotEqualToZero(*((LARGE_INTEGER *)&lastMod)) && 
             lastMod.dwLowDateTime != -1 && lastMod.dwHighDateTime != -1) {
            attr.mask |= CM_ATTRMASK_CLIENTMODTIME;
            smb_UnixTimeFromLargeSearchTime(&attr.clientModTime, &lastMod);
            fidp->flags |= SMB_FID_MTIMESETDONE;
        }
		
        attribute = sfi->u.QFbasicInfo.attributes;
        if (attribute != 0) {
            if ((scp->unixModeBits & 0222)
                 && (attribute & SMB_ATTR_READONLY) != 0) {
                /* make a writable file read-only */
                attr.mask |= CM_ATTRMASK_UNIXMODEBITS;
                attr.unixModeBits = scp->unixModeBits & ~0222;
            }
            else if ((scp->unixModeBits & 0222) == 0
                      && (attribute & SMB_ATTR_READONLY) == 0) {
                /* make a read-only file writable */
                attr.mask |= CM_ATTRMASK_UNIXMODEBITS;
                attr.unixModeBits = scp->unixModeBits | 0222;
            }
        }
        lock_ReleaseMutex(&scp->mx);
	lock_ReleaseMutex(&fidp->mx);

        /* call setattr */
        if (attr.mask)
            code = cm_SetAttr(scp, &attr, userp, &req);
        else
            code = 0;
    }
    else if (infoLevel == SMB_SET_FILE_DISPOSITION_INFO) {
	int delflag = *((char *)(p->datap));
	osi_Log3(smb_logp,"smb_ReceiveTran2SetFileInfo Delete? %d fidp 0x%p scp 0x%p", 
		 delflag, fidp, scp);
        if (*((char *)(p->datap))) {	/* File is Deleted */
            code = cm_CheckNTDelete(fidp->NTopen_dscp, scp, userp,
                                     &req);
            if (code == 0) {
		lock_ObtainMutex(&fidp->mx);
                fidp->flags |= SMB_FID_DELONCLOSE;
		lock_ReleaseMutex(&fidp->mx);
	    } else {
		osi_Log3(smb_logp,"smb_ReceiveTran2SetFileInfo CheckNTDelete fidp 0x%p scp 0x%p code 0x%x", 
			 fidp, scp, code);
	    }
	}               
        else {  
            code = 0;
	    lock_ObtainMutex(&fidp->mx);
            fidp->flags &= ~SMB_FID_DELONCLOSE;
	    lock_ReleaseMutex(&fidp->mx);
        }
    }       
    else if (infoLevel == SMB_SET_FILE_ALLOCATION_INFO ||
	     infoLevel == SMB_SET_FILE_END_OF_FILE_INFO) {
        LARGE_INTEGER size = *((LARGE_INTEGER *)(p->datap));
        cm_attr_t attr;

        attr.mask = CM_ATTRMASK_LENGTH;
        attr.length.LowPart = size.LowPart;
        attr.length.HighPart = size.HighPart;
        code = cm_SetAttr(scp, &attr, userp, &req);
    }       

  done:
    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);
    if (code == 0) 
        smb_SendTran2Packet(vcp, outp, opx);
    else 
        smb_SendTran2Error(vcp, p, opx, code);
    smb_FreeTran2Packet(outp);

    return 0;
}

long 
smb_ReceiveTran2FSCTL(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
    osi_Log0(smb_logp,"ReceiveTran2FSCTL - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
}

long 
smb_ReceiveTran2IOCTL(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
    osi_Log0(smb_logp,"ReceiveTran2IOCTL - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
}

long 
smb_ReceiveTran2FindNotifyFirst(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
    osi_Log0(smb_logp,"ReceiveTran2FindNotifyFirst - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
}

long 
smb_ReceiveTran2FindNotifyNext(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
    osi_Log0(smb_logp,"ReceiveTran2FindNotifyNext - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
}

long 
smb_ReceiveTran2CreateDirectory(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
    osi_Log0(smb_logp,"ReceiveTran2CreateDirectory - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
}

long 
smb_ReceiveTran2SessionSetup(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
    osi_Log0(smb_logp,"ReceiveTran2SessionSetup - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
}

struct smb_v2_referral {
    USHORT ServerType;
    USHORT ReferralFlags;
    ULONG  Proximity;
    ULONG  TimeToLive;
    USHORT DfsPathOffset;
    USHORT DfsAlternativePathOffset;
    USHORT NetworkAddressOffset;
};

long 
smb_ReceiveTran2GetDFSReferral(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *op)
{
    /* This is a UNICODE only request (bit15 of Flags2) */
    /* The TID must be IPC$ */

    /* The documentation for the Flags response field is contradictory */

    /* Use Version 1 Referral Element Format */
    /* ServerType = 0; indicates the next server should be queried for the file */
    /* ReferralFlags = 0x01; PathConsumed characters should be stripped */
    /* Node = UnicodeString of UNC path of the next share name */
#ifdef DFS_SUPPORT
    long code = 0;
    int maxReferralLevel = 0;
    char requestFileName[1024] = "";
    smb_tran2Packet_t *outp = 0;
    cm_user_t *userp = 0;
    cm_req_t req;
    CPINFO CodePageInfo;
    int i, nbnLen, reqLen;
    int idx;

    cm_InitReq(&req);

    maxReferralLevel = p->parmsp[0];

    GetCPInfo(CP_ACP, &CodePageInfo);
    WideCharToMultiByte(CP_ACP, 0, (LPCWSTR) &p->parmsp[1], -1, 
                        requestFileName, 1024, NULL, NULL);

    osi_Log2(smb_logp,"ReceiveTran2GetDfsReferral [%d][%s]", 
             maxReferralLevel, osi_LogSaveString(smb_logp, requestFileName));

    nbnLen = strlen(cm_NetbiosName);
    reqLen = strlen(requestFileName);

    if (reqLen == nbnLen + 5 &&
        requestFileName[0] == '\\' &&
        !_strnicmp(cm_NetbiosName,&requestFileName[1],nbnLen) &&
        requestFileName[nbnLen+1] == '\\' &&
        (!_strnicmp("all",&requestFileName[nbnLen+2],3) || 
	  !_strnicmp("*.",&requestFileName[nbnLen+2],2)))
    {
        USHORT * sp;
        struct smb_v2_referral * v2ref;
        outp = smb_GetTran2ResponsePacket(vcp, p, op, 0, 2 * (reqLen + 8));

        sp = (USHORT *)outp->datap;
        idx = 0;
        sp[idx++] = reqLen;   /* path consumed */
        sp[idx++] = 1;        /* number of referrals */
        sp[idx++] = 0x03;     /* flags */
#ifdef DFS_VERSION_1
        sp[idx++] = 1;        /* Version Number */
        sp[idx++] = reqLen + 4;  /* Referral Size */ 
        sp[idx++] = 1;        /* Type = SMB Server */
        sp[idx++] = 0;        /* Do not strip path consumed */
        for ( i=0;i<=reqLen; i++ )
            sp[i+idx] = requestFileName[i];
#else /* DFS_VERSION_2 */
        sp[idx++] = 2;      /* Version Number */
        sp[idx++] = sizeof(struct smb_v2_referral);     /* Referral Size */
        idx += (sizeof(struct smb_v2_referral) / 2);
        v2ref = (struct smb_v2_referral *) &sp[5];
        v2ref->ServerType = 1;  /* SMB Server */
        v2ref->ReferralFlags = 0x03;
        v2ref->Proximity = 0;   /* closest */
        v2ref->TimeToLive = 3600; /* seconds */
        v2ref->DfsPathOffset = idx * 2;
        v2ref->DfsAlternativePathOffset = idx * 2;
        v2ref->NetworkAddressOffset = 0;
        for ( i=0;i<=reqLen; i++ )
            sp[i+idx] = requestFileName[i];
#endif
    } else {
        userp = smb_GetTran2User(vcp, p);
        if (!userp) {
            osi_Log1(smb_logp,"ReceiveTran2GetDfsReferral unable to resolve user [%d]", p->uid);
            code = CM_ERROR_BADSMB;
            goto done;
        }   

		/* not done yet */
        code = CM_ERROR_NOSUCHPATH;
    }

  done:
    if (userp)
        cm_ReleaseUser(userp);
    if (code == 0) 
        smb_SendTran2Packet(vcp, outp, op);
    else 
        smb_SendTran2Error(vcp, p, op, code);
    if (outp)
        smb_FreeTran2Packet(outp);
 
    return 0;
#else /* DFS_SUPPORT */
    osi_Log0(smb_logp,"ReceiveTran2GetDfsReferral - NOT_SUPPORTED"); 
    return CM_ERROR_BADOP;
#endif /* DFS_SUPPORT */
}

long 
smb_ReceiveTran2ReportDFSInconsistency(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *outp)
{
    /* This is a UNICODE only request (bit15 of Flags2) */

    /* There is nothing we can do about this operation.  The client is going to
     * tell us that there is a Version 1 Referral Element for which there is a DFS Error.
     * Unfortunately, there is really nothing we can do about it other then log it 
     * somewhere.  Even then I don't think there is anything for us to do.
     * So let's return an error value.
     */

    osi_Log0(smb_logp,"ReceiveTran2ReportDFSInconsistency - NOT_SUPPORTED");
    return CM_ERROR_BADOP;
}

long 
smb_ApplyV3DirListPatches(cm_scache_t *dscp,
	smb_dirListPatch_t **dirPatchespp, int infoLevel, cm_user_t *userp,
	cm_req_t *reqp)
{
    long code = 0;
    cm_scache_t *scp;
    cm_scache_t *targetScp;			/* target if scp is a symlink */
    char *dptr;
    afs_uint32 dosTime;
    FILETIME ft;
    int shortTemp;
    unsigned short attr;
    unsigned long lattr;
    smb_dirListPatch_t *patchp;
    smb_dirListPatch_t *npatchp;
    afs_uint32 rights;
    afs_int32 mustFake = 0;

    code = cm_FindACLCache(dscp, userp, &rights);
    if (code == 0 && !(rights & PRSFS_READ))
        mustFake = 1;
    else if (code == -1) {
        lock_ObtainMutex(&dscp->mx);
        code = cm_SyncOp(dscp, NULL, userp, reqp, PRSFS_READ,
                          CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        lock_ReleaseMutex(&dscp->mx);
        if (code == CM_ERROR_NOACCESS) {
            mustFake = 1;
            code = 0;
        }
    }
    if (code)
        return code;

    for(patchp = *dirPatchespp; patchp; patchp =
         (smb_dirListPatch_t *) osi_QNext(&patchp->q)) {
        code = cm_GetSCache(&patchp->fid, &scp, userp, reqp);
        if (code) 
            continue;

        lock_ObtainMutex(&scp->mx);
        if (mustFake == 0)
            code = cm_SyncOp(scp, NULL, userp, reqp, 0,
                             CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
        if (mustFake || code) { 
            lock_ReleaseMutex(&scp->mx);

            dptr = patchp->dptr;

            /* Plug in fake timestamps. A time stamp of 0 causes 'invalid parameter'
               errors in the client. */
            if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO) {
                /* 1969-12-31 23:59:59 +00 */
                ft.dwHighDateTime = 0x19DB200;
                ft.dwLowDateTime = 0x5BB78980;

                /* copy to Creation Time */
                *((FILETIME *)dptr) = ft;
                dptr += 8;

                /* copy to Last Access Time */
                *((FILETIME *)dptr) = ft;
                dptr += 8;

                /* copy to Last Write Time */
                *((FILETIME *)dptr) = ft;
                dptr += 8;

                /* copy to Change Time */
                *((FILETIME *)dptr) = ft;
                dptr += 24;

                switch (scp->fileType) {
                case CM_SCACHETYPE_DIRECTORY:
                case CM_SCACHETYPE_MOUNTPOINT:
                case CM_SCACHETYPE_SYMLINK:
                case CM_SCACHETYPE_INVALID:
                    *((u_long *)dptr) = SMB_ATTR_DIRECTORY;
                    break;
                default:
                    /* if we get here we either have a normal file
                     * or we have a file for which we have never 
                     * received status info.  In this case, we can
                     * check the even/odd value of the entry's vnode.
                     * even means it is to be treated as a directory
                     * and odd means it is to be treated as a file.
                     */
                    if (mustFake && (scp->fid.vnode % 2 == 0))
                        *((u_long *)dptr) = SMB_ATTR_DIRECTORY;
                    else
                        *((u_long *)dptr) = SMB_ATTR_NORMAL;
                        
                }
                /* merge in hidden attribute */
                if ( patchp->flags & SMB_DIRLISTPATCH_DOTFILE ) {
                    *((u_long *)dptr) |= SMB_ATTR_HIDDEN;
                }
                dptr += 4;
            } else {
                /* 1969-12-31 23:59:58 +00*/
                dosTime = 0xEBBFBF7D;

                /* and copy out date */
                shortTemp = (dosTime>>16) & 0xffff;
                *((u_short *)dptr) = shortTemp;
                dptr += 2;

                /* copy out creation time */
                shortTemp = dosTime & 0xffff;
                *((u_short *)dptr) = shortTemp;
                dptr += 2;

                /* and copy out date */
                shortTemp = (dosTime>>16) & 0xffff;
                *((u_short *)dptr) = shortTemp;
                dptr += 2;
    			
                /* copy out access time */
                shortTemp = dosTime & 0xffff;
                *((u_short *)dptr) = shortTemp;
                dptr += 2;

                /* and copy out date */
                shortTemp = (dosTime>>16) & 0xffff;
                *((u_short *)dptr) = shortTemp;
                dptr += 2;
    			
                /* copy out mod time */
                shortTemp = dosTime & 0xffff;
                *((u_short *)dptr) = shortTemp;
                dptr += 10;

                /* set the attribute */
                switch (scp->fileType) {
                case CM_SCACHETYPE_DIRECTORY:
                case CM_SCACHETYPE_MOUNTPOINT:
                case CM_SCACHETYPE_SYMLINK:
                case CM_SCACHETYPE_INVALID:
                    attr = SMB_ATTR_DIRECTORY;
                default:
                    attr = SMB_ATTR_NORMAL;
                }
                /* merge in hidden (dot file) attribute */
                if ( patchp->flags & SMB_DIRLISTPATCH_DOTFILE ) {
                    attr |= SMB_ATTR_HIDDEN;
                }       
                *dptr++ = attr & 0xff;
                *dptr++ = (attr >> 8) & 0xff;
            }
            
            cm_ReleaseSCache(scp);
            continue;
        }
        
	cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

        /* now watch for a symlink */
        code = 0;
        while (code == 0 && scp->fileType == CM_SCACHETYPE_SYMLINK) {
            lock_ReleaseMutex(&scp->mx);
            code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, reqp);
            if (code == 0) {
                /* we have a more accurate file to use (the
                 * target of the symbolic link).  Otherwise,
                 * we'll just use the symlink anyway.
                 */
                osi_Log2(smb_logp, "symlink vp %x to vp %x",
                          scp, targetScp);
                cm_ReleaseSCache(scp);
                scp = targetScp;
            }
            lock_ObtainMutex(&scp->mx);
        }

        dptr = patchp->dptr;

        if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO) {
            /* get filetime */
            smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);

            /* copy to Creation Time */
            *((FILETIME *)dptr) = ft;
            dptr += 8;

            /* copy to Last Access Time */
            *((FILETIME *)dptr) = ft;
            dptr += 8;

            /* copy to Last Write Time */
            *((FILETIME *)dptr) = ft;
            dptr += 8;

            /* copy to Change Time */
            *((FILETIME *)dptr) = ft;
            dptr += 8;

            /* Use length for both file length and alloc length */
            *((LARGE_INTEGER *)dptr) = scp->length;
            dptr += 8;
            *((LARGE_INTEGER *)dptr) = scp->length;
            dptr += 8;

            /* Copy attributes */
            lattr = smb_ExtAttributes(scp);
            if (code == CM_ERROR_NOSUCHPATH && scp->fileType == CM_SCACHETYPE_SYMLINK) {
                if (lattr == SMB_ATTR_NORMAL)
                    lattr = SMB_ATTR_DIRECTORY;
                else
                    lattr |= SMB_ATTR_DIRECTORY;
            }
            /* merge in hidden (dot file) attribute */
            if ( patchp->flags & SMB_DIRLISTPATCH_DOTFILE ) {
                if (lattr == SMB_ATTR_NORMAL)
                    lattr = SMB_ATTR_HIDDEN;
                else
                    lattr |= SMB_ATTR_HIDDEN;
            }
            *((u_long *)dptr) = lattr;
            dptr += 4;
        } else {
            /* get dos time */
            smb_SearchTimeFromUnixTime(&dosTime, scp->clientModTime);

            /* and copy out date */
            shortTemp = (dosTime>>16) & 0xffff;
            *((u_short *)dptr) = shortTemp;
            dptr += 2;

            /* copy out creation time */
            shortTemp = dosTime & 0xffff;
            *((u_short *)dptr) = shortTemp;
            dptr += 2;

            /* and copy out date */
            shortTemp = (dosTime>>16) & 0xffff;
            *((u_short *)dptr) = shortTemp;
            dptr += 2;

            /* copy out access time */
            shortTemp = dosTime & 0xffff;
            *((u_short *)dptr) = shortTemp;
            dptr += 2;

            /* and copy out date */
            shortTemp = (dosTime>>16) & 0xffff;
            *((u_short *)dptr) = shortTemp;
            dptr += 2;

            /* copy out mod time */
            shortTemp = dosTime & 0xffff;
            *((u_short *)dptr) = shortTemp;
            dptr += 2;

            /* copy out file length and alloc length,
             * using the same for both
             */
            *((u_long *)dptr) = scp->length.LowPart;
            dptr += 4;
            *((u_long *)dptr) = scp->length.LowPart;
            dptr += 4;

            /* finally copy out attributes as short */
            attr = smb_Attributes(scp);
            /* merge in hidden (dot file) attribute */
            if ( patchp->flags & SMB_DIRLISTPATCH_DOTFILE ) {
                if (lattr == SMB_ATTR_NORMAL)
                    lattr = SMB_ATTR_HIDDEN;
                else
                    lattr |= SMB_ATTR_HIDDEN;
            }
            *dptr++ = attr & 0xff;
            *dptr++ = (attr >> 8) & 0xff;
        }

        lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
    }
        
    /* now free the patches */
    for (patchp = *dirPatchespp; patchp; patchp = npatchp) {
        npatchp = (smb_dirListPatch_t *) osi_QNext(&patchp->q);
        free(patchp);
    }
        
    /* and mark the list as empty */
    *dirPatchespp = NULL;

    return code;
}

#ifndef USE_OLD_MATCHING
// char table for case insensitive comparison
char mapCaseTable[256];

VOID initUpperCaseTable(VOID) 
{
    int i;
    for (i = 0; i < 256; ++i) 
       mapCaseTable[i] = toupper(i);
    // make '"' match '.' 
    mapCaseTable[(int)'"'] = toupper('.');
    // make '<' match '*' 
    mapCaseTable[(int)'<'] = toupper('*');
    // make '>' match '?' 
    mapCaseTable[(int)'>'] = toupper('?');    
}

// Compare 'pattern' (containing metacharacters '*' and '?') with the file
// name 'name'.
// Note : this procedure works recursively calling itself.
// Parameters
// PSZ pattern    : string containing metacharacters.
// PSZ name       : file name to be compared with 'pattern'.
// Return value
// BOOL : TRUE/FALSE (match/mistmatch)

BOOL 
szWildCardMatchFileName(PSZ pattern, PSZ name, int casefold) 
{
    PSZ pename;         // points to the last 'name' character
    PSZ p;
    pename = name + strlen(name) - 1;
    while (*name) {
        switch (*pattern) {
        case '?':
	    ++pattern;
            if (*name == '.')
		continue;
            ++name;
            break;
         case '*':
            ++pattern;
            if (*pattern == '\0')
                return TRUE;
            for (p = pename; p >= name; --p) {
                if ((casefold && (mapCaseTable[*p] == mapCaseTable[*pattern]) ||
                     !casefold && (*p == *pattern)) &&
                     szWildCardMatchFileName(pattern + 1, p + 1, casefold))
                    return TRUE;
            } /* endfor */
            return FALSE;
        default:
            if ((casefold && mapCaseTable[*name] != mapCaseTable[*pattern]) ||
                (!casefold && *name != *pattern))
                return FALSE;
            ++pattern, ++name;
            break;
        } /* endswitch */
    } /* endwhile */ 

    /* if all we have left are wildcards, then we match */
    for (;*pattern; pattern++) {
	if (*pattern != '*' && *pattern != '?')
	    return FALSE;
    }
    return TRUE;
}

/* do a case-folding search of the star name mask with the name in namep.
 * Return 1 if we match, otherwise 0.
 */
int smb_V3MatchMask(char *namep, char *maskp, int flags) 
{
    char * newmask;
    int    i, j, star, qmark, casefold, retval;

    /* make sure we only match 8.3 names, if requested */
    if ((flags & CM_FLAG_8DOT3) && !cm_Is8Dot3(namep)) 
        return 0;
    
    casefold = (flags & CM_FLAG_CASEFOLD) ? 1 : 0;

    /* optimize the pattern:
     * if there is a mixture of '?' and '*',
     * for example  the sequence "*?*?*?*"
     * must be turned into the form "*"
     */
    newmask = (char *)malloc(strlen(maskp)+1);
    for ( i=0, j=0, star=0, qmark=0; maskp[i]; i++) {
        switch ( maskp[i] ) {
        case '?':
        case '>':
            qmark++;
            break;
        case '<':
        case '*':
            star++;
            break;
        default:
            if ( star ) {
                newmask[j++] = '*';
            } else if ( qmark ) {
                while ( qmark-- )
                    newmask[j++] = '?';
            }
            newmask[j++] = maskp[i];
            star = 0;
            qmark = 0;
        }
    }
    if ( star ) {
        newmask[j++] = '*';
    } else if ( qmark ) {
        while ( qmark-- )
            newmask[j++] = '?';
    }
    newmask[j++] = '\0';

    retval = szWildCardMatchFileName(newmask, namep, casefold) ? 1:0;

    free(newmask);
    return retval;
}

#else /* USE_OLD_MATCHING */
/* do a case-folding search of the star name mask with the name in namep.
 * Return 1 if we match, otherwise 0.
 */
int smb_V3MatchMask(char *namep, char *maskp, int flags)
{
    unsigned char tcp1, tcp2;	/* Pattern characters */
    unsigned char tcn1;		/* Name characters */
    int sawDot = 0, sawStar = 0, req8dot3 = 0;
    char *starNamep, *starMaskp;
    static char nullCharp[] = {0};
    int casefold = flags & CM_FLAG_CASEFOLD;

    /* make sure we only match 8.3 names, if requested */
    req8dot3 = (flags & CM_FLAG_8DOT3);
    if (req8dot3 && !cm_Is8Dot3(namep)) 
        return 0;

    /* loop */
    while (1) {
        /* Next pattern character */
        tcp1 = *maskp++;

        /* Next name character */
        tcn1 = *namep;

        if (tcp1 == 0) {
            /* 0 - end of pattern */
            if (tcn1 == 0)
                return 1;
            else
                return 0;
        }
        else if (tcp1 == '.' || tcp1 == '"') {
            if (sawDot) {
                if (tcn1 == '.') {
                    namep++;
                    continue;
                } else
                    return 0;
            }
            else {
                /*
                 * first dot in pattern;
                 * must match dot or end of name
                 */
                sawDot = 1;
                if (tcn1 == 0)
                    continue;
                else if (tcn1 == '.') {
                    sawStar = 0;
                    namep++;
                    continue;
                }
                else
                    return 0;
            }
        }
        else if (tcp1 == '?') {
            if (tcn1 == 0 || tcn1 == '.')
                return 0;
            namep++;
            continue;
        }
        else if (tcp1 == '>') {
            if (tcn1 != 0 && tcn1 != '.')
                namep++;
            continue;
        }
        else if (tcp1 == '*' || tcp1 == '<') {
            tcp2 = *maskp++;
            if (tcp2 == 0)
                return 1;
            else if ((req8dot3 && tcp2 == '.') || tcp2 == '"') {
                while (req8dot3 && tcn1 != '.' && tcn1 != 0)
                    tcn1 = *++namep;
                if (tcn1 == 0) {
                    if (sawDot)
                        return 0;
                    else
                        continue;
                }
                else {
                    namep++;
                    continue;
                }
            }
            else {
                /*
                 * pattern character after '*' is not null or
                 * period.  If it is '?' or '>', we are not
                 * going to understand it.  If it is '*' or
                 * '<', we are going to skip over it.  None of
                 * these are likely, I hope.
                 */
                /* skip over '*' and '<' */
                while (tcp2 == '*' || tcp2 == '<')
                    tcp2 = *maskp++;

                /* skip over characters that don't match tcp2 */
                while (req8dot3 && tcn1 != '.' && tcn1 != 0 && 
                        ((casefold && cm_foldUpper[tcn1] != cm_foldUpper[tcp2]) || 
                          (!casefold && tcn1 != tcp2)))
                    tcn1 = *++namep;

                /* No match */
                if ((req8dot3 && tcn1 == '.') || tcn1 == 0)
                    return 0;

                /* Remember where we are */
                sawStar = 1;
                starMaskp = maskp;
                starNamep = namep;

                namep++;
                continue;
            }
        }
        else {
            /* tcp1 is not a wildcard */
            if ((casefold && cm_foldUpper[tcn1] == cm_foldUpper[tcp1]) || 
                 (!casefold && tcn1 == tcp1)) {
                /* they match */
                namep++;
                continue;
            }
            /* if trying to match a star pattern, go back */
            if (sawStar) {
                maskp = starMaskp - 2;
                namep = starNamep + 1;
                sawStar = 0;
                continue;
            }
            /* that's all */
            return 0;
        }
    }
}
#endif /* USE_OLD_MATCHING */

/* smb_ReceiveTran2SearchDir implements both 
 * Tran2_Find_First and Tran2_Find_Next
 */
#define TRAN2_FIND_FLAG_CLOSE_SEARCH		0x01
#define TRAN2_FIND_FLAG_CLOSE_SEARCH_IF_END	0x02
#define TRAN2_FIND_FLAG_RETURN_RESUME_KEYS	0x04
#define TRAN2_FIND_FLAG_CONTINUE_SEARCH		0x08
#define TRAN2_FIND_FLAG_BACKUP_INTENT		0x10

/* this is an optimized handler for T2SearchDir that handles the case
   where there are no wildcards in the search path.  I.e. an
   application is using FindFirst(Ex) to get information about a
   single file or directory.  It will attempt to do a single lookup.
   If that fails, then smb_ReceiveTran2SearchDir() will fall back to
   the usual mechanism. 
   
   This function will return either CM_ERROR_NOSUCHFILE or SUCCESS.
   */
long smb_T2SearchDirSingle(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
    int attribute;
    long nextCookie;
    long code = 0, code2 = 0;
    char *pathp;
    int maxCount;
    smb_dirListPatch_t *dirListPatchesp;
    smb_dirListPatch_t *curPatchp;
    long orbytes;			/* # of bytes in this output record */
    long ohbytes;			/* # of bytes, except file name */
    long onbytes;			/* # of bytes in name, incl. term. null */
    cm_scache_t *scp = NULL;
    cm_scache_t *targetscp = NULL;
    cm_user_t *userp = NULL;
    char *op;				/* output data ptr */
    char *origOp;			/* original value of op */
    cm_space_t *spacep;			/* for pathname buffer */
    long maxReturnData;			/* max # of return data */
    long maxReturnParms;		/* max # of return parms */
    long bytesInBuffer;			/* # data bytes in the output buffer */
    char *maskp;			/* mask part of path */
    int infoLevel;
    int searchFlags;
    int eos;
    smb_tran2Packet_t *outp;		/* response packet */
    char *tidPathp;
    int align;
    char shortName[13];			/* 8.3 name if needed */
    int NeedShortName;
    char *shortNameEnd;
    cm_req_t req;
    char * s;

    cm_InitReq(&req);

    eos = 0;
    osi_assert(p->opcode == 1);

    /* find first; obtain basic parameters from request */

    /* note that since we are going to failover to regular
     * processing at smb_ReceiveTran2SearchDir(), we shouldn't
     * modify any of the input parameters here. */
    attribute = p->parmsp[0];
    maxCount = p->parmsp[1];
    infoLevel = p->parmsp[3];
    searchFlags = p->parmsp[2];
    pathp = ((char *) p->parmsp) + 12;	/* points to path */
    nextCookie = 0;
    maskp = strrchr(pathp, '\\');
    if (maskp == NULL) 
	maskp = pathp;
    else 
	maskp++;	/* skip over backslash */
    /* track if this is likely to match a lot of entries */

    osi_Log2(smb_logp, "smb_T2SearchDirSingle : path[%s], mask[%s]",
            osi_LogSaveString(smb_logp, pathp),
            osi_LogSaveString(smb_logp, maskp));

    switch ( infoLevel ) {
    case SMB_INFO_STANDARD:
	s = "InfoStandard";
    	break;
    case SMB_INFO_QUERY_EA_SIZE:
	s = "InfoQueryEaSize";
    	break;
    case SMB_INFO_QUERY_EAS_FROM_LIST:
	s = "InfoQueryEasFromList";
    	break;
    case SMB_FIND_FILE_DIRECTORY_INFO:
	s = "FindFileDirectoryInfo";
    	break;
    case SMB_FIND_FILE_FULL_DIRECTORY_INFO:
	s = "FindFileFullDirectoryInfo";
    	break;
    case SMB_FIND_FILE_NAMES_INFO:
	s = "FindFileNamesInfo";
    	break;
    case SMB_FIND_FILE_BOTH_DIRECTORY_INFO:
	s = "FindFileBothDirectoryInfo";
    	break;
    default:
	s = "unknownInfoLevel";
    }

    osi_Log1(smb_logp, "smb_T2SearchDirSingle info level: %s", s);

    osi_Log4(smb_logp,
             "smb_T2SearchDirSingle attr 0x%x, info level 0x%x, max count %d, flags 0x%x",
             attribute, infoLevel, maxCount, searchFlags);
    
    if (infoLevel > SMB_FIND_FILE_BOTH_DIRECTORY_INFO) {
        osi_Log1(smb_logp, "Unsupported InfoLevel 0x%x", infoLevel);
        return CM_ERROR_INVAL;
    }

    if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO)
        searchFlags &= ~TRAN2_FIND_FLAG_RETURN_RESUME_KEYS;	/* no resume keys */

    dirListPatchesp = NULL;

    maxReturnData = p->maxReturnData;
    maxReturnParms = 10;	/* return params for findfirst, which
                                   is the only one we handle.*/

#ifndef CM_CONFIG_MULTITRAN2RESPONSES
    if (maxReturnData > 6000) 
        maxReturnData = 6000;
#endif /* CM_CONFIG_MULTITRAN2RESPONSES */

    outp = smb_GetTran2ResponsePacket(vcp, p, opx, maxReturnParms,
                                      maxReturnData);

    osi_Log2(smb_logp, "T2SDSingle search dir count %d [%s]",
             maxCount, osi_LogSaveString(smb_logp, pathp));
        
    /* bail out if request looks bad */
    if (!pathp) {
        smb_FreeTran2Packet(outp);
        return CM_ERROR_BADSMB;
    }
        
    userp = smb_GetTran2User(vcp, p);
    if (!userp) {
    	osi_Log1(smb_logp, "T2 search dir unable to resolve user [%d]", p->uid);
    	smb_FreeTran2Packet(outp);
    	return CM_ERROR_BADSMB;
    }

    /* try to get the vnode for the path name next */
    spacep = cm_GetSpace();
    smb_StripLastComponent(spacep->data, NULL, pathp);
    code = smb_LookupTIDPath(vcp, p->tid, &tidPathp);
    if (code) {
        cm_ReleaseUser(userp);
        smb_SendTran2Error(vcp, p, opx, CM_ERROR_NOFILES);
        smb_FreeTran2Packet(outp);
        return 0;
    }

    code = cm_NameI(cm_data.rootSCachep, spacep->data,
                    CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                    userp, tidPathp, &req, &scp);
    cm_FreeSpace(spacep);

    if (code) {
        cm_ReleaseUser(userp);
	smb_SendTran2Error(vcp, p, opx, code);
        smb_FreeTran2Packet(outp);
        return 0;
    }

#ifdef DFS_SUPPORT_BUT_NOT_FIND_FIRST
    if (scp->fileType == CM_SCACHETYPE_DFSLINK) {
	cm_ReleaseSCache(scp);
	cm_ReleaseUser(userp);
	if ( WANTS_DFS_PATHNAMES(p) )
	    code = CM_ERROR_PATH_NOT_COVERED;
	else
	    code = CM_ERROR_BADSHARENAME;
	smb_SendTran2Error(vcp, p, opx, code);
	smb_FreeTran2Packet(outp);
	return 0;
    }
#endif /* DFS_SUPPORT */
    osi_Log1(smb_logp,"smb_ReceiveTran2SearchDir scp 0x%p", scp);
    lock_ObtainMutex(&scp->mx);
    if ((scp->flags & CM_SCACHEFLAG_BULKSTATTING) == 0 &&
	 LargeIntegerGreaterOrEqualToZero(scp->bulkStatProgress)) {
	scp->flags |= CM_SCACHEFLAG_BULKSTATTING;
    }
    lock_ReleaseMutex(&scp->mx);

    /* now do a single case sensitive lookup for the file in question */
    code = cm_Lookup(scp, maskp, CM_FLAG_NOMOUNTCHASE, userp, &req, &targetscp);

    /* if a case sensitive match failed, we try a case insensitive one
       next. */
    if (code == CM_ERROR_NOSUCHFILE) {
        code = cm_Lookup(scp, maskp, CM_FLAG_NOMOUNTCHASE | CM_FLAG_CASEFOLD, userp, &req, &targetscp);
    }

    if (code == 0 && targetscp->fid.vnode == 0) {
        cm_ReleaseSCache(targetscp);
        code = CM_ERROR_NOSUCHFILE;
    }

    if (code) {
        /* if we can't find the directory entry, this block will
           return CM_ERROR_NOSUCHFILE, which we will pass on to
           smb_ReceiveTran2SearchDir(). */
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
	if (code != CM_ERROR_NOSUCHFILE) {
	    smb_SendTran2Error(vcp, p, opx, code);
	    code = 0;
	}
	smb_FreeTran2Packet(outp);
        return code;
    }

    /* now that we have the target in sight, we proceed with filling
       up the return data. */

    op = origOp = outp->datap;
    bytesInBuffer = 0;

    if (searchFlags & TRAN2_FIND_FLAG_RETURN_RESUME_KEYS) {
        /* skip over resume key */
        op += 4;
    }

    if (infoLevel == SMB_FIND_FILE_BOTH_DIRECTORY_INFO
        && targetscp->fid.vnode != 0
        && !cm_Is8Dot3(maskp)) {

        cm_dirFid_t dfid;
        dfid.vnode = htonl(targetscp->fid.vnode);
        dfid.unique = htonl(targetscp->fid.unique);

        cm_Gen8Dot3NameInt(maskp, &dfid, shortName, &shortNameEnd);
        NeedShortName = 1;
    } else {
        NeedShortName = 0;
    }

    osi_Log4(smb_logp, "T2SDSingle dir vn %u uniq %u name %s (%s)",
	      htonl(targetscp->fid.vnode),
	      htonl(targetscp->fid.unique),
	      osi_LogSaveString(smb_logp, pathp),
	      NeedShortName ? osi_LogSaveString(smb_logp, shortName) : "");

    /* Eliminate entries that don't match requested attributes */
    if (smb_hideDotFiles && !(attribute & SMB_ATTR_HIDDEN) &&
        smb_IsDotFile(maskp)) {

        code = CM_ERROR_NOSUCHFILE;
        osi_Log0(smb_logp, "T2SDSingle skipping hidden file");
        goto skip_file;

    }

    if (!(attribute & SMB_ATTR_DIRECTORY) &&
        (targetscp->fileType == CM_SCACHETYPE_DIRECTORY ||
         targetscp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
         targetscp->fileType == CM_SCACHETYPE_DFSLINK ||
         targetscp->fileType == CM_SCACHETYPE_INVALID)) {

        code = CM_ERROR_NOSUCHFILE;
        osi_Log0(smb_logp, "T2SDSingle skipping directory or bad link");
        goto skip_file;

    }

    /* Check if the name will fit */
    if (infoLevel < 0x101)
        ohbytes = 23;           /* pre-NT */
    else if (infoLevel == SMB_FIND_FILE_NAMES_INFO)
        ohbytes = 12;           /* NT names only */
    else
        ohbytes = 64;           /* NT */

    if (infoLevel == SMB_FIND_FILE_BOTH_DIRECTORY_INFO)
        ohbytes += 26;          /* Short name & length */

    if (searchFlags & TRAN2_FIND_FLAG_RETURN_RESUME_KEYS) {
        ohbytes += 4;           /* if resume key required */
    }

    if (infoLevel != SMB_INFO_STANDARD
        && infoLevel != SMB_FIND_FILE_DIRECTORY_INFO
        && infoLevel != SMB_FIND_FILE_NAMES_INFO)
        ohbytes += 4;           /* EASIZE */

    /* add header to name & term. null */
    onbytes = strlen(maskp);
    orbytes = ohbytes + onbytes + 1;

    /* now, we round up the record to a 4 byte alignment, and we make
     * sure that we have enough room here for even the aligned version
     * (so we don't have to worry about an * overflow when we pad
     * things out below).  That's the reason for the alignment
     * arithmetic below.
     */
    if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO)
        align = (4 - (orbytes & 3)) & 3;
    else
        align = 0;

    if (orbytes + align > maxReturnData) {

        /* even though this request is unlikely to succeed with a
           failover, we do it anyway. */
        code = CM_ERROR_NOSUCHFILE;
        osi_Log1(smb_logp, "T2 dir search exceed max return data %d",
                 maxReturnData);
        goto skip_file;
    }

    /* this is one of the entries to use: it is not deleted and it
     * matches the star pattern we're looking for.  Put out the name,
     * preceded by its length.
     */
    /* First zero everything else */
    memset(origOp, 0, ohbytes);

    if (infoLevel <= SMB_FIND_FILE_DIRECTORY_INFO)
        *(origOp + ohbytes - 1) = (unsigned char) onbytes;
    else if (infoLevel == SMB_FIND_FILE_NAMES_INFO)
        *((u_long *)(op + 8)) = onbytes;
    else
        *((u_long *)(op + 60)) = onbytes;
    strcpy(origOp+ohbytes, maskp);
    if (smb_StoreAnsiFilenames)
        CharToOem(origOp+ohbytes, origOp+ohbytes);

    /* Short name if requested and needed */
    if (infoLevel == SMB_FIND_FILE_BOTH_DIRECTORY_INFO) {
        if (NeedShortName) {
            strcpy(op + 70, shortName);
            if (smb_StoreAnsiFilenames)
                CharToOem(op + 70, op + 70);
            *(op + 68) = (char)(shortNameEnd - shortName);
        }
    }

    /* NextEntryOffset and FileIndex */
    if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO) {
        int entryOffset = orbytes + align;
        *((u_long *)op) = 0;
        *((u_long *)(op+4)) = 0;
    }

    if (infoLevel != SMB_FIND_FILE_NAMES_INFO) {
        curPatchp = malloc(sizeof(*curPatchp));
        osi_QAdd((osi_queue_t **) &dirListPatchesp,
                 &curPatchp->q);
        curPatchp->dptr = op;
        if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO)
            curPatchp->dptr += 8;

        if (smb_hideDotFiles && smb_IsDotFile(maskp)) {
            curPatchp->flags = SMB_DIRLISTPATCH_DOTFILE;
        } else {
            curPatchp->flags = 0;
        }

        curPatchp->fid.cell = targetscp->fid.cell;
        curPatchp->fid.volume = targetscp->fid.volume;
        curPatchp->fid.vnode = targetscp->fid.vnode;
        curPatchp->fid.unique = targetscp->fid.unique;

        /* temp */
        curPatchp->dep = NULL;
    }   

    if (searchFlags & TRAN2_FIND_FLAG_RETURN_RESUME_KEYS) {
        /* put out resume key */
        *((u_long *)origOp) = 0;
    }

    /* Adjust byte ptr and count */
    origOp += orbytes;	/* skip entire record */
    bytesInBuffer += orbytes;

    /* and pad the record out */
    while (--align >= 0) {
        *origOp++ = 0;
        bytesInBuffer++;
    }

    /* apply the patches */
    code2 = smb_ApplyV3DirListPatches(scp, &dirListPatchesp, infoLevel, userp, &req);

    outp->parmsp[0] = 0;
    outp->parmsp[1] = 1;        /* number of names returned */
    outp->parmsp[2] = 1;        /* end of search */
    outp->parmsp[3] = 0;        /* nothing wrong with EAS */
    outp->parmsp[4] = 0;

    outp->totalParms = 10;      /* in bytes */

    outp->totalData = bytesInBuffer;

    osi_Log0(smb_logp, "T2SDSingle done.");

    if (code != CM_ERROR_NOSUCHFILE) {
	if (code)
	    smb_SendTran2Error(vcp, p, opx, code);
	else
	    smb_SendTran2Packet(vcp, outp, opx);
	code = 0;
    }

 skip_file:
    smb_FreeTran2Packet(outp);
    cm_ReleaseSCache(scp);
    cm_ReleaseSCache(targetscp);
    cm_ReleaseUser(userp);

    return code;
}


long smb_ReceiveTran2SearchDir(smb_vc_t *vcp, smb_tran2Packet_t *p, smb_packet_t *opx)
{
    int attribute;
    long nextCookie;
    char *tp;
    long code = 0, code2 = 0;
    char *pathp;
    cm_dirEntry_t *dep;
    int maxCount;
    smb_dirListPatch_t *dirListPatchesp;
    smb_dirListPatch_t *curPatchp;
    cm_buf_t *bufferp;
    long temp;
    long orbytes;			/* # of bytes in this output record */
    long ohbytes;			/* # of bytes, except file name */
    long onbytes;			/* # of bytes in name, incl. term. null */
    osi_hyper_t dirLength;
    osi_hyper_t bufferOffset;
    osi_hyper_t curOffset;
    osi_hyper_t thyper;
    smb_dirSearch_t *dsp;
    cm_scache_t *scp;
    long entryInDir;
    long entryInBuffer;
    cm_pageHeader_t *pageHeaderp;
    cm_user_t *userp = NULL;
    int slotInPage;
    int returnedNames;
    long nextEntryCookie;
    int numDirChunks;		/* # of 32 byte dir chunks in this entry */
    char *op;			/* output data ptr */
    char *origOp;			/* original value of op */
    cm_space_t *spacep;		/* for pathname buffer */
    long maxReturnData;		/* max # of return data */
    long maxReturnParms;		/* max # of return parms */
    long bytesInBuffer;		/* # data bytes in the output buffer */
    int starPattern;
    char *maskp;			/* mask part of path */
    int infoLevel;
    int searchFlags;
    int eos;
    smb_tran2Packet_t *outp;	/* response packet */
    char *tidPathp;
    int align;
    char shortName[13];		/* 8.3 name if needed */
    int NeedShortName;
    int foundInexact;
    char *shortNameEnd;
    int fileType;
    cm_fid_t fid;
    cm_req_t req;
    char * s;

    cm_InitReq(&req);

    eos = 0;
    if (p->opcode == 1) {
        /* find first; obtain basic parameters from request */
        attribute = p->parmsp[0];
        maxCount = p->parmsp[1];
        infoLevel = p->parmsp[3];
        searchFlags = p->parmsp[2];
        pathp = ((char *) p->parmsp) + 12;	/* points to path */
        if (smb_StoreAnsiFilenames)
            OemToChar(pathp,pathp);
        nextCookie = 0;
        maskp = strrchr(pathp, '\\');
        if (maskp == NULL) 
            maskp = pathp;
        else 
            maskp++;	/* skip over backslash */

        /* track if this is likely to match a lot of entries */
        starPattern = smb_V3IsStarMask(maskp);

#ifndef NOFINDFIRSTOPTIMIZE
        if (!starPattern) {
            /* if this is for a single directory or file, we let the
               optimized routine handle it.  The only error it 
	       returns is CM_ERROR_NOSUCHFILE.  The  */
            code = smb_T2SearchDirSingle(vcp, p, opx);

            /* we only failover if we see a CM_ERROR_NOSUCHFILE */
            if (code != CM_ERROR_NOSUCHFILE) {
                return code;
            }
        }
#endif

        dsp = smb_NewDirSearch(1);
        dsp->attribute = attribute;
        strcpy(dsp->mask, maskp);	/* and save mask */
    }
    else {
        osi_assert(p->opcode == 2);
        /* find next; obtain basic parameters from request or open dir file */
        dsp = smb_FindDirSearch(p->parmsp[0]);
        maxCount = p->parmsp[1];
        infoLevel = p->parmsp[2];
        nextCookie = p->parmsp[3] | (p->parmsp[4] << 16);
        searchFlags = p->parmsp[5];
        if (!dsp) {
            osi_Log2(smb_logp, "T2 search dir bad search ID: id %d nextCookie 0x%x",
                     p->parmsp[0], nextCookie);
            return CM_ERROR_BADFD;
        }
        attribute = dsp->attribute;
        pathp = NULL;
        maskp = dsp->mask;
        starPattern = 1;	/* assume, since required a Find Next */
    }

    switch ( infoLevel ) {
    case SMB_INFO_STANDARD:
	s = "InfoStandard";
    	break;
    case SMB_INFO_QUERY_EA_SIZE:
	s = "InfoQueryEaSize";
    	break;
    case SMB_INFO_QUERY_EAS_FROM_LIST:
	s = "InfoQueryEasFromList";
    	break;
    case SMB_FIND_FILE_DIRECTORY_INFO:
	s = "FindFileDirectoryInfo";
    	break;
    case SMB_FIND_FILE_FULL_DIRECTORY_INFO:
	s = "FindFileFullDirectoryInfo";
    	break;
    case SMB_FIND_FILE_NAMES_INFO:
	s = "FindFileNamesInfo";
    	break;
    case SMB_FIND_FILE_BOTH_DIRECTORY_INFO:
	s = "FindFileBothDirectoryInfo";
    	break;
    default:
	s = "unknownInfoLevel";
    }

    osi_Log1(smb_logp, "T2 search dir info level: %s", s);

    osi_Log4(smb_logp,
              "T2 search dir attr 0x%x, info level 0x%x, max count %d, flags 0x%x",
              attribute, infoLevel, maxCount, searchFlags);

    osi_Log3(smb_logp, "...T2 search op %d, id %d, nextCookie 0x%x",
              p->opcode, dsp->cookie, nextCookie);

    if (infoLevel > SMB_FIND_FILE_BOTH_DIRECTORY_INFO) {
        osi_Log1(smb_logp, "Unsupported InfoLevel 0x%x", infoLevel);
        smb_ReleaseDirSearch(dsp);
        return CM_ERROR_INVAL;
    }

    if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO)
        searchFlags &= ~TRAN2_FIND_FLAG_RETURN_RESUME_KEYS;	/* no resume keys */

    dirListPatchesp = NULL;

    maxReturnData = p->maxReturnData;
    if (p->opcode == 1)	/* find first */
        maxReturnParms = 10;	/* bytes */
    else    
        maxReturnParms = 8;	/* bytes */

#ifndef CM_CONFIG_MULTITRAN2RESPONSES
    if (maxReturnData > 6000) 
        maxReturnData = 6000;
#endif /* CM_CONFIG_MULTITRAN2RESPONSES */

    outp = smb_GetTran2ResponsePacket(vcp, p, opx, maxReturnParms,
                                      maxReturnData);

    osi_Log2(smb_logp, "T2 receive search dir count %d [%s]",
             maxCount, osi_LogSaveString(smb_logp, pathp));
        
    /* bail out if request looks bad */
    if (p->opcode == 1 && !pathp) {
        smb_ReleaseDirSearch(dsp);
        smb_FreeTran2Packet(outp);
        return CM_ERROR_BADSMB;
    }
        
    osi_Log3(smb_logp, "T2 search dir id %d, nextCookie 0x%x, attr 0x%x",
             dsp->cookie, nextCookie, attribute);

    userp = smb_GetTran2User(vcp, p);
    if (!userp) {
    	osi_Log1(smb_logp, "T2 search dir unable to resolve user [%d]", p->uid);
    	smb_ReleaseDirSearch(dsp);
    	smb_FreeTran2Packet(outp);
    	return CM_ERROR_BADSMB;
    }

    /* try to get the vnode for the path name next */
    lock_ObtainMutex(&dsp->mx);
    if (dsp->scp) {
        scp = dsp->scp;
	osi_Log2(smb_logp,"smb_ReceiveTran2SearchDir dsp 0x%p scp 0x%p", dsp, scp);
        cm_HoldSCache(scp);
        code = 0;
    } else {
        spacep = cm_GetSpace();
        smb_StripLastComponent(spacep->data, NULL, pathp);
        code = smb_LookupTIDPath(vcp, p->tid, &tidPathp);
        if (code) {
            cm_ReleaseUser(userp);
            smb_SendTran2Error(vcp, p, opx, CM_ERROR_NOFILES);
            smb_FreeTran2Packet(outp);
            lock_ReleaseMutex(&dsp->mx);
            smb_DeleteDirSearch(dsp);
            smb_ReleaseDirSearch(dsp);
            return 0;
        }
        code = cm_NameI(cm_data.rootSCachep, spacep->data,
                        CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                        userp, tidPathp, &req, &scp);
        cm_FreeSpace(spacep);

        if (code == 0) {
#ifdef DFS_SUPPORT_BUT_NOT_FIND_FIRST
            if (scp->fileType == CM_SCACHETYPE_DFSLINK) {
                cm_ReleaseSCache(scp);
                cm_ReleaseUser(userp);
                if ( WANTS_DFS_PATHNAMES(p) )
                    code = CM_ERROR_PATH_NOT_COVERED;
                else
                    code = CM_ERROR_BADSHARENAME;
                smb_SendTran2Error(vcp, p, opx, code);
                smb_FreeTran2Packet(outp);
                lock_ReleaseMutex(&dsp->mx);
                smb_DeleteDirSearch(dsp);
                smb_ReleaseDirSearch(dsp);
                return 0;
            }
#endif /* DFS_SUPPORT */
            dsp->scp = scp;
	    osi_Log2(smb_logp,"smb_ReceiveTran2SearchDir dsp 0x%p scp 0x%p", dsp, scp);
            /* we need one hold for the entry we just stored into,
             * and one for our own processing.  When we're done
             * with this function, we'll drop the one for our own
             * processing.  We held it once from the namei call,
             * and so we do another hold now.
             */
            cm_HoldSCache(scp);
            lock_ObtainMutex(&scp->mx);
            if ((scp->flags & CM_SCACHEFLAG_BULKSTATTING) == 0 &&
                 LargeIntegerGreaterOrEqualToZero(scp->bulkStatProgress)) {
                scp->flags |= CM_SCACHEFLAG_BULKSTATTING;
                dsp->flags |= SMB_DIRSEARCH_BULKST;
            }
            lock_ReleaseMutex(&scp->mx);
        } 
    }
    lock_ReleaseMutex(&dsp->mx);
    if (code) {
        cm_ReleaseUser(userp);
        smb_FreeTran2Packet(outp);
        smb_DeleteDirSearch(dsp);
        smb_ReleaseDirSearch(dsp);
        return code;
    }

    /* get the directory size */
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) {
        lock_ReleaseMutex(&scp->mx);
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        smb_FreeTran2Packet(outp);
        smb_DeleteDirSearch(dsp);
        smb_ReleaseDirSearch(dsp);
        return code;
    }

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

  startsearch:
    dirLength = scp->length;
    bufferp = NULL;
    bufferOffset.LowPart = bufferOffset.HighPart = 0;
    curOffset.HighPart = 0;
    curOffset.LowPart = nextCookie;
    origOp = outp->datap;

    foundInexact = 0;
    code = 0;
    returnedNames = 0;
    bytesInBuffer = 0;
    while (1) {
        op = origOp;
        if (searchFlags & TRAN2_FIND_FLAG_RETURN_RESUME_KEYS)
            /* skip over resume key */
            op += 4;

        /* make sure that curOffset.LowPart doesn't point to the first
         * 32 bytes in the 2nd through last dir page, and that it doesn't
         * point at the first 13 32-byte chunks in the first dir page,
         * since those are dir and page headers, and don't contain useful
         * information.
         */
        temp = curOffset.LowPart & (2048-1);
        if (curOffset.HighPart == 0 && curOffset.LowPart < 2048) {
            /* we're in the first page */
            if (temp < 13*32) temp = 13*32;
        }
        else {
            /* we're in a later dir page */
            if (temp < 32) temp = 32;
        }
		
        /* make sure the low order 5 bits are zero */
        temp &= ~(32-1);
                
        /* now put temp bits back ito curOffset.LowPart */
        curOffset.LowPart &= ~(2048-1);
        curOffset.LowPart |= temp;

        /* check if we've passed the dir's EOF */
        if (LargeIntegerGreaterThanOrEqualTo(curOffset, dirLength)) {
            osi_Log0(smb_logp, "T2 search dir passed eof");
            eos = 1;
            break;
        }

        /* check if we've returned all the names that will fit in the
         * response packet; we check return count as well as the number
         * of bytes requested.  We check the # of bytes after we find
         * the dir entry, since we'll need to check its size.
         */
        if (returnedNames >= maxCount) {
            osi_Log2(smb_logp, "T2 search dir returnedNames %d >= maxCount %d",
                      returnedNames, maxCount);
            break;
        }

        /* see if we can use the bufferp we have now; compute in which
         * page the current offset would be, and check whether that's
         * the offset of the buffer we have.  If not, get the buffer.
         */
        thyper.HighPart = curOffset.HighPart;
        thyper.LowPart = curOffset.LowPart & ~(cm_data.buf_blockSize-1);
        if (!bufferp || !LargeIntegerEqualTo(thyper, bufferOffset)) {
            /* wrong buffer */
            if (bufferp) {
                buf_Release(bufferp);
                bufferp = NULL;
            }       
            lock_ReleaseMutex(&scp->mx);
            lock_ObtainRead(&scp->bufCreateLock);
            code = buf_Get(scp, &thyper, &bufferp);
            lock_ReleaseRead(&scp->bufCreateLock);
            lock_ObtainMutex(&dsp->mx);

            /* now, if we're doing a star match, do bulk fetching
             * of all of the status info for files in the dir.
             */
            if (starPattern) {
                smb_ApplyV3DirListPatches(scp, &dirListPatchesp,
                                           infoLevel, userp,
                                           &req);
                lock_ObtainMutex(&scp->mx);
                if ((dsp->flags & SMB_DIRSEARCH_BULKST) &&
                    LargeIntegerGreaterThanOrEqualTo(thyper, scp->bulkStatProgress)) {
                    /* Don't bulk stat if risking timeout */
                    DWORD now = GetTickCount();
                    if (now - req.startTime > RDRtimeout) {
                        scp->bulkStatProgress = thyper;
                        scp->flags &= ~CM_SCACHEFLAG_BULKSTATTING;
                        dsp->flags &= ~SMB_DIRSEARCH_BULKST;
                    } else
                        code = cm_TryBulkStat(scp, &thyper, userp, &req);
                }
            } else {
                lock_ObtainMutex(&scp->mx);
            }
            lock_ReleaseMutex(&dsp->mx);
            if (code) {
                osi_Log2(smb_logp, "T2 search dir buf_Get scp %x failed %d", scp, code);
                break;
            }

            bufferOffset = thyper;

            /* now get the data in the cache */
            while (1) {
                code = cm_SyncOp(scp, bufferp, userp, &req,
                                 PRSFS_LOOKUP,
                                 CM_SCACHESYNC_NEEDCALLBACK
                                 | CM_SCACHESYNC_READ);
                if (code) {
                    osi_Log2(smb_logp, "T2 search dir cm_SyncOp scp %x failed %d", scp, code);
                    break;
                }
                       
		cm_SyncOpDone(scp, bufferp, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_READ);

                if (cm_HaveBuffer(scp, bufferp, 0)) {
                    osi_Log2(smb_logp, "T2 search dir !HaveBuffer scp %x bufferp %x", scp, bufferp);
                    break;
                }

                /* otherwise, load the buffer and try again */
                code = cm_GetBuffer(scp, bufferp, NULL, userp,
                                    &req);
                if (code) {
                    osi_Log3(smb_logp, "T2 search dir cm_GetBuffer failed scp %x bufferp %x code %d", 
                              scp, bufferp, code);
                    break;
                }
            }
            if (code) {
                buf_Release(bufferp);
                bufferp = NULL;
                break;
            }
        }	/* if (wrong buffer) ... */
                
        /* now we have the buffer containing the entry we're interested
         * in; copy it out if it represents a non-deleted entry.
         */
        entryInDir = curOffset.LowPart & (2048-1);
        entryInBuffer = curOffset.LowPart & (cm_data.buf_blockSize - 1);

        /* page header will help tell us which entries are free.  Page
         * header can change more often than once per buffer, since
         * AFS 3 dir page size may be less than (but not more than)
         * a buffer package buffer.
         */
        /* only look intra-buffer */
        temp = curOffset.LowPart & (cm_data.buf_blockSize - 1);
        temp &= ~(2048 - 1);	/* turn off intra-page bits */
        pageHeaderp = (cm_pageHeader_t *) (bufferp->datap + temp);

        /* now determine which entry we're looking at in the page.
         * If it is free (there's a free bitmap at the start of the
         * dir), we should skip these 32 bytes.
         */
        slotInPage = (entryInDir & 0x7e0) >> 5;
        if (!(pageHeaderp->freeBitmap[slotInPage>>3] &
            (1 << (slotInPage & 0x7)))) {
            /* this entry is free */
            numDirChunks = 1;	/* only skip this guy */
            goto nextEntry;
        }

        tp = bufferp->datap + entryInBuffer;
        dep = (cm_dirEntry_t *) tp;	/* now points to AFS3 dir entry */

        /* while we're here, compute the next entry's location, too,
         * since we'll need it when writing out the cookie into the dir
         * listing stream.
         *
         * XXXX Probably should do more sanity checking.
         */
        numDirChunks = cm_NameEntries(dep->name, &onbytes);
		
        /* compute offset of cookie representing next entry */
        nextEntryCookie = curOffset.LowPart + (CM_DIR_CHUNKSIZE * numDirChunks);

        /* Need 8.3 name? */
        NeedShortName = 0;
        if (infoLevel == SMB_FIND_FILE_BOTH_DIRECTORY_INFO
             && dep->fid.vnode != 0
             && !cm_Is8Dot3(dep->name)) {
            cm_Gen8Dot3Name(dep, shortName, &shortNameEnd);
            NeedShortName = 1;
        }

        osi_Log4(smb_logp, "T2 search dir vn %u uniq %u name %s (%s)",
                  dep->fid.vnode, dep->fid.unique, 
		  osi_LogSaveString(smb_logp, dep->name),
                  NeedShortName ? osi_LogSaveString(smb_logp, shortName) : "");

        /* When matching, we are using doing a case fold if we have a wildcard mask.
         * If we get a non-wildcard match, it's a lookup for a specific file. 
         */
        if (dep->fid.vnode != 0 && 
            (smb_V3MatchMask(dep->name, maskp, (starPattern? CM_FLAG_CASEFOLD : 0)) ||
             (NeedShortName &&
              smb_V3MatchMask(shortName, maskp, CM_FLAG_CASEFOLD)))) {

            /* Eliminate entries that don't match requested attributes */
            if (smb_hideDotFiles && !(dsp->attribute & SMB_ATTR_HIDDEN) && 
                 smb_IsDotFile(dep->name)) {
                osi_Log0(smb_logp, "T2 search dir skipping hidden");
                goto nextEntry; /* no hidden files */
            }
            if (!(dsp->attribute & SMB_ATTR_DIRECTORY))  /* no directories */
            {
                /* We have already done the cm_TryBulkStat above */
                fid.cell = scp->fid.cell;
                fid.volume = scp->fid.volume;
                fid.vnode = ntohl(dep->fid.vnode);
                fid.unique = ntohl(dep->fid.unique);
                fileType = cm_FindFileType(&fid);
                /*osi_Log2(smb_logp, "smb_ReceiveTran2SearchDir: file %s "
                 "has filetype %d", dep->name,
                 fileType);*/
                if (fileType == CM_SCACHETYPE_DIRECTORY ||
                    fileType == CM_SCACHETYPE_MOUNTPOINT ||
                    fileType == CM_SCACHETYPE_DFSLINK ||
                    fileType == CM_SCACHETYPE_INVALID)
                    osi_Log0(smb_logp, "T2 search dir skipping directory or bad link");
                    goto nextEntry;
            }

            /* finally check if this name will fit */

            /* standard dir entry stuff */
            if (infoLevel < 0x101)
                ohbytes = 23;	/* pre-NT */
            else if (infoLevel == SMB_FIND_FILE_NAMES_INFO)
                ohbytes = 12;	/* NT names only */
            else
                ohbytes = 64;	/* NT */

            if (infoLevel == SMB_FIND_FILE_BOTH_DIRECTORY_INFO)
                ohbytes += 26;	/* Short name & length */

            if (searchFlags & TRAN2_FIND_FLAG_RETURN_RESUME_KEYS) {
                ohbytes += 4;	/* if resume key required */
            }   

            if (infoLevel != SMB_INFO_STANDARD
                 && infoLevel != SMB_FIND_FILE_DIRECTORY_INFO
                 && infoLevel != SMB_FIND_FILE_NAMES_INFO)
                ohbytes += 4;	/* EASIZE */

            /* add header to name & term. null */
            orbytes = onbytes + ohbytes + 1;

            /* now, we round up the record to a 4 byte alignment,
             * and we make sure that we have enough room here for
             * even the aligned version (so we don't have to worry
             * about an * overflow when we pad things out below).
             * That's the reason for the alignment arithmetic below.
             */
            if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO)
                align = (4 - (orbytes & 3)) & 3;
            else
                align = 0;
            if (orbytes + bytesInBuffer + align > maxReturnData) {
                osi_Log1(smb_logp, "T2 dir search exceed max return data %d",
                          maxReturnData);
                break;
            }

            /* this is one of the entries to use: it is not deleted
             * and it matches the star pattern we're looking for.
             * Put out the name, preceded by its length.
             */
            /* First zero everything else */
            memset(origOp, 0, ohbytes);

            if (infoLevel <= SMB_FIND_FILE_DIRECTORY_INFO)
                *(origOp + ohbytes - 1) = (unsigned char) onbytes;
            else if (infoLevel == SMB_FIND_FILE_NAMES_INFO)
                *((u_long *)(op + 8)) = onbytes;
            else
                *((u_long *)(op + 60)) = onbytes;
            strcpy(origOp+ohbytes, dep->name);
            if (smb_StoreAnsiFilenames)
                CharToOem(origOp+ohbytes, origOp+ohbytes);

            /* Short name if requested and needed */
            if (infoLevel == SMB_FIND_FILE_BOTH_DIRECTORY_INFO) {
                if (NeedShortName) {
                    strcpy(op + 70, shortName);
                    if (smb_StoreAnsiFilenames)
                        CharToOem(op + 70, op + 70);
                    *(op + 68) = (char)(shortNameEnd - shortName);
                }
            }

            /* now, adjust the # of entries copied */
            returnedNames++;

            /* NextEntryOffset and FileIndex */
            if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO) {
                int entryOffset = orbytes + align;
                *((u_long *)op) = entryOffset;
                *((u_long *)(op+4)) = nextEntryCookie;
            }

            /* now we emit the attribute.  This is tricky, since
             * we need to really stat the file to find out what
             * type of entry we've got.  Right now, we're copying
             * out data from a buffer, while holding the scp
             * locked, so it isn't really convenient to stat
             * something now.  We'll put in a place holder
             * now, and make a second pass before returning this
             * to get the real attributes.  So, we just skip the
             * data for now, and adjust it later.  We allocate a
             * patch record to make it easy to find this point
             * later.  The replay will happen at a time when it is
             * safe to unlock the directory.
             */
            if (infoLevel != SMB_FIND_FILE_NAMES_INFO) {
                curPatchp = malloc(sizeof(*curPatchp));
                osi_QAdd((osi_queue_t **) &dirListPatchesp,
                          &curPatchp->q);
                curPatchp->dptr = op;
                if (infoLevel >= SMB_FIND_FILE_DIRECTORY_INFO)
                    curPatchp->dptr += 8;

                if (smb_hideDotFiles && smb_IsDotFile(dep->name)) {
                    curPatchp->flags = SMB_DIRLISTPATCH_DOTFILE;
                }       
                else    
                    curPatchp->flags = 0;

                curPatchp->fid.cell = scp->fid.cell;
                curPatchp->fid.volume = scp->fid.volume;
                curPatchp->fid.vnode = ntohl(dep->fid.vnode);
                curPatchp->fid.unique = ntohl(dep->fid.unique);

                /* temp */
                curPatchp->dep = dep;
            }   

            if (searchFlags & TRAN2_FIND_FLAG_RETURN_RESUME_KEYS)
                /* put out resume key */
                *((u_long *)origOp) = nextEntryCookie;

            /* Adjust byte ptr and count */
            origOp += orbytes;	/* skip entire record */
            bytesInBuffer += orbytes;

            /* and pad the record out */
            while (--align >= 0) {
                *origOp++ = 0;
                bytesInBuffer++;
            }
        }	/* if we're including this name */
        else if (!starPattern &&
                 !foundInexact &&
                 dep->fid.vnode != 0 &&
                 smb_V3MatchMask(dep->name, maskp, CM_FLAG_CASEFOLD)) {
            /* We were looking for exact matches, but here's an inexact one*/
            foundInexact = 1;
        }
                
      nextEntry:
        /* and adjust curOffset to be where the new cookie is */
        thyper.HighPart = 0;
        thyper.LowPart = CM_DIR_CHUNKSIZE * numDirChunks;
        curOffset = LargeIntegerAdd(thyper, curOffset);
    } /* while copying data for dir listing */

    /* If we didn't get a star pattern, we did an exact match during the first pass. 
     * If there were no exact matches found, we fail over to inexact matches by
     * marking the query as a star pattern (matches all case permutations), and
     * re-running the query. 
     */
    if (returnedNames == 0 && !starPattern && foundInexact) {
        osi_Log0(smb_logp,"T2 Search: No exact matches. Re-running for inexact matches");
        starPattern = 1;
        goto startsearch;
    }

    /* release the mutex */
    lock_ReleaseMutex(&scp->mx);
    if (bufferp) {
        buf_Release(bufferp);
	bufferp = NULL;
    }

    /* apply and free last set of patches; if not doing a star match, this
     * will be empty, but better safe (and freeing everything) than sorry.
     */
    code2 = smb_ApplyV3DirListPatches(scp, &dirListPatchesp, infoLevel, userp,
                              &req);
        
    /* now put out the final parameters */
    if (returnedNames == 0) 
        eos = 1;
    if (p->opcode == 1) {
        /* find first */
        outp->parmsp[0] = (unsigned short) dsp->cookie;
        outp->parmsp[1] = returnedNames;
        outp->parmsp[2] = eos;
        outp->parmsp[3] = 0;		/* nothing wrong with EAS */
        outp->parmsp[4] = 0;	
        /* don't need last name to continue
         * search, cookie is enough.  Normally,
         * this is the offset of the file name
         * of the last entry returned.
         */
        outp->totalParms = 10;	/* in bytes */
    }
    else {
        /* find next */
        outp->parmsp[0] = returnedNames;
        outp->parmsp[1] = eos;
        outp->parmsp[2] = 0;	/* EAS error */
        outp->parmsp[3] = 0;	/* last name, as above */
        outp->totalParms = 8;	/* in bytes */
    }   

    /* return # of bytes in the buffer */
    outp->totalData = bytesInBuffer;

    /* Return error code if unsuccessful on first request */
    if (code == 0 && p->opcode == 1 && returnedNames == 0)
        code = CM_ERROR_NOSUCHFILE;

    osi_Log4(smb_logp, "T2 search dir done, opcode %d, id %d, %d names, code %d",
             p->opcode, dsp->cookie, returnedNames, code);

    /* if we're supposed to close the search after this request, or if
     * we're supposed to close the search if we're done, and we're done,
     * or if something went wrong, close the search.
     */
    if ((searchFlags & TRAN2_FIND_FLAG_CLOSE_SEARCH) || 
	(returnedNames == 0) ||
        ((searchFlags & TRAN2_FIND_FLAG_CLOSE_SEARCH_IF_END) && eos) || 
	code != 0)
        smb_DeleteDirSearch(dsp);

    if (code)
        smb_SendTran2Error(vcp, p, opx, code);
    else
        smb_SendTran2Packet(vcp, outp, opx);

    smb_FreeTran2Packet(outp);
    smb_ReleaseDirSearch(dsp);
    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    return 0;
}

long smb_ReceiveV3FindClose(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    int dirHandle;
    smb_dirSearch_t *dsp;

    dirHandle = smb_GetSMBParm(inp, 0);
	
    osi_Log1(smb_logp, "SMB3 find close handle %d", dirHandle);

    dsp = smb_FindDirSearch(dirHandle);
        
    if (!dsp)
        return CM_ERROR_BADFD;
	
    /* otherwise, we have an FD to destroy */
    smb_DeleteDirSearch(dsp);
    smb_ReleaseDirSearch(dsp);
        
    /* and return results */
    smb_SetSMBDataLength(outp, 0);

    return 0;
}

long smb_ReceiveV3FindNotifyClose(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    smb_SetSMBDataLength(outp, 0);
    return 0;
}

long smb_ReceiveV3OpenX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    char *pathp;
    long code = 0;
    cm_space_t *spacep;
    int excl;
    cm_user_t *userp;
    cm_scache_t *dscp;		/* dir we're dealing with */
    cm_scache_t *scp;		/* file we're creating */
    cm_attr_t setAttr;
    int initialModeBits;
    smb_fid_t *fidp;
    int attributes;
    char *lastNamep;
    afs_uint32 dosTime;
    int openFun;
    int trunc;
    int openMode;
    int extraInfo;
    int openAction;
    int parmSlot;			/* which parm we're dealing with */
    char *tidPathp;
    cm_req_t req;
    int created = 0;

    cm_InitReq(&req);

    scp = NULL;
        
    extraInfo = (smb_GetSMBParm(inp, 2) & 1); /* return extra info */
    openFun = smb_GetSMBParm(inp, 8); /* open function */
    excl = ((openFun & 3) == 0);
    trunc = ((openFun & 3) == 2); /* truncate it */
    openMode = (smb_GetSMBParm(inp, 3) & 0x7);
    openAction = 0;             /* tracks what we did */

    attributes = smb_GetSMBParm(inp, 5);
    dosTime = smb_GetSMBParm(inp, 6) | (smb_GetSMBParm(inp, 7) << 16);

                                /* compute initial mode bits based on read-only flag in attributes */
    initialModeBits = 0666;
    if (attributes & SMB_ATTR_READONLY) 
	initialModeBits &= ~0222;
        
    pathp = smb_GetSMBData(inp, NULL);
    if (smb_StoreAnsiFilenames)
        OemToChar(pathp,pathp);

    spacep = inp->spacep;
    smb_StripLastComponent(spacep->data, &lastNamep, pathp);

    if (lastNamep && strcmp(lastNamep, SMB_IOCTL_FILENAME) == 0) {
        /* special case magic file name for receiving IOCTL requests
         * (since IOCTL calls themselves aren't getting through).
         */
#ifdef NOTSERVICE
        osi_Log0(smb_logp, "IOCTL Open");
#endif

        fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
        smb_SetupIoctlFid(fidp, spacep);

        /* set inp->fid so that later read calls in same msg can find fid */
        inp->fid = fidp->fid;
        
        /* copy out remainder of the parms */
        parmSlot = 2;
        smb_SetSMBParm(outp, parmSlot, fidp->fid); parmSlot++;
        if (extraInfo) {
            smb_SetSMBParm(outp, parmSlot, /* attrs */ 0); parmSlot++;
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* mod time */
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* len */
            smb_SetSMBParm(outp, parmSlot, 0x7fff); parmSlot++;
            smb_SetSMBParm(outp, parmSlot, openMode); parmSlot++;
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++; /* file type 0 ==> normal file or dir */
            smb_SetSMBParm(outp, parmSlot, 0); parmSlot++; /* IPC junk */
        }   
        /* and the final "always present" stuff */
        smb_SetSMBParm(outp, parmSlot, /* openAction found existing file */ 1); parmSlot++;
        /* next write out the "unique" ID */
        smb_SetSMBParm(outp, parmSlot, 0x1234); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, 0x5678); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;
        smb_SetSMBDataLength(outp, 0);

        /* and clean up fid reference */
        smb_ReleaseFID(fidp);
        return 0;
    }

#ifdef DEBUG_VERBOSE
    {
    	char *hexp, *asciip;
    	asciip = (lastNamep ? lastNamep : pathp );
    	hexp = osi_HexifyString(asciip);
    	DEBUG_EVENT2("AFS", "V3Open H[%s] A[%s]", hexp, asciip );
    	free(hexp);
    }
#endif
    userp = smb_GetUserFromVCP(vcp, inp);

    dscp = NULL;
    code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &tidPathp);
    if (code) {
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHPATH;
    }
    code = cm_NameI(cm_data.rootSCachep, pathp,
                    CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                    userp, tidPathp, &req, &scp);

#ifdef DFS_SUPPORT
    if (code == 0 && scp->fileType == CM_SCACHETYPE_DFSLINK) {
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        if ( WANTS_DFS_PATHNAMES(inp) )
            return CM_ERROR_PATH_NOT_COVERED;
        else
            return CM_ERROR_BADSHARENAME;
    }
#endif /* DFS_SUPPORT */

    if (code != 0) {
        code = cm_NameI(cm_data.rootSCachep, spacep->data,
                        CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                        userp, tidPathp, &req, &dscp);
        if (code) {
            cm_ReleaseUser(userp);
            return code;
        }

#ifdef DFS_SUPPORT
        if (dscp->fileType == CM_SCACHETYPE_DFSLINK) {
            cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            if ( WANTS_DFS_PATHNAMES(inp) )
                return CM_ERROR_PATH_NOT_COVERED;
            else
                return CM_ERROR_BADSHARENAME;
        }
#endif /* DFS_SUPPORT */
        /* otherwise, scp points to the parent directory.  Do a lookup,
         * and truncate the file if we find it, otherwise we create the
         * file.
         */
        if (!lastNamep) 
            lastNamep = pathp;
        else 
            lastNamep++;
        code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD, userp,
                          &req, &scp);
        if (code && code != CM_ERROR_NOSUCHFILE) {
            cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            return code;
        }
    }
        
    /* if we get here, if code is 0, the file exists and is represented by
     * scp.  Otherwise, we have to create it.  The dir may be represented
     * by dscp, or we may have found the file directly.  If code is non-zero,
     * scp is NULL.
     */
    if (code == 0) {
        code = cm_CheckOpen(scp, openMode, trunc, userp, &req);
        if (code) {
            if (dscp) cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            return code;
        }

        if (excl) {
            /* oops, file shouldn't be there */
            if (dscp) 
                cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            return CM_ERROR_EXISTS;
        }

        if (trunc) {
            setAttr.mask = CM_ATTRMASK_LENGTH;
            setAttr.length.LowPart = 0;
            setAttr.length.HighPart = 0;
            code = cm_SetAttr(scp, &setAttr, userp, &req);
            openAction = 3;	/* truncated existing file */
        }
        else openAction = 1;	/* found existing file */
    }
    else if (!(openFun & SMB_ATTR_DIRECTORY)) {
        /* don't create if not found */
        if (dscp) cm_ReleaseSCache(dscp);
        cm_ReleaseUser(userp);
        return CM_ERROR_NOSUCHFILE;
    }
    else {
        osi_assert(dscp != NULL);
        osi_Log1(smb_logp, "smb_ReceiveV3OpenX creating file %s",
                 osi_LogSaveString(smb_logp, lastNamep));
        openAction = 2;	/* created file */
        setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
        smb_UnixTimeFromDosUTime(&setAttr.clientModTime, dosTime);
        code = cm_Create(dscp, lastNamep, 0, &setAttr, &scp, userp,
                         &req);
        if (code == 0) {
	    created = 1;
	    if (dscp->flags & CM_SCACHEFLAG_ANYWATCH)
		smb_NotifyChange(FILE_ACTION_ADDED,
				 FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
				 dscp, lastNamep, NULL, TRUE);
	} else if (!excl && code == CM_ERROR_EXISTS) {
            /* not an exclusive create, and someone else tried
             * creating it already, then we open it anyway.  We
             * don't bother retrying after this, since if this next
             * fails, that means that the file was deleted after we
             * started this call.
             */
            code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
                             userp, &req, &scp);
            if (code == 0) {
                if (trunc) {
                    setAttr.mask = CM_ATTRMASK_LENGTH;
                    setAttr.length.LowPart = 0;
                    setAttr.length.HighPart = 0;
                    code = cm_SetAttr(scp, &setAttr, userp, &req);
                }   
            }	/* lookup succeeded */
        }
    }
        
    /* we don't need this any longer */
    if (dscp) 
        cm_ReleaseSCache(dscp);

    if (code) {
        /* something went wrong creating or truncating the file */
        if (scp) 
            cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        return code;
    }
        
    /* make sure we're about to open a file */
    if (scp->fileType != CM_SCACHETYPE_FILE) {
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        return CM_ERROR_ISDIR;
    }

    /* now all we have to do is open the file itself */
    fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
    osi_assert(fidp);
	
    cm_HoldUser(userp);
    lock_ObtainMutex(&fidp->mx);
    /* save a pointer to the vnode */
    fidp->scp = scp;
    lock_ObtainMutex(&scp->mx);
    scp->flags |= CM_SCACHEFLAG_SMB_FID;
    lock_ReleaseMutex(&scp->mx);
    osi_Log2(smb_logp,"smb_ReceiveV3OpenX fidp 0x%p scp 0x%p", fidp, scp);
    /* also the user */
    fidp->userp = userp;
        
    /* compute open mode */
    if (openMode != 1) 
        fidp->flags |= SMB_FID_OPENREAD_LISTDIR;
    if (openMode == 1 || openMode == 2)
        fidp->flags |= SMB_FID_OPENWRITE;

    /* remember if the file was newly created */
    if (created)
	fidp->flags |= SMB_FID_CREATED;

    lock_ReleaseMutex(&fidp->mx);
    smb_ReleaseFID(fidp);
        
    cm_Open(scp, 0, userp);

    /* set inp->fid so that later read calls in same msg can find fid */
    inp->fid = fidp->fid;
        
    /* copy out remainder of the parms */
    parmSlot = 2;
    smb_SetSMBParm(outp, parmSlot, fidp->fid); parmSlot++;
    lock_ObtainMutex(&scp->mx);
    if (extraInfo) {
        smb_SetSMBParm(outp, parmSlot, smb_Attributes(scp)); parmSlot++;
        smb_DosUTimeFromUnixTime(&dosTime, scp->clientModTime);
        smb_SetSMBParm(outp, parmSlot, dosTime & 0xffff); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, (dosTime>>16) & 0xffff); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, scp->length.LowPart & 0xffff); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, (scp->length.LowPart >> 16) & 0xffff); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, openMode); parmSlot++;
        smb_SetSMBParm(outp, parmSlot, 0); parmSlot++; /* file type 0 ==> normal file or dir */
        smb_SetSMBParm(outp, parmSlot, 0); parmSlot++; /* IPC junk */
    }
    /* and the final "always present" stuff */
    smb_SetSMBParm(outp, parmSlot, openAction); parmSlot++;
    /* next write out the "unique" ID */
    smb_SetSMBParm(outp, parmSlot, scp->fid.vnode & 0xffff); parmSlot++;
    smb_SetSMBParm(outp, parmSlot, scp->fid.volume & 0xffff); parmSlot++;
    smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;
    lock_ReleaseMutex(&scp->mx);
    smb_SetSMBDataLength(outp, 0);

    osi_Log1(smb_logp, "SMB OpenX opening fid %d", fidp->fid);

    cm_ReleaseUser(userp);
    /* leave scp held since we put it in fidp->scp */
    return 0;
}       

static void smb_GetLockParams(unsigned char LockType, 
                              char ** buf, 
                              unsigned int * ppid, 
                              LARGE_INTEGER * pOffset, 
                              LARGE_INTEGER * pLength)
{
    if (LockType & LOCKING_ANDX_LARGE_FILES) {
        /* Large Files */
        *ppid = *((USHORT *) *buf);
        pOffset->HighPart = *((LONG *)(*buf + 4));
        pOffset->LowPart = *((DWORD *)(*buf + 8));
        pLength->HighPart = *((LONG *)(*buf + 12));
        pLength->LowPart = *((DWORD *)(*buf + 16));
        *buf += 20;
    }
    else {
        /* Not Large Files */
        *ppid = *((USHORT *) *buf);
        pOffset->HighPart = 0;
        pOffset->LowPart = *((DWORD *)(*buf + 2));
        pLength->HighPart = 0;
        pLength->LowPart = *((DWORD *)(*buf + 6));
        *buf += 10;
    }
}

long smb_ReceiveV3LockingX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    cm_req_t req;
    cm_user_t *userp;
    unsigned short fid;
    smb_fid_t *fidp;
    cm_scache_t *scp;
    unsigned char LockType;
    unsigned short NumberOfUnlocks, NumberOfLocks;
    long Timeout;
    char *op;
    char *op_locks;
    LARGE_INTEGER LOffset, LLength;
    smb_waitingLockRequest_t *wlRequest = NULL;
    cm_file_lock_t *lockp;
    long code = 0;
    int i;
    cm_key_t key;
    unsigned int pid;

    cm_InitReq(&req);

    fid = smb_GetSMBParm(inp, 2);
    fid = smb_ChainFID(fid, inp);

    fidp = smb_FindFID(vcp, fid, 0);
    if (!fidp)
	return CM_ERROR_BADFD;
    
    lock_ObtainMutex(&fidp->mx);
    if (fidp->flags & SMB_FID_IOCTL) {
        osi_Log0(smb_logp, "smb_ReceiveV3Locking BadFD");
	lock_ReleaseMutex(&fidp->mx);
	smb_ReleaseFID(fidp);
        return CM_ERROR_BADFD;
    }
    scp = fidp->scp;
    osi_Log2(smb_logp,"smb_ReceiveV3LockingX fidp 0x%p scp 0x%p", fidp, scp);
    cm_HoldSCache(scp);
    lock_ReleaseMutex(&fidp->mx);

    /* set inp->fid so that later read calls in same msg can find fid */
    inp->fid = fid;

    userp = smb_GetUserFromVCP(vcp, inp);


    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                      CM_SCACHESYNC_NEEDCALLBACK
			 | CM_SCACHESYNC_GETSTATUS
			 | CM_SCACHESYNC_LOCK);
    if (code) {
        osi_Log1(smb_logp, "smb_ReceiveV3Locking SyncOp failure code 0x%x", code);
        goto doneSync;
    }

    LockType = smb_GetSMBParm(inp, 3) & 0xff;
    Timeout = (smb_GetSMBParm(inp, 5) << 16) + smb_GetSMBParm(inp, 4);
    NumberOfUnlocks = smb_GetSMBParm(inp, 6);
    NumberOfLocks = smb_GetSMBParm(inp, 7);

    if (!(fidp->flags & SMB_FID_OPENWRITE) &&
        !(LockType & LOCKING_ANDX_SHARED_LOCK)) {
        /* somebody wants exclusive locks on a file that they only
           opened for reading.  We downgrade this to a shared lock. */
        osi_Log0(smb_logp, "smb_ReceiveV3Locking reinterpreting exclusive lock as shared for read-only fid");
        LockType |= LOCKING_ANDX_SHARED_LOCK;
    }

    if ((LockType & LOCKING_ANDX_CANCEL_LOCK) ||
        (LockType & LOCKING_ANDX_CHANGE_LOCKTYPE)) {

        /* We don't support these requests.  Apparently, we can safely
           not deal with them too. */
        osi_Log1(smb_logp, "smb_ReceiveV3Locking received unsupported request [%s]",
                 ((LockType & LOCKING_ANDX_CANCEL_LOCK)?
                  "LOCKING_ANDX_CANCEL_LOCK":
                  "LOCKING_ANDX_CHANGE_LOCKTYPE")); 
        /* No need to call osi_LogSaveString since these are string
           constants.*/

        code = CM_ERROR_BADOP;
        goto done;

    }

    op = smb_GetSMBData(inp, NULL);

    for (i=0; i<NumberOfUnlocks; i++) {
        smb_GetLockParams(LockType, &op, &pid, &LOffset, &LLength);

        key = cm_GenerateKey(vcp->vcID, pid, fidp->fid);

        code = cm_Unlock(scp, LockType, LOffset, LLength, key, userp, &req);

        if (code) 
            goto done;
    }

    op_locks = op;

    for (i=0; i<NumberOfLocks; i++) {
        smb_GetLockParams(LockType, &op, &pid, &LOffset, &LLength);

        key = cm_GenerateKey(vcp->vcID, pid, fidp->fid);

        code = cm_Lock(scp, LockType, LOffset, LLength, key, (Timeout != 0),
                        userp, &req, &lockp);

	if (code == CM_ERROR_NOACCESS && LockType == LockWrite && 
	    (fidp->flags & (SMB_FID_OPENREAD_LISTDIR | SMB_FID_OPENWRITE)) == SMB_FID_OPENREAD_LISTDIR)
	{
	    code = cm_Lock(scp, LockRead, LOffset, LLength, key, (Timeout != 0),
			    userp, &req, &lockp);
	}

        if (code == CM_ERROR_WOULDBLOCK && Timeout != 0) {
            smb_waitingLock_t * wLock;

            /* Put on waiting list */
            if(wlRequest == NULL) {
                int j;
                char * opt;
                cm_key_t tkey;
                LARGE_INTEGER tOffset, tLength;

                wlRequest = malloc(sizeof(smb_waitingLockRequest_t));

                osi_assert(wlRequest != NULL);

                wlRequest->vcp = vcp;
                smb_HoldVC(vcp);
                wlRequest->scp = scp;
		osi_Log2(smb_logp,"smb_ReceiveV3LockingX wlRequest 0x%p scp 0x%p", wlRequest, scp);
                cm_HoldSCache(scp);
                wlRequest->inp = smb_CopyPacket(inp);
                wlRequest->outp = smb_CopyPacket(outp);
                wlRequest->lockType = LockType;
                wlRequest->timeRemaining = Timeout;
                wlRequest->locks = NULL;

                /* The waiting lock request needs to have enough
                   information to undo all the locks in the request.
                   We do the following to store info about locks that
                   have already been granted.  Sure, we can get most
                   of the info from the packet, but the packet doesn't
                   hold the result of cm_Lock call.  In practice we
                   only receive packets with one or two locks, so we
                   are only wasting a few bytes here and there and
                   only for a limited period of time until the waiting
                   lock times out or is freed. */

                for(opt = op_locks, j=i; j > 0; j--) {
                    smb_GetLockParams(LockType, &opt, &pid, &tOffset, &tLength);

                    tkey = cm_GenerateKey(vcp->vcID, pid, fidp->fid);

                    wLock = malloc(sizeof(smb_waitingLock_t));

                    osi_assert(wLock != NULL);

                    wLock->key = tkey;
                    wLock->LOffset = tOffset;
                    wLock->LLength = tLength;
                    wLock->lockp = NULL;
                    wLock->state = SMB_WAITINGLOCKSTATE_DONE;
                    osi_QAdd((osi_queue_t **) &wlRequest->locks,
                             &wLock->q);
                }
            }

            wLock = malloc(sizeof(smb_waitingLock_t));

            osi_assert(wLock != NULL);

            wLock->key = key;
            wLock->LOffset = LOffset;
            wLock->LLength = LLength;
            wLock->lockp = lockp;
            wLock->state = SMB_WAITINGLOCKSTATE_WAITING;
            osi_QAdd((osi_queue_t **) &wlRequest->locks,
                     &wLock->q);

            osi_Log1(smb_logp, "smb_ReceiveV3Locking WaitingLock created 0x%p",
                     wLock);

            code = 0;
            continue;
        }

        if (code) {
            osi_Log1(smb_logp, "smb_ReceiveV3Locking cm_Lock failure code 0x%x", code);
            break;
        }
    }

    if (code) {

        /* Since something went wrong with the lock number i, we now
           have to go ahead and release any locks acquired before the
           failure.  All locks before lock number i (of which there
           are i of them) have either been successful or are waiting.
           Either case requires calling cm_Unlock(). */

        /* And purge the waiting lock */
        if(wlRequest != NULL) {
            smb_waitingLock_t * wl;
            smb_waitingLock_t * wlNext;
            long ul_code;

            for(wl = wlRequest->locks; wl; wl = wlNext) {

                wlNext = (smb_waitingLock_t *) osi_QNext(&wl->q);

                ul_code = cm_Unlock(scp, LockType, wl->LOffset, wl->LLength, wl->key, userp, &req);
                
                if(ul_code != 0) {
                    osi_Log1(smb_logp, "smb_ReceiveV3Locking cm_Unlock returns code %d", ul_code);
                } else {
                    osi_Log0(smb_logp, "smb_ReceiveV3Locking cm_Unlock successful");
                }

                osi_QRemove((osi_queue_t **) &wlRequest->locks, &wl->q);
                free(wl);

            }

            smb_ReleaseVC(wlRequest->vcp);
            cm_ReleaseSCache(wlRequest->scp);
            smb_FreePacket(wlRequest->inp);
            smb_FreePacket(wlRequest->outp);

            free(wlRequest);

            wlRequest = NULL;
        }

    } else {

        if (wlRequest != NULL) {

            lock_ObtainWrite(&smb_globalLock);
            osi_QAdd((osi_queue_t **)&smb_allWaitingLocks,
                     &wlRequest->q);
            osi_Wakeup((LONG_PTR)&smb_allWaitingLocks);
            lock_ReleaseWrite(&smb_globalLock);

            /* don't send reply immediately */
            outp->flags |= SMB_PACKETFLAG_NOSEND;
        }

        smb_SetSMBDataLength(outp, 0);
    }

  done:   
    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_LOCK);

  doneSync:
    lock_ReleaseMutex(&scp->mx);
    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);

    return code;
}

long smb_ReceiveV3GetAttributes(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    unsigned short fid;
    smb_fid_t *fidp;
    cm_scache_t *scp;
    long code = 0;
    afs_uint32 searchTime;
    cm_user_t *userp;
    cm_req_t req;

    cm_InitReq(&req);

    fid = smb_GetSMBParm(inp, 0);
    fid = smb_ChainFID(fid, inp);
        
    fidp = smb_FindFID(vcp, fid, 0);
    if (!fidp)
	return CM_ERROR_BADFD;
    
    lock_ObtainMutex(&fidp->mx);
    if (fidp->flags & SMB_FID_IOCTL) {
	lock_ReleaseMutex(&fidp->mx);
	smb_ReleaseFID(fidp);
        return CM_ERROR_BADFD;
    }
    scp = fidp->scp;
    osi_Log2(smb_logp,"smb_ReceiveV3GetAttributes fidp 0x%p scp 0x%p", fidp, scp);
    cm_HoldSCache(scp);
    lock_ReleaseMutex(&fidp->mx);
        
    userp = smb_GetUserFromVCP(vcp, inp);
        
        
    /* otherwise, stat the file */
    lock_ObtainMutex(&scp->mx);
    code = cm_SyncOp(scp, NULL, userp, &req, 0,
                     CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);
    if (code) 
	goto done;

    cm_SyncOpDone(scp, NULL, CM_SCACHESYNC_NEEDCALLBACK | CM_SCACHESYNC_GETSTATUS);

    /* decode times.  We need a search time, but the response to this
     * call provides the date first, not the time, as returned in the
     * searchTime variable.  So we take the high-order bits first.
     */
    smb_SearchTimeFromUnixTime(&searchTime, scp->clientModTime);
    smb_SetSMBParm(outp, 0, (searchTime >> 16) & 0xffff);	/* ctime */
    smb_SetSMBParm(outp, 1, searchTime & 0xffff);
    smb_SetSMBParm(outp, 2, (searchTime >> 16) & 0xffff);	/* atime */
    smb_SetSMBParm(outp, 3, searchTime & 0xffff);
    smb_SetSMBParm(outp, 4, (searchTime >> 16) & 0xffff);	/* mtime */
    smb_SetSMBParm(outp, 5, searchTime & 0xffff);

    /* now handle file size and allocation size */
    smb_SetSMBParm(outp, 6, scp->length.LowPart & 0xffff);		/* file size */
    smb_SetSMBParm(outp, 7, (scp->length.LowPart >> 16) & 0xffff);
    smb_SetSMBParm(outp, 8, scp->length.LowPart & 0xffff);		/* alloc size */
    smb_SetSMBParm(outp, 9, (scp->length.LowPart >> 16) & 0xffff);

    /* file attribute */
    smb_SetSMBParm(outp, 10, smb_Attributes(scp));
        
    /* and finalize stuff */
    smb_SetSMBDataLength(outp, 0);
    code = 0;

  done:
    lock_ReleaseMutex(&scp->mx);
    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);
    return code;
}       

long smb_ReceiveV3SetAttributes(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    unsigned short fid;
    smb_fid_t *fidp;
    cm_scache_t *scp;
    long code = 0;
    afs_uint32 searchTime;
    time_t unixTime;
    cm_user_t *userp;
    cm_attr_t attrs;
    cm_req_t req;

    cm_InitReq(&req);

    fid = smb_GetSMBParm(inp, 0);
    fid = smb_ChainFID(fid, inp);
        
    fidp = smb_FindFID(vcp, fid, 0);
    if (!fidp)
	return CM_ERROR_BADFD;
    
    lock_ObtainMutex(&fidp->mx);
    if (fidp->flags & SMB_FID_IOCTL) {
	lock_ReleaseMutex(&fidp->mx);
	smb_ReleaseFID(fidp);
        return CM_ERROR_BADFD;
    }
    scp = fidp->scp;
    osi_Log2(smb_logp,"smb_ReceiveV3SetAttributes fidp 0x%p scp 0x%p", fidp, scp);
    cm_HoldSCache(scp);
    lock_ReleaseMutex(&fidp->mx);
        
    userp = smb_GetUserFromVCP(vcp, inp);
        
        
    /* now prepare to call cm_setattr.  This message only sets various times,
     * and AFS only implements mtime, and we'll set the mtime if that's
     * requested.  The others we'll ignore.
     */
    searchTime = smb_GetSMBParm(inp, 5) | (smb_GetSMBParm(inp, 6) << 16);
        
    if (searchTime != 0) {
        smb_UnixTimeFromSearchTime(&unixTime, searchTime);

        if ( unixTime != -1 ) {
            attrs.mask = CM_ATTRMASK_CLIENTMODTIME;
            attrs.clientModTime = unixTime;
            code = cm_SetAttr(scp, &attrs, userp, &req);

            osi_Log1(smb_logp, "SMB receive V3SetAttributes [fid=%ld]", fid);
        } else {
            osi_Log1(smb_logp, "**smb_UnixTimeFromSearchTime failed searchTime=%ld", searchTime);
        }
    }
    else 
	code = 0;

    cm_ReleaseSCache(scp);
    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);
    return code;
}

long smb_ReceiveV3WriteX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    osi_hyper_t offset;
    long count, written = 0, total_written = 0;
    unsigned short fd;
    unsigned pid;
    smb_fid_t *fidp;
    long code = 0;
    cm_user_t *userp;
    char *op;
    int inDataBlockCount;

    fd = smb_GetSMBParm(inp, 2);
    count = smb_GetSMBParm(inp, 10);

    offset.HighPart = 0;
    offset.LowPart = smb_GetSMBParm(inp, 3) | (smb_GetSMBParm(inp, 4) << 16);

    if (*inp->wctp == 14) {
        /* we have a request with 64-bit file offsets */
#ifdef AFS_LARGEFILES
        offset.HighPart = smb_GetSMBParm(inp, 12) | (smb_GetSMBParm(inp, 13) << 16);
#else
        if ((smb_GetSMBParm(inp, 12) | (smb_GetSMBParm(inp, 13) << 16)) != 0) {
            /* uh oh */
            osi_Log0(smb_logp, "smb_ReceiveV3WriteX offset requires largefile support");
            /* we shouldn't have received this op if we didn't specify
               largefile support */
            return CM_ERROR_BADOP;
        }
#endif
    }

    op = inp->data + smb_GetSMBParm(inp, 11);
    inDataBlockCount = count;

    osi_Log4(smb_logp, "smb_ReceiveV3WriteX fid %d, off 0x%x:%08x, size 0x%x",
             fd, offset.HighPart, offset.LowPart, count);
        
    fd = smb_ChainFID(fd, inp);
    fidp = smb_FindFID(vcp, fd, 0);
    if (!fidp)
        return CM_ERROR_BADFD;
        
    lock_ObtainMutex(&fidp->mx);
    if (fidp->flags & SMB_FID_IOCTL) {
	lock_ReleaseMutex(&fidp->mx);
        code = smb_IoctlV3Write(fidp, vcp, inp, outp);
	smb_ReleaseFID(fidp);
	return code;
    }
    lock_ReleaseMutex(&fidp->mx);
    userp = smb_GetUserFromVCP(vcp, inp);

    /* special case: 0 bytes transferred means there is no data
       transferred.  A slight departure from SMB_COM_WRITE where this
       means that we are supposed to truncate the file at this
       position. */

    {
        cm_key_t key;
        LARGE_INTEGER LOffset;
        LARGE_INTEGER LLength;

        pid = ((smb_t *) inp)->pid;
        key = cm_GenerateKey(vcp->vcID, pid, fd);

        LOffset.HighPart = offset.HighPart;
        LOffset.LowPart = offset.LowPart;
        LLength.HighPart = 0;
        LLength.LowPart = count;

        lock_ObtainMutex(&fidp->scp->mx);
        code = cm_LockCheckWrite(fidp->scp, LOffset, LLength, key);
        lock_ReleaseMutex(&fidp->scp->mx);

        if (code)
            goto done;
    }

    /*
     * Work around bug in NT client
     *
     * When copying a file, the NT client should first copy the data,
     * then copy the last write time.  But sometimes the NT client does
     * these in the wrong order, so the data copies would inadvertently
     * cause the last write time to be overwritten.  We try to detect this,
     * and don't set client mod time if we think that would go against the
     * intention.
     */
    lock_ObtainMutex(&fidp->mx);
    if ((fidp->flags & SMB_FID_MTIMESETDONE) != SMB_FID_MTIMESETDONE) {
        fidp->scp->mask |= CM_SCACHEMASK_CLIENTMODTIME;
        fidp->scp->clientModTime = time(NULL);
    }
    lock_ReleaseMutex(&fidp->mx);

    code = 0;
    while ( code == 0 && count > 0 ) {
#ifndef DJGPP
	code = smb_WriteData(fidp, &offset, count, op, userp, &written);
#else /* DJGPP */
	code = smb_WriteData(fidp, &offset, count, op, userp, &written, FALSE);
#endif /* !DJGPP */
	if (code == 0 && written == 0)
            code = CM_ERROR_PARTIALWRITE;

        offset = LargeIntegerAdd(offset,
                                 ConvertLongToLargeInteger(written));
        count -= written;
        total_written += written;
        written = 0;
    }

 done_writing:
    
    /* slots 0 and 1 are reserved for request chaining and will be
       filled in when we return. */
    smb_SetSMBParm(outp, 2, total_written);
    smb_SetSMBParm(outp, 3, 0); /* reserved */
    smb_SetSMBParm(outp, 4, 0); /* reserved */
    smb_SetSMBParm(outp, 5, 0); /* reserved */
    smb_SetSMBDataLength(outp, 0);

 done:
    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);

    return code;
}

long smb_ReceiveV3ReadX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    osi_hyper_t offset;
    long count;
    long finalCount = 0;
    unsigned short fd;
    unsigned pid;
    smb_fid_t *fidp;
    long code = 0;
    cm_user_t *userp;
    cm_key_t key;
    char *op;
        
    fd = smb_GetSMBParm(inp, 2);
    count = smb_GetSMBParm(inp, 5);
    offset.LowPart = smb_GetSMBParm(inp, 3) | (smb_GetSMBParm(inp, 4) << 16);

    if (*inp->wctp == 12) {
        /* a request with 64-bit offsets */
#ifdef AFS_LARGEFILES
        offset.HighPart = smb_GetSMBParm(inp, 10) | (smb_GetSMBParm(inp, 11) << 16);

        if (LargeIntegerLessThanZero(offset)) {
            osi_Log2(smb_logp, "smb_ReceiveV3Read offset too large (0x%x:%08x)",
                     offset.HighPart, offset.LowPart);
            return CM_ERROR_BADSMB;
        }
#else
        if ((smb_GetSMBParm(inp, 10) | (smb_GetSMBParm(inp, 11) << 16)) != 0) {
            osi_Log0(smb_logp, "smb_ReceiveV3Read offset is 64-bit.  Dropping");
            return CM_ERROR_BADSMB;
        } else {
            offset.HighPart = 0;
        }
#endif
    } else {
        offset.HighPart = 0;
    }

    osi_Log4(smb_logp, "smb_ReceiveV3Read fd %d, off 0x%x:%08x, size 0x%x",
             fd, offset.HighPart, offset.LowPart, count);

    fd = smb_ChainFID(fd, inp);
    fidp = smb_FindFID(vcp, fd, 0);
    if (!fidp) {
        return CM_ERROR_BADFD;
    }

    pid = ((smb_t *) inp)->pid;
    key = cm_GenerateKey(vcp->vcID, pid, fd);
    {
        LARGE_INTEGER LOffset, LLength;

        LOffset.HighPart = offset.HighPart;
        LOffset.LowPart = offset.LowPart;
        LLength.HighPart = 0;
        LLength.LowPart = count;

        lock_ObtainMutex(&fidp->scp->mx);
        code = cm_LockCheckRead(fidp->scp, LOffset, LLength, key);
        lock_ReleaseMutex(&fidp->scp->mx);
    }

    if (code) {
        smb_ReleaseFID(fidp);
        return code;
    }

    /* set inp->fid so that later read calls in same msg can find fid */
    inp->fid = fd;

    lock_ObtainMutex(&fidp->mx);
    if (fidp->flags & SMB_FID_IOCTL) {
	lock_ReleaseMutex(&fidp->mx);
        code = smb_IoctlV3Read(fidp, vcp, inp, outp);
	smb_ReleaseFID(fidp);
	return code;
    }
    lock_ReleaseMutex(&fidp->mx);

    userp = smb_GetUserFromVCP(vcp, inp);

    /* 0 and 1 are reserved for request chaining, were setup by our caller,
     * and will be further filled in after we return.
     */
    smb_SetSMBParm(outp, 2, 0);	/* remaining bytes, for pipes */
    smb_SetSMBParm(outp, 3, 0);	/* resvd */
    smb_SetSMBParm(outp, 4, 0);	/* resvd */
    smb_SetSMBParm(outp, 5, count);	/* # of bytes we're going to read */
    /* fill in #6 when we have all the parameters' space reserved */
    smb_SetSMBParm(outp, 7, 0);	/* resv'd */
    smb_SetSMBParm(outp, 8, 0);	/* resv'd */
    smb_SetSMBParm(outp, 9, 0);	/* resv'd */
    smb_SetSMBParm(outp, 10, 0);	/* resv'd */
    smb_SetSMBParm(outp, 11, 0);	/* reserved */

    /* get op ptr after putting in the parms, since otherwise we don't
     * know where the data really is.
     */
    op = smb_GetSMBData(outp, NULL);
        
    /* now fill in offset from start of SMB header to first data byte (to op) */
    smb_SetSMBParm(outp, 6, ((int) (op - outp->data)));

    /* set the packet data length the count of the # of bytes */
    smb_SetSMBDataLength(outp, count);

#ifndef DJGPP
    code = smb_ReadData(fidp, &offset, count, op, userp, &finalCount);
#else /* DJGPP */
    code = smb_ReadData(fidp, &offset, count, op, userp, &finalCount, FALSE);
#endif /* !DJGPP */

    /* fix some things up */
    smb_SetSMBParm(outp, 5, finalCount);
    smb_SetSMBDataLength(outp, finalCount);

    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);
    return code;
}   
        
/*
 * Values for createDisp, copied from NTDDK.H
 */
#define  FILE_SUPERSEDE	0	// (???)
#define  FILE_OPEN     	1	// (open)
#define  FILE_CREATE	2	// (exclusive)
#define  FILE_OPEN_IF	3	// (non-exclusive)
#define  FILE_OVERWRITE	4	// (open & truncate, but do not create)
#define  FILE_OVERWRITE_IF 5	// (open & truncate, or create)

/* Flags field */
#define REQUEST_OPLOCK 2
#define REQUEST_BATCH_OPLOCK 4
#define OPEN_DIRECTORY 8
#define EXTENDED_RESPONSE_REQUIRED 0x10

/* CreateOptions field. */
#define FILE_DIRECTORY_FILE       0x0001
#define FILE_WRITE_THROUGH        0x0002
#define FILE_SEQUENTIAL_ONLY      0x0004
#define FILE_NON_DIRECTORY_FILE   0x0040
#define FILE_NO_EA_KNOWLEDGE      0x0200
#define FILE_EIGHT_DOT_THREE_ONLY 0x0400
#define FILE_RANDOM_ACCESS        0x0800
#define FILE_DELETE_ON_CLOSE      0x1000
#define FILE_OPEN_BY_FILE_ID      0x2000

long smb_ReceiveNTCreateX(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    char *pathp, *realPathp;
    long code = 0;
    cm_space_t *spacep;
    cm_user_t *userp;
    cm_scache_t *dscp;		/* parent dir */
    cm_scache_t *scp;		/* file to create or open */
    cm_scache_t *targetScp;	/* if scp is a symlink */
    cm_attr_t setAttr;
    char *lastNamep;
    char *treeStartp;
    unsigned short nameLength;
    unsigned int flags;
    unsigned int requestOpLock;
    unsigned int requestBatchOpLock;
    unsigned int mustBeDir;
    unsigned int extendedRespRequired;
    unsigned int treeCreate;
    int realDirFlag;
    unsigned int desiredAccess;
    unsigned int extAttributes;
    unsigned int createDisp;
    unsigned int createOptions;
    unsigned int shareAccess;
    int initialModeBits;
    unsigned short baseFid;
    smb_fid_t *baseFidp;
    smb_fid_t *fidp;
    cm_scache_t *baseDirp;
    unsigned short openAction;
    int parmSlot;
    long fidflags;
    FILETIME ft;
    LARGE_INTEGER sz;
    char *tidPathp;
    BOOL foundscp;
    cm_req_t req;
    int created = 0;
    cm_lock_data_t *ldp = NULL;

    cm_InitReq(&req);

    /* This code is very long and has a lot of if-then-else clauses
     * scp and dscp get reused frequently and we need to ensure that 
     * we don't lose a reference.  Start by ensuring that they are NULL.
     */
    scp = NULL;
    dscp = NULL;
    treeCreate = FALSE;
    foundscp = FALSE;

    nameLength = smb_GetSMBOffsetParm(inp, 2, 1);
    flags = smb_GetSMBOffsetParm(inp, 3, 1)
        | (smb_GetSMBOffsetParm(inp, 4, 1) << 16);
    requestOpLock = flags & REQUEST_OPLOCK;
    requestBatchOpLock = flags & REQUEST_BATCH_OPLOCK;
    mustBeDir = flags & OPEN_DIRECTORY;
    extendedRespRequired = flags & EXTENDED_RESPONSE_REQUIRED;

    /*
     * Why all of a sudden 32-bit FID?
     * We will reject all bits higher than 16.
     */
    if (smb_GetSMBOffsetParm(inp, 6, 1) != 0)
        return CM_ERROR_INVAL;
    baseFid = smb_GetSMBOffsetParm(inp, 5, 1);
    desiredAccess = smb_GetSMBOffsetParm(inp, 7, 1)
        | (smb_GetSMBOffsetParm(inp, 8, 1) << 16);
    extAttributes = smb_GetSMBOffsetParm(inp, 13, 1)
        | (smb_GetSMBOffsetParm(inp, 14, 1) << 16);
    shareAccess = smb_GetSMBOffsetParm(inp, 15, 1)
        | (smb_GetSMBOffsetParm(inp, 16, 1) << 16);
    createDisp = smb_GetSMBOffsetParm(inp, 17, 1)
        | (smb_GetSMBOffsetParm(inp, 18, 1) << 16);
    createOptions = smb_GetSMBOffsetParm(inp, 19, 1)
        | (smb_GetSMBOffsetParm(inp, 20, 1) << 16);

    /* mustBeDir is never set; createOptions directory bit seems to be
     * more important
     */
    if (createOptions & FILE_DIRECTORY_FILE)
        realDirFlag = 1;
    else if (createOptions & FILE_NON_DIRECTORY_FILE)
        realDirFlag = 0;
    else
        realDirFlag = -1;

    /*
     * compute initial mode bits based on read-only flag in
     * extended attributes
     */
    initialModeBits = 0666;
    if (extAttributes & SMB_ATTR_READONLY) 
        initialModeBits &= ~0222;

    pathp = smb_GetSMBData(inp, NULL);
    /* Sometimes path is not null-terminated, so we make a copy. */
    realPathp = malloc(nameLength+1);
    memcpy(realPathp, pathp, nameLength);
    realPathp[nameLength] = 0;
    if (smb_StoreAnsiFilenames)
        OemToChar(realPathp,realPathp);

    spacep = inp->spacep;
    smb_StripLastComponent(spacep->data, &lastNamep, realPathp);

    osi_Log1(smb_logp,"NTCreateX for [%s]",osi_LogSaveString(smb_logp,realPathp));
    osi_Log4(smb_logp,"... da=[%x] ea=[%x] cd=[%x] co=[%x]", desiredAccess, extAttributes, createDisp, createOptions);
    osi_Log3(smb_logp,"... share=[%x] flags=[%x] lastNamep=[%s]", shareAccess, flags, osi_LogSaveString(smb_logp,(lastNamep?lastNamep:"null")));

    if (lastNamep && strcmp(lastNamep, SMB_IOCTL_FILENAME) == 0) {
        /* special case magic file name for receiving IOCTL requests
         * (since IOCTL calls themselves aren't getting through).
         */
        fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
        smb_SetupIoctlFid(fidp, spacep);
        osi_Log1(smb_logp,"NTCreateX Setting up IOCTL on fid[%d]",fidp->fid);

        /* set inp->fid so that later read calls in same msg can find fid */
        inp->fid = fidp->fid;

        /* out parms */
        parmSlot = 2;
        smb_SetSMBParmByte(outp, parmSlot, 0);	/* oplock */
        smb_SetSMBParm(outp, parmSlot, fidp->fid); parmSlot++;
        smb_SetSMBParmLong(outp, parmSlot, 1); parmSlot += 2; /* Action */
        /* times */
        memset(&ft, 0, sizeof(ft));
        smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
        smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
        smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
        smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2; /* attr */
        sz.HighPart = 0x7fff; sz.LowPart = 0;
        smb_SetSMBParmDouble(outp, parmSlot, (char *)&sz); parmSlot += 4; /* alen */
        smb_SetSMBParmDouble(outp, parmSlot, (char *)&sz); parmSlot += 4; /* len */
        smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* filetype */
        smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* dev state */
        smb_SetSMBParmByte(outp, parmSlot, 0);	/* is a dir? */
        smb_SetSMBDataLength(outp, 0);

        /* clean up fid reference */
        smb_ReleaseFID(fidp);
        free(realPathp);
        return 0;
    }

#ifdef DEBUG_VERBOSE
    {
    	char *hexp, *asciip;
    	asciip = (lastNamep? lastNamep : realPathp);
    	hexp = osi_HexifyString( asciip );
    	DEBUG_EVENT2("AFS", "NTCreateX H[%s] A[%s]", hexp, asciip);
    	free(hexp);
    }
#endif

    userp = smb_GetUserFromVCP(vcp, inp);
    if (!userp) {
    	osi_Log1(smb_logp, "NTCreateX Invalid user [%d]", ((smb_t *) inp)->uid);
    	free(realPathp);
    	return CM_ERROR_INVAL;
    }

    if (baseFid == 0) {
	baseFidp = NULL;
        baseDirp = cm_data.rootSCachep;
        code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &tidPathp);
        if (code == CM_ERROR_TIDIPC) {
            /* Attempt to use a TID allocated for IPC.  The client
             * is probably looking for DCE RPC end points which we
             * don't support OR it could be looking to make a DFS
             * referral request. 
             */
            osi_Log0(smb_logp, "NTCreateX received IPC TID");
#ifndef DFS_SUPPORT
            free(realPathp);
            cm_ReleaseUser(userp);
            return CM_ERROR_NOSUCHFILE;
#endif /* DFS_SUPPORT */
        }
    } else {
        baseFidp = smb_FindFID(vcp, baseFid, 0);
        if (!baseFidp) {
            osi_Log1(smb_logp, "NTCreateX Invalid base fid [%d]", baseFid);
            free(realPathp);
            cm_ReleaseUser(userp);
            return CM_ERROR_INVAL;
        }       
        baseDirp = baseFidp->scp;
        tidPathp = NULL;
    }

    osi_Log1(smb_logp, "NTCreateX tidPathp=[%s]", (tidPathp==NULL)?"null": osi_LogSaveString(smb_logp,tidPathp));

    /* compute open mode */
    fidflags = 0;
    if (desiredAccess & DELETE)
        fidflags |= SMB_FID_OPENDELETE;
    if (desiredAccess & AFS_ACCESS_READ)
        fidflags |= SMB_FID_OPENREAD_LISTDIR;
    if (desiredAccess & AFS_ACCESS_WRITE)
        fidflags |= SMB_FID_OPENWRITE;
    if (createOptions & FILE_DELETE_ON_CLOSE)
        fidflags |= SMB_FID_DELONCLOSE;
    if (createOptions & FILE_SEQUENTIAL_ONLY && !(createOptions & FILE_RANDOM_ACCESS))
	fidflags |= SMB_FID_SEQUENTIAL;
    if (createOptions & FILE_RANDOM_ACCESS && !(createOptions & FILE_SEQUENTIAL_ONLY))
	fidflags |= SMB_FID_RANDOM;

    /* and the share mode */
    if (shareAccess & FILE_SHARE_READ)
        fidflags |= SMB_FID_SHARE_READ;
    if (shareAccess & FILE_SHARE_WRITE)
        fidflags |= SMB_FID_SHARE_WRITE;

    osi_Log1(smb_logp, "NTCreateX fidflags 0x%x", fidflags);
    code = 0;

    /* For an exclusive create, we want to do a case sensitive match for the last component. */
    if ( createDisp == FILE_CREATE || 
         createDisp == FILE_OVERWRITE ||
         createDisp == FILE_OVERWRITE_IF) {
        code = cm_NameI(baseDirp, spacep->data, CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                        userp, tidPathp, &req, &dscp);
        if (code == 0) {
#ifdef DFS_SUPPORT
            if (dscp->fileType == CM_SCACHETYPE_DFSLINK) {
                cm_ReleaseSCache(dscp);
                cm_ReleaseUser(userp);
                free(realPathp);
		if (baseFidp) 
		    smb_ReleaseFID(baseFidp);
                if ( WANTS_DFS_PATHNAMES(inp) )
                    return CM_ERROR_PATH_NOT_COVERED;
                else
                    return CM_ERROR_BADSHARENAME;
            }
#endif /* DFS_SUPPORT */
            code = cm_Lookup(dscp, (lastNamep)?(lastNamep+1):realPathp, CM_FLAG_FOLLOW,
                             userp, &req, &scp);
            if (code == CM_ERROR_NOSUCHFILE) {
                code = cm_Lookup(dscp, (lastNamep)?(lastNamep+1):realPathp, 
                                 CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD, userp, &req, &scp);
                if (code == 0 && realDirFlag == 1) {
                    cm_ReleaseSCache(scp);
                    cm_ReleaseSCache(dscp);
                    cm_ReleaseUser(userp);
                    free(realPathp);
		    if (baseFidp) 
			smb_ReleaseFID(baseFidp);
                    return CM_ERROR_EXISTS;
                }
            }
        }
        /* we have both scp and dscp */
    } else {
        code = cm_NameI(baseDirp, realPathp, CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                        userp, tidPathp, &req, &scp);
#ifdef DFS_SUPPORT
        if (code == 0 && scp->fileType == CM_SCACHETYPE_DFSLINK) {
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            free(realPathp);
	    if (baseFidp) 
		smb_ReleaseFID(baseFidp);
            if ( WANTS_DFS_PATHNAMES(inp) )
                return CM_ERROR_PATH_NOT_COVERED;
            else
                return CM_ERROR_BADSHARENAME;
        }
#endif /* DFS_SUPPORT */
        /* we might have scp but not dscp */
    }

    if (scp)
        foundscp = TRUE;
    
    if (!foundscp || (fidflags & (SMB_FID_OPENDELETE | SMB_FID_OPENWRITE))) {
        /* look up parent directory */
        /* If we are trying to create a path (i.e. multiple nested directories), then we don't *need*
         * the immediate parent.  We have to work our way up realPathp until we hit something that we
         * recognize.
         */

        /* we might or might not have scp */

        if (dscp == NULL) {
            do {
                char *tp;

                code = cm_NameI(baseDirp, spacep->data,
                             CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                             userp, tidPathp, &req, &dscp);

#ifdef DFS_SUPPORT
                if (code == 0 && dscp->fileType == CM_SCACHETYPE_DFSLINK) {
                    if (scp)
                        cm_ReleaseSCache(scp);
                    cm_ReleaseSCache(dscp);
                    cm_ReleaseUser(userp);
                    free(realPathp);
		    if (baseFidp) 
			smb_ReleaseFID(baseFidp);
                    if ( WANTS_DFS_PATHNAMES(inp) )
                        return CM_ERROR_PATH_NOT_COVERED;
                    else
                        return CM_ERROR_BADSHARENAME;
                }
#endif /* DFS_SUPPORT */

                if (code && 
                     (tp = strrchr(spacep->data,'\\')) &&
                     (createDisp == FILE_CREATE) &&
                     (realDirFlag == 1)) {
                    *tp++ = 0;
                    treeCreate = TRUE;
                    treeStartp = realPathp + (tp - spacep->data);

                    if (*tp && !smb_IsLegalFilename(tp)) {
                        cm_ReleaseUser(userp);
                        if (baseFidp) 
                            smb_ReleaseFID(baseFidp);
                        free(realPathp);
                        if (scp)
                            cm_ReleaseSCache(scp);
                        return CM_ERROR_BADNTFILENAME;
                    }
                    code = 0;
                }
            } while (dscp == NULL && code == 0);
        } else
            code = 0;

        /* we might have scp and we might have dscp */

        if (baseFidp)
            smb_ReleaseFID(baseFidp);

        if (code) {
            osi_Log0(smb_logp,"NTCreateX parent not found");
            if (scp)
                cm_ReleaseSCache(scp);
            if (dscp)
                cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return code;
        }

        if (treeCreate && dscp->fileType == CM_SCACHETYPE_FILE) {
            /* A file exists where we want a directory. */
            if (scp)
                cm_ReleaseSCache(scp);
            cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return CM_ERROR_EXISTS;
        }

        if (!lastNamep) 
            lastNamep = realPathp;
        else 
            lastNamep++;

        if (!smb_IsLegalFilename(lastNamep)) {
            if (scp)
                cm_ReleaseSCache(scp);
            if (dscp)
                cm_ReleaseSCache(dscp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return CM_ERROR_BADNTFILENAME;
        }

        if (!foundscp && !treeCreate) {
            if ( createDisp == FILE_CREATE || 
                 createDisp == FILE_OVERWRITE ||
                 createDisp == FILE_OVERWRITE_IF) 
            {
                code = cm_Lookup(dscp, lastNamep,
                                  CM_FLAG_FOLLOW, userp, &req, &scp);
            } else {
                code = cm_Lookup(dscp, lastNamep,
                                 CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                                 userp, &req, &scp);
            }
            if (code && code != CM_ERROR_NOSUCHFILE) {
                if (dscp)
                    cm_ReleaseSCache(dscp);
                cm_ReleaseUser(userp);
                free(realPathp);
                return code;
            }
        }
        /* we have scp and dscp */
    } else {
        /* we have scp but not dscp */
        if (baseFidp)
            smb_ReleaseFID(baseFidp);
    }

    /* if we get here, if code is 0, the file exists and is represented by
     * scp.  Otherwise, we have to create it.  The dir may be represented
     * by dscp, or we may have found the file directly.  If code is non-zero,
     * scp is NULL.
     */
    if (code == 0 && !treeCreate) {
        code = cm_CheckNTOpen(scp, desiredAccess, createDisp, userp, &req, &ldp);
        if (code) {
            if (dscp)
                cm_ReleaseSCache(dscp);
            if (scp)
                cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return code;
        }

	if (createDisp == FILE_CREATE) {
            /* oops, file shouldn't be there */
	    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
            if (dscp)
                cm_ReleaseSCache(dscp);
            if (scp)
                cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return CM_ERROR_EXISTS;
        }

        if ( createDisp == FILE_OVERWRITE || 
             createDisp == FILE_OVERWRITE_IF) {

            setAttr.mask = CM_ATTRMASK_LENGTH;
            setAttr.length.LowPart = 0;
            setAttr.length.HighPart = 0;
            /* now watch for a symlink */
            code = 0;
            while (code == 0 && scp->fileType == CM_SCACHETYPE_SYMLINK) {
                targetScp = 0;
                osi_assert(dscp != NULL);
                code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
                if (code == 0) {
                    /* we have a more accurate file to use (the
                     * target of the symbolic link).  Otherwise,
                     * we'll just use the symlink anyway.
                     */
                    osi_Log2(smb_logp, "symlink vp %x to vp %x",
                              scp, targetScp);
		    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
                    cm_ReleaseSCache(scp);
                    scp = targetScp;
		    code = cm_CheckNTOpen(scp, desiredAccess, createDisp, userp, &req, &ldp);
		    if (code) {
			if (dscp)
			    cm_ReleaseSCache(dscp);
			if (scp)
			    cm_ReleaseSCache(scp);
			cm_ReleaseUser(userp);
			free(realPathp);
			return code;
		    }
		}
            }
            code = cm_SetAttr(scp, &setAttr, userp, &req);
            openAction = 3;	/* truncated existing file */
        }
        else 
            openAction = 1;	/* found existing file */

    } else if (createDisp == FILE_OPEN || createDisp == FILE_OVERWRITE) {
        /* don't create if not found */
        if (dscp)
            cm_ReleaseSCache(dscp);
        if (scp)
            cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        free(realPathp);
        return CM_ERROR_NOSUCHFILE;
    } else if (realDirFlag == 0 || realDirFlag == -1) {
        osi_assert(dscp != NULL);
        osi_Log1(smb_logp, "smb_ReceiveNTCreateX creating file %s",
                  osi_LogSaveString(smb_logp, lastNamep));
        openAction = 2;		/* created file */
        setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
        setAttr.clientModTime = time(NULL);
        code = cm_Create(dscp, lastNamep, 0, &setAttr, &scp, userp, &req);
        if (code == 0) {
	    created = 1;
	    if (dscp->flags & CM_SCACHEFLAG_ANYWATCH)
		smb_NotifyChange(FILE_ACTION_ADDED,
				 FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
				 dscp, lastNamep, NULL, TRUE);
	} else if (code == CM_ERROR_EXISTS && createDisp != FILE_CREATE) {
            /* Not an exclusive create, and someone else tried
             * creating it already, then we open it anyway.  We
             * don't bother retrying after this, since if this next
             * fails, that means that the file was deleted after we
             * started this call.
             */
            code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
                              userp, &req, &scp);
            if (code == 0) {
                if (createDisp == FILE_OVERWRITE_IF) {
                    setAttr.mask = CM_ATTRMASK_LENGTH;
                    setAttr.length.LowPart = 0;
                    setAttr.length.HighPart = 0;

                    /* now watch for a symlink */
                    code = 0;
                    while (code == 0 && scp->fileType == CM_SCACHETYPE_SYMLINK) {
                        targetScp = 0;
                        code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
                        if (code == 0) {
                            /* we have a more accurate file to use (the
                             * target of the symbolic link).  Otherwise,
                             * we'll just use the symlink anyway.
                             */
                            osi_Log2(smb_logp, "symlink vp %x to vp %x",
                                      scp, targetScp);
                            cm_ReleaseSCache(scp);
                            scp = targetScp;
                        }
                    }
                    code = cm_SetAttr(scp, &setAttr, userp, &req);
                }
            }	/* lookup succeeded */
        }
    } else {
        char *tp, *pp;
        char *cp; /* This component */
        int clen = 0; /* length of component */
        cm_scache_t *tscp1, *tscp2;
        int isLast = 0;

        /* create directory */
        if ( !treeCreate ) 
            treeStartp = lastNamep;
        osi_assert(dscp != NULL);
        osi_Log1(smb_logp, "smb_ReceiveNTCreateX creating directory [%s]",
                  osi_LogSaveString(smb_logp, treeStartp));
        openAction = 2;		/* created directory */

	/* if the request is to create the root directory 
	 * it will appear as a directory name of the nul-string
	 * and a code of CM_ERROR_NOSUCHFILE
	 */
	if ( !*treeStartp && code == CM_ERROR_NOSUCHFILE)
	    code = CM_ERROR_EXISTS;

        setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
        setAttr.clientModTime = time(NULL);

        pp = treeStartp;
        cp = spacep->data;
        tscp1 = dscp;
        cm_HoldSCache(tscp1);
        tscp2 = NULL;

        while (pp && *pp) {
            tp = strchr(pp, '\\');
            if (!tp) {
                strcpy(cp,pp);
                clen = (int)strlen(cp);
                isLast = 1; /* indicate last component.  the supplied path never ends in a slash */
            } else {
                clen = (int)(tp - pp);
                strncpy(cp,pp,clen);
                *(cp + clen) = 0;
                tp++;
            }
            pp = tp;

            if (clen == 0) 
                continue; /* the supplied path can't have consecutive slashes either , but */

            /* cp is the next component to be created. */
            code = cm_MakeDir(tscp1, cp, 0, &setAttr, userp, &req);
            if (code == 0 && (tscp1->flags & CM_SCACHEFLAG_ANYWATCH))
                smb_NotifyChange(FILE_ACTION_ADDED,
                                  FILE_NOTIFY_CHANGE_DIR_NAME,
                                  tscp1, cp, NULL, TRUE);
            if (code == 0 || 
                 (code == CM_ERROR_EXISTS && createDisp != FILE_CREATE)) {
                /* Not an exclusive create, and someone else tried
                 * creating it already, then we open it anyway.  We
                 * don't bother retrying after this, since if this next
                 * fails, that means that the file was deleted after we
                 * started this call.
                 */
                code = cm_Lookup(tscp1, cp, CM_FLAG_CASEFOLD,
                                  userp, &req, &tscp2);
            }       
            if (code) 
                break;

            if (!isLast) { /* for anything other than dscp, release it unless it's the last one */
                cm_ReleaseSCache(tscp1);
                tscp1 = tscp2; /* Newly created directory will be next parent */
                /* the hold is transfered to tscp1 from tscp2 */
            }
        }

        if (dscp)
            cm_ReleaseSCache(dscp);
        dscp = tscp1;
        if (scp)
            cm_ReleaseSCache(scp);
        scp = tscp2;
        /* 
         * if we get here and code == 0, then scp is the last directory created, and dscp is the
         * parent of scp.
         */
    }

    if (code) {
        /* something went wrong creating or truncating the file */
	if (ldp)
	    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
        if (scp) 
            cm_ReleaseSCache(scp);
        if (dscp) 
            cm_ReleaseSCache(dscp);
        cm_ReleaseUser(userp);
        free(realPathp);
        return code;
    }

    /* make sure we have file vs. dir right (only applies for single component case) */
    if (realDirFlag == 0 && scp->fileType != CM_SCACHETYPE_FILE) {
        /* now watch for a symlink */
        code = 0;
        while (code == 0 && scp->fileType == CM_SCACHETYPE_SYMLINK) {
            cm_scache_t * targetScp = 0;
            code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
            if (code == 0) {
                /* we have a more accurate file to use (the
                * target of the symbolic link).  Otherwise,
                * we'll just use the symlink anyway.
                */
                osi_Log2(smb_logp, "symlink vp %x to vp %x", scp, targetScp);
		if (ldp)
		    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
                cm_ReleaseSCache(scp);
                scp = targetScp;
            }
        }

        if (scp->fileType != CM_SCACHETYPE_FILE) {
	    if (ldp)
		cm_CheckNTOpenDone(scp, userp, &req, &ldp);
            if (dscp)
                cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return CM_ERROR_ISDIR;
        }
    }

    /* (only applies to single component case) */
    if (realDirFlag == 1 && scp->fileType == CM_SCACHETYPE_FILE) {
	if (ldp)
	    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
        cm_ReleaseSCache(scp);
        if (dscp)
            cm_ReleaseSCache(dscp);
        cm_ReleaseUser(userp);
        free(realPathp);
        return CM_ERROR_NOTDIR;
    }

    /* open the file itself */
    fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
    osi_assert(fidp);

    /* save a reference to the user */
    cm_HoldUser(userp);
    fidp->userp = userp;

    /* If we are restricting sharing, we should do so with a suitable
       share lock. */
    if (scp->fileType == CM_SCACHETYPE_FILE &&
        !(fidflags & SMB_FID_SHARE_WRITE)) {
        cm_key_t key;
        LARGE_INTEGER LOffset, LLength;
        int sLockType;

        LOffset.HighPart = SMB_FID_QLOCK_HIGH;
        LOffset.LowPart = SMB_FID_QLOCK_LOW;
        LLength.HighPart = 0;
        LLength.LowPart = SMB_FID_QLOCK_LENGTH;

        /* If we are not opening the file for writing, then we don't
           try to get an exclusive lock.  Noone else should be able to
           get an exclusive lock on the file anyway, although someone
           else can get a shared lock. */
        if ((fidflags & SMB_FID_SHARE_READ) ||
            !(fidflags & SMB_FID_OPENWRITE)) {
            sLockType = LOCKING_ANDX_SHARED_LOCK;
        } else {
            sLockType = 0;
        }

        key = cm_GenerateKey(vcp->vcID, SMB_FID_QLOCK_PID, fidp->fid);
        
        lock_ObtainMutex(&scp->mx);
        code = cm_Lock(scp, sLockType, LOffset, LLength, key, 0, userp, &req, NULL);
        lock_ReleaseMutex(&scp->mx);

        if (code) {
	    if (ldp)
		cm_CheckNTOpenDone(scp, userp, &req, &ldp);
            cm_ReleaseSCache(scp);
            if (dscp)
                cm_ReleaseSCache(dscp);
	    cm_ReleaseUser(userp);
	    /* Shouldn't this be smb_CloseFID()?  fidp->flags = SMB_FID_DELETE; */
	    smb_CloseFID(vcp, fidp, NULL, 0);
	    smb_ReleaseFID(fidp);
            free(realPathp);
            return code;
        }
    }

    /* Now its safe to release the file server lock obtained by cm_CheckNTOpen() */
    if (ldp)
	cm_CheckNTOpenDone(scp, userp, &req, &ldp);

    lock_ObtainMutex(&fidp->mx);
    /* save a pointer to the vnode */
    fidp->scp = scp;    /* Hold transfered to fidp->scp and no longer needed */
    lock_ObtainMutex(&scp->mx);
    scp->flags |= CM_SCACHEFLAG_SMB_FID;
    lock_ReleaseMutex(&scp->mx);
    osi_Log2(smb_logp,"smb_ReceiveNTCreateX fidp 0x%p scp 0x%p", fidp, scp);

    fidp->flags = fidflags;

    /* remember if the file was newly created */
    if (created)
	fidp->flags |= SMB_FID_CREATED;

    /* save parent dir and pathname for delete or change notification */
    if (fidflags & (SMB_FID_OPENDELETE | SMB_FID_OPENWRITE)) {
	osi_Log2(smb_logp,"smb_ReceiveNTCreateX fidp 0x%p dscp 0x%p", fidp, dscp);
        fidp->flags |= SMB_FID_NTOPEN;
        fidp->NTopen_dscp = dscp;
	dscp = NULL;
        fidp->NTopen_pathp = strdup(lastNamep);
    }
    fidp->NTopen_wholepathp = realPathp;
    lock_ReleaseMutex(&fidp->mx);

    /* we don't need this any longer */
    if (dscp) {
        cm_ReleaseSCache(dscp);
        dscp = NULL;
    }

    cm_Open(scp, 0, userp);

    /* set inp->fid so that later read calls in same msg can find fid */
    inp->fid = fidp->fid;

    /* out parms */
    parmSlot = 2;
    lock_ObtainMutex(&scp->mx);
    smb_SetSMBParmByte(outp, parmSlot, 0);	/* oplock */
    smb_SetSMBParm(outp, parmSlot, fidp->fid); parmSlot++;
    smb_SetSMBParmLong(outp, parmSlot, openAction); parmSlot += 2;
    smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
    smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
    smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
    smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
    smb_SetSMBParmDouble(outp, parmSlot, (char *)&ft); parmSlot += 4;
    smb_SetSMBParmLong(outp, parmSlot, smb_ExtAttributes(scp));
    parmSlot += 2;
    smb_SetSMBParmDouble(outp, parmSlot, (char *)&scp->length); parmSlot += 4;
    smb_SetSMBParmDouble(outp, parmSlot, (char *)&scp->length); parmSlot += 4;
    smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* filetype */
    smb_SetSMBParm(outp, parmSlot, 0); parmSlot++;	/* dev state */
    smb_SetSMBParmByte(outp, parmSlot,
                        (scp->fileType == CM_SCACHETYPE_DIRECTORY ||
			 scp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
			 scp->fileType == CM_SCACHETYPE_INVALID) ? 1 : 0); /* is a dir? */
    lock_ReleaseMutex(&scp->mx);
    smb_SetSMBDataLength(outp, 0);

    osi_Log2(smb_logp, "SMB NT CreateX opening fid %d path %s", fidp->fid,
              osi_LogSaveString(smb_logp, realPathp));

    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);

    /* Can't free realPathp if we get here since
       fidp->NTopen_wholepathp is pointing there */

    /* leave scp held since we put it in fidp->scp */
    return 0;
}       

/*
 * A lot of stuff copied verbatim from NT Create&X to NT Tran Create.
 * Instead, ultimately, would like to use a subroutine for common code.
 */
long smb_ReceiveNTTranCreate(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    char *pathp, *realPathp;
    long code = 0;
    cm_space_t *spacep;
    cm_user_t *userp;
    cm_scache_t *dscp;		/* parent dir */
    cm_scache_t *scp;		/* file to create or open */
    cm_scache_t *targetScp;     /* if scp is a symlink */
    cm_attr_t setAttr;
    char *lastNamep;
    unsigned long nameLength;
    unsigned int flags;
    unsigned int requestOpLock;
    unsigned int requestBatchOpLock;
    unsigned int mustBeDir;
    unsigned int extendedRespRequired;
    int realDirFlag;
    unsigned int desiredAccess;
#ifdef DEBUG_VERBOSE    
    unsigned int allocSize;
#endif
    unsigned int shareAccess;
    unsigned int extAttributes;
    unsigned int createDisp;
#ifdef DEBUG_VERBOSE
    unsigned int sdLen;
#endif
    unsigned int createOptions;
    int initialModeBits;
    unsigned short baseFid;
    smb_fid_t *baseFidp;
    smb_fid_t *fidp;
    cm_scache_t *baseDirp;
    unsigned short openAction;
    int parmSlot;
    long fidflags;
    FILETIME ft;
    char *tidPathp;
    BOOL foundscp;
    int parmOffset, dataOffset;
    char *parmp;
    ULONG *lparmp;
    char *outData;
    cm_req_t req;
    int created = 0;
    cm_lock_data_t *ldp = NULL;

    cm_InitReq(&req);

    foundscp = FALSE;
    scp = NULL;

    parmOffset = smb_GetSMBOffsetParm(inp, 11, 1)
        | (smb_GetSMBOffsetParm(inp, 12, 1) << 16);
    parmp = inp->data + parmOffset;
    lparmp = (ULONG *) parmp;

    flags = lparmp[0];
    requestOpLock = flags & REQUEST_OPLOCK;
    requestBatchOpLock = flags & REQUEST_BATCH_OPLOCK;
    mustBeDir = flags & OPEN_DIRECTORY;
    extendedRespRequired = flags & EXTENDED_RESPONSE_REQUIRED;

    /*
     * Why all of a sudden 32-bit FID?
     * We will reject all bits higher than 16.
     */
    if (lparmp[1] & 0xFFFF0000)
        return CM_ERROR_INVAL;
    baseFid = (unsigned short)lparmp[1];
    desiredAccess = lparmp[2];
#ifdef DEBUG_VERBOSE
    allocSize = lparmp[3];
#endif /* DEBUG_VERSOSE */
    extAttributes = lparmp[5];
    shareAccess = lparmp[6];
    createDisp = lparmp[7];
    createOptions = lparmp[8];
#ifdef DEBUG_VERBOSE
    sdLen = lparmp[9];
#endif
    nameLength = lparmp[11];

#ifdef DEBUG_VERBOSE
    osi_Log4(smb_logp,"NTTranCreate with da[%x],ea[%x],sa[%x],cd[%x]",desiredAccess,extAttributes,shareAccess,createDisp);
    osi_Log3(smb_logp,"... co[%x],sdl[%x],as[%x]",createOptions,sdLen,allocSize);
    osi_Log1(smb_logp,"... flags[%x]",flags);
#endif

    /* mustBeDir is never set; createOptions directory bit seems to be
     * more important
     */
    if (createOptions & FILE_DIRECTORY_FILE)
        realDirFlag = 1;
    else if (createOptions & FILE_NON_DIRECTORY_FILE)
        realDirFlag = 0;
    else
        realDirFlag = -1;

    /*
     * compute initial mode bits based on read-only flag in
     * extended attributes
     */
    initialModeBits = 0666;
    if (extAttributes & SMB_ATTR_READONLY) 
        initialModeBits &= ~0222;

    pathp = parmp + (13 * sizeof(ULONG)) + sizeof(UCHAR);
    /* Sometimes path is not null-terminated, so we make a copy. */
    realPathp = malloc(nameLength+1);
    memcpy(realPathp, pathp, nameLength);
    realPathp[nameLength] = 0;
    if (smb_StoreAnsiFilenames)
        OemToChar(realPathp,realPathp);

    spacep = cm_GetSpace();
    smb_StripLastComponent(spacep->data, &lastNamep, realPathp);

    /*
     * Nothing here to handle SMB_IOCTL_FILENAME.
     * Will add it if necessary.
     */

#ifdef DEBUG_VERBOSE
    {
        char *hexp, *asciip;
        asciip = (lastNamep? lastNamep : realPathp);
        hexp = osi_HexifyString( asciip );
        DEBUG_EVENT2("AFS", "NTTranCreate H[%s] A[%s]", hexp, asciip);
        free(hexp);
    }
#endif

    userp = smb_GetUserFromVCP(vcp, inp);
    if (!userp) {
    	osi_Log1(smb_logp, "NTTranCreate invalid user [%d]", ((smb_t *) inp)->uid);
    	free(realPathp);
    	return CM_ERROR_INVAL;
    }

    if (baseFid == 0) {
	baseFidp = NULL;
        baseDirp = cm_data.rootSCachep;
        code = smb_LookupTIDPath(vcp, ((smb_t *)inp)->tid, &tidPathp);
        if (code == CM_ERROR_TIDIPC) {
            /* Attempt to use a TID allocated for IPC.  The client
             * is probably looking for DCE RPC end points which we
             * don't support OR it could be looking to make a DFS
             * referral request. 
             */
            osi_Log0(smb_logp, "NTTranCreate received IPC TID");
#ifndef DFS_SUPPORT
            free(realPathp);
            cm_ReleaseUser(userp);
            return CM_ERROR_NOSUCHPATH;
#endif 
        }
    } else {
        baseFidp = smb_FindFID(vcp, baseFid, 0);
        if (!baseFidp) {
	    osi_Log1(smb_logp, "NTTranCreate Invalid fid [%d]", baseFid);
            free(realPathp);
            cm_ReleaseUser(userp);
            return CM_ERROR_INVAL;
        }       
        baseDirp = baseFidp->scp;
        tidPathp = NULL;
    }

    /* compute open mode */
    fidflags = 0;
    if (desiredAccess & DELETE)
        fidflags |= SMB_FID_OPENDELETE;
    if (desiredAccess & AFS_ACCESS_READ)
        fidflags |= SMB_FID_OPENREAD_LISTDIR;
    if (desiredAccess & AFS_ACCESS_WRITE)
        fidflags |= SMB_FID_OPENWRITE;
    if (createOptions & FILE_DELETE_ON_CLOSE)
        fidflags |= SMB_FID_DELONCLOSE;
    if (createOptions & FILE_SEQUENTIAL_ONLY && !(createOptions & FILE_RANDOM_ACCESS))
	fidflags |= SMB_FID_SEQUENTIAL;
    if (createOptions & FILE_RANDOM_ACCESS && !(createOptions & FILE_SEQUENTIAL_ONLY))
	fidflags |= SMB_FID_RANDOM;

    /* And the share mode */
    if (shareAccess & FILE_SHARE_READ)
        fidflags |= SMB_FID_SHARE_READ;
    if (shareAccess & FILE_SHARE_WRITE)
        fidflags |= SMB_FID_SHARE_WRITE;

    dscp = NULL;
    code = 0;
    if ( createDisp == FILE_OPEN || 
         createDisp == FILE_OVERWRITE ||
         createDisp == FILE_OVERWRITE_IF) {
        code = cm_NameI(baseDirp, spacep->data, CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                        userp, tidPathp, &req, &dscp);
        if (code == 0) {
#ifdef DFS_SUPPORT
            if (dscp->fileType == CM_SCACHETYPE_DFSLINK) {
                cm_ReleaseSCache(dscp);
                cm_ReleaseUser(userp);
                free(realPathp);
		if (baseFidp)
		    smb_ReleaseFID(baseFidp);
                if ( WANTS_DFS_PATHNAMES(inp) )
                    return CM_ERROR_PATH_NOT_COVERED;
                else
                    return CM_ERROR_BADSHARENAME;
            }
#endif /* DFS_SUPPORT */
            code = cm_Lookup(dscp, (lastNamep)?(lastNamep+1):realPathp, CM_FLAG_FOLLOW,
                             userp, &req, &scp);
            if (code == CM_ERROR_NOSUCHFILE) {
                code = cm_Lookup(dscp, (lastNamep)?(lastNamep+1):realPathp, 
                                 CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD, userp, &req, &scp);
                if (code == 0 && realDirFlag == 1) {
                    cm_ReleaseSCache(scp);
                    cm_ReleaseSCache(dscp);
                    cm_ReleaseUser(userp);
                    free(realPathp);
		    if (baseFidp)
			smb_ReleaseFID(baseFidp);
                    return CM_ERROR_EXISTS;
                }
            }
        } else 
            dscp = NULL;
    } else {
        code = cm_NameI(baseDirp, realPathp, CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                        userp, tidPathp, &req, &scp);
#ifdef DFS_SUPPORT
        if (code == 0 && scp->fileType == CM_SCACHETYPE_DFSLINK) {
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            free(realPathp);
	    if (baseFidp)
		smb_ReleaseFID(baseFidp);
            if ( WANTS_DFS_PATHNAMES(inp) )
                return CM_ERROR_PATH_NOT_COVERED;
            else
                return CM_ERROR_BADSHARENAME;
        }
#endif /* DFS_SUPPORT */
    }

    if (code == 0) 
        foundscp = TRUE;

    if (code != 0 || (fidflags & (SMB_FID_OPENDELETE | SMB_FID_OPENWRITE))) {
        /* look up parent directory */
        if ( !dscp ) {
            code = cm_NameI(baseDirp, spacep->data,
                             CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                             userp, tidPathp, &req, &dscp);
#ifdef DFS_SUPPORT
            if (code == 0 && dscp->fileType == CM_SCACHETYPE_DFSLINK) {
                cm_ReleaseSCache(dscp);
                cm_ReleaseUser(userp);
                free(realPathp);
		if (baseFidp)
		    smb_ReleaseFID(baseFidp);
                if ( WANTS_DFS_PATHNAMES(inp) )
                    return CM_ERROR_PATH_NOT_COVERED;
                else
                    return CM_ERROR_BADSHARENAME;
            }
#endif /* DFS_SUPPORT */
        } else
            code = 0;
        
        cm_FreeSpace(spacep);

        if (baseFidp)
            smb_ReleaseFID(baseFidp);

        if (code) {
            cm_ReleaseUser(userp);
            free(realPathp);
            return code;
        }

        if (!lastNamep)
	    lastNamep = realPathp;
        else 
	    lastNamep++;

        if (!smb_IsLegalFilename(lastNamep))
            return CM_ERROR_BADNTFILENAME;

        if (!foundscp) {
            if (createDisp == FILE_CREATE || createDisp == FILE_OVERWRITE_IF) {
                code = cm_Lookup(dscp, lastNamep,
                                  CM_FLAG_FOLLOW, userp, &req, &scp);
            } else {
                code = cm_Lookup(dscp, lastNamep,
                                 CM_FLAG_FOLLOW | CM_FLAG_CASEFOLD,
                                 userp, &req, &scp);
            }
            if (code && code != CM_ERROR_NOSUCHFILE) {
                cm_ReleaseSCache(dscp);
                cm_ReleaseUser(userp);
                free(realPathp);
                return code;
            }
        }
    } else {
        if (baseFidp)
            smb_ReleaseFID(baseFidp);
        cm_FreeSpace(spacep);
    }

    /* if we get here, if code is 0, the file exists and is represented by
     * scp.  Otherwise, we have to create it.  The dir may be represented
     * by dscp, or we may have found the file directly.  If code is non-zero,
     * scp is NULL.
     */
    if (code == 0) {
        code = cm_CheckNTOpen(scp, desiredAccess, createDisp, userp, &req, &ldp);
        if (code) {     
            if (dscp) 
                cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return code;
        }

        if (createDisp == FILE_CREATE) {
            /* oops, file shouldn't be there */
	    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
            if (dscp) 
                cm_ReleaseSCache(dscp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return CM_ERROR_EXISTS;
        }

        if (createDisp == FILE_OVERWRITE ||
            createDisp == FILE_OVERWRITE_IF) {
            setAttr.mask = CM_ATTRMASK_LENGTH;
            setAttr.length.LowPart = 0;
            setAttr.length.HighPart = 0;

            /* now watch for a symlink */
            code = 0;
            while (code == 0 && scp->fileType == CM_SCACHETYPE_SYMLINK) {
                targetScp = 0;
                code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
                if (code == 0) {
                    /* we have a more accurate file to use (the
                    * target of the symbolic link).  Otherwise,
                    * we'll just use the symlink anyway.
                    */
                    osi_Log2(smb_logp, "symlink vp %x to vp %x",
                              scp, targetScp);
		    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
                    cm_ReleaseSCache(scp);
                    scp = targetScp;
		    code = cm_CheckNTOpen(scp, desiredAccess, createDisp, userp, &req, &ldp);
		    if (code) {
			if (dscp)
			    cm_ReleaseSCache(dscp);
			if (scp)
			    cm_ReleaseSCache(scp);
			cm_ReleaseUser(userp);
			free(realPathp);
			return code;
		    }
                }
            }
            code = cm_SetAttr(scp, &setAttr, userp, &req);
            openAction = 3;	/* truncated existing file */
        }
        else openAction = 1;	/* found existing file */
    }
    else if (createDisp == FILE_OPEN || createDisp == FILE_OVERWRITE) {
        /* don't create if not found */
        if (dscp) 
            cm_ReleaseSCache(dscp);
        cm_ReleaseUser(userp);
        free(realPathp);
        return CM_ERROR_NOSUCHFILE;
    }
    else if (realDirFlag == 0 || realDirFlag == -1) {
        osi_assert(dscp != NULL);
        osi_Log1(smb_logp, "smb_ReceiveNTTranCreate creating file %s",
                  osi_LogSaveString(smb_logp, lastNamep));
        openAction = 2;		/* created file */
        setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
        setAttr.clientModTime = time(NULL);
        code = cm_Create(dscp, lastNamep, 0, &setAttr, &scp, userp,
                          &req);
        if (code == 0) {
	    created = 1;
	    if (dscp->flags & CM_SCACHEFLAG_ANYWATCH)
		smb_NotifyChange(FILE_ACTION_ADDED,
				 FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
				 dscp, lastNamep, NULL, TRUE);
	} else if (code == CM_ERROR_EXISTS && createDisp != FILE_CREATE) {
            /* Not an exclusive create, and someone else tried
             * creating it already, then we open it anyway.  We
             * don't bother retrying after this, since if this next
             * fails, that means that the file was deleted after we
             * started this call.
             */
            code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
                              userp, &req, &scp);
            if (code == 0) {
                if (createDisp == FILE_OVERWRITE_IF) {
                    setAttr.mask = CM_ATTRMASK_LENGTH;
                    setAttr.length.LowPart = 0;
                    setAttr.length.HighPart = 0;

                    /* now watch for a symlink */
                    code = 0;
                    while (code == 0 && scp->fileType == CM_SCACHETYPE_SYMLINK) {
                        targetScp = 0;
                        code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
                        if (code == 0) {
                            /* we have a more accurate file to use (the
                            * target of the symbolic link).  Otherwise,
                            * we'll just use the symlink anyway.
                            */
                            osi_Log2(smb_logp, "symlink vp %x to vp %x",
                                      scp, targetScp);
                            cm_ReleaseSCache(scp);
                            scp = targetScp;
                        }
                    }
                    code = cm_SetAttr(scp, &setAttr, userp, &req);
                }       
            }	/* lookup succeeded */
        }
    } else {
        /* create directory */
        osi_assert(dscp != NULL);
        osi_Log1(smb_logp,
                  "smb_ReceiveNTTranCreate creating directory %s",
                  osi_LogSaveString(smb_logp, lastNamep));
        openAction = 2;		/* created directory */
        setAttr.mask = CM_ATTRMASK_CLIENTMODTIME;
        setAttr.clientModTime = time(NULL);
        code = cm_MakeDir(dscp, lastNamep, 0, &setAttr, userp, &req);
        if (code == 0 && (dscp->flags & CM_SCACHEFLAG_ANYWATCH))
            smb_NotifyChange(FILE_ACTION_ADDED,
                              FILE_NOTIFY_CHANGE_DIR_NAME,
                              dscp, lastNamep, NULL, TRUE);
        if (code == 0 ||
            (code == CM_ERROR_EXISTS && createDisp != FILE_CREATE)) {
            /* Not an exclusive create, and someone else tried
             * creating it already, then we open it anyway.  We
             * don't bother retrying after this, since if this next
             * fails, that means that the file was deleted after we
             * started this call.
             */
            code = cm_Lookup(dscp, lastNamep, CM_FLAG_CASEFOLD,
                              userp, &req, &scp);
        }       
    }

    if (code) {
        /* something went wrong creating or truncating the file */
	if (ldp)
	    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
	if (scp) 
            cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        free(realPathp);
        return code;
    }

    /* make sure we have file vs. dir right */
    if (realDirFlag == 0 && scp->fileType != CM_SCACHETYPE_FILE) {
        /* now watch for a symlink */
        code = 0;
        while (code == 0 && scp->fileType == CM_SCACHETYPE_SYMLINK) {
            targetScp = 0;
            code = cm_EvaluateSymLink(dscp, scp, &targetScp, userp, &req);
            if (code == 0) {
                /* we have a more accurate file to use (the
                * target of the symbolic link).  Otherwise,
                * we'll just use the symlink anyway.
                */
                osi_Log2(smb_logp, "symlink vp %x to vp %x",
                          scp, targetScp);
		if (ldp)
		    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
                cm_ReleaseSCache(scp);
                scp = targetScp;
            }
        }

        if (scp->fileType != CM_SCACHETYPE_FILE) {
	    if (ldp)
		cm_CheckNTOpenDone(scp, userp, &req, &ldp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
            free(realPathp);
            return CM_ERROR_ISDIR;
        }
    }

    if (realDirFlag == 1 && scp->fileType == CM_SCACHETYPE_FILE) {
	if (ldp)
	    cm_CheckNTOpenDone(scp, userp, &req, &ldp);
        cm_ReleaseSCache(scp);
        cm_ReleaseUser(userp);
        free(realPathp);
        return CM_ERROR_NOTDIR;
    }

    /* open the file itself */
    fidp = smb_FindFID(vcp, 0, SMB_FLAG_CREATE);
    osi_assert(fidp);

    /* save a reference to the user */
    cm_HoldUser(userp);
    fidp->userp = userp;

    /* If we are restricting sharing, we should do so with a suitable
       share lock. */
    if (scp->fileType == CM_SCACHETYPE_FILE &&
        !(fidflags & SMB_FID_SHARE_WRITE)) {
        cm_key_t key;
        LARGE_INTEGER LOffset, LLength;
        int sLockType;

        LOffset.HighPart = SMB_FID_QLOCK_HIGH;
        LOffset.LowPart = SMB_FID_QLOCK_LOW;
        LLength.HighPart = 0;
        LLength.LowPart = SMB_FID_QLOCK_LENGTH;

        /* Similar to what we do in handling NTCreateX.  We get a
           shared lock if we are only opening the file for reading. */
        if ((fidflags & SMB_FID_SHARE_READ) ||
            !(fidflags & SMB_FID_OPENWRITE)) {
            sLockType = LOCKING_ANDX_SHARED_LOCK;
        } else {
            sLockType = 0;
        }

        key = cm_GenerateKey(vcp->vcID, SMB_FID_QLOCK_PID, fidp->fid);
        
        lock_ObtainMutex(&scp->mx);
        code = cm_Lock(scp, sLockType, LOffset, LLength, key, 0, userp, &req, NULL);
        lock_ReleaseMutex(&scp->mx);

        if (code) {
	    if (ldp)
		cm_CheckNTOpenDone(scp, userp, &req, &ldp);
            cm_ReleaseSCache(scp);
            cm_ReleaseUser(userp);
	    /* Shouldn't this be smb_CloseFID()?  fidp->flags = SMB_FID_DELETE; */
	    smb_CloseFID(vcp, fidp, NULL, 0);
	    smb_ReleaseFID(fidp);
	    free(realPathp);
            return CM_ERROR_SHARING_VIOLATION;
        }
    }

    /* Now its safe to drop the file server lock obtained by cm_CheckNTOpen() */
    if (ldp)
	cm_CheckNTOpenDone(scp, userp, &req, &ldp);

    lock_ObtainMutex(&fidp->mx);
    /* save a pointer to the vnode */
    fidp->scp = scp;
    lock_ObtainMutex(&scp->mx);
    scp->flags |= CM_SCACHEFLAG_SMB_FID;
    lock_ReleaseMutex(&scp->mx);
    osi_Log2(smb_logp,"smb_ReceiveNTTranCreate fidp 0x%p scp 0x%p", fidp, scp);

    fidp->flags = fidflags;

    /* remember if the file was newly created */
    if (created)
	fidp->flags |= SMB_FID_CREATED;

    /* save parent dir and pathname for deletion or change notification */
    if (fidflags & (SMB_FID_OPENDELETE | SMB_FID_OPENWRITE)) {
        fidp->flags |= SMB_FID_NTOPEN;
        fidp->NTopen_dscp = dscp;
	osi_Log2(smb_logp,"smb_ReceiveNTTranCreate fidp 0x%p dscp 0x%p", fidp, dscp);
	dscp = NULL;
        fidp->NTopen_pathp = strdup(lastNamep);
    }
    fidp->NTopen_wholepathp = realPathp;
    lock_ReleaseMutex(&fidp->mx);

    /* we don't need this any longer */
    if (dscp) 
        cm_ReleaseSCache(dscp);

    cm_Open(scp, 0, userp);

    /* set inp->fid so that later read calls in same msg can find fid */
    inp->fid = fidp->fid;

    /* check whether we are required to send an extended response */
    if (!extendedRespRequired) {
        /* out parms */
        parmOffset = 8*4 + 39;
        parmOffset += 1;	/* pad to 4 */
        dataOffset = parmOffset + 70;

        parmSlot = 1;
        outp->oddByte = 1;
        /* Total Parameter Count */
        smb_SetSMBParmLong(outp, parmSlot, 70); parmSlot += 2;
        /* Total Data Count */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Parameter Count */
        smb_SetSMBParmLong(outp, parmSlot, 70); parmSlot += 2;
        /* Parameter Offset */
        smb_SetSMBParmLong(outp, parmSlot, parmOffset); parmSlot += 2;
        /* Parameter Displacement */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Data Count */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Data Offset */
        smb_SetSMBParmLong(outp, parmSlot, dataOffset); parmSlot += 2;
        /* Data Displacement */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        smb_SetSMBParmByte(outp, parmSlot, 0);	/* Setup Count */
        smb_SetSMBDataLength(outp, 70);

        lock_ObtainMutex(&scp->mx);
        outData = smb_GetSMBData(outp, NULL);
        outData++;			/* round to get to parmOffset */
        *outData = 0; outData++;	/* oplock */
        *outData = 0; outData++;	/* reserved */
        *((USHORT *)outData) = fidp->fid; outData += 2;	/* fid */
        *((ULONG *)outData) = openAction; outData += 4;
        *((ULONG *)outData) = 0; outData += 4;	/* EA error offset */
        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        *((FILETIME *)outData) = ft; outData += 8;	/* creation time */
        *((FILETIME *)outData) = ft; outData += 8;	/* last access time */
        *((FILETIME *)outData) = ft; outData += 8;	/* last write time */
        *((FILETIME *)outData) = ft; outData += 8;	/* change time */
        *((ULONG *)outData) = smb_ExtAttributes(scp); outData += 4;
        *((LARGE_INTEGER *)outData) = scp->length; outData += 8; /* alloc sz */
        *((LARGE_INTEGER *)outData) = scp->length; outData += 8; /* EOF */
        *((USHORT *)outData) = 0; outData += 2;	/* filetype */
        *((USHORT *)outData) = 0; outData += 2;	/* dev state */
        *((USHORT *)outData) = ((scp->fileType == CM_SCACHETYPE_DIRECTORY ||
				scp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
				scp->fileType == CM_SCACHETYPE_INVALID) ? 1 : 0);
        outData += 2;	/* is a dir? */
        lock_ReleaseMutex(&scp->mx);
    } else {
        /* out parms */
        parmOffset = 8*4 + 39;
        parmOffset += 1;	/* pad to 4 */
        dataOffset = parmOffset + 104;
        
        parmSlot = 1;
        outp->oddByte = 1;
        /* Total Parameter Count */
        smb_SetSMBParmLong(outp, parmSlot, 101); parmSlot += 2;
        /* Total Data Count */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Parameter Count */
        smb_SetSMBParmLong(outp, parmSlot, 101); parmSlot += 2;
        /* Parameter Offset */
        smb_SetSMBParmLong(outp, parmSlot, parmOffset); parmSlot += 2;
        /* Parameter Displacement */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Data Count */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        /* Data Offset */
        smb_SetSMBParmLong(outp, parmSlot, dataOffset); parmSlot += 2;
        /* Data Displacement */
        smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
        smb_SetSMBParmByte(outp, parmSlot, 0);	/* Setup Count */
        smb_SetSMBDataLength(outp, 105);
        
        lock_ObtainMutex(&scp->mx);
        outData = smb_GetSMBData(outp, NULL);
        outData++;			/* round to get to parmOffset */
        *outData = 0; outData++;	/* oplock */
        *outData = 1; outData++;	/* response type */
        *((USHORT *)outData) = fidp->fid; outData += 2;	/* fid */
        *((ULONG *)outData) = openAction; outData += 4;
        *((ULONG *)outData) = 0; outData += 4;	/* EA error offset */
        smb_LargeSearchTimeFromUnixTime(&ft, scp->clientModTime);
        *((FILETIME *)outData) = ft; outData += 8;	/* creation time */
        *((FILETIME *)outData) = ft; outData += 8;	/* last access time */
        *((FILETIME *)outData) = ft; outData += 8;	/* last write time */
        *((FILETIME *)outData) = ft; outData += 8;	/* change time */
        *((ULONG *)outData) = smb_ExtAttributes(scp); outData += 4;
        *((LARGE_INTEGER *)outData) = scp->length; outData += 8; /* alloc sz */
        *((LARGE_INTEGER *)outData) = scp->length; outData += 8; /* EOF */
        *((USHORT *)outData) = 0; outData += 2;	/* filetype */
        *((USHORT *)outData) = 0; outData += 2;	/* dev state */
        *((USHORT *)outData) = ((scp->fileType == CM_SCACHETYPE_DIRECTORY ||
				scp->fileType == CM_SCACHETYPE_MOUNTPOINT ||
				scp->fileType == CM_SCACHETYPE_INVALID) ? 1 : 0);
        outData += 1;	/* is a dir? */
        memset(outData,0,24); outData += 24; /* Volume ID and file ID */
        *((ULONG *)outData) = 0x001f01ffL; outData += 4; /* Maxmimal access rights */
        *((ULONG *)outData) = 0; outData += 4; /* Guest Access rights */
        lock_ReleaseMutex(&scp->mx);
    }

    osi_Log1(smb_logp, "SMB NTTranCreate opening fid %d", fidp->fid);

    cm_ReleaseUser(userp);
    smb_ReleaseFID(fidp);

    /* free(realPathp); Can't free realPathp here because fidp->NTopen_wholepathp points there */
    /* leave scp held since we put it in fidp->scp */
    return 0;
}

long smb_ReceiveNTTranNotifyChange(smb_vc_t *vcp, smb_packet_t *inp,
	smb_packet_t *outp)
{
    smb_packet_t *savedPacketp;
    ULONG filter; 
    USHORT fid, watchtree;
    smb_fid_t *fidp;
    cm_scache_t *scp;
        
    filter = smb_GetSMBParm(inp, 19) |
             (smb_GetSMBParm(inp, 20) << 16);
    fid = smb_GetSMBParm(inp, 21);
    watchtree = (smb_GetSMBParm(inp, 22) & 0xff) ? 1 : 0;

    fidp = smb_FindFID(vcp, fid, 0);
    if (!fidp) {
        osi_Log1(smb_logp, "ERROR: NotifyChange given invalid fid [%d]", fid);
        return CM_ERROR_BADFD;
    }

    /* Create a copy of the Directory Watch Packet to use when sending the
     * notification if in the future a matching change is detected.
     */
    savedPacketp = smb_CopyPacket(inp);
    smb_HoldVC(vcp);
    if (savedPacketp->vcp)
	smb_ReleaseVC(savedPacketp->vcp);
    savedPacketp->vcp = vcp;

    /* Add the watch to the list of events to send notifications for */
    lock_ObtainMutex(&smb_Dir_Watch_Lock);
    savedPacketp->nextp = smb_Directory_Watches;
    smb_Directory_Watches = savedPacketp;
    lock_ReleaseMutex(&smb_Dir_Watch_Lock);

    scp = fidp->scp;
    osi_Log3(smb_logp,"smb_ReceiveNTTranNotifyChange fidp 0x%p scp 0x%p file \"%s\"", 
	      fidp, scp, osi_LogSaveString(smb_logp, fidp->NTopen_wholepathp));
    osi_Log3(smb_logp, "Request for NotifyChange filter 0x%x fid %d wtree %d",
             filter, fid, watchtree);
    if (filter & FILE_NOTIFY_CHANGE_FILE_NAME)
	osi_Log0(smb_logp, "      Notify Change File Name");
    if (filter & FILE_NOTIFY_CHANGE_DIR_NAME)
	osi_Log0(smb_logp, "      Notify Change Directory Name");
    if (filter & FILE_NOTIFY_CHANGE_ATTRIBUTES)
	osi_Log0(smb_logp, "      Notify Change Attributes");
    if (filter & FILE_NOTIFY_CHANGE_SIZE)
	osi_Log0(smb_logp, "      Notify Change Size");
    if (filter & FILE_NOTIFY_CHANGE_LAST_WRITE)
	osi_Log0(smb_logp, "      Notify Change Last Write");
    if (filter & FILE_NOTIFY_CHANGE_LAST_ACCESS)
	osi_Log0(smb_logp, "      Notify Change Last Access");
    if (filter & FILE_NOTIFY_CHANGE_CREATION)
	osi_Log0(smb_logp, "      Notify Change Creation");
    if (filter & FILE_NOTIFY_CHANGE_EA)
	osi_Log0(smb_logp, "      Notify Change Extended Attributes");
    if (filter & FILE_NOTIFY_CHANGE_SECURITY)
	osi_Log0(smb_logp, "      Notify Change Security");
    if (filter & FILE_NOTIFY_CHANGE_STREAM_NAME)
	osi_Log0(smb_logp, "      Notify Change Stream Name");
    if (filter & FILE_NOTIFY_CHANGE_STREAM_SIZE)
	osi_Log0(smb_logp, "      Notify Change Stream Size");
    if (filter & FILE_NOTIFY_CHANGE_STREAM_WRITE)
	osi_Log0(smb_logp, "      Notify Change Stream Write");

    lock_ObtainMutex(&scp->mx);
    if (watchtree)
        scp->flags |= CM_SCACHEFLAG_WATCHEDSUBTREE;
    else
        scp->flags |= CM_SCACHEFLAG_WATCHED;
    lock_ReleaseMutex(&scp->mx);
    smb_ReleaseFID(fidp);

    outp->flags |= SMB_PACKETFLAG_NOSEND;
    return 0;
}

unsigned char nullSecurityDesc[36] = {
    0x01,				/* security descriptor revision */
    0x00,				/* reserved, should be zero */
    0x00, 0x80,			        /* security descriptor control;
                                         * 0x8000 : self-relative format */
    0x14, 0x00, 0x00, 0x00,		/* offset of owner SID */
    0x1c, 0x00, 0x00, 0x00,		/* offset of group SID */
    0x00, 0x00, 0x00, 0x00,		/* offset of DACL would go here */
    0x00, 0x00, 0x00, 0x00,		/* offset of SACL would go here */
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        /* "null SID" owner SID */
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                        /* "null SID" group SID */
};      

long smb_ReceiveNTTranQuerySecurityDesc(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    int parmOffset, parmCount, dataOffset, dataCount;
    int parmSlot;
    int maxData;
    char *outData;
    char *parmp;
    USHORT *sparmp;
    ULONG *lparmp;
    USHORT fid;
    ULONG securityInformation;

    parmOffset = smb_GetSMBOffsetParm(inp, 11, 1)
        | (smb_GetSMBOffsetParm(inp, 12, 1) << 16);
    parmp = inp->data + parmOffset;
    sparmp = (USHORT *) parmp;
    lparmp = (ULONG *) parmp;

    fid = sparmp[0];
    securityInformation = lparmp[1];

    maxData = smb_GetSMBOffsetParm(inp, 7, 1)
        | (smb_GetSMBOffsetParm(inp, 8, 1) << 16);

    if (maxData < 36)
        dataCount = 0;
    else
        dataCount = 36;

    /* out parms */
    parmOffset = 8*4 + 39;
    parmOffset += 1;	/* pad to 4 */
    parmCount = 4;
    dataOffset = parmOffset + parmCount;

    parmSlot = 1;
    outp->oddByte = 1;
    /* Total Parameter Count */
    smb_SetSMBParmLong(outp, parmSlot, parmCount); parmSlot += 2;
    /* Total Data Count */
    smb_SetSMBParmLong(outp, parmSlot, dataCount); parmSlot += 2;
    /* Parameter Count */
    smb_SetSMBParmLong(outp, parmSlot, parmCount); parmSlot += 2;
    /* Parameter Offset */
    smb_SetSMBParmLong(outp, parmSlot, parmOffset); parmSlot += 2;
    /* Parameter Displacement */
    smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
    /* Data Count */
    smb_SetSMBParmLong(outp, parmSlot, dataCount); parmSlot += 2;
    /* Data Offset */
    smb_SetSMBParmLong(outp, parmSlot, dataOffset); parmSlot += 2;
    /* Data Displacement */
    smb_SetSMBParmLong(outp, parmSlot, 0); parmSlot += 2;
    smb_SetSMBParmByte(outp, parmSlot, 0);	/* Setup Count */
    smb_SetSMBDataLength(outp, 1 + parmCount + dataCount);

    outData = smb_GetSMBData(outp, NULL);
    outData++;			/* round to get to parmOffset */
    *((ULONG *)outData) = 36; outData += 4;	/* length */

    if (maxData >= 36) {
        memcpy(outData, nullSecurityDesc, 36);
        outData += 36;
        return 0;
    } else
        return CM_ERROR_BUFFERTOOSMALL;
}

long smb_ReceiveNTTransact(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    unsigned short function;

    function = smb_GetSMBParm(inp, 18);

    osi_Log1(smb_logp, "SMB NT Transact function %d", function);

    /* We can handle long names */
    if (vcp->flags & SMB_VCFLAG_USENT)
        ((smb_t *)outp)->flg2 |= SMB_FLAGS2_IS_LONG_NAME;
        
    switch (function) {
    case 1: 
        return smb_ReceiveNTTranCreate(vcp, inp, outp);
    case 2:
	osi_Log0(smb_logp, "SMB NT Transact Ioctl - not implemented");
	break;
    case 3:
	osi_Log0(smb_logp, "SMB NT Transact SetSecurityDesc - not implemented");
	break;
    case 4:
        return smb_ReceiveNTTranNotifyChange(vcp, inp, outp);
    case 5:
	osi_Log0(smb_logp, "SMB NT Transact Rename - not implemented");
	break;
    case 6:
        return smb_ReceiveNTTranQuerySecurityDesc(vcp, inp, outp);
    }
    return CM_ERROR_INVAL;
}

/*
 * smb_NotifyChange -- find relevant change notification messages and
 *		       reply to them
 *
 * If we don't know the file name (i.e. a callback break), filename is
 * NULL, and we return a zero-length list.
 *
 * At present there is not a single call to smb_NotifyChange that 
 * has the isDirectParent parameter set to FALSE.  
 */
void smb_NotifyChange(DWORD action, DWORD notifyFilter,
	cm_scache_t *dscp, char *filename, char *otherFilename,
	BOOL isDirectParent)
{
    smb_packet_t *watch, *lastWatch, *nextWatch;
    ULONG parmSlot, parmCount, parmOffset, dataOffset, nameLen;
    char *outData, *oldOutData;
    ULONG filter;
    USHORT fid, wtree;
    ULONG maxLen;
    BOOL twoEntries = FALSE;
    ULONG otherNameLen, oldParmCount = 0;
    DWORD otherAction;
    smb_fid_t *fidp;

    /* Get ready for rename within directory */
    if (action == FILE_ACTION_RENAMED_OLD_NAME && otherFilename != NULL) {
        twoEntries = TRUE;
        otherAction = FILE_ACTION_RENAMED_NEW_NAME;
    }

    osi_Log4(smb_logp,"in smb_NotifyChange for file [%s] dscp [%p] notification 0x%x parent %d",
             osi_LogSaveString(smb_logp,filename),dscp, notifyFilter, isDirectParent);
    if (action == 0)
	osi_Log0(smb_logp,"      FILE_ACTION_NONE");
    if (action == FILE_ACTION_ADDED)
	osi_Log0(smb_logp,"      FILE_ACTION_ADDED");
    if (action == FILE_ACTION_REMOVED)
	osi_Log0(smb_logp,"      FILE_ACTION_REMOVED");
    if (action == FILE_ACTION_MODIFIED)
	osi_Log0(smb_logp,"      FILE_ACTION_MODIFIED");
    if (action == FILE_ACTION_RENAMED_OLD_NAME)
	osi_Log0(smb_logp,"      FILE_ACTION_RENAMED_OLD_NAME");
    if (action == FILE_ACTION_RENAMED_NEW_NAME)
	osi_Log0(smb_logp,"      FILE_ACTION_RENAMED_NEW_NAME");

    lock_ObtainMutex(&smb_Dir_Watch_Lock);
    watch = smb_Directory_Watches;
    while (watch) {
        filter = smb_GetSMBParm(watch, 19)
            | (smb_GetSMBParm(watch, 20) << 16);
        fid = smb_GetSMBParm(watch, 21);
        wtree = (smb_GetSMBParm(watch, 22) & 0xff) ? 1 : 0;

        maxLen = smb_GetSMBOffsetParm(watch, 5, 1)
            | (smb_GetSMBOffsetParm(watch, 6, 1) << 16);

        /*
         * Strange hack - bug in NT Client and NT Server that we must emulate?
         */
        if ((filter == (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME)) && wtree)
            filter |= FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_ATTRIBUTES;

        fidp = smb_FindFID(watch->vcp, fid, 0);
        if (!fidp) {
            osi_Log2(smb_logp," no fidp for fid[%d] in vcp 0x%p",fid, watch->vcp);
            lastWatch = watch;
            watch = watch->nextp;
            continue;
        }       
        if (fidp->scp != dscp
             || (filter & notifyFilter) == 0
             || (!isDirectParent && !wtree)) {
            osi_Log1(smb_logp," skipping fidp->scp[%x]", fidp->scp);
            smb_ReleaseFID(fidp);
            lastWatch = watch;
            watch = watch->nextp;
            continue;
        }
        smb_ReleaseFID(fidp);

        osi_Log4(smb_logp,
                  "Sending Change Notification for fid %d filter 0x%x wtree %d file %s",
                  fid, filter, wtree, osi_LogSaveString(smb_logp, filename));
	if (filter & FILE_NOTIFY_CHANGE_FILE_NAME)
	    osi_Log0(smb_logp, "      Notify Change File Name");
	if (filter & FILE_NOTIFY_CHANGE_DIR_NAME)
	    osi_Log0(smb_logp, "      Notify Change Directory Name");
	if (filter & FILE_NOTIFY_CHANGE_ATTRIBUTES)
	    osi_Log0(smb_logp, "      Notify Change Attributes");
	if (filter & FILE_NOTIFY_CHANGE_SIZE)
	    osi_Log0(smb_logp, "      Notify Change Size");
	if (filter & FILE_NOTIFY_CHANGE_LAST_WRITE)
	    osi_Log0(smb_logp, "      Notify Change Last Write");
	if (filter & FILE_NOTIFY_CHANGE_LAST_ACCESS)
	    osi_Log0(smb_logp, "      Notify Change Last Access");
	if (filter & FILE_NOTIFY_CHANGE_CREATION)
	    osi_Log0(smb_logp, "      Notify Change Creation");
	if (filter & FILE_NOTIFY_CHANGE_EA)
	    osi_Log0(smb_logp, "      Notify Change Extended Attributes");
	if (filter & FILE_NOTIFY_CHANGE_SECURITY)
	    osi_Log0(smb_logp, "      Notify Change Security");
	if (filter & FILE_NOTIFY_CHANGE_STREAM_NAME)
	    osi_Log0(smb_logp, "      Notify Change Stream Name");
	if (filter & FILE_NOTIFY_CHANGE_STREAM_SIZE)
	    osi_Log0(smb_logp, "      Notify Change Stream Size");
	if (filter & FILE_NOTIFY_CHANGE_STREAM_WRITE)
	    osi_Log0(smb_logp, "      Notify Change Stream Write");
		     
	/* A watch can only be notified once.  Remove it from the list */
        nextWatch = watch->nextp;
        if (watch == smb_Directory_Watches)
            smb_Directory_Watches = nextWatch;
        else
            lastWatch->nextp = nextWatch;

        /* Turn off WATCHED flag in dscp */
        lock_ObtainMutex(&dscp->mx);
        if (wtree)
            dscp->flags &= ~CM_SCACHEFLAG_WATCHEDSUBTREE;
        else
            dscp->flags &= ~CM_SCACHEFLAG_WATCHED;
        lock_ReleaseMutex(&dscp->mx);

        /* Convert to response packet */
        ((smb_t *) watch)->reb = SMB_FLAGS_SERVER_TO_CLIENT | SMB_FLAGS_CANONICAL_PATHNAMES;
        ((smb_t *) watch)->wct = 0;

        /* out parms */
        if (filename == NULL)
            parmCount = 0;
        else {
            nameLen = (ULONG)strlen(filename);
            parmCount = 3*4 + nameLen*2;
            parmCount = (parmCount + 3) & ~3;	/* pad to 4 */
            if (twoEntries) {
                otherNameLen = (ULONG)strlen(otherFilename);
                oldParmCount = parmCount;
                parmCount += 3*4 + otherNameLen*2;
                parmCount = (parmCount + 3) & ~3; /* pad to 4 */
            }
            if (maxLen < parmCount)
                parmCount = 0;	/* not enough room */
        }
        parmOffset = 8*4 + 39;
        parmOffset += 1;			/* pad to 4 */
        dataOffset = parmOffset + parmCount;

        parmSlot = 1;
        watch->oddByte = 1;
        /* Total Parameter Count */
        smb_SetSMBParmLong(watch, parmSlot, parmCount); parmSlot += 2;
        /* Total Data Count */
        smb_SetSMBParmLong(watch, parmSlot, 0); parmSlot += 2;
        /* Parameter Count */
        smb_SetSMBParmLong(watch, parmSlot, parmCount); parmSlot += 2;
        /* Parameter Offset */
        smb_SetSMBParmLong(watch, parmSlot, parmOffset); parmSlot += 2;
        /* Parameter Displacement */
        smb_SetSMBParmLong(watch, parmSlot, 0); parmSlot += 2;
        /* Data Count */
        smb_SetSMBParmLong(watch, parmSlot, 0); parmSlot += 2;
        /* Data Offset */
        smb_SetSMBParmLong(watch, parmSlot, dataOffset); parmSlot += 2;
        /* Data Displacement */
        smb_SetSMBParmLong(watch, parmSlot, 0); parmSlot += 2;
        smb_SetSMBParmByte(watch, parmSlot, 0);	/* Setup Count */
        smb_SetSMBDataLength(watch, parmCount + 1);

        if (parmCount != 0) {
            char * p;
            outData = smb_GetSMBData(watch, NULL);
            outData++;	/* round to get to parmOffset */
            oldOutData = outData;
            *((DWORD *)outData) = oldParmCount; outData += 4;
            /* Next Entry Offset */
            *((DWORD *)outData) = action; outData += 4;
            /* Action */
            *((DWORD *)outData) = nameLen*2; outData += 4;
            /* File Name Length */
            p = strdup(filename);
            if (smb_StoreAnsiFilenames)
                CharToOem(p,p);
            mbstowcs((WCHAR *)outData, p, nameLen);
            free(p);
            /* File Name */
            if (twoEntries) {
                outData = oldOutData + oldParmCount;
                *((DWORD *)outData) = 0; outData += 4;
                /* Next Entry Offset */
                *((DWORD *)outData) = otherAction; outData += 4;
                /* Action */
                *((DWORD *)outData) = otherNameLen*2;
                outData += 4;	/* File Name Length */
                p = strdup(otherFilename);
                if (smb_StoreAnsiFilenames)
                    CharToOem(p,p);
                mbstowcs((WCHAR *)outData, p, otherNameLen);	/* File Name */
                free(p);
            }       
        }       

        /*
         * If filename is null, we don't know the cause of the
         * change notification.  We return zero data (see above),
         * and set error code to NT_STATUS_NOTIFY_ENUM_DIR
         * (= 0x010C).  We set the error code here by hand, without
         * modifying wct and bcc.
         */
        if (filename == NULL) {
            ((smb_t *) watch)->rcls = 0x0C;
            ((smb_t *) watch)->reh = 0x01;
            ((smb_t *) watch)->errLow = 0;
            ((smb_t *) watch)->errHigh = 0;
            /* Set NT Status codes flag */
            ((smb_t *) watch)->flg2 |= SMB_FLAGS2_32BIT_STATUS;
        }

        smb_SendPacket(watch->vcp, watch);
        smb_FreePacket(watch);
        watch = nextWatch;
    }
    lock_ReleaseMutex(&smb_Dir_Watch_Lock);
}       

long smb_ReceiveNTCancel(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    unsigned char *replyWctp;
    smb_packet_t *watch, *lastWatch;
    USHORT fid, watchtree;
    smb_fid_t *fidp;
    cm_scache_t *scp;

    osi_Log0(smb_logp, "SMB3 receive NT cancel");

    lock_ObtainMutex(&smb_Dir_Watch_Lock);
    watch = smb_Directory_Watches;
    while (watch) {
        if (((smb_t *)watch)->uid == ((smb_t *)inp)->uid
             && ((smb_t *)watch)->pid == ((smb_t *)inp)->pid
             && ((smb_t *)watch)->mid == ((smb_t *)inp)->mid
             && ((smb_t *)watch)->tid == ((smb_t *)inp)->tid) {
            if (watch == smb_Directory_Watches)
                smb_Directory_Watches = watch->nextp;
            else
                lastWatch->nextp = watch->nextp;
            lock_ReleaseMutex(&smb_Dir_Watch_Lock);

            /* Turn off WATCHED flag in scp */
            fid = smb_GetSMBParm(watch, 21);
            watchtree = smb_GetSMBParm(watch, 22) & 0xffff;

            if (vcp != watch->vcp)
                osi_Log2(smb_logp, "smb_ReceiveNTCancel: vcp %x not equal to watch vcp %x", 
                          vcp, watch->vcp);

            fidp = smb_FindFID(vcp, fid, 0);
            if (fidp) {
                osi_Log3(smb_logp, "Cancelling change notification for fid %d wtree %d file %s", 
                          fid, watchtree,
                          osi_LogSaveString(smb_logp, (fidp)?fidp->NTopen_wholepathp:""));

                scp = fidp->scp;
		osi_Log2(smb_logp,"smb_ReceiveNTCancel fidp 0x%p scp 0x%p", fidp, scp);
                lock_ObtainMutex(&scp->mx);
                if (watchtree)
                    scp->flags &= ~CM_SCACHEFLAG_WATCHEDSUBTREE;
                else
                    scp->flags &= ~CM_SCACHEFLAG_WATCHED;
                lock_ReleaseMutex(&scp->mx);
                smb_ReleaseFID(fidp);
            } else {
                osi_Log2(smb_logp,"NTCancel unable to resolve fid [%d] in vcp[%x]", fid,vcp);
            }

            /* assume STATUS32; return 0xC0000120 (CANCELED) */
            replyWctp = watch->wctp;
            *replyWctp++ = 0;
            *replyWctp++ = 0;
            *replyWctp++ = 0;
            ((smb_t *)watch)->rcls = 0x20;
            ((smb_t *)watch)->reh = 0x1;
            ((smb_t *)watch)->errLow = 0;
            ((smb_t *)watch)->errHigh = 0xC0;
            ((smb_t *)watch)->flg2 |= SMB_FLAGS2_32BIT_STATUS;
            smb_SendPacket(vcp, watch);
            smb_FreePacket(watch);
            return 0;
        }
        lastWatch = watch;
        watch = watch->nextp;
    }
    lock_ReleaseMutex(&smb_Dir_Watch_Lock);

    return 0;
}

/*
 * NT rename also does hard links.
 */

#define RENAME_FLAG_MOVE_CLUSTER_INFORMATION 0x102
#define RENAME_FLAG_HARD_LINK                0x103
#define RENAME_FLAG_RENAME                   0x104
#define RENAME_FLAG_COPY                     0x105

long smb_ReceiveNTRename(smb_vc_t *vcp, smb_packet_t *inp, smb_packet_t *outp)
{
    char *oldPathp, *newPathp;
    long code = 0;
    char * tp;
    int attrs;
    int rename_type;

    attrs = smb_GetSMBParm(inp, 0);
    rename_type = smb_GetSMBParm(inp, 1);

    if (rename_type != RENAME_FLAG_RENAME && rename_type != RENAME_FLAG_HARD_LINK) {
        osi_Log1(smb_logp, "NTRename invalid rename_type [%x]", rename_type);
        return CM_ERROR_NOACCESS;
    }

    tp = smb_GetSMBData(inp, NULL);
    oldPathp = smb_ParseASCIIBlock(tp, &tp);
    if (smb_StoreAnsiFilenames)
        OemToChar(oldPathp,oldPathp);
    newPathp = smb_ParseASCIIBlock(tp, &tp);
    if (smb_StoreAnsiFilenames)
        OemToChar(newPathp,newPathp);

    osi_Log3(smb_logp, "NTRename for [%s]->[%s] type [%s]",
             osi_LogSaveString(smb_logp, oldPathp),
             osi_LogSaveString(smb_logp, newPathp),
             ((rename_type==RENAME_FLAG_RENAME)?"rename":"hardlink"));

    if (rename_type == RENAME_FLAG_RENAME) {
        code = smb_Rename(vcp,inp,oldPathp,newPathp,attrs);
    } else { /* RENAME_FLAG_HARD_LINK */
        code = smb_Link(vcp,inp,oldPathp,newPathp);
    }
    return code;
}

void smb3_Init()
{
    lock_InitializeMutex(&smb_Dir_Watch_Lock, "Directory Watch List Lock");
}

cm_user_t *smb_FindCMUserByName(char *usern, char *machine, afs_uint32 flags)
{
    smb_username_t *unp;
    cm_user_t *     userp;

    unp = smb_FindUserByName(usern, machine, flags);
    if (!unp->userp) {
        lock_ObtainMutex(&unp->mx);
        unp->userp = cm_NewUser();
        lock_ReleaseMutex(&unp->mx);
        osi_Log2(smb_logp,"smb_FindCMUserByName New user name[%s] machine[%s]",osi_LogSaveString(smb_logp,usern),osi_LogSaveString(smb_logp,machine));
    }  else	{
        osi_Log2(smb_logp,"smb_FindCMUserByName Not found name[%s] machine[%s]",osi_LogSaveString(smb_logp,usern),osi_LogSaveString(smb_logp,machine));
    }
    userp = unp->userp;
    cm_HoldUser(userp);
    smb_ReleaseUsername(unp);
    return userp;
}

