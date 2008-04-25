//
//  afscellPane.m
//  afscell
//
//  Created by David Botsch on 10/23/07.
//  Copyright (c) 2007 __MyCompanyName__. All rights reserved.
//

#import "afscellPane.h"


@implementation afscellPane

- (NSString *)title
{
	return [[NSBundle bundleForClass:[self class]] localizedStringForKey:@"PaneTitle" value:nil table:nil];
}



/* called when user clicks "Continue" -- return value indicates if application should exit pane */
- (BOOL)shouldExitPane:(InstallerSectionDirection)dir
{
	if(InstallerDirectionForward == dir) {
		
		/* Update ThisCell and CellAlias files with input */
		NSString * WSCell = [ThisCell stringValue];
		NSString * WSAlias = [CellAlias stringValue];
		
		
					
				BOOL wr1 = [WSCell writeToFile:@"/private/tmp/ThisCell" atomically:YES];
				BOOL wr2;
		
				if ([WSAlias length] != 0) {
					NSMutableString * aliasString = [[NSMutableString alloc] init];
					[aliasString appendString:WSCell];
					[aliasString appendFormat:@" "];
					[aliasString appendString:WSAlias];
					wr2 = [aliasString writeToFile:@"/private/tmp/CellAlias" atomically:YES];
				}
		
				NSMutableString * results = [[NSMutableString alloc] init];
				[results appendFormat:@"Write 1 is %d and write 2 is %d\n", wr1, wr2];
				[results writeToFile:@"/private/tmp/writefile" atomically:YES];
			
		return YES;
	}
	return YES;
}

@end

