//
//  AFSMenuCredentialContoller.m
//  AFSCommander
//
//  Created by Claudio on 14/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "AFSMenuCredentialContoller.h"
#import "CredentialWindowController.h"
#import "global.h"
#import "AFSPropertyManager.h"

@implementation AFSMenuCredentialContoller

-(id) initWhitRec:(NSRect)rect  afsPropManager:(AFSPropertyManager*)afsPropManager;
{
	viewRect = rect;
	credentialWindow = nil;
	afsPropMngr = [afsPropManager retain];
	NSLog(@"init AFSMenuCredentialContoller");
	return 	[self init];
}

- (void)dealloc {
	NSLog(@"dealloc AFSMenuCredentialContoller");
	if(credentialWindow) [credentialWindow release];
    [super dealloc];
	
} // dealloc

-(void) showWindow
{	
	// calculate the point where show the window
	NSPoint topLeft = {viewRect.origin.x-160,[[NSScreen mainScreen] frame].size.height-kMenuBarHeight};
	NSLog(@"viewRect.origin.x=%d, topLeft.x=%d", viewRect.origin.x, topLeft.x);
	// load the bundle for
	[NSBundle loadNibNamed:@"CredentialWindow.nib" owner:self];
	NSLog(@"prepare to open window CredentialWindow");

	credentialWindow = [[NSWindow alloc] initWithContentRect:[((NSView*) credentialView) frame]
												   styleMask:NSTitledWindowMask /*| NSUtilityWindowMask*/
													 backing:NSBackingStoreBuffered
													   defer:YES screen:[NSScreen mainScreen]];
	NSLog(@"Finestra");
	[credentialWindow setTitle:@"Klog"];
	[credentialWindow setFrameTopLeftPoint:topLeft];
	[credentialWindow setContentView:credentialView];
	[credentialWindow makeKeyAndOrderFront:self];
	NSLog(@"creata");
	
}

-(void) closeWindow
{
	NSLog(@"closeWindow");
	if([(CredentialWindowController*)credWinController takenToken] && afsPropMngr) {
		[afsPropMngr getTokens:true 
						   usr:[(CredentialWindowController*)credWinController uName] 
						   pwd:[(CredentialWindowController*)credWinController uPwd]];
		[afsPropMngr release];
		afsPropMngr = nil;
	}
	[credentialWindow close];
	credentialWindow = nil;
}
@end
