//
//  AFSMenuExtraView.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 11/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "AFSMenuExtraView.h"
#import "AFSPropertyManager.h"
#import "global.h"

@implementation AFSMenuExtraView

- initWithFrame:(NSRect)myRect 
   backgrounder:(AFSBackgrounderDelegate*)backgrounder  
		   menu:(NSMenu*)menu{
	
	// Have super init
	self = [super initWithFrame:myRect];
	if(!self) {
		return nil;
	}
	
	// Store our extra
	backgrounderDelegator = backgrounder;
	statusItem = [backgrounderDelegator statusItem];
	statusItemMenu = menu;
	isMenuVisible = NO;
    return self;
	
} // initWithFrame

- (void)dealloc {
	
    [super dealloc];
	
} // dealloc

- (void)drawRect:(NSRect)rect 
{
	NSImage *image = nil;
	int fontHeight = 0;
	NSAttributedString *kerberosStringIndicator = nil;
	
	//check if we need to simulate the background menu clicked
	[statusItem drawStatusBarBackgroundInRect:[self bounds] 
								withHighlight:isMenuVisible];
	image = [backgrounderDelegator imageToRender];
    if (image) {
		// Live updating even when menu is down handled by making the extra
		// draw the background if needed.		
		[image compositeToPoint:NSMakePoint(0, 0) operation:NSCompositeSourceOver];
	}
	//Draw, if necessary, the kerberos indicator for aklog usage for get token
	if([backgrounderDelegator useAklogPrefValue] == NSOnState) {
		kerberosStringIndicator = [[self makeKerberosIndicator:&fontHeight] autorelease];
		if(kerberosStringIndicator) [kerberosStringIndicator drawAtPoint:NSMakePoint(0, kMenuBarHeight-fontHeight)];
	}
}

/*!
    @method     makeKerberosIndicator
    @abstract   Make the kerberos indicator
    @discussion Make a letter to render in menu view to inform the user if is enable aklog use
*/
- (NSAttributedString*) makeKerberosIndicator:(int*)fontHeight  {
	NSFont *font = [NSFont systemFontOfSize:9.0];
	NSDictionary *attrsDictionary =	[NSDictionary dictionaryWithObject:font
																forKey:NSFontAttributeName];
	NSAttributedString *attrString = [[NSAttributedString alloc] initWithString:@"K"
																	 attributes:attrsDictionary];
	*fontHeight = [attrString size].height;
	return attrString;
}

-(void)mouseDown:(NSEvent *)event {
	[statusItemMenu setDelegate:self];
	[statusItem popUpStatusItemMenu:statusItemMenu];
	[self setNeedsDisplay:YES];
}

- (void)menuWillOpen:(NSMenu *)menu {
    isMenuVisible = YES;
    [self setNeedsDisplay:YES];
}

- (void)menuDidClose:(NSMenu *)menu {
    isMenuVisible = NO;
    [statusItemMenu setDelegate:nil];    
    [self setNeedsDisplay:YES];
}

// -------------------------------------------------------------------------------
//  - (void)menuNeedsUpdate:(NSMenu *)menu
// -------------------------------------------------------------------------------
- (void)menuNeedsUpdate:(NSMenu *)menu {
	[backgrounderDelegator menuNeedsUpdate:menu];
}
@end
