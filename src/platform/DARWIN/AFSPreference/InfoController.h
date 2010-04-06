//
//  InfoCommander.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 06/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface InfoController : NSObject {
	id infoPanel;
	id texEditInfo;
	
	NSAttributedString *htmlLicence;
}
- (IBAction) closePanel:(id) sender;
- (void)showHtmlResource :(NSString*)resourcePath;

@end
