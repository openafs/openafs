//
//  AFSBackgrounder.h
//  OpenAFS
//
//  Created by Claudio Bisegni on 29/07/09.
//  Copyright 2009 Infn. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface AFSBackgrounderDelegate : NSObject {
	IBOutlet NSMenu *backgrounderMenu;
	NSStatusItem    *statusItem;
    NSImage         *statusImage;
    NSImage         *statusHighlightImage;
}

@end
