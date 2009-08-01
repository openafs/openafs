//
//  PListManager.m
//  OpenAFS
//
//  Created by Claudio Bisegni on 25/04/08.
//  Copyright 2008 Infn. All rights reserved.
//

#import "PListManager.h"
#import "FileUtil.h"
#import "TaskUtil.h"

@implementation PListManager
// -------------------------------------------------------------------------------
//  krb5TiketAtLoginTime:
// -------------------------------------------------------------------------------
+(void) krb5TiketAtLoginTime:(BOOL)enable{
	NSData					*plistData = nil;
	NSString				*error = nil;
	NSString				*toRemove = nil;
	NSString				*toAdd = nil;
	NSPropertyListFormat	format;
	NSMutableDictionary		*plist = nil;
	SInt32					osxMJVers = 0;
	SInt32					osxMnVers = 0;
	FileUtil				*futil = nil;
	
	//check system 
	if (Gestalt(gestaltSystemVersionMajor, &osxMJVers) != noErr || Gestalt(gestaltSystemVersionMinor, &osxMnVers) != noErr) @throw [NSException exceptionWithName:@"PListManager:krb5TiketAtLoginTime" 
																																						   reason:@"Error getting system version"
																																						 userInfo:nil];
	//get auth plist file
	plistData = [NSData dataWithContentsOfFile:AUTH_FILE];
	
	//Get plist for updating with NSPropertyListMutableContainersAndLeaves
	plist = [NSPropertyListSerialization propertyListFromData:plistData
											 mutabilityOption:NSPropertyListMutableContainersAndLeaves
													   format:&format
											 errorDescription:&error];
	if(!plist) {
		@throw [NSException exceptionWithName:@"PListManager:krb5TiketAtLoginTime" 
									   reason:error
									 userInfo:nil];
		
	}
	
	//Get "rights" dic
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
	[mechanismsArray removeObject:toRemove];
	[mechanismsArray addObject:toAdd];
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
	if([futil startAutorization] == noErr) {
		if(![[NSFileManager defaultManager] fileExistsAtPath:AUTH_FILE_BK]) {
			//bk file doesn't exist so make it
			[futil autorizedCopy:AUTH_FILE toPath:AUTH_FILE_BK];
		}
		// chmod on tmp file
		[futil autorizedChown:TMP_FILE owner:@"root" group:@"wheel"];
		//mov ethe file 
		[futil autorizedMoveFile:TMP_FILE toPath:AUTH_FILE_DIR];
	}
	[futil release];
}

// -------------------------------------------------------------------------------
//  checkAklogAtLoginTimeLaunchdEnable:
// -------------------------------------------------------------------------------
+(BOOL) checkKrb5AtLoginTimeLaunchdEnable {
	BOOL result = false;
	NSString *authFileContent = nil;
	authFileContent = [NSString stringWithContentsOfFile:AUTH_FILE];
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
			launchdDic = [[NSMutableDictionary alloc] init];
			
			[launchdDic setObject:@"it.infn.lnf.network.AFSBackgrounder" 
						   forKey:@"Label"];
			
			[launchdDic setObject:@"Aqua"
						   forKey:@"LimitLoadToSessionType"];
			
			[launchdDic setObject:[NSArray arrayWithObject:backgrounderPath]
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
			launchdDic = [[NSMutableDictionary alloc] init];
			//argDic = [[NSMutableArray alloc] init];
			
			[launchdDic setObject:@"it.infn.lnf.afsstartup" 
						   forKey:@"Label"];
			
			//[argDic addObject:afsStartupScript];
			//[argDic addObject:afsBasePath/*"/var/db/openafs"*/];
			//[argDic addObject:afsdPath/*"/usr/sbin/afsd"*/];
			
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

@end
