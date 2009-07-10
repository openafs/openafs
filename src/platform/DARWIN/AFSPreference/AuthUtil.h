//
//  AuthUtil.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 21/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

@interface AuthUtil : NSObject {
	AuthorizationRef authorizationRef;
}
-(id) init;
-(OSStatus) autorize;
-(BOOL) deautorize;
-(AuthorizationRef) authorization;
-(NSData*) extFormAuth;
-(OSStatus) execUnixCommand:(const char*) commandPath args:(const char*[])args output:(NSMutableString*)output;
+(AuthUtil*) shared;
+ (id)allocWithZone:(NSZone *)zone;
- (id)copyWithZone:(NSZone *)zone;
- (id)retain;
- (unsigned)retainCount;
- (void)release;
- (id)autorelease;
@end
