//
//  NSString+search.m
//  AFSCommander
//
//  Created by MacDeveloper on 18/03/08.
//   Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "NSString+search.h"


@implementation NSString(search)

-(NSString*) estractTokenByDelimiter:(NSString*)startToken endToken:(NSString*)endTk {
	NSString *result = nil;
	NSScanner *strScan = [NSScanner scannerWithString:self];
	if(!endTk) return nil;
	//make the CharacterSet for scanner
	
	if(startToken) [strScan scanUpToString:startToken intoString:&result];
	[strScan scanUpToString:endTk intoString:&result];
	if(startToken) result = [result substringFromIndex:[startToken length]];
	return result;
}

@end
