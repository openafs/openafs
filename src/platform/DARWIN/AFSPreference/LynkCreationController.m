//
//  LynkCreationController.m
//  OpenAFS
//
//  Created by MacDeveloper on 03/06/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "LynkCreationController.h"


@implementation LynkCreationController

-(NSPanel*)getView {
	return lynkCreationSheet;
}

- (IBAction) save:(id) sender {
	[NSApp endSheet:lynkCreationSheet];
}

- (IBAction) cancell:(id) sender{
	[NSApp endSheet:lynkCreationSheet];
}

- (IBAction) selectLinkDest:(id) sender {
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];
	[openPanel setCanChooseFiles:NO];
	[openPanel setCanChooseDirectories:YES];
	[openPanel setAllowsMultipleSelection:NO];
	choiceResult = [openPanel runModalForTypes:nil];
	switch(choiceResult){
		case NSOKButton:
			if([[openPanel filenames] count] == 1)  {
				[textFieldLinkDestPath setStringValue:[[openPanel filenames] objectAtIndex:0]];
			}
			break;
			
		case NSCancelButton:
			[textFieldLinkDestPath setStringValue:@""];
			break;
	}
}
@end
