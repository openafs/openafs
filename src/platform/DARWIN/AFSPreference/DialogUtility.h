//
//  DialogUtility.h
//  OpenAFS
//
//  Created by Claudio Bisegni on 01/05/08.
//  Copyright 2008 Infn. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface DialogUtility : NSObject {

}
+ (BOOL) showMessageYesNo:(NSString*) message delegator:(id)delegator functionSelecter:(SEL)fSelector window:(NSWindow*)window;
@end
