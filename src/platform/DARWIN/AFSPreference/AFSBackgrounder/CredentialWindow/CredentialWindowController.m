//
//  CredentialWindowController.m
//  AFSCommander
//
//  Created by Claudio on 14/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "CredentialWindowController.h"
#import "AFSPropertyManager.h"

@implementation CredentialWindowController
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
	
	
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kLogWindowClosed];
}


// -------------------------------------------------------------------------------
//  closePanel:
// -------------------------------------------------------------------------------
- (IBAction) closePanel:(id) sender
{
	taken = NO;
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kLogWindowClosed];
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
