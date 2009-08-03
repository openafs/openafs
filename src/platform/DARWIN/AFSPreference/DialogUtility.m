//
//  DialogUtility.m
//  OpenAFS
//
//  Created by Claudio Bisegni on 01/05/08.
//  Copyright 2008 Infn. All rights reserved.
//

#import "DialogUtility.h"


@implementation DialogUtility
// -------------------------------------------------------------------------------
//  showMessage:
// -------------------------------------------------------------------------------
+ (BOOL) showMessageYesNo:(NSString*) message delegator:(id)delegator functionSelecter:(SEL)fSelector window:(NSWindow*)window {
	NSAlert *alert = [[[NSAlert alloc] init] autorelease];
	[alert setMessageText:message];
	[alert addButtonWithTitle:@"Yes"];
	[alert addButtonWithTitle:@"No"];
	[alert beginSheetModalForWindow:window
					  modalDelegate:delegator 
					 didEndSelector:fSelector
						contextInfo:nil];
	return YES;
}
@end
