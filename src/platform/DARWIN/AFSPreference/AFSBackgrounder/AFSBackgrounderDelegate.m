//
//  AFSBackgrounder.m
//  OpenAFS
//
//  Created by Claudio Bisegni on 29/07/09.
//  Copyright 2009 Infn. All rights reserved.
//

#import "AFSBackgrounderDelegate.h"


@implementation AFSBackgrounderDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	NSLog(@"applicationDidFinishLaunching");
	//Create the NSStatusBar and set its length
    statusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength] retain];
    
    //Used to detect where our files are
    NSBundle *bundle = [NSBundle mainBundle];
    
    //Allocates and loads the images into the application which will be used for our NSStatusItem
    statusImage = [[NSImage alloc] initWithContentsOfFile:[bundle pathForResource:@"hasToken" ofType:@"png"]];
    statusHighlightImage = [[NSImage alloc] initWithContentsOfFile:[bundle pathForResource:@"noToken" ofType:@"png"]];
    
    //Sets the images in our NSStatusItem
    [statusItem setImage:statusImage];
    [statusItem setAlternateImage:statusHighlightImage];
    
    //Tells the NSStatusItem what menu to load
    [statusItem setMenu:backgrounderMenu];
    //Sets the tooptip for our item
    [statusItem setToolTip:@"Andrews Menu Item"];
    //Enables highlighting
    [statusItem setHighlightMode:YES];
}
@end
