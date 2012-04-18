//
//  AFSMenuExtra.h
//  AFSCommander
//
//  Created by Claudio on 10/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//


#import <Cocoa/Cocoa.h>
#import "SystemUIPlugin.h"
#import "global.h"
#import "AFSMenuCredentialContoller.h"
@class AFSMenuExtraView;
@interface AFSMenuExtra : NSMenuExtra {
	@public
	BOOL afsState; //0-off 1-on
	BOOL gotToken; //0-no 1-one o more token
	
	@protected
	NSString *afsSysPath;
	NSNumber *useAklogPrefValue;
	//menu extra
	NSMenu *theMenu;
	
	//Menu extra view
	AFSMenuExtraView *theView;
	// menu reference
	NSMenuItem *startStopMenu;
	NSMenuItem *loginMenu;
	NSMenuItem *unlogMenu;
	
	//Icon for state visualization
	NSImage			*hasTokenImage;
	NSImage			*noTokenImage;
	
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
- (NSImage*)getImageFromBundle:(NSString*)fileName fileExt:(NSString*)ext;
- (NSImage*)imageToRender;
- (void)updateMenu;
- (void) afsVolumeMountChange:(NSNotification *)notification;
@end
