/*
 *
 * aklog auth plugin, Taken from rot13 with guidance from NullAuthPlugin
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/kauth.h>

#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationPlugin.h>
#include <DirectoryService/DirectoryService.h>

typedef struct PluginRef
{
    const AuthorizationCallbacks *callbacks;
} PluginRef;

typedef struct MechanismRef
{
    const PluginRef *plugin;
    AuthorizationEngineRef engine;
    char *mechanismArg;
} MechanismRef;

static OSStatus do_aklog(char *cell)
{
    /*
     * The first draft used aklog source inline. In an rxgk
     * universe, that code should be encapsulated in a library
     * we call. In this version we simply fork aklog.
     * This function should be replaced with calls to a "libaklog"
     */

    char *aklogCmd;
    if (cell) {
	asprintf(&aklogCmd, "/usr/bin/aklog %s", cell);
	if (aklogCmd)
	    return system(aklogCmd);
	else
	    return -1;
    }

    return system("/usr/bin/aklog");
}

static OSStatus invokeAklog(MechanismRef *mechanism)
{
    AuthorizationContextFlags contextFlags;
    const AuthorizationValue *value;
    OSStatus status;
    uid_t uid = 0;
    gid_t gid = 0;

    /* a list of context values appears in the Null sample plugin */
    status = mechanism->plugin->callbacks->GetContextValue(mechanism->engine,
							   kDS1AttrUniqueID,
							   &contextFlags,
							   &value);

    /* This and the group are strings. I don't know why. */
    if (!status)
	uid = atoi(value->data);
    else
	goto failed;

    status = mechanism->plugin->callbacks->GetContextValue(mechanism->engine,
							   kDS1AttrPrimaryGroupID,
							   &contextFlags,
							   &value);
    if (!status)
	gid = atoi(value->data);
    else
	goto failed;

    /* in a PAGless universe, this trick works */
    status = pthread_setugid_np(uid, gid);
    if (status == 0) {
	status = do_aklog(mechanism->mechanismArg);
	pthread_setugid_np(KAUTH_UID_NONE, KAUTH_GID_NONE);
	if (status)
	    goto failed;
    }

    status = 
	mechanism->plugin->callbacks->SetResult(mechanism->engine,
						kAuthorizationResultAllow);

    if (!status)
	return errAuthorizationSuccess;

failed:
    return errAuthorizationInternal;
}

static OSStatus pluginDestroy(AuthorizationPluginRef inPlugin)
{
    /* seems to not be called. can't do cleanup? */
    PluginRef *plugin = (PluginRef *)inPlugin;
    free(plugin);
    return 0;
}

static OSStatus mechanismCreate(AuthorizationPluginRef inPlugin,
        AuthorizationEngineRef inEngine,
        AuthorizationMechanismId mechanismId,
        AuthorizationMechanismRef *outMechanism)
{
    const PluginRef *plugin = (const PluginRef *)inPlugin;

    MechanismRef *mechanism = calloc(1, sizeof(MechanismRef));

    mechanism->plugin = plugin;
    mechanism->engine = inEngine;
    /*
     * consider supporting a variant which backgrounds aklog and returns
     * success where tokens are desired but not critical.
     */
    mechanism->mechanismArg = strdup(mechanismId);

    *outMechanism = mechanism;

    return 0;
}


static OSStatus mechanismInvoke(AuthorizationMechanismRef inMechanism)
{
    MechanismRef *mechanism = (MechanismRef *)inMechanism;
    OSStatus status;

    status = invokeAklog(mechanism);
    if (status)
	return errAuthorizationInternal;

    /* mark success */
    return mechanism->plugin->callbacks->SetResult(mechanism->engine, kAuthorizationResultAllow);
}


/**
 *   Since a authorization result is provided within invoke, we don't have to 
 *   cancel a long(er) term operation that might have been spawned.
 *   A timeout could be done here.
 */
static OSStatus mechanismDeactivate(AuthorizationMechanismRef inMechanism)
{
    return 0;
}


static OSStatus mechanismDestroy(AuthorizationMechanismRef inMechanism)
{
    MechanismRef *mechanism = (MechanismRef *)inMechanism;
    free(mechanism->mechanismArg);
    free(mechanism);

    return 0;
}


AuthorizationPluginInterface pluginInterface =
{
    kAuthorizationPluginInterfaceVersion, /* UInt32 version; */
    pluginDestroy,
    mechanismCreate,
    mechanismInvoke,
    mechanismDeactivate,
    mechanismDestroy
};


/**
 *  Entry point for all plugins.  Plugin and the host loading it exchange interfaces.
 *  Normally you'd allocate resources shared amongst all mechanisms here.
 *  When a plugin is created it may not necessarily be used, so be conservative
 */
OSStatus AuthorizationPluginCreate(const AuthorizationCallbacks *callbacks,
    AuthorizationPluginRef *outPlugin,
    const AuthorizationPluginInterface **outPluginInterface)
{
    PluginRef *plugin = calloc(1, sizeof(PluginRef));

    plugin->callbacks = callbacks;
    *outPlugin = (AuthorizationPluginRef) plugin;
    *outPluginInterface = &pluginInterface;
    return 0;
}
