//
//  LynkCreationController.m
//  OpenAFS
//
//  Created by MacDeveloper on 03/06/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "LynkCreationController.h"
#import "global.h"

@implementation LynkCreationController

-(NSPanel*)getView {
	return lynkCreationSheet;
}

- (IBAction) save:(id) sender {
	NSMutableDictionary *linkConfiguration = nil;
	if([[[textFieldLinkDestPath stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] length] == 0 ||
	   [[[textfieldLinkName stringValue] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] length] == 0 )
		return;
	
	//load all configuration
	NSData *prefData = (NSData*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_LINK_CONFIGURATION,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	if(prefData) {
		linkConfiguration = [NSPropertyListSerialization propertyListFromData:prefData
															 mutabilityOption:NSPropertyListMutableContainers
																	   format:nil
															 errorDescription:nil];
	} else {
		linkConfiguration = [NSMutableDictionary dictionaryWithCapacity:1];
	}

	[linkConfiguration setObject:[textFieldLinkDestPath stringValue] 
						  forKey:[textfieldLinkName stringValue]];
	
	//save new configuration
	prefData = [NSPropertyListSerialization dataWithPropertyList:linkConfiguration
														  format:NSPropertyListXMLFormat_v1_0
														 options:0
														   error:nil];
	CFPreferencesSetValue((CFStringRef)PREFERENCE_LINK_CONFIGURATION,
						  (CFDataRef)prefData,
						  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
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
