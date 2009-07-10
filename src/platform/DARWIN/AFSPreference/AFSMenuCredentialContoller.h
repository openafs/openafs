//
//  AFSMenuCredentialContoller.h
//  AFSCommander
//
//  Created by Claudio on 14/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "CredentialWindowController.h"
#import "AFSPropertyManager.h"

@interface AFSMenuCredentialContoller : NSObject {
	NSRect viewRect;
	id credentialView;
	id credentialController;
	NSPanel *credentialWindow;
	id credWinController;
	AFSPropertyManager *afsPropMngr;
}
-(id) initWhitRec:(NSRect)rect afsPropManager:(AFSPropertyManager*)afsPropManager;
-(void) showWindow;
-(void) closeWindow;
@end
