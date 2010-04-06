//
//  TokenCredentialController.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 28/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface TokenCredentialController : NSObject {
	@public
	id credentialPanel;
	
	@protected
	id afsCommanderPref;
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
