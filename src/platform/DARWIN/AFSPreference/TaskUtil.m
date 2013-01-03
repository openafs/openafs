//
//  TaskUtil.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 25/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

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
	NSDictionary *environment =  [NSDictionary dictionaryWithObjectsAndKeys: @"$PATH:/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin",@"PATH",nil];
		
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


// -------------------------------------------------------------------------------
//  executeTask:
// -------------------------------------------------------------------------------
+(int) executeTaskWithAuth:(NSString*) taskName arguments:(NSArray *)args helper:(NSString *)helper withAuthRef:(AuthorizationRef)authRef {
    const char *rootHelperApp = [helper fileSystemRepresentation];
    OSStatus status;
    AuthorizationFlags flags = kAuthorizationFlagDefaults;
    int count = [args count];
    char **myArguments = calloc(count + 2, sizeof(char *));
    int i=0;

    myArguments[0] = strdup([taskName UTF8String]);
    for(i=0;i < count;i++) {
	const char *string = [[args objectAtIndex:i] UTF8String];
	if(!string)
	    break;
	myArguments[1+i] = strdup(string);
    }
    myArguments[1+i] = NULL;

    // should use SMJobBless but we need to sign things...
    status = AuthorizationExecuteWithPrivileges(authRef, rootHelperApp, flags, myArguments, NULL);

    i = 0;
    while (myArguments[i] != NULL) {
        free(myArguments[i]);
        i++;
    }

    free(myArguments);
    return status;
}

+(int) executeTaskWithAuth:(NSString*) taskName arguments:(NSArray *)args authExtForm:(NSData*)auth {
	NSString *result = nil;
	int status = 0;
	NSFileHandle *file = nil;
	NSDictionary *environment =  [NSDictionary dictionaryWithObjectsAndKeys: @"$PATH:/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin",@"PATH",nil];
	
	NSPipe *pipe = [NSPipe pipe];
	NSPipe *pipeIn = [NSPipe pipe];
	NSTask *taskToRun  = [[NSTask alloc] init];
	
	[taskToRun setLaunchPath: taskName];
	[taskToRun setArguments: args];
	[taskToRun setEnvironment:environment];
	[taskToRun setStandardOutput: pipe];
	[taskToRun setStandardInput: pipeIn];
	file = [pipe fileHandleForReading];
	//Write to standard in
	[taskToRun launch];
	
	NSFileHandle* taskInput = [[ taskToRun standardInput ] fileHandleForWriting ];
	[taskInput writeData:auth];
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
			[result release];
		}
	} else {
	}
	
	return status;
}
@end
