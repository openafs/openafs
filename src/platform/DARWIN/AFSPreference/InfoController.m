//
//  InfoCommander.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 06/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "InfoController.h"


@implementation InfoController
// -------------------------------------------------------------------------------
//  awakeFromNib:
// -------------------------------------------------------------------------------
- (void)awakeFromNib
{
	htmlLicence = nil;
}

// -------------------------------------------------------------------------------
//  awakeFromNib:
// -------------------------------------------------------------------------------
- (void)showHtmlResource :(NSString*)resourcePath
{
	NSData *rtfData = [NSData dataWithContentsOfFile:resourcePath];
	htmlLicence = [[NSAttributedString alloc] initWithRTF:rtfData 
										documentAttributes:nil];
	[[((NSTextView*) texEditInfo ) textStorage] setAttributedString:htmlLicence];
}


// -------------------------------------------------------------------------------
//  awakeFromNib:
// -------------------------------------------------------------------------------
- (IBAction) closePanel:(id) sender
{
	if(htmlLicence) [htmlLicence release];
	[NSApp endSheet:infoPanel];
}
@end
