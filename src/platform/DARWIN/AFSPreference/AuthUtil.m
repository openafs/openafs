//
//  AuthUtil.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 21/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "AuthUtil.h"
//#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
static AuthUtil *sharedAuthUtil = nil;

@implementation AuthUtil

+(AuthUtil*) shared
{
	@synchronized(self) {
        if (sharedAuthUtil == nil) {
            [[self alloc] init]; // assignment not done here
        }
    }
    return sharedAuthUtil;
}

+ (id)allocWithZone:(NSZone *)zone
{
    @synchronized(self) {
        if (sharedAuthUtil == nil) {
            sharedAuthUtil = [super allocWithZone:zone];
            return sharedAuthUtil;  // assignment and return on first allocation
        }
    }
    return nil; //on subsequent allocation attempts return nil
}

- (id)copyWithZone:(NSZone *)zone
{
    return self;
}

// -------------------------------------------------------------------------------
//  init:
// -------------------------------------------------------------------------------
-(id) init {
	[super init];
	authorizationRef = nil; 
	return self;
}

// -------------------------------------------------------------------------------
//  autorize:
// -------------------------------------------------------------------------------
-(OSStatus) autorize
{
	OSStatus status = noErr;
	AuthorizationFlags flags;
	AuthorizationItem myItems = {kAuthorizationRightExecute, 0, NULL, 0};
	AuthorizationRights myRights = {1, &myItems};
	flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;
	
	/*if(authorizationRef) {
		[self deautorize];
	}*/
	
	// chek if autorization is valid for and old password request
	status = AuthorizationCopyRights (authorizationRef, &myRights, NULL, flags, NULL );

	if (status != errAuthorizationSuccess) { 
		//
		flags = kAuthorizationFlagDefaults;
		if(!authorizationRef){
			status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, flags, &authorizationRef);
			if (status != errAuthorizationSuccess) {
				return status;
			}
		}
		
		flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;
		status = AuthorizationCopyRights (authorizationRef, &myRights, NULL, flags, NULL );

		if (status != errAuthorizationSuccess) {
			AuthorizationFree (authorizationRef, kAuthorizationFlagDefaults);
			return status;
		}
	}

	return status;
}


// -------------------------------------------------------------------------------
//  deatorize:
// -------------------------------------------------------------------------------
-(BOOL) deautorize
{
	OSStatus status = 0L;
	if(authorizationRef){
		status = AuthorizationFree (authorizationRef, kAuthorizationFlagDefaults);
		authorizationRef = 0L; 
	}
	return status == noErr;
}


// -------------------------------------------------------------------------------
//  authorization:
// -------------------------------------------------------------------------------
-(AuthorizationRef) authorization
{
	return authorizationRef;
}

// -------------------------------------------------------------------------------
//  authorization:
// -------------------------------------------------------------------------------
-(NSData*) extFormAuth {
	AuthorizationExternalForm extAuth; // external authorization
	NSData *authorizationData = nil;
	if(AuthorizationMakeExternalForm([self authorization], &extAuth))
    {
        NSLog(@"Could not create external authorization form.");
		return nil;
	}
	authorizationData = [NSData dataWithBytes:&extAuth length:sizeof(AuthorizationExternalForm)];
	return authorizationData;
}

// -------------------------------------------------------------------------------
//  execUnixCommand:
// -------------------------------------------------------------------------------
-(OSStatus) execUnixCommand:(const char*) commandPath args:(const char*[])args output:(NSMutableString*)output
{
	OSStatus status = noErr;
	FILE *commandOutIn = NULL;
	char buff[1024];
	int pid, pidStatus;

	status = AuthorizationExecuteWithPrivileges (authorizationRef, commandPath, kAuthorizationFlagDefaults , (char *const *)args, &commandOutIn);
	if (status == errAuthorizationSuccess && commandOutIn){
		for(;;)
		{
			int bytesRead = read(fileno (commandOutIn), buff, sizeof (buff));
			if (bytesRead < 1) break;
			//write (fileno (stdout), buff, bytesRead);
			if(output) {
				[output appendString:[NSString stringWithCString:buff  encoding:NSUTF8StringEncoding]];
			}
		}
	}
	if(commandOutIn){
		fflush(commandOutIn);
		fclose(commandOutIn);
	}
	// whait for comand finish
	pid = wait( &pidStatus );
	if (pid == -1 || !WIFEXITED( pidStatus ))
	{
		NSLog( @"Error: problem with wait pid status: %d", pidStatus );
		return 1;
	}
	return status;
}

- (id)retain
{
    return self;
}

- (unsigned)retainCount
{
    return UINT_MAX;  //denotes an object that cannot be released
}

- (void)release
{
    //do nothing
}

- (id)autorelease
{
    return self;
}
@end
