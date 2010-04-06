//
//  afsltd.m
//  OpenAFS
//
//  Created by Claudio Bisegni LNF-INFN on 26/04/08.
//  Copyright 2008 Infn. All rights reserved.
//
#import "global.h"
#import "AFSPropertyManager.h"
#include <unistd.h>

BOOL useAklog = NO;
NSString *afsSysPath = nil;
AFSPropertyManager *propManager = nil;

void readPreferenceFile();
void makeAFSPropertyManager();
void callAKlog();

int main(int argc, char *argv[])
{
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSLog(@"Start AFS Preference Daemon");
	// read the preference file
	readPreferenceFile();
	
	//check if base afs path is set
	if(!afsSysPath) return 1;
	
	//Sleep ten seconds
	sleep(10);
	
	//make the afs property manager and load all afs configuration
	makeAFSPropertyManager();
	
	//call aklog
	callAKlog();
	if(propManager) [propManager release];
	
	[pool release];
	return 0;
}

// -------------------------------------------------------------------------------
//  readPreferenceFile:
// -------------------------------------------------------------------------------
void readPreferenceFile()
{
	// read the preference for afs path
	afsSysPath = (NSString*)CFPreferencesCopyAppValue((CFStringRef)PREFERENCE_AFS_SYS_PAT, (CFStringRef)kAfsCommanderID);
	
	
	// read the preference for aklog use
	NSNumber *useAklogPrefValue = (NSNumber*)CFPreferencesCopyAppValue((CFStringRef)PREFERENCE_USE_AKLOG, (CFStringRef)kAfsCommanderID);
	useAklog = [useAklogPrefValue boolValue];
	
}

// -------------------------------------------------------------------------------
//  makeAFSPropertyManager:
// -------------------------------------------------------------------------------
void makeAFSPropertyManager() {
	if(propManager) {
		[propManager release];
	}
	propManager = [[AFSPropertyManager alloc] initWithAfsPath:afsSysPath];
	[propManager loadConfiguration];
}

// -------------------------------------------------------------------------------
//  makeAFSPropertyManager:
// -------------------------------------------------------------------------------
void callAKlog() {
	//call aklog for all selected cell, but first check if afs is up
	if([propManager checkAfsStatus] && useAklog) {
		[propManager getTokens:false 
						   usr:nil 
						   pwd:nil];
		
		// send notification to 
		[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kMExtraAFSStateChange];
	}
}