//
//  TaskUtil.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 25/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <ServiceManagement/ServiceManagement.h>
#import <Security/Authorization.h>

#import "TaskUtil.h"
#import "AuthUtil.h"


@implementation TaskUtil

// -------------------------------------------------------------------------------
//  executeTaskSearchingPath:
// -------------------------------------------------------------------------------
+(NSString*) executeTaskSearchingPath:(NSString*)unixCommand args:(NSArray*)args
{
	NSString *commResult = nil;
	NSString *commPath = [self searchExecutablePath:unixCommand];
	if(commPath != nil){
		commResult = [self executeTask:commPath
						   arguments:args];
	}
	return commResult;	
}

// -------------------------------------------------------------------------------
//  executeTask:
// -------------------------------------------------------------------------------
+(NSString*) searchExecutablePath:(NSString*)unixCommand
{
	NSString *commPath = [self executeTask:@"/usr/bin/which" arguments:[NSArray arrayWithObjects:unixCommand, nil]];
	return commPath;	
}

// -------------------------------------------------------------------------------
//  executeTask:
// -------------------------------------------------------------------------------
+(NSString*) executeTask:(NSString*) taskName arguments:(NSArray *)args{
	NSString *result = nil;
	int status = 0;
	NSFileHandle *file = nil;
	NSDictionary *environment = [NSDictionary dictionaryWithObjectsAndKeys: @"$PATH:/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin:/opt/openafs/bin:/opt/openafs/sbin",@"PATH",nil];
		
	NSPipe *pipe = [NSPipe pipe];
	NSTask *taskToRun  = [[NSTask alloc] init];
	
	[taskToRun setLaunchPath: taskName];
	[taskToRun setArguments: args];
	[taskToRun setEnvironment:environment];
	[taskToRun setStandardOutput: pipe];
	file = [pipe fileHandleForReading];
	[taskToRun launch];
	[taskToRun waitUntilExit];
	status = [taskToRun terminationStatus];
	if (status == 0){
		NSData *data = [file readDataToEndOfFile];
		// remove the \n character from unix command
		if([data length] > 0){
			NSData *realData = [NSData dataWithBytes:[data bytes] 
											  length:[data length]-1];
		
			[taskToRun release];
			result = [[NSString alloc] initWithData: realData 
										   encoding: NSUTF8StringEncoding];
		}
	} else {
		NSLog(@"Task failed.");
	}
	return [result autorelease];
}

/**
 * Perform a task that requires root access via privhelper.
 *
 * This performs a task that requires root access, like starting/stopping the
 * client, or updating configuration files like CellServDB. This function sends
 * the request to the privhelper daemon, which performs the actual actions for
 * the task, and waits for a response.
 *
 * @param[in] task  The name of the task to perform (e.g. "startup_enable").
 *		    See privhelper.c:ProcessRequest for what tasks we define.
 * @param[in] filename	Used for the "backup"/"write" tasks. Don't specify this with
 *			executePrivTask; use executePrivTaskBackup or
 *			executePrivTaskWrite instead.
 * @param[in] data	Used for the "write" task. Don't specify this with
 *			executePrivTask; use executePrivTaskWrite instead.
 *
 * @returns Return status of the task
 * @retval 0 success
 * @retval -1 internal error
 * @retval nonzero The return value of system() of a failed command for the
 *                 task.
 */
+(int) executePrivTask:(const char *)task filename:(char *)filename data:(char *)data {
    int status = -1;

    OSErr autherr = [[AuthUtil shared] autorize];
    if (autherr != noErr) {
	NSLog(@"executePrivTask: Could not get authorization: %d.", autherr);
	return status;
    }

    AuthorizationExternalForm extForm;
    AuthorizationRef authRef = [[AuthUtil shared] authorization];
    if (authRef == NULL) {
	NSLog(@"executePrivTask: Authorization reference is null.");
	return status;
    }

    OSStatus code = AuthorizationMakeExternalForm(authRef, &extForm);
    if (code != errAuthorizationSuccess) {
	NSLog(@"executePrivTask: Could not serialize authorization: %d.", code);
	return status;
    }

    const uint64_t flags = XPC_CONNECTION_MACH_SERVICE_PRIVILEGED;
    xpc_connection_t conn = xpc_connection_create_mach_service(PRIVHELPER_ID, NULL, flags);
    if (conn == NULL) {
	NSLog(@"executePrivTask: Could not create service connection.");
	return status;
    }

    xpc_connection_set_event_handler(conn, ^(xpc_object_t event) {
	NSLog(@"executePrivTask: Received unexpected event.");
    });
    xpc_connection_resume(conn);

    xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);
    if (msg == NULL) {
	NSLog(@"executePrivTask: Could not create XPC message.");
	xpc_connection_cancel(conn);
	xpc_release(conn);
	return status;
    }
    xpc_dictionary_set_string(msg, "task", task);
    xpc_dictionary_set_data(msg, "auth", &extForm, sizeof(extForm));
    if (filename != NULL) {
	xpc_dictionary_set_string(msg, "filename", filename);
    }
    if (data != NULL) {
	xpc_dictionary_set_string(msg, "data", data);
    }

    xpc_object_t reply = xpc_connection_send_message_with_reply_sync(conn, msg);
    if (reply == NULL) {
	NSLog(@"executePrivTask: No reply received for task: %s", task);
	xpc_release(msg);
	xpc_connection_cancel(conn);
	xpc_release(conn);
	return status;
    }

    xpc_object_t status_obj = xpc_dictionary_get_value(reply, "status");
    if (status_obj != NULL && xpc_get_type(status_obj) == XPC_TYPE_INT64) {
	status = (int)xpc_int64_get_value(status_obj);
    } else {
	NSLog(@"executePrivTask: Received invalid reply status for task: %s", task);
    }

    xpc_release(msg);
    xpc_release(reply);
    xpc_connection_cancel(conn);
    xpc_release(conn);

    return status;
}

+(int) executePrivTask:(const char *)task {
    return [self executePrivTask:task filename:NULL data:NULL];
}

+(int) executePrivTaskBackup:(NSString *)filename {
    return [self executePrivTask:"backup" filename:[filename UTF8String] data:NULL];
}

+(int) executePrivTaskWrite:(NSString *)filename data:(NSString *)data {
    return [self executePrivTask:"write" filename:[filename UTF8String] data:[data UTF8String]];
}
@end
