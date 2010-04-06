//
//  TokenCredentialController.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 28/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "TokenCredentialController.h"
#import "TaskUtil.h"

@implementation TokenCredentialController
// -------------------------------------------------------------------------------
//  awakeFromNib:
// -------------------------------------------------------------------------------
- (void)awakeFromNib
{
}


// -------------------------------------------------------------------------------
//  getToken:
// -------------------------------------------------------------------------------
- (IBAction) getToken:(id) sender
{
	uName	= [((NSTextField*) textEditUserName) stringValue];
	uPwd	= [((NSTextField*) textEditPassword) stringValue];
	if(uName == @"" ||  uPwd == @"") return;
	taken = YES;
	[NSApp endSheet:credentialPanel];
}

	
// -------------------------------------------------------------------------------
//  closePanel:
// -------------------------------------------------------------------------------
- (IBAction) closePanel:(id) sender
{
	taken = NO;
	[NSApp endSheet:credentialPanel];
}


// -------------------------------------------------------------------------------
//  takenToken:
// -------------------------------------------------------------------------------
- (BOOL) takenToken
{
	return taken;
}

// -------------------------------------------------------------------------------
//  takenToken:
// -------------------------------------------------------------------------------
- (NSString*) uName
{
	return uName;
}

// -------------------------------------------------------------------------------
//  takenToken:
// -------------------------------------------------------------------------------
- (NSString*) uPwd
{
	return uPwd;
}

@end
