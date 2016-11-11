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
#import <sys/xattr.h>
#import "Krb5Util.h"

#define LINK_ICON 'srvr'

@implementation AFSBackgrounderDelegate
#pragma mark NSApp Delegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	
	linkCreationLock = [[NSLock alloc] init];
	
	afsMngr = [[AFSPropertyManager alloc] initWithAfsPath:PREFERENCE_AFS_SYS_PAT_STATIC];
	
	// allocate the lock for concurent afs check state
	tokensLock = [[NSLock alloc] init];
	renewTicketLock = [[NSLock alloc] init];
	
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
	//get the size of the menu icon
	menuSize = [hasTokenImage size];
	
	//inizialize the local link mode status
	currentLinkActivationStatus = NO;
	
	//Start to read the afs path
	[self readPreferenceFile:nil];	
	[self startTimer];

	
	
	// Register for preference user change
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self
														selector:@selector(readPreferenceFile:)
															name:kAFSMenuExtraID object:kPrefChangeNotification];
	
	// Register for afs state change
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self
														selector:@selector(afsVolumeMountChange:)
															name:kAFSMenuExtraID object:kMExtraAFSStateChange];
	
	// Register for menu state change
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self
														selector:@selector(chageMenuVisibility:)
															name:kAFSMenuExtraID object:kMExtraAFSMenuChangeState];
	
	//Register for mount/unmount afs volume
	[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
														   selector:@selector(afsVolumeMountChange:)
															   name:NSWorkspaceDidMountNotification object:nil];
	
	[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
														   selector:@selector(afsVolumeMountChange:)
															   name:NSWorkspaceDidUnmountNotification object:nil];
	
	[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
														   selector:@selector(switchHandler:)
															   name:NSWorkspaceSessionDidBecomeActiveNotification
															 object:nil];
	
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
														   selector:@selector(switchHandler:)
															   name:NSWorkspaceSessionDidResignActiveNotification
															 object:nil];
	
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
	[self stopTimerRenewTicket];
	
	if(hasTokenImage) [hasTokenImage release];
	if(noTokenImage) [noTokenImage release];
	
	// Unregister for preference change
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:NSWorkspaceSessionDidBecomeActiveNotification object:nil];
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:NSWorkspaceSessionDidResignActiveNotification object:nil];
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kPrefChangeNotification];
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kMExtraAFSStateChange];
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kMExtraAFSMenuChangeState];
	[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self name:NSWorkspaceDidMountNotification object:nil];
	[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self name:NSWorkspaceDidUnmountNotification object:nil];
	

	// send notify that menuextra has closed
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAfsCommanderID object:kPrefChangeNotification];
	
	if(tokensLock) [tokensLock release];
	if(afsMngr) [afsMngr release];
	if(linkCreationLock) [linkCreationLock release];
	return NSTerminateNow;
}
#pragma mark Notification Handler
// -------------------------------------------------------------------------------
//  -(void) readPreferenceFile
// -------------------------------------------------------------------------------
- (void) readPreferenceFile:(NSNotification *)notification
{
	if(afsSysPath) {
		[afsSysPath release];
		afsSysPath = nil;
	}
	CFPreferencesSynchronize((CFStringRef)kAfsCommanderID, kCFPreferencesAnyUser, kCFPreferencesAnyHost);
	CFPreferencesSynchronize((CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	
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
	
	aklogTokenAtLogin = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_AKLOG_TOKEN_AT_LOGIN, (CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

	//get link configuration
	NSData *prefData = (NSData*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_LINK_CONFIGURATION, (CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	NSDictionary *linkDict = (NSMutableDictionary*)[NSPropertyListSerialization propertyListFromData:prefData
										    mutabilityOption:NSPropertyListMutableContainers
										    format:nil
										    errorDescription:nil];
	linkConfiguration = [linkDict mutableCopy];
	
	//get link enabled status
	NSNumber *linkEnabledStatus =  (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_USE_LINK, (CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	[self updateLinkModeStatusWithpreferenceStatus:[linkEnabledStatus boolValue]];
	
	//check the user preference for manage the renew
	krb5CheckRenew =  (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_KRB5_CHECK_ENABLE,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	krb5RenewTime = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_KRB5_RENEW_TIME,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	krb5RenewCheckTimeInterval = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_KRB5_RENEW_CHECK_TIME_INTERVALL,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	krb5SecToExpireTimeForRenew = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_KRB5_SEC_TO_EXPIRE_TIME_FOR_RENEW,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

	
	//set the menu name
	[self updateAfsStatus:nil];

		//stop and start the timer for krb5 renew
	[self stopTimerRenewTicket];
	[self startTimerRenewTicket];
}

// -------------------------------------------------------------------------------
//  - (void) updateLinkModeStatusWithpreferenceStatus:(BOOL)status
// -------------------------------------------------------------------------------
- (void) updateLinkModeStatusWithpreferenceStatus:(BOOL)status {
	//exec the link operation on thread
		[NSThread detachNewThreadSelector:@selector(performLinkOpeartionOnThread:)
							 toTarget:self
						   withObject:[NSNumber numberWithBool:status]];
}


// -------------------------------------------------------------------------------
//  - (void) performLinkOpenartionOnThread:(id)object
// -------------------------------------------------------------------------------
- (void) performLinkOpeartionOnThread:(id)object {
	[linkCreationLock lock];
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSError	 *error = nil;
	NSString *key = nil;
	NSString *linkDstPath = nil;
	NSString *linkSrcPath = nil;
	NSArray  *paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, YES);
	NSString *documentFolderPath = [paths objectAtIndex:0];
	NSNumber *number = (NSNumber*)object;
	//NSString *fType = NSFileTypeForHFSTypeCode(LINK_ICON);
	//NSImage  *picture = [[NSWorkspace sharedWorkspace] iconForFileType:fType];
	
	BOOL linkSourcePathExist = NO;
	BOOL linkDestinationPathExist = NO;
	
	NSLog(@"updateLinkModeStatusWithpreferenceStatus %d", [number boolValue]);
	NSEnumerator *keys = [linkConfiguration keyEnumerator];
	while ((key = [keys nextObject])) {
		//link path
		linkSrcPath = [documentFolderPath  stringByAppendingPathComponent:key];
		//afs destionation path
		linkDstPath = [linkConfiguration objectForKey:key];
		linkSourcePathExist = [[NSFileManager defaultManager] fileExistsAtPath:linkSrcPath];
		linkDestinationPathExist = [[NSFileManager defaultManager] fileExistsAtPath:linkDstPath];
		
		if([number boolValue]) {
			if(!linkSourcePathExist) {
				if(linkDestinationPathExist) {
					NSLog(@"Creating link \"%@\" to point to \"%@\"", linkSrcPath, linkDstPath);
					[[NSFileManager defaultManager] createSymbolicLinkAtPath:linkSrcPath
														 withDestinationPath:linkDstPath
																	   error:&error];
					if(!error) {
						//Link has been created so i can chnge the icon
					/*	[[NSWorkspace sharedWorkspace] setIcon:picture
													   forFile:linkName
													   options:0];*/
						NSLog(@"Link \"%@\" created", linkSrcPath);
					} else {
						NSLog(@"Link Creation Error: %@", [error localizedDescription]);
					}
				} else {
					NSLog(@"Deleting Link: %@", linkSrcPath);
					[[NSFileManager defaultManager] removeItemAtPath:linkSrcPath
															   error:&error];
				}
			} else {
				//the lynk already exist check if the dest path is accesible
				if(!linkSourcePathExist) {
					NSLog(@"Deleting Link: %@", linkSrcPath);
					[[NSFileManager defaultManager] removeItemAtPath:linkSrcPath
															   error:&error];
				}
			}
		} else {
			//delete the link
			NSLog(@"Deleting Link: %@", linkSrcPath);
			[[NSFileManager defaultManager] removeItemAtPath:linkSrcPath
													   error:&error];
			
		}
	}
	
	//update the status
	currentLinkActivationStatus = [number boolValue];
	//release thread resource
	[pool release];
	[linkCreationLock unlock];
}

// -------------------------------------------------------------------------------
//  - (void)chageMenuVisibility:(NSNotification *)notification
// -------------------------------------------------------------------------------
- (void)chageMenuVisibility:(NSNotification *)notification {
	[self readPreferenceFile:nil];
	[self setStatusItem:[showStatusMenu boolValue]];
}

// -------------------------------------------------------------------------------
// - (void) switchHandler:(NSNotification*) notification
// -------------------------------------------------------------------------------
- (void) switchHandler:(NSNotification*) notification
{
    if ([[notification name] isEqualToString:NSWorkspaceSessionDidResignActiveNotification])
    {
        // user has switched out
    }
    else
    {
		// user has switched in
		NSLog(@"User has switch in again");
		if([aklogTokenAtLogin boolValue] && afsState && !gotToken) {
			NSLog(@"Proceed to get token");
			//check if we must get the aklog at logint(first run of backgrounder
			[self getToken:nil];
		}
    }
}
// -------------------------------------------------------------------------------
//  -(void) afsVolumeMountChange - Track for mount unmount afs volume
// -------------------------------------------------------------------------------
- (void) afsVolumeMountChange:(NSNotification *)notification{
	[self updateAfsStatus:nil];
	[self readPreferenceFile:nil];
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
#pragma mark Action
// -------------------------------------------------------------------------------
//  - (void)startStopAfs:(id)sender
// -------------------------------------------------------------------------------
- (void)startStopAfs:(id)sender
{
	@try {
	    BOOL currentAfsState = NO;
	    OSErr status = [[AuthUtil shared] autorize];
	    if(status == noErr){
		currentAfsState = [afsMngr checkAfsStatus];
		// make the parameter to call the root helper app
		if(currentAfsState){
		    //shutdown afs
		    NSLog(@"Shutting down afs");
		    [afsMngr shutdown];
		} else {
		    //Start afs
		    NSLog(@"Starting up afs");
		    [afsMngr startup];
		}
	    }
	}@catch (NSException * e) {
		NSLog(@"error %@", [e reason]);
	}@finally {
		[self updateAfsStatus:nil];
		[[AuthUtil shared] deautorize];
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
	if([useAklogPrefValue boolValue]) {
		[afsMngr getTokens:false
					   usr:nil
					   pwd:nil];
		[self klogUserEven:nil];
	} else {
		globalRect.origin = [[[statusItem view] window] convertBaseToScreen:[[statusItem view] frame].origin];
		globalRect.size = [[statusItem view] frame].size;
		
		// register for user event
		[[NSDistributedNotificationCenter defaultCenter] addObserver:self
															selector:@selector(klogUserEven:)
																name:kAFSMenuExtraID
															  object:kLogWindowClosed];
		
		credentialMenuController = [[AFSMenuCredentialContoller alloc] initWhitRec:globalRect
																	afsPropManager:afsMngr];
		[credentialMenuController showWindow];
	}
	//Dispose afs manager
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAfsCommanderID
																   object:kMExtraTokenOperation];
}

// -------------------------------------------------------------------------------
//  -(void) releaseToken
// -------------------------------------------------------------------------------
- (void)releaseToken:(id)sender
{
	[afsMngr unlog:nil];
	[self updateAfsStatus:nil];
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAfsCommanderID 
																   object:kMExtraTokenOperation];
}


// -------------------------------------------------------------------------------
//  -(void) updateAfsStatus
// -------------------------------------------------------------------------------
- (void)updateAfsStatus:(NSTimer*)timer
{
	//Try to locking
	if(![tokensLock tryLock]) return;
	
	//reload configuration
	[afsMngr loadConfiguration];
	
	// check the afs state in esclusive mode
	afsState = [afsMngr checkAfsStatus];
	
	NSArray *tokens = [afsMngr getTokenList];
	gotToken = [tokens count] > 0;
	[tokens release];
	
	//update the menu icon
	[[statusItem view] setNeedsDisplay:YES];
	//unlock
	[tokensLock unlock];
}

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
//  startTimerRenewTicket:
// -------------------------------------------------------------------------------
- (void)startTimerRenewTicket {
	//start the time for check ticket renew
	if(timerForCheckRenewTicket || !krb5RenewCheckTimeInterval || ![krb5RenewCheckTimeInterval intValue]) return;
	NSLog(@"startTimerRenewTicket with sec %d", [krb5RenewCheckTimeInterval intValue]);
	timerForCheckRenewTicket = [NSTimer scheduledTimerWithTimeInterval:(krb5RenewCheckTimeInterval?[krb5RenewCheckTimeInterval intValue]:PREFERENCE_KRB5_RENEW_CHECK_TIME_INTERVALL_DEFAULT_VALUE)
															   target:self
															 selector:@selector(krb5RenewAction:)
															 userInfo:nil
															  repeats:YES];
	[timerForCheckRenewTicket fire];
}

// -------------------------------------------------------------------------------
//  stopTimerRenewTicket:
// -------------------------------------------------------------------------------
- (void)stopTimerRenewTicket {
	NSLog(@"stopTimerRenewTicket");
	if(!timerForCheckRenewTicket) return;
	[timerForCheckRenewTicket invalidate];
	timerForCheckRenewTicket = nil;
}

// -------------------------------------------------------------------------------
//  krb5RenewAction:
// -------------------------------------------------------------------------------
- (void)krb5RenewAction:(NSTimer*)timer {
	//Try to locking
	if(![renewTicketLock tryLock]) return;
	NSLog(@"krb5RenewAction %@", [NSDate date]);
	@try {
		[Krb5Util renewTicket:[krb5SecToExpireTimeForRenew intValue]
					renewTime:[krb5RenewTime intValue]];
	}
	@catch (NSException * e) {
	}
	@finally {
		[renewTicketLock unlock];
	}


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
		statusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:menuSize.width] retain];
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
	if(gotToken)
		[self releaseToken:sender];
	else
		[self getToken:sender];
}
@end
