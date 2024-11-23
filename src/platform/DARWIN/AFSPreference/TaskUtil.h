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

#define PRIVHELPER_ID	"org.openafs.privhelper"

@interface TaskUtil : NSObject {
}
+(NSString*) searchExecutablePath:(NSString*)unixCommand;
+(NSString*) executeTaskSearchingPath:(NSString*)unixCommand args:(NSArray*)args;
+(NSString*) executeTask:(NSString*) taskName arguments:(NSArray *)args;
+(int) executePrivTask:(const char *)task;
@end
