//
//  PListManager.m
//  OpenAFS
//
//  Created by Claudio Bisegni on 25/04/08.
//  Copyright 2008 Infn. All rights reserved.
//

#import "portability.h"
#import "PListManager.h"
#import "FileUtil.h"
#import "TaskUtil.h"

#define BACKGROUNDER_AGENT_NAME					@"AFSBackgrounder.app/Contents/MacOS/AFSBackgrounder"
#define AD_CONF_FILE @"/Library/Preferences/DirectoryService/ActiveDirectory.plist"
@implementation PListManager
// -------------------------------------------------------------------------------
//  krb5TiketAtLoginTime:
// -------------------------------------------------------------------------------
+(void) krb5TiketAtLoginTime:(BOOL)enable helper:(NSString *)helper
{
	NSData					*plistData = nil;
	NSString				*error = nil;
	NSString				*toRemove = nil;
	NSString				*toAdd = nil;
	NSPropertyListFormat	format;
	NSMutableDictionary		*plist = nil;
	SInt32					osxMJVers = 0;
	SInt32					osxMnVers = 0;
	FileUtil				*futil = nil;
	SInt32                                  object_index = 0;

	// check system
	if (Gestalt(gestaltSystemVersionMajor, &osxMJVers) != noErr || Gestalt(gestaltSystemVersionMinor, &osxMnVers) != noErr) @throw [NSException exceptionWithName:@"PListManager:krb5TiketAtLoginTime" reason:@"Error getting system version" userInfo:nil];

	// are we eligible to run?
	plistData = [NSData dataWithContentsOfFile:AD_CONF_FILE];

	// Get plist for updating with NSPropertyListMutableContainersAndLeaves
	plist = [NSPropertyListSerialization propertyListFromData:plistData mutabilityOption:NSPropertyListMutableContainersAndLeaves format:&format errorDescription:&error];

	if(plist) {
		// Get "AD Advanced Options" dic
		NSMutableDictionary *rightsDic = [plist objectForKey:@"AD Advanced Options"];
		if ([[rightsDic objectForKey:@"AD Generate AuthAuthority"] boolValue])
			return;
	}

	// get auth plist file
	plistData = [NSData dataWithContentsOfFile:AUTH_FILE];

	// Get plist for updating with NSPropertyListMutableContainersAndLeaves
	plist = [NSPropertyListSerialization propertyListFromData:plistData mutabilityOption:NSPropertyListMutableContainersAndLeaves format:&format errorDescription:&error];

	if(!plist) {
		@throw [NSException exceptionWithName:@"PListManager:krb5TiketAtLoginTime" reason:error userInfo:nil];
	}

	// Get "rights" dic
	NSMutableDictionary *rightsDic = [plist objectForKey:@"rights"];

	//Get "system.login.console" dic
	NSMutableDictionary *loginConsoleDic = [rightsDic objectForKey:@"system.login.console"];
	
	//Get "mechanisms" dic
	NSMutableArray *mechanismsArray = [loginConsoleDic objectForKey:@"mechanisms"];
	switch(osxMnVers){
		case 4:
			if(enable){
				//remove
				toRemove = DELETE_IN_10_4;
				toAdd = ADD_IN_PLIST;
			} else {
				toRemove = ADD_IN_PLIST;
				toAdd = DELETE_IN_10_4;
			}
			break;
			
		case 5:
		case 6:
			if(enable){
				//remove
				toRemove = DELETE_IN_10_5;
				toAdd = ADD_IN_PLIST;
			} else {
				toRemove = ADD_IN_PLIST;
				toAdd = DELETE_IN_10_5;
			}
			
			break;
	}
	
	//Make change
	object_index = [mechanismsArray indexOfObject: toRemove];
	[mechanismsArray replaceObjectAtIndex:object_index withObject:toAdd];

	//write plist
	plistData  = [NSPropertyListSerialization dataFromPropertyList:plist
															format:NSPropertyListXMLFormat_v1_0
												  errorDescription:&error];
	if(!plistData) {
		@throw [NSException exceptionWithName:@"PListManager:krb5TiketAtLoginTime" 
									   reason:error
									 userInfo:nil];
		
	}
	if(![plistData writeToFile:TMP_FILE atomically:NO]) {
		@throw [NSException exceptionWithName:@"PListManager:krb5TiketAtLoginTime" 
									   reason:@"Temp file write error"
									 userInfo:nil];
		
	}
	
	//now we can move the file
	futil = [[FileUtil alloc] init];
	if(![[NSFileManager defaultManager] fileExistsAtPath:AUTH_FILE_BK]) {
	    //bk file doesn't exist so make it
	    [futil autorizedCopy:AUTH_FILE toPath:AUTH_FILE_BK];
	}
	// chmod on tmp file
	[futil autorizedChown:TMP_FILE owner:@"root" group:@"wheel"];
	//move the file
	[futil autorizedMoveFile:TMP_FILE toPath:AUTH_FILE_DIR];

	[futil release];
}

// -------------------------------------------------------------------------------
//  checkAklogAtLoginTimeLaunchdEnable:
// -------------------------------------------------------------------------------
+(BOOL) checkKrb5AtLoginTimeLaunchdEnable {
	BOOL result = false;
	NSString *authFileContent = nil;
	authFileContent = [NSString stringWithContentsOfFile:AUTH_FILE 
														  encoding:NSUTF8StringEncoding 
															 error:nil];
	if(authFileContent) {
		result = [authFileContent rangeOfString:ADD_IN_PLIST].location != NSNotFound;	
	}
	return result;
}

	
// -------------------------------------------------------------------------------
//  installLaunchdFile:
// -------------------------------------------------------------------------------
+(void) installBackgrounderLaunchdFile:(BOOL)install resourcePath:(NSString*) rsrcPath {
	NSData				*plistData = nil;
	NSMutableDictionary *launchdDic = nil;	
	NSMutableDictionary *keepAliveDic = nil;
	NSString			*error = nil;
	NSString			*backgrounderPath = [[rsrcPath stringByAppendingString:@"/"] stringByAppendingString:BACKGROUNDER_AGENT_NAME];
	
	
	if(![[NSFileManager defaultManager] fileExistsAtPath:[HOME_LAUNCHD_AGENT_FOLDER stringByExpandingTildeInPath]]) {
		@throw [NSException exceptionWithName:@"PListManager:installLaunchdFile" 
									   reason:@"The folder ~/Library/LaunchAgent doesn't exist!"
									 userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithBool:YES] 
																		  forKey:@"agent_folder_error"]];
	}
	
	if(install) {
		if(![[NSFileManager defaultManager] fileExistsAtPath:[BACKGROUNDER_LAUNCHD_CONTROL_FILE stringByExpandingTildeInPath]]) {
			launchdDic = [[[NSMutableDictionary alloc] init] autorelease];
			keepAliveDic = [[[NSMutableDictionary alloc] init] autorelease];
			
			[launchdDic setObject:@"it.infn.lnf.network.AFSBackgrounder" 
						   forKey:@"Label"];
			
			[keepAliveDic setObject:[NSNumber numberWithBool:NO]
							 forKey:@"SuccessfulExit"];
			
			[launchdDic setObject:keepAliveDic 
						   forKey:@"KeepAlive"];
			
			[launchdDic setObject:@"Aqua"
						   forKey:@"LimitLoadToSessionType"];
			
			/*[launchdDic setObject:[NSArray arrayWithObject:backgrounderPath]
						   forKey:@"ProgramArguments"];*/
			[launchdDic setObject:backgrounderPath
						   forKey:@"Program"];
			[launchdDic setObject:[NSNumber numberWithBool:YES] 
						   forKey:@"RunAtLoad"];
			
			plistData  = [NSPropertyListSerialization dataFromPropertyList:launchdDic
																	format:NSPropertyListXMLFormat_v1_0
														  errorDescription:&error];
			
			if(!plistData) {
				@throw [NSException exceptionWithName:@"PListManager:installLaunchdFile" 
											   reason:error
											 userInfo:nil];
				
			}
			
			if(![plistData writeToFile:BACKGROUNDER_LAUNCHD_TMP_CONTROL_FILE atomically:NO]) {
				@throw [NSException exceptionWithName:@"PListManager:installLaunchdFile" 
											   reason:@"Temp file write error"
											 userInfo:nil];
				
			}
			
			//now we can move the file
			[TaskUtil executeTaskSearchingPath:@"mv" args:[NSArray arrayWithObjects:BACKGROUNDER_LAUNCHD_TMP_CONTROL_FILE, [BACKGROUNDER_LAUNCHD_CONTROL_FILE stringByExpandingTildeInPath], nil]];
		}
	} else {
		// delete launchd configuration file
		[TaskUtil executeTaskSearchingPath:@"rm" args:[NSArray arrayWithObjects:[BACKGROUNDER_LAUNCHD_CONTROL_FILE stringByExpandingTildeInPath], nil]];
	}
	
}

// -------------------------------------------------------------------------------
//  checkAklogAtLoginTimeLaunchdEnable:
// -------------------------------------------------------------------------------
+(BOOL) checkLoginTimeLaunchdBackgrounder {
	BOOL result = [[NSFileManager defaultManager] fileExistsAtPath:[BACKGROUNDER_LAUNCHD_CONTROL_FILE stringByExpandingTildeInPath]];
	return result;
}

// -------------------------------------------------------------------------------
//  installAfsStartupLaunchdFile:
// -------------------------------------------------------------------------------
+(void) manageAfsStartupLaunchdFile:(BOOL)enable 
		   afsStartupScript:(NSString*)afsStartupScript
			afsBasePath:(NSString*)afsBasePath
			   afsdPath:(NSString*)afsdPath {
	NSData				*plistData = nil;
	NSMutableDictionary *launchdDic = nil;
	NSString			*error = nil;
	OSErr				status = noErr;
	
	
	if(![[NSFileManager defaultManager] fileExistsAtPath:[LAUNCHD_DAEMON_FOLDER stringByExpandingTildeInPath]]) {
		@throw [NSException exceptionWithName:@"PListManager:installAfsStartupLaunchdFile" 
									   reason:@"The folder /Library/LaunchDaemon doesn't exist!"
									 userInfo:nil];
	}
	
	status = [[AuthUtil shared] autorize];
	if(status != noErr)@throw [NSException exceptionWithName:@"PListManager:installAfsStartupLaunchdFile" 
													  reason:@"Autorization Error"
													userInfo:nil];
	
	if(enable) {
		//Check first if the launchd configuration file for startup is present
		if(![[NSFileManager defaultManager] fileExistsAtPath:[AFS_STARTUP_CONTROL_FILE stringByExpandingTildeInPath]]) {
			launchdDic = [[[NSMutableDictionary alloc] init] autorelease];
			//argDic = [[NSMutableArray alloc] init];
			
			[launchdDic setObject:@"it.infn.lnf.afsstartup" 
						   forKey:@"Label"];
			
			
			[launchdDic setObject:[NSArray arrayWithObjects:afsStartupScript, afsBasePath, afsdPath, nil]
						   forKey:@"ProgramArguments"];
			
			[launchdDic setObject:[NSNumber numberWithBool:YES] 
						   forKey:@"RunAtLoad"];
			
			plistData  = [NSPropertyListSerialization dataFromPropertyList:launchdDic
																	format:NSPropertyListXMLFormat_v1_0
														  errorDescription:&error];
			
			if(!plistData) {
				@throw [NSException exceptionWithName:@"PListManager:installLaunchdFile" 
											   reason:error
											 userInfo:nil];
				
			}
			
			if(![plistData writeToFile:AFS_STARTUP_TMP_CONTROL_FILE atomically:NO]) {
				@throw [NSException exceptionWithName:@"PListManager:installLaunchdFile" 
											   reason:@"Temp file write error"
											 userInfo:nil];
				
			}
			
			//now we can move the file
			[TaskUtil executeTaskSearchingPath:@"mv" args:[NSArray arrayWithObjects:AFS_STARTUP_TMP_CONTROL_FILE, [LAUNCHD_DAEMON_FOLDER stringByExpandingTildeInPath], nil]];
		}
	}
	
}

// -------------------------------------------------------------------------------
//  launchctlCommand:
// -------------------------------------------------------------------------------
+(void) launchctlCommand:(BOOL)enable
	      userDomain:(BOOL)userDomain
		  option:(NSArray*)option
	       plistName:(NSString*)plistName
{
	NSMutableArray *argument = [NSMutableArray array];
	NSMutableString *commandPath = [NSMutableString stringWithCapacity:0];
	NSUInteger searchDomain = userDomain?NSUserDomainMask:NSSystemDomainMask;
	//
	NSArray *libraryPath = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, searchDomain,  YES);
	//set the load unload
	[argument addObject:enable?@"load":@"unload"];

	//if there are load the user custo option
	if(option) [argument addObjectsFromArray:option];
		
	//construct the path
	[commandPath appendString:[libraryPath objectAtIndex:0]];
	[commandPath appendFormat:@"/LaunchAgents/%@", plistName];
	
	[argument addObject:commandPath];

	//exec the command
	[TaskUtil executeTask:@"/bin/launchctl"
		  arguments:argument];
}

// -------------------------------------------------------------------------------
//  launchctlCommand:
// -------------------------------------------------------------------------------
+(void) launchctlStringCommandAuth:(NSString *)operation
			    option:(NSArray *)option
			 plistName:(NSString *)plistName
			    helper:(NSString *)helper
		       withAuthRef:(AuthorizationRef)authRef
{
	NSMutableArray *argument = [NSMutableArray array];

	//set the load unload
	[argument addObject:operation];

	//if there are load the user custom option
	if(option) [argument addObjectsFromArray:option];

	//construct the path
	[argument addObject: plistName];

	//exec the command
	[TaskUtil executeTaskWithAuth:@"/bin/launchctl"
		  arguments:argument helper:helper withAuthRef:authRef];
}

// -------------------------------------------------------------------------------
//  launchdJobState:
// -------------------------------------------------------------------------------
+(BOOL) launchdJobState:(NSString*)jobName {
	NSMutableArray *argument = [NSMutableArray array];
	
	//set the load unload
	[argument addObject:@"list"];
	[argument addObject:jobName];
	//exec the command
	NSString *taskResult =[TaskUtil executeTaskSearchingPath:@"launchctl"  
														args:argument];
	return taskResult && [taskResult rangeOfString:jobName].location != NSNotFound;
}
@end
