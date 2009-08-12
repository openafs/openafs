//
//  AFSBackgrounder.h
//  OpenAFS
//
//  Created by Claudio Bisegni on 29/07/09.
//  Copyright 2009 Infn. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "AFSMenuCredentialContoller.h"

@interface AFSBackgrounderDelegate : NSObject {
@public
	IBOutlet NSMenu *backgrounderMenu;
	IBOutlet NSMenuItem *startStopMenuItem;
	IBOutlet NSMenuItem *getReleaseTokenMenuItem;
	NSStatusItem    *statusItem;
	

	BOOL afsState; //0-off 1-on
	BOOL gotToken; //0-no 1-one o more token
	NSSize menuSize;
	
@protected
	AFSPropertyManager *afsMngr;
	NSString *afsSysPath;
	NSNumber *useAklogPrefValue;
	NSNumber *showStatusMenu;
	NSNumber *aklogTokenAtLogin;
	//Icon for state visualization
	NSImage	*hasTokenImage;
	NSImage	*noTokenImage;
	
	//credential windows mainWindow
	AFSMenuCredentialContoller *credentialMenuController;

	//NSTimer for tokens refresh
	NSTimer *timerForCheckTokensList;
	NSLock *tokensLock;
	
}
- (void)startTimer;
- (void)stopTimer;
- (BOOL)useAklogPrefValue;
- (void)readPreferenceFile:(NSNotification *)notification;
- (void)getToken:(id)sender;
- (void)releaseToken:(id)sender;
- (void)updateAfsStatus:(NSTimer*)timer;
- (void)klogUserEven:(NSNotification *)notification;
- (void)chageMenuVisibility:(NSNotification *)notification;
- (NSImage*)getImageFromBundle:(NSString*)fileName fileExt:(NSString*)ext;
- (NSImage*)imageToRender;
- (void)menuNeedsUpdate:(NSMenu *)menu;
- (void)repairHelperTool;
- (void) afsVolumeMountChange:(NSNotification *)notification;

-(NSStatusItem*)statusItem;
-(void) setStatusItem:(BOOL)show;
-(NSImage*)imageToRender;
-(IBAction) startStopEvent:(id)sender;
-(IBAction) getReleaseTokenEvent:(id)sender;
@end
