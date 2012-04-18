//
//  AFSMenuExtra.m
//  AFSCommander
//
//  Created by Claudio on 10/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "AFSMenuExtra.h"
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

@implementation AFSMenuExtra
// -------------------------------------------------------------------------------
//  -(id) initWithBundle
// -------------------------------------------------------------------------------
- (id) initWithBundle:(NSBundle *)bundle
{
    self = [super initWithBundle:bundle];
    if( self == nil )
        return nil;
	
	// allocate the lock
	tokensLock = [[NSLock alloc] init];
	// inizialize the afspath
	afsSysPath = nil;
	credentialMenuController = nil;

    theView = [[AFSMenuExtraView alloc] initWithFrame:[[self view] frame] 
											menuExtra:self];
	[self setView:theView];
    
	
    // Get the imge for menu
	//Load image for menu rappresentation
	hasTokenImage = [self getImageFromBundle:@"hasToken" 
											 fileExt:@"png"];
	
	noTokenImage = [self getImageFromBundle:@"noToken" 
											fileExt:@"png"];
	
    theMenu = [[NSMenu alloc] initWithTitle: @""];
    [theMenu setAutoenablesItems: NO];
    startStopMenu = [theMenu addItemWithTitle: kAfsOff action: @selector(startStopAfs:)  keyEquivalent: @""];
	[startStopMenu setTarget:self];
	
	loginMenu = [theMenu addItemWithTitle: kMenuLogin action: @selector(getToken:)  keyEquivalent: @""];
	[loginMenu setTarget:self];

	unlogMenu = [theMenu addItemWithTitle: kMenuUnlog action: @selector(releaseToken:)  keyEquivalent: @""];
	[unlogMenu setTarget:self];

	// Register for preference user change
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(readPreferenceFile:) 
															name:kAFSMenuExtraID object:kPrefChangeNotification];
	
	// Register for afs state change
	[[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(afsVolumeMountChange:) 
															name:kAFSMenuExtraID object:kMExtraAFSStateChange];
	
	
	//Register for mount/unmount afs volume
	[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self 
														   selector:@selector(afsVolumeMountChange:) 
															   name:NSWorkspaceDidMountNotification object:nil];
	
	[[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self 
														   selector:@selector(afsVolumeMountChange:) 
															   name:NSWorkspaceDidUnmountNotification object:nil];
	//Start to read the afs path
	[self readPreferenceFile:nil];	
	[self startTimer];
    return self;
}

// -------------------------------------------------------------------------------
//  -(void) willUnload
// -------------------------------------------------------------------------------
- (void) willUnload {
	//release the lock
	[self stopTimer];
	
	if(hasTokenImage) [hasTokenImage release];
	if(noTokenImage) [noTokenImage release];
	
	// Unregister for preference change
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kPrefChangeNotification];
	[[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:kAFSMenuExtraID object:kMExtraAFSStateChange];
	[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self name:NSWorkspaceDidMountNotification object:nil];
	[[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self name:NSWorkspaceDidUnmountNotification object:nil];

	
	[tokensLock release];
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
//  -(void) dealloc
// -------------------------------------------------------------------------------
- (void) dealloc
{
    [theMenu release];
    [theView release];
	if(afsSysPath) [afsSysPath release];
	// send notify that menuextra has closed
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:afsCommanderID object:kPrefChangeNotification];
    [super dealloc];
}

// -------------------------------------------------------------------------------
//  -(NSMenu*) menu
// -------------------------------------------------------------------------------
- (NSMenu *) menu
{
    return theMenu;
}


// -------------------------------------------------------------------------------
//  -(void) readPreferenceFile
// -------------------------------------------------------------------------------
- (void) readPreferenceFile:(NSNotification *)notification
{
	NSLog(@"Reading preference file");
	//CFPreferencesSynchronize((CFStringRef)afsCommanderID,  kCFPreferencesAnyUser, kCFPreferencesAnyHost);
	//CFPreferencesSynchronize((CFStringRef)afsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	
	if(afsSysPath) {
		[afsSysPath release];
		afsSysPath = nil;
	}
	
	afsSysPath = PREFERENCE_AFS_SYS_PAT_STATIC;
		
	// read the preference for aklog use
	
	useAklogPrefValue = [[NSUserDefaults standardUserDefaults] boolForKey:PREFERENCE_USE_AKLOG];
	NSLog(@"NSUserDefaults:%d", useAklogPrefValue);
	useAklogPrefValue = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_USE_AKLOG, 
														  (CFStringRef)afsCommanderID, 
														  kCFPreferencesCurrentUser, 
														  kCFPreferencesAnyHost);
	NSLog(@"CFPreferencesCopyValue:%d", useAklogPrefValue);
	[self updateAfsStatus:nil];
}

// -------------------------------------------------------------------------------
//  -(void) getToken
// -------------------------------------------------------------------------------
- (void)getToken:(id)sender
{
	
	NSRect globalRect;
	globalRect.origin = [[[self view] window] convertBaseToScreen:[[self view] frame].origin];
	globalRect.size = [[self view] frame].size;
	AFSPropertyManager *afsPropMngr = [[AFSPropertyManager alloc] initWithAfsPath:afsSysPath ];
	[afsPropMngr loadConfiguration]; 
	
	
	if([useAklogPrefValue intValue]==NSOnState ) {
		[afsPropMngr getTokens:false 
						   usr:nil 
						   pwd:nil];
		[self klogUserEven:nil];
	} else {
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
	AFSPropertyManager *afsMngr = [[AFSPropertyManager alloc] initWithAfsPath:afsSysPath];
	[afsMngr unlog:nil];
	[afsMngr release];
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
	AFSPropertyManager *afsMngr = [[AFSPropertyManager alloc] initWithAfsPath:afsSysPath];
	afsState = [afsMngr checkAfsStatus];
	
	NSArray *tokens = [afsMngr getTokenList];
	[afsMngr release];
	gotToken = [tokens count] > 0;
	[tokens release];
	// update the menu item title
	[startStopMenu setTitle:afsState?kAfsButtonShutdown:kAfsButtonStartup];
	
	[self updateMenu];
	
	[theView setNeedsDisplay:YES];
	
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
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:afsCommanderID object:kMenuExtraEventOccured];
	
	[self updateAfsStatus:nil];
}

// -------------------------------------------------------------------------------
//  -(void) getImageFromBundle
// -------------------------------------------------------------------------------
- (NSImage*)getImageFromBundle:(NSString*)fileName fileExt:(NSString*)ext
{
	return [[NSImage alloc]initWithContentsOfFile:[[self bundle] pathForResource:fileName 
																   ofType:ext]];
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


// -------------------------------------------------------------------------------
//  -(void) updateMenu
// -------------------------------------------------------------------------------
- (void)updateMenu{
	[loginMenu setEnabled:afsState];
	[unlogMenu setEnabled:afsState];
}


// -------------------------------------------------------------------------------
//  -(void) useAklogPrefValue
// -------------------------------------------------------------------------------
- (BOOL)useAklogPrefValue
{
	if(useAklogPrefValue) return [useAklogPrefValue intValue] == NSOnState; 
	else return NSOffState;
}
@end
