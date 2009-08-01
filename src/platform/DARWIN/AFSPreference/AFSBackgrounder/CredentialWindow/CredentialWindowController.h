//
//  CredentialWindowController.h
//  AFSCommander
//
//  Created by Claudio on 14/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "AFSMenuCredentialContoller.h"
#import "global.h"

@interface CredentialWindowController : NSObject {
	id credentialView;
	id afsMenuController;
	id textEditUserName;
	id textEditPassword;
	
	BOOL taken;
	NSString *uName;
	NSString *uPwd;
	
}

- (IBAction) getToken:(id) sender;
- (IBAction) closePanel:(id) sender;
- (BOOL) takenToken;
- (NSString*) uName;
- (NSString*) uPwd;
@end
