//
//  FileUtil.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 21/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "FileUtil.h"

@implementation FileUtil

-(id) init;
{
	[super init];
	return self;
}

-(void) dealloc
{
	[super dealloc];
}

// -------------------------------------------------------------------------------
//  startAutorization:
// -------------------------------------------------------------------------------
-(OSStatus) startAutorization
{
	OSStatus err = noErr;
	err = [[AuthUtil shared] autorize];
	return err;
}

// -------------------------------------------------------------------------------
//  autorizedMoveFile:
// -------------------------------------------------------------------------------
-(OSStatus) autorizedMoveFile:(NSString*)srcPath toPath:(NSString*)dstPath
{
	OSStatus status = noErr;
	const char *arguments[] = {[srcPath UTF8String], [dstPath UTF8String], 0L};
	status = [[AuthUtil shared] execUnixCommand:"/bin/mv" args:arguments output:nil];
	return status;
}

// -------------------------------------------------------------------------------
//  autorizedMoveFile:
// -------------------------------------------------------------------------------
-(OSStatus) autorizedCopy:(NSString*)srcPath toPath:(NSString*)dstPath
{
	OSStatus status = noErr;
	const char *arguments[] = {[srcPath UTF8String], [dstPath UTF8String], 0L};
	status = [[AuthUtil shared] execUnixCommand:"/bin/cp" args:arguments output:nil];
	return status;
}

// -------------------------------------------------------------------------------
//  autorizedChown:
// -------------------------------------------------------------------------------
-(OSStatus) autorizedChown:(NSString*)filePath owner:(NSString*)owner group:(NSString*)group;
{
	OSStatus status = noErr;
	NSMutableString *chownParam = [[NSMutableString alloc] init];
	[chownParam appendString:owner];
	[chownParam appendString:@":"];
	[chownParam appendString:group];
	
	const char *arguments[] = {[chownParam UTF8String], [filePath UTF8String],  0L};
	status = [[AuthUtil shared] execUnixCommand:"/usr/sbin/chown" args:arguments output:nil];
	[chownParam release];
	return status;
}

// -------------------------------------------------------------------------------
//  autorizedDelete:
// -------------------------------------------------------------------------------
-(OSStatus) autorizedDelete:(NSString*)destFilePath{
	OSStatus status = noErr;
	const char *arguments[] = {[destFilePath UTF8String], 0L};
	status = [[AuthUtil shared] execUnixCommand:"/bin/rm" args:arguments output:nil];
	return status;
}

// -------------------------------------------------------------------------------
//  endAutorization:
// -------------------------------------------------------------------------------
-(void) endAutorization
{
	[[AuthUtil shared] deautorize];
}

@end