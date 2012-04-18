//
//  TaskUtil.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 25/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

@interface TaskUtil : NSObject {
}
+(NSString*) searchExecutablePath:(NSString*)unixCommand;
+(NSString*) executeTaskSearchingPath:(NSString*)unixCommand args:(NSArray*)args;
+(NSString*) executeTask:(NSString*) taskName arguments:(NSArray *)args;
+(int) executeTaskWithAuth:(NSString*) taskName arguments:(NSArray *)args authExtForm:(NSData*)auth;
+(int) executeTaskWithAuth:(NSString*) taskName arguments:(NSArray *)args helper:(NSString *)helper withAuthRef:(AuthorizationRef)authRef;
@end
