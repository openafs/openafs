//
//  CellIp.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 14/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "CellIp.h"


@implementation CellIp
// -------------------------------------------------------------------------------
//  init:
// -------------------------------------------------------------------------------
-(id)init {
	[super init];
	ip = @"0.0.0.0";
	ipComment = @"-----";
	return self;
}

// -------------------------------------------------------------------------------
//  dealloc:
// -------------------------------------------------------------------------------
-(void)dealloc{
	if(ip) [ip release];
	if(ipComment) [ipComment release];
	[super dealloc];
}

// -------------------------------------------------------------------------------
//  setCellIp:
// -------------------------------------------------------------------------------
-(void)setCellIp:(NSString*)newip{
	if(ip){
		[ip release];
	}
	ip = [newip retain];
}

// -------------------------------------------------------------------------------
//  getCellIp:
// -------------------------------------------------------------------------------
-(NSString*) getCellIp{
	return ip;
}

// -------------------------------------------------------------------------------
//  setCellComment:(NSString*)newcomment
// -------------------------------------------------------------------------------
-(void)setCellComment:(NSString*)newcomment{
	if(ipComment){
		[ipComment release];
	}
	ipComment = [newcomment retain];
}

// -------------------------------------------------------------------------------
//  getCellComment:
// -------------------------------------------------------------------------------
-(NSString*) getCellComment{
	return ipComment;
}

// -------------------------------------------------------------------------------
//  description:
// -------------------------------------------------------------------------------
-(NSString*) description{
	NSMutableString *desc = [[NSMutableString alloc] init];
	[desc autorelease];
	[desc appendString:ip];[desc appendString:@" "];[desc appendString:@"#"];[desc appendString:ipComment];[desc appendString:@"\n"];
	return desc;
}

@end
