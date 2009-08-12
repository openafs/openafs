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
	return 	[self init];
}

- (void)dealloc {
	if(credentialWindow) [credentialWindow release];
    [super dealloc];
	
} // dealloc

-(void) showWindow
{	
	// calculate the point where show the window
	NSPoint topLeft = {viewRect.origin.x-160,[[NSScreen mainScreen] frame].size.height-kMenuBarHeight};
	// load the bundle for
	[NSBundle loadNibNamed:@"CredentialWindow.nib" owner:self];

	credentialWindow = [[NSWindow alloc] initWithContentRect:[((NSView*) credentialView) frame]
												   styleMask:NSTitledWindowMask /*| NSUtilityWindowMask*/
													 backing:NSBackingStoreBuffered
													   defer:YES screen:[NSScreen mainScreen]];
	[credentialWindow setTitle:@"Klog"];
	[credentialWindow setFrameTopLeftPoint:topLeft];
	[credentialWindow setContentView:credentialView];
	[credentialWindow makeKeyAndOrderFront:self];
}

-(void) closeWindow
{
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
