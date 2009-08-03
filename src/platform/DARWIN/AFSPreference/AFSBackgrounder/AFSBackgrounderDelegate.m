//
//  AFSBackgrounder.m
//  OpenAFS
//
//  Created by Claudio Bisegni on 29/07/09.
//  Copyright 2009 Infn. All rights reserved.
//

#import "AFSBackgrounderDelegate.h"
#import "AFSMenuExtraView.h"
#import "AFSPropertyManager.h"
#import "TaskUtil.h"
#import "TokenCredentialController.h"
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

@implementation AFSBackgrounderDelegate
#pragma mark NSApp Delegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	NSLog(@"Init the afs manager");
	afsMngr = [[AFSPropertyManager alloc] initWithAfsPath:afsSysPath];
	// allocate the lock for concurent afs check state
	tokensLock = [[NSLock alloc] init];
	
	//remove the auto eanble on menu item
	[backgrounderMenu setAutoenablesItems:NO];
    //Sets the images in our NSStatusItem
	statusItem = nil;
	
  
	// Get the imge for menu
	//Load image for menu rappresentation
	hasTokenImage = [self getImageFromBundle:@"hasToken" 
									 fileExt:@"png"];
	
	noTokenImage = [self getImageFromBundle:@"noToken" 
									fileExt:@"png"];

	//Start to read the afs path
	[self readPreferenceFile:nil];	
	[self startTimer];

	
	
	// Register for preference user change
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(readPreferenceFile:) 
															name:kAFSMenuExtraID object:kPrefChangeNotification];
	
	// Register for afs state change
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(afsVolumeMountChange:) 
															name:kAFSMenuExtraID object:kMExtraAFSStateChange];
	
	// Register for menu state change
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(chageMenuVisibility:) 
															name:kAFSMenuExtraID object:kMExtraAFSMenuChangeState];
	
	//Register for mount/unmount afs volume
	[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self 
														   selector:@selector(afsVolumeMountChange:) 
															   name:NSWorkspaceDidMountNotification object:nil];
	
	[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self 
														   selector:@selector(afsVolumeMountChange:) 
															   name:NSWorkspaceDidUnmountNotification object:nil];
	
	//try to see if we need tho show the menu at startup
	
	[self setStatusItem:[showStatusMenu boolValue]];
	
	NSLog(@"Check if we need to get token at login time %d", [aklogTokenAtLogin intValue]);
	if([aklogTokenAtLogin boolValue] && afsState && !gotToken) {
		NSLog(@"Proceed to get token");
		//check if we must get the aklog at logint(first run of backgrounder
		[self getToken:nil];
	}	
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
	if(afsSysPath) [afsSysPath release];
	
	//release the lock
	[self stopTimer];
	
	if(hasTokenImage) [hasTokenImage release];
	if(noTokenImage) [noTokenImage release];
	
	// Unregister for preference change
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kPrefChangeNotification];
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kMExtraAFSStateChange];
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kMExtraAFSMenuChangeState];
	[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self name:NSWorkspaceDidMountNotification object:nil];
	[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self name:NSWorkspaceDidUnmountNotification object:nil];
	

	// send notify that menuextra has closed
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAfsCommanderID object:kPrefChangeNotification];
	
	if(tokensLock) [tokensLock release];
	if(afsMngr) [afsMngr release];
	return NSTerminateNow;
}
// -------------------------------------------------------------------------------
//  -(void) readPreferenceFile
// -------------------------------------------------------------------------------
- (void) readPreferenceFile:(NSNotification *)notification
{
	NSLog(@"Reading preference file");	
	if(afsSysPath) {
		[afsSysPath release];
		afsSysPath = nil;
	}
	CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesAnyUser, kCFPreferencesAnyHost);
	CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	
	afsSysPath = PREFERENCE_AFS_SYS_PAT_STATIC;
	
	// read the preference for aklog use
	useAklogPrefValue = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_USE_AKLOG, 
														  (CFStringRef)kAfsCommanderID, 
														  kCFPreferencesCurrentUser, 
														  kCFPreferencesAnyHost);
	
	showStatusMenu = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_SHOW_STATUS_MENU, 
													   (CFStringRef)kAfsCommanderID, 
													   kCFPreferencesCurrentUser, 
													   kCFPreferencesAnyHost);
	
	aklogTokenAtLogin = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_AKLOG_TOKEN_AT_LOGIN, (CFStringRef)kAfsCommanderID,  
																	kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

	//set the menu name
	[self updateAfsStatus:nil];
}

// -------------------------------------------------------------------------------
//  - (void)chageMenuVisibility:(NSNotification *)notification
// -------------------------------------------------------------------------------
/**/
- (void)chageMenuVisibility:(NSNotification *)notification {
	NSLog(@"chageMenuVisibility");
	[self readPreferenceFile:nil];
	NSLog(@"showStatusMenu: %d", [showStatusMenu intValue]);
	[self setStatusItem:[showStatusMenu boolValue]];
}

// -------------------------------------------------------------------------------
//  - (void)startStopAfs:(id)sender
// -------------------------------------------------------------------------------
- (void)startStopAfs:(id)sender
{
	NSLog(@"startStopAfs: %s",[afsSysPath UTF8String]);
	if(!afsSysPath) return;
	
	OSStatus status = noErr;
	NSString *afsdPath = [TaskUtil searchExecutablePath:@"afsd"];
	NSString *rootHelperApp = nil;
	BOOL currentAfsState = NO;
	
	@try {
		if(afsdPath == nil) return;
		currentAfsState = [afsMngr checkAfsStatus];
		rootHelperApp = [[NSBundle mainBundle] pathForResource:@"afshlp" ofType:@""];
		
		//[startStopScript setString: resourcePath];
		//NSLog(@"LAunch repair HelperTool");
		//Check helper app
		[self repairHelperTool];
		
		// make the parameter to call the root helper app
		status = [[AuthUtil shared] autorize];
		if(status == noErr){
			if(currentAfsState){
				//shutdown afs
				//NSLog(@"Shutting down afs");
				NSMutableString *afsKextPath = [[NSMutableString alloc] initWithCapacity:256];
				[afsKextPath setString:afsSysPath];
				[afsKextPath appendString:@"/etc/afs.kext"];
				
				//NSLog(@"executeTaskWithAuth");
				const char *stopAfsArgs[] = {"stop_afs", [afsKextPath  UTF8String], [afsdPath UTF8String], 0L};
				[[AuthUtil shared] execUnixCommand:[rootHelperApp UTF8String] 
											  args:stopAfsArgs
											output:nil];
			} else {
				//NSLog(@"Starting up afs");
				const char *startAfsArgs[] = {[[ [NSBundle mainBundle] pathForResource:@"start_afs" ofType:@"sh"]  UTF8String], [afsSysPath UTF8String], [afsdPath UTF8String], 0L};
				[[AuthUtil shared] execUnixCommand:[rootHelperApp UTF8String] 
											  args:startAfsArgs
											output:nil];
			}
		}
	}
	@catch (NSException * e) {
		NSLog([e reason]);
	}
	@finally {
		[[AuthUtil shared] deautorize];
		[self updateAfsStatus:nil];
		//Send notification to preferencepane
		[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAfsCommanderID object:kMenuExtraEventOccured];
	}
	
}

// -------------------------------------------------------------------------------
//  -(void) getToken
// -------------------------------------------------------------------------------
- (void)getToken:(id)sender
{
	
	NSRect globalRect;
	globalRect.origin = [[[statusItem view] window] convertBaseToScreen:[[statusItem view] frame].origin];
	globalRect.size = [[statusItem view] frame].size;
	AFSPropertyManager *afsPropMngr = [[AFSPropertyManager alloc] initWithAfsPath:afsSysPath ];
	[afsPropMngr loadConfiguration]; 
	
	
	if([useAklogPrefValue boolValue]) {
		NSLog(@"Use Aklog");
		[afsPropMngr getTokens:false 
						   usr:nil 
						   pwd:nil];
		[self klogUserEven:nil];
	} else {
		NSLog(@"Use Klog");
		// register for user event
		[[NSDistributedNotificationCenter defaultCenter] addObserver:self 
															selector:@selector(klogUserEven:) 
																name:kAFSMenuExtraID 
															  object:kLogWindowClosed];
		
		credentialMenuController = [[AFSMenuCredentialContoller alloc] initWhitRec:globalRect 
																	afsPropManager:afsPropMngr];
		[credentialMenuController showWindow];
	}
	
	//Dispose afs manager
	[afsPropMngr release];
}

// -------------------------------------------------------------------------------
//  -(void) releaseToken
// -------------------------------------------------------------------------------
- (void)releaseToken:(id)sender
{
	[afsMngr unlog:nil];
	[self updateAfsStatus:nil];
}


// -------------------------------------------------------------------------------
//  -(void) afsVolumeMountChange - Track for mount unmount afs volume
// -------------------------------------------------------------------------------
- (void) afsVolumeMountChange:(NSNotification *)notification{
	[self updateAfsStatus:nil];
}

// -------------------------------------------------------------------------------
//  -(void) updateAfsStatus
// -------------------------------------------------------------------------------
- (void)updateAfsStatus:(NSTimer*)timer
{
	//Try to locking
	if(![tokensLock tryLock]) return;
	
	// check the afs state in esclusive mode
	afsState = [afsMngr checkAfsStatus];
	
	NSArray *tokens = [afsMngr getTokenList];
	gotToken = [tokens count] > 0;
	[tokens release];
	
	//unlock
	[tokensLock unlock];
}

// -------------------------------------------------------------------------------
//  -(void) klogUserEven
// -------------------------------------------------------------------------------
-(void) klogUserEven:(NSNotification *)notification
{
	if(credentialMenuController) {
		[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kLogWindowClosed];
		[credentialMenuController closeWindow];
		[credentialMenuController release];
		credentialMenuController = nil;
	}
	//Send notification to PreferencePane
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAfsCommanderID object:kMenuExtraEventOccured];
	
	[self updateAfsStatus:nil];
}
#pragma mark Operational Function
// -------------------------------------------------------------------------------
//  startTimer:
// -------------------------------------------------------------------------------
- (void)startTimer{
	//start the time for check tokens validity
	if(timerForCheckTokensList) return;
	timerForCheckTokensList = [NSTimer scheduledTimerWithTimeInterval:TOKENS_REFRESH_TIME_IN_SEC 
															   target:self 
															 selector:@selector(updateAfsStatus:) 
															 userInfo:nil 
															  repeats:YES];
	[timerForCheckTokensList fire];	
}

// -------------------------------------------------------------------------------
//  stopTimer:
// -------------------------------------------------------------------------------
- (void)stopTimer{
	if(!timerForCheckTokensList) return;
	[timerForCheckTokensList invalidate];	
	timerForCheckTokensList = nil;
}
// -------------------------------------------------------------------------------
//  -(void) getImageFromBundle
// -------------------------------------------------------------------------------
- (NSImage*)getImageFromBundle:(NSString*)fileName fileExt:(NSString*)ext
{
	return [[NSImage alloc]initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:fileName 
																		  ofType:ext]];
}


// -------------------------------------------------------------------------------
//  - (void)menuNeedsUpdate:(NSMenu *)menu
// -------------------------------------------------------------------------------
- (void)menuNeedsUpdate:(NSMenu *)menu {
	if (menu == backgrounderMenu)
	{
		[startStopMenuItem setTitle:afsState?@"Shutdown AFS":@"Startup AFS"];
		[getReleaseTokenMenuItem setTitle:gotToken?@"Release token":@"Get New Token"];
		[getReleaseTokenMenuItem setEnabled:afsState];
		[[statusItem view] setNeedsDisplay:YES];
		
	}
}


// -------------------------------------------------------------------------------
//  -(void) useAklogPrefValue
// -------------------------------------------------------------------------------
- (BOOL)useAklogPrefValue
{
	if(useAklogPrefValue) return [useAklogPrefValue intValue] == NSOnState; 
	else return NSOffState;
}

// -------------------------------------------------------------------------------
//  repairHelperTool:
// -------------------------------------------------------------------------------
- (void) repairHelperTool
{
	struct stat st;
    int fdTool;
	int status = 0;
	NSLog(@"repairHelperTool"); 
	NSString *afshlpPath = [[NSBundle mainBundle] pathForResource:@"afshlp" ofType:nil];
	
	
    
    // Open tool exclusively, so nobody can change it while we bless it.
    fdTool = open([afshlpPath UTF8String], O_NONBLOCK | O_RDONLY | O_EXLOCK, 0);
    
    if(fdTool == -1)
    {
        NSLog(@"Exclusive open while repairing tool failed: %d.", errno);
        exit(-1);
    }
    
    if(fstat(fdTool, &st))
    {
        NSLog(@"fstat failed.");
        exit(-1);
    }
    
    if(st.st_uid != 0)
    {
		status = [[AuthUtil shared] autorize];
		if(status == noErr){
			fchown(fdTool, 0, st.st_gid);
			
			// Disable group and world writability and make setuid root.
			fchmod(fdTool, (st.st_mode & (~(S_IWGRP | S_IWOTH)))/* | S_ISUID*/);
			const char *args[] = {"root", [afshlpPath UTF8String],0L};
			[[AuthUtil shared] execUnixCommand:"/usr/sbin/chown" 
										  args:args
										output:nil];
			[[AuthUtil shared] deautorize];
		}
    } else  NSLog(@"st_uid = 0");
    
	
    
    close(fdTool);
    
    NSLog(@"Self-repair done.");
	
}

#pragma mark accessor
// -------------------------------------------------------------------------------
//  statusItem
// -------------------------------------------------------------------------------
-(NSStatusItem*)statusItem {
		return statusItem;
}

// -------------------------------------------------------------------------------
//  setStatusItem
// -------------------------------------------------------------------------------
-(void)setStatusItem:(BOOL)show {
	if(show) {
		if(statusItem) return;
		statusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength] retain];
		[statusItem setView:[[AFSMenuExtraView alloc] initWithFrame:[[statusItem view] frame]  
													   backgrounder:self
															   menu:backgrounderMenu]];
		//Tells the NSStatusItem what menu to load
		[statusItem setMenu:backgrounderMenu];
		//Sets the tooptip for our item
		[statusItem setToolTip:@"OpenAFS Preference"];
		//Enables highlighting
		[statusItem setHighlightMode:YES];
		[statusItem setImage:noTokenImage];
	} else {
		if(!statusItem) return;
		[[NSStatusBar systemStatusBar] removeStatusItem:statusItem];
		[statusItem autorelease];
		statusItem = nil;
	}
}
// -------------------------------------------------------------------------------
//  -(void) imageToRender
// -------------------------------------------------------------------------------
- (NSImage*)imageToRender
{
	if(gotToken){
		return hasTokenImage;
	} else {
		return noTokenImage;
	}
}


-(IBAction) startStopEvent:(id)sender {
	[self startStopAfs:sender];
}

-(IBAction) getReleaseTokenEvent:(id)sender {
	[self getToken:sender];
}
@end
