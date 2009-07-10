//
//  AFSMenuExtraView.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 11/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "AFSMenuExtraView.h"
#import "AFSPropertyManager.h"
@implementation AFSMenuExtraView

- initWithFrame:(NSRect)myRect menuExtra:(AFSMenuExtra*)myExtra {
	
	// Have super init
	self = [super initWithFrame:myRect];
	if(!self) {
		return nil;
	}
	
	// Store our extra
	theMenuExtra = myExtra;

	// Send it on back
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
	
	image = [theMenuExtra imageToRender];
    if (image) {
		// Live updating even when menu is down handled by making the extra
		// draw the background if needed.
		if ([theMenuExtra isMenuDown]) {
			[theMenuExtra drawMenuBackground:YES];
		}
		[image compositeToPoint:NSMakePoint(0, 0) operation:NSCompositeSourceOver];
	}
	
	//Draw, if necessary, the kerberos indicator for aklog usage for get token
	if([theMenuExtra useAklogPrefValue] == NSOnState) {
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
	NSFont *font = [NSFont fontWithName:@"Palatino-Roman" size:9.0];
	NSDictionary *attrsDictionary =	[NSDictionary dictionaryWithObject:font
																forKey:NSFontAttributeName];
	NSAttributedString *attrString = [[NSAttributedString alloc] initWithString:@"K"
																	 attributes:attrsDictionary];
	*fontHeight = [attrString size].height;
	return attrString;
}
@end
