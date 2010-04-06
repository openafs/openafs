//
//  ViewUtility.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 25/12/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface ViewUtility : NSObject {

}
+(void) enbleDisableControlView: (NSView*)parentView controlState:(BOOL)controlState;
@end
