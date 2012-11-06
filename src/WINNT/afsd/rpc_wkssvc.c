/*
 * Copyright (c) 2009 Secure Endpoints Inc.
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

#include <windows.h>
#include <lmcons.h>
#include <stdlib.h>
#include "afsd.h"
#include "ms-wkssvc.h"
#include "AFS_component_version_number.h"

#define PLATFORM_ID_AFS 800

#pragma warning( disable: 4027 )  /* func w/o formal parameter list */

unsigned long NetrWkstaGetInfo(
    /* [unique][string][in] */ WKSSVC_IDENTIFY_HANDLE ServerName,
    /* [in] */ unsigned long Level,
    /* [switch_is][out] */ LPWKSTA_INFO WkstaInfo)
{
    wchar_t *s;

    osi_Log1(afsd_logp, "NetrWkstaGetInfo level %u", Level);

    /*
     * How much space do we need and do we have that much room?
     * For now, just assume we can return everything in one shot
     * because the reality is that in this function call we do
     * not know the max size of the RPC response.
     */
    switch (Level) {
    case 102:
        WkstaInfo->WkstaInfo102 = MIDL_user_allocate(sizeof(WKSTA_INFO_102));
        break;
    case 101:
        WkstaInfo->WkstaInfo101 = MIDL_user_allocate(sizeof(WKSTA_INFO_101));
        break;
    case 100:
        WkstaInfo->WkstaInfo100 = MIDL_user_allocate(sizeof(WKSTA_INFO_100));
        break;
    }

    if (WkstaInfo->WkstaInfo100 == NULL) {
        return ERROR_INVALID_LEVEL;
    }

    /*
     * Remove any leading slashes since they are not part of the
     * server name.
     */
    for ( s=ServerName; *s == '\\' || *s == '/'; s++);

    switch (Level) {
    case 102:
        WkstaInfo->WkstaInfo102->wki102_logged_on_users = 0;
    case 101:
        WkstaInfo->WkstaInfo101->wki101_lanroot = NULL;
    case  100:
        WkstaInfo->WkstaInfo100->wki100_computername = _wcsupr(wcsdup(s));
        WkstaInfo->WkstaInfo100->wki100_langroup = _wcsdup(L"AFS");
        WkstaInfo->WkstaInfo100->wki100_platform_id = PLATFORM_ID_AFS;
        WkstaInfo->WkstaInfo100->wki100_ver_major = AFSPRODUCT_VERSION_MAJOR;
        WkstaInfo->WkstaInfo100->wki100_ver_minor = AFSPRODUCT_VERSION_MINOR;
        return 0;
    case  502:
    case 1013:
    case 1018:
    case 1046:
    default:
        return ERROR_INVALID_LEVEL;
    }
}

unsigned long NetrWkstaSetInfo(
    /* [unique][string][in] */ WKSSVC_IDENTIFY_HANDLE ServerName,
    /* [in] */ unsigned long Level,
    /* [switch_is][in] */ LPWKSTA_INFO WkstaInfo,
    /* [unique][out][in] */ unsigned long *ErrorParameter)
{
    osi_Log0(afsd_logp, "NetrWkstaSetInfo not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrWkstaUserEnum(
    /* [unique][string][in] */ WKSSVC_IDENTIFY_HANDLE ServerName,
    /* [out][in] */ LPWKSTA_USER_ENUM_STRUCT UserInfo,
    /* [in] */ unsigned long PreferredMaximumLength,
    /* [out] */ unsigned long *TotalEntries,
    /* [unique][out][in] */ unsigned long *ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrWkstaUserEnum not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrWkstaTransportEnum(
    /* [unique][string][in] */ WKSSVC_IDENTIFY_HANDLE ServerName,
    /* [out][in] */ LPWKSTA_TRANSPORT_ENUM_STRUCT TransportInfo,
    /* [in] */ unsigned long PreferredMaximumLength,
    /* [out] */ unsigned long *TotalEntries,
    /* [unique][out][in] */ unsigned long *ResumeHandle)
{
    osi_Log0(afsd_logp, "NetrWkstaTransportEnum not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrWkstaTransportAdd(
    /* [unique][string][in] */ WKSSVC_IDENTIFY_HANDLE ServerName,
    /* [in] */ unsigned long Level,
    /* [in] */ LPWKSTA_TRANSPORT_INFO_0 TransportInfo,
    /* [unique][out][in] */ unsigned long *ErrorParameter)
{
    osi_Log0(afsd_logp, "NetrWkstaTransportAdd not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrWkstaTransportDel(
    /* [unique][string][in] */ WKSSVC_IDENTIFY_HANDLE ServerName,
    /* [unique][string][in] */ wchar_t *TransportName,
    /* [in] */ unsigned long ForceLevel)
{
    osi_Log0(afsd_logp, "NetrWkstaTransportDel not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrWorkstationStatisticsGet(
    /* [unique][string][in] */ WKSSVC_IDENTIFY_HANDLE ServerName,
    /* [unique][string][in] */ wchar_t *ServiceName,
    /* [in] */ unsigned long Level,
    /* [in] */ unsigned long Options,
    /* [out] */ LPSTAT_WORKSTATION_0 *Buffer)
{
    osi_Log0(afsd_logp, "NetrWorkstationStatisticsGet not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrGetJoinInformation(
    /* [unique][string][in] */ WKSSVC_IMPERSONATE_HANDLE ServerName,
    /* [string][out][in] */ wchar_t **NameBuffer,
    /* [out] */ PNETSETUP_JOIN_STATUS BufferType)
{
    osi_Log0(afsd_logp, "NetrGetJoinInformation not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrJoinDomain2(
    /* [in] */ handle_t RpcBindingHandle,
    /* [unique][string][in] */ wchar_t *ServerName,
    /* [string][in] */ wchar_t *DomainName,
    /* [unique][string][in] */ wchar_t *MachineAccountOU,
    /* [unique][string][in] */ wchar_t *AccountName,
    /* [unique][in] */ PJOINPR_ENCRYPTED_USER_PASSWORD Password,
    /* [in] */ unsigned long Options)
{
    osi_Log0(afsd_logp, "NetrJoinDomain2 not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrUnjoinDomain2(
    /* [in] */ handle_t RpcBindingHandle,
    /* [unique][string][in] */ wchar_t *ServerName,
    /* [unique][string][in] */ wchar_t *AccountName,
    /* [unique][in] */ PJOINPR_ENCRYPTED_USER_PASSWORD Password,
    /* [in] */ unsigned long Options)
{
    osi_Log0(afsd_logp, "NetrUnjoinDomain2 not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrRenameMachineInDomain2(
    /* [in] */ handle_t RpcBindingHandle,
    /* [unique][string][in] */ wchar_t *ServerName,
    /* [unique][string][in] */ wchar_t *MachineName,
    /* [unique][string][in] */ wchar_t *AccountName,
    /* [unique][in] */ PJOINPR_ENCRYPTED_USER_PASSWORD Password,
    /* [in] */ unsigned long Options)
{
    osi_Log0(afsd_logp, "NetrRenameMachineInDomain2 not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrValidateName2(
    /* [in] */ handle_t RpcBindingHandle,
    /* [unique][string][in] */ wchar_t *ServerName,
    /* [string][in] */ wchar_t *NameToValidate,
    /* [unique][string][in] */ wchar_t *AccountName,
    /* [unique][in] */ PJOINPR_ENCRYPTED_USER_PASSWORD Password,
    /* [in] */ NETSETUP_NAME_TYPE NameType)
{
    osi_Log0(afsd_logp, "NetrValidateName2 not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrGetJoinableOUs2(
    /* [in] */ handle_t RpcBindingHandle,
    /* [unique][string][in] */ wchar_t *ServerName,
    /* [string][in] */ wchar_t *DomainName,
    /* [unique][string][in] */ wchar_t *AccountName,
    /* [unique][in] */ PJOINPR_ENCRYPTED_USER_PASSWORD Password,
    /* [out][in] */ unsigned long *OUCount,
    /* [size_is][size_is][string][out] */ wchar_t ***OUs)
{
    osi_Log0(afsd_logp, "NetrGetJoinableOUs2 not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrAddAlternateComputerName(
    /* [in] */ handle_t RpcBindingHandle,
    /* [unique][string][in] */ wchar_t *ServerName,
    /* [unique][string][in] */ wchar_t *AlternateName,
    /* [unique][string][in] */ wchar_t *DomainAccount,
    /* [unique][in] */ PJOINPR_ENCRYPTED_USER_PASSWORD EncryptedPassword,
    /* [in] */ unsigned long Reserved)
{
    osi_Log0(afsd_logp, "NetrAddAlternateComputerName not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrRemoveAlternateComputerName(
    /* [in] */ handle_t RpcBindingHandle,
    /* [unique][string][in] */ wchar_t *ServerName,
    /* [unique][string][in] */ wchar_t *AlternateName,
    /* [unique][string][in] */ wchar_t *DomainAccount,
    /* [unique][in] */ PJOINPR_ENCRYPTED_USER_PASSWORD EncryptedPassword,
    /* [in] */ unsigned long Reserved)
{
    osi_Log0(afsd_logp, "NetrRemoveAlternateComputerName not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrSetPrimaryComputerName(
    /* [in] */ handle_t RpcBindingHandle,
    /* [unique][string][in] */ wchar_t *ServerName,
    /* [unique][string][in] */ wchar_t *PrimaryName,
    /* [unique][string][in] */ wchar_t *DomainAccount,
    /* [unique][in] */ PJOINPR_ENCRYPTED_USER_PASSWORD EncryptedPassword,
    /* [in] */ unsigned long Reserved)
{
    osi_Log0(afsd_logp, "NetrSetPrimaryComputerName not supported");
    return ERROR_NOT_SUPPORTED;
}

unsigned long NetrEnumerateComputerNames(
    /* [unique][string][in] */ WKSSVC_IMPERSONATE_HANDLE ServerName,
    /* [in] */ NET_COMPUTER_NAME_TYPE NameType,
    /* [in] */ unsigned long Reserved,
    /* [out] */ PNET_COMPUTER_NAME_ARRAY *ComputerNames)
{
    osi_Log0(afsd_logp, "NetrEnumerateComputerName not supported");
    return ERROR_NOT_SUPPORTED;
}

/* [nocode] */ void Opnum3NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum4NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum8NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum9NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum10NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum11NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum12NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}


/* [nocode] */ void Opnum14NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum15NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum16NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum17NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum18NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

/* [nocode] */ void Opnum19NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}


/* [nocode] */ void Opnum21NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{
}

