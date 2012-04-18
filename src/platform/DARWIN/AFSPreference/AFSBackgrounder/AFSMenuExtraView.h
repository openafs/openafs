//
//  AFSMenuExtraView.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 11/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "AFSBackgrounderDelegate.h"

@interface AFSMenuExtraView : NSView <NSMenuDelegate> {
	AFSBackgrounderDelegate 	*backgrounderDelegator;
	NSStatusItem 	*statusItem;
	NSMenu	*statusItemMenu;
	BOOL isMenuVisible;
}
- initWithFrame:(NSRect)myRect backgrounder:(AFSBackgrounderDelegate*)backgrounder menu:(NSMenu*)menu;
- (NSAttributedString*) makeKerberosIndicator:(int*)fontHeight;
- (void)mouseDown:(NSEvent *)event;
- (void)menuWillOpen:(NSMenu *)menu;
- (void)menuDidClose:(NSMenu *)menu;
- (void)menuNeedsUpdate:(NSMenu *)menu;
@end
