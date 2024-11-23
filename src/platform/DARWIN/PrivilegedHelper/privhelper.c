/*
 * Copyright (c) 2022 Sine Nomine Associates. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
 * Tool that performs privileged operations on behalf of Prefpane.
 */

#include <syslog.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <spawn.h>

#include <xpc/xpc.h>
#include <Security/Security.h>
#include <Security/Authorization.h>

#include <mach/message.h>
#include <CoreFoundation/CoreFoundation.h>

#include <xpc/xpc.h>

#define LAUNCHCTL   "/bin/launchctl"

#define PRIVHELPER_ID	"org.openafs.privhelper"
#define AFS_ID		"org.openafs.filesystems.afs"
#define AFS_PLIST	"/Library/LaunchDaemons/org.openafs.filesystems.afs.plist"

/*
 * This is the code signing requirement imposed on anyone that connects to our
 * XPC service.
 *
 * com.apple.systempreferences.legacyLoader.{arm64, arm64} and
 * com.apple.systempreferences.legacyLoader refer to the "legacyloader"
 * program that runs our prefpane code (signed by apple).
 *
 * it.infn.lnf.network.{openafs, AFSMenuExtra, AFSBackgrounder} refers to our
 * AFSBackgrounder; the menubar provided by it may talk to privhelper.
 *
 * "certificate 1[field.1.2.840.113635.100.6.2.6]" means the code signature is
 * signed by the Apple Developer ID cert authority.
 *
 * "certificate leaf[field.1.2.840.113635.100.6.1.13]" means the code is a
 * Developer ID application.
 *
 * "certificate leaf[subject.OU] = @MACOS_TEAM_ID@" means the code was signed
 * by us.
 *
 * Replace @MACOS_TEAM_ID@ by your team ID. For example:
 *     "certificate leaf[subject.OU] = SKMME9E2Y8"
 */
#define CLI_SIGNATURES	"((identifier \"com.apple.systempreferences.legacyLoader.x86_64\" and anchor apple) or " \
			 "(identifier \"com.apple.systempreferences.legacyLoader.arm64\" and anchor apple) or " \
			 "(identifier \"com.apple.systempreferences.legacyLoader\" and anchor apple) or " \
			 "(identifier \"it.infn.lnf.network.openafs\" and anchor apple generic and " \
			  "certificate 1[field.1.2.840.113635.100.6.2.6] /* exists */ and " \
			  "certificate leaf[field.1.2.840.113635.100.6.1.13] /* exists */ and " \
			  "certificate leaf[subject.OU] = @MACOS_TEAM_ID@) or " \
			 "(identifier \"it.infn.lnf.network.AFSMenuExtra\" and anchor apple generic and " \
			  "certificate 1[field.1.2.840.113635.100.6.2.6] /* exists */ and " \
			  "certificate leaf[field.1.2.840.113635.100.6.1.13] /* exists */ and " \
			  "certificate leaf[subject.OU] = @MACOS_TEAM_ID@) or " \
			 "(identifier \"it.infn.lnf.network.AFSBackgrounder\" and anchor apple generic and " \
			  "certificate 1[field.1.2.840.113635.100.6.2.6] /* exists */ and " \
			  "certificate leaf[field.1.2.840.113635.100.6.1.13] /* exists */ and " \
			  "certificate leaf[subject.OU] = @MACOS_TEAM_ID@))"

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 120000
/* macOS 12.0 introduced xpc_connection_set_peer_code_signing_requirement(). */
# define HAVE_XPC_CONNECTION_SET_PEER_CODE_SIGNING_REQUIREMENT
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 110000
/* macOS 11.0 introduced SecCodeCreateWithXPCMessage(). */
# define HAVE_SECCODECREATEWITHXPCMESSAGE
#endif

static void
mem_error(void)
{
    syslog(LOG_ERR, "%s: Memory alloc failed", PRIVHELPER_ID);
}

#if !defined(HAVE_SECCODECREATEWITHXPCMESSAGE)
/*
 * We need xpc_dictionary_get_audit_token() to implement our own version of
 * SecCodeCreateWithXPCMessage(). This isn't declared in public headers, but we
 * can use it if we declare it ourselves.
 */
extern void xpc_dictionary_get_audit_token(xpc_object_t, audit_token_t *);

/*
 * Our own version of SecCodeCreateWithXPCMessage(), implemented in terms of
 * xpc_dictionary_get_audit_token() and SecCodeCopyGuestWithAttributes().
 *
 * This gets the SecCodeRef of the peer that sent us the XPC message 'event',
 * and returns it in *a_codeRef on success.
 */
static OSStatus
SecCodeCreateWithXPCMessage(xpc_object_t event, SecCSFlags secflags, SecCodeRef *a_codeRef)
{
    OSStatus status;
    audit_token_t auditToken;
    CFDataRef tokenData = NULL;
    CFDictionaryRef attr = NULL;

    xpc_dictionary_get_audit_token(event, &auditToken);

    tokenData = CFDataCreate(NULL, (const UInt8 *)&auditToken, sizeof(auditToken));
    if (tokenData == NULL) {
	mem_error();
	status = errSecAllocate;
	goto done;
    }

    const void *keys[] = {
	kSecGuestAttributeAudit
    };
    const void *values[] = {
	tokenData
    };
    attr = CFDictionaryCreate(NULL, keys, values, 1,
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    if (attr == NULL) {
	mem_error();
	status = errSecAllocate;
	goto done;
    }

    status = SecCodeCopyGuestWithAttributes(NULL, attr, secflags, a_codeRef);

 done:
    if (attr != NULL) {
	CFRelease(attr);
    }
    if (tokenData != NULL) {
	CFRelease(tokenData);
    }

    return status;
}
#endif /* !HAVE_SECCODECREATEWITHXPCMESSAGE */

static int
RunCommand(int log_failure, const char *arg1, const char *arg2,
	   const char *arg3, const char *arg4)
{
    extern char **environ;
    pid_t pid;
    int code;
    int status;
    const char *argv[] = {
	arg1, arg2, arg3, arg4, NULL
    };

    code = posix_spawn(&pid, argv[0], NULL, NULL, argv, environ);
    if (code != 0) {
	syslog(LOG_ERR, "%s: Failed to execute %s, error %d", PRIVHELPER_ID,
	       argv[0], code);
	return -1;
    }

    code = waitpid(pid, &status, 0);
    if (code <= 0) {
	syslog(LOG_ERR, "%s: waitpid for %s failed, error %d/%d",
	       PRIVHELPER_ID, argv[0], code, errno);
	return -1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
	/* Command exited successfully. */
	return 0;
    }

    if (log_failure) {
	syslog(LOG_WARNING, "%s: Command %s failed, status %d",
	       PRIVHELPER_ID, argv[0], status);
    }

    return status;
}

/**
 * Run the requested privileged task.
 *
 * We implement the following tasks:
 *
 * - startup_enable: Run "launchctl load" to configure the OpenAFS client to
 *   run at startup.
 *
 * - startup_disable: Run "launchctl unload" to configure the OpenAFS client to
 *   not run at startup.
 *
 * - startup_check: Run "launchctl list" to check whether OpenAFS is configured
 *   to run at startup. If it is, return 0; otherwise, return nonzero.
 *
 * @param[in] task	The name of the requested task.
 * @param[in] event	The XPC dictionary containing other task-specific
 *			arguments.
 *
 * @returns Return status of the task
 * @retval 0 success
 * @retval -1 internal error
 * @retval nonzero The return value of system() of a failed command for the
 *                 task.
 */
static int
ProcessRequest(const char *task, xpc_object_t event)
{
    if (strcmp(task, "startup_enable") == 0) {
	return RunCommand(1, LAUNCHCTL, "load", "-w", AFS_PLIST);

    } else if (strcmp(task, "startup_disable") == 0) {
	return RunCommand(1, LAUNCHCTL, "unload", "-w", AFS_PLIST);

    } else if (strcmp(task, "startup_check") == 0) {
	return RunCommand(0, LAUNCHCTL, "list", AFS_ID, NULL);
    }

    syslog(LOG_WARNING, "%s: Received unknown task '%s'", PRIVHELPER_ID, task);

    return -1;
}

/*
 * Is the task for the given event authorized to run commands as root? Check
 * the AuthorizationExternalForm in event["auth"], and see if it is authorized
 * for running commands as root.
 *
 * Returns 1 if the task is authorized, 0 otherwise.
 */
static int
IsEventAuthorized(xpc_object_t event)
{
    OSStatus status;
    int authorized = 0;

    const void *auth_data;
    size_t data_size = 0;

    AuthorizationRef authRef;
    AuthorizationExternalForm extForm;

    /*
     * Check that the given authRef has the kAuthorizationRightExecute right
     * (the right to execute commands as root).
     */
    AuthorizationItem authItems[] = {
	{ kAuthorizationRightExecute, 0, NULL, 0 }
    };
    AuthorizationRights rights = {
	1, authItems
    };

    auth_data = xpc_dictionary_get_data(event, "auth", &data_size);
    if (auth_data == NULL || data_size != sizeof(AuthorizationExternalForm)) {
	syslog(LOG_WARNING, "%s: Authorization not found.", PRIVHELPER_ID);
	return authorized;
    }

    memcpy(&extForm, auth_data, sizeof(extForm));

    status = AuthorizationCreateFromExternalForm(&extForm, &authRef);
    if (status != errAuthorizationSuccess) {
	syslog(LOG_ERR, "%s: Deserialization failed.\n", PRIVHELPER_ID);
	return authorized;
    }

    status = AuthorizationCopyRights(authRef, &rights,
				     kAuthorizationEmptyEnvironment,
				     kAuthorizationFlagDefaults, NULL);
    AuthorizationFree(authRef, kAuthorizationFlagDefaults);
    if (status == errAuthorizationSuccess) {
	authorized = 1;
    }
    return authorized;
}

#if defined(HAVE_XPC_CONNECTION_SET_PEER_CODE_SIGNING_REQUIREMENT)

/*
 * Set the given code signing requirements for the given service.
 *
 * 'req_cf' is the requirement string as a CFStringRef.
 */
static int
SetCodeReq(xpc_connection_t service, CFStringRef req_cf)
{
    const char *req_c = CFStringGetCStringPtr(req_cf, kCFStringEncodingUTF8);
    if (req_c == NULL) {
	syslog(LOG_ERR, "%s: Could not convert CFStringRef.", PRIVHELPER_ID);
	return -1;
    }
    if (xpc_connection_set_peer_code_signing_requirement(service, req_c) != 0) {
	syslog(LOG_ERR, "%s: Could not set peer code signing requirement.", PRIVHELPER_ID);
	return -1;
    }
    return 0;
}

/*
 * Is the xpc conn for the given event authorized to connect at all? This
 * checks whether the peer satisfies the code signing requirements given in
 * SetCodeReq().
 *
 * Returns 1 if the connection is authorized, 0 otherwise.
 */
static int
IsConnAuthorized(xpc_object_t event)
{
    /*
     * xpc_connection_set_peer_code_signing_requirement() ensures that all
     * incoming connections and messages are validated against the given
     * code-signing requirement. So we don't need to do any checks ourselves.
     */
    return 1;
}

#else /* HAVE_XPC_CONNECTION_SET_PEER_CODE_SIGNING_REQUIREMENT */

/* Our compiled code security requirements, which we use to check every
 * connection that comes in. */
static SecRequirementRef secRequirementRef;

static int
SetCodeReq(xpc_connection_t service, CFStringRef req_cf)
{
    OSStatus code;
    code = SecRequirementCreateWithString(req_cf,
					  kSecCSDefaultFlags,
					  &secRequirementRef);
    if (code != errSecSuccess || secRequirementRef == NULL) {
	syslog(LOG_ERR, "%s: Could not compile code sec requirement.", PRIVHELPER_ID);
	return -1;
    }
    return 0;
}

static int
IsConnAuthorized(xpc_object_t event)
{
    SecCodeRef codeRef = NULL;
    OSStatus status;

    status = SecCodeCreateWithXPCMessage(event, kSecCSDefaultFlags, &codeRef);
    if (status != errSecSuccess || codeRef == NULL) {
	return 0;
    }

    status = SecCodeCheckValidity(codeRef, kSecCSDefaultFlags, secRequirementRef);
    CFRelease(codeRef);
    if (status != errSecSuccess) {
	return 0;
    }

    return 1;
}

#endif /* HAVE_XPC_CONNECTION_SET_PEER_CODE_SIGNING_REQUIREMENT */

static void
XPCEventHandler(xpc_connection_t conn, xpc_object_t event)
{
    int status;
    xpc_type_t type;
    xpc_object_t reply;
    xpc_connection_t client;
    const char *task;

    type = xpc_get_type(event);
    if (type == XPC_TYPE_ERROR) {
	syslog(LOG_WARNING, "%s: Could not get event type.", PRIVHELPER_ID);
	return;
    }
    if (!IsConnAuthorized(event)) {
	syslog(LOG_WARNING, "%s: Connection not authorized.", PRIVHELPER_ID);
	return;
    }
    if (!IsEventAuthorized(event)) {
	syslog(LOG_WARNING, "%s: Event not authorized.", PRIVHELPER_ID);
	return;
    }

    task = xpc_dictionary_get_string(event, "task");
    if (task == NULL) {
	syslog(LOG_WARNING, "%s: Could not get event task.", PRIVHELPER_ID);
	return;
    }

    status = ProcessRequest(task, event);

    reply = xpc_dictionary_create_reply(event);
    if (reply == NULL) {
	syslog(LOG_ERR, "%s: Could not create reply for event.", PRIVHELPER_ID);
	return;
    }

    xpc_dictionary_set_int64(reply, "status", status);

    client = xpc_dictionary_get_remote_connection(event);
    if (client == NULL) {
	syslog(LOG_ERR, "%s: No remote connection for event.", PRIVHELPER_ID);
	xpc_release(reply);
	return;
    }

    xpc_connection_send_message(client, reply);
    xpc_release(reply);
}

static void
XPCConnectionHandler(xpc_connection_t conn)
{
    xpc_connection_set_event_handler(conn, ^(xpc_object_t event) {
	XPCEventHandler(conn, event);
    });
    xpc_connection_resume(conn);
}

int
main(int argc, char *argv[])
{
    xpc_connection_t service;
    dispatch_queue_t targetq = dispatch_get_main_queue();
    uint64_t flags = XPC_CONNECTION_MACH_SERVICE_LISTENER;

    service = xpc_connection_create_mach_service(PRIVHELPER_ID, targetq, flags);
    if (service == NULL) {
	syslog(LOG_ERR, "%s: Could not create service.", PRIVHELPER_ID);
	exit(EXIT_FAILURE);
    }

    if (SetCodeReq(service, CFSTR(CLI_SIGNATURES)) != 0) {
	xpc_release(service);
	exit(EXIT_FAILURE);
    }

    xpc_connection_set_event_handler(service, ^(xpc_object_t conn) {
	XPCConnectionHandler(conn);
    });
    xpc_connection_resume(service);

    dispatch_main();
    xpc_release(service);

    return EXIT_SUCCESS;
}
