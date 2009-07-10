//
//  LynkCreationController.h
//  OpenAFS
//
//  Created by MacDeveloper on 03/06/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface LynkCreationController : NSObject {
	IBOutlet NSPanel		*lynkCreationSheet;
	IBOutlet NSTextField	*textFieldLinkDestPath;
	IBOutlet NSTextField	*textfieldLinkName;
	int						choiceResult;
}

-(NSPanel*)getView;
- (IBAction) save:(id) sender;
- (IBAction) cancell:(id) sender;
- (IBAction) selectLinkDest:(id) sender;
@end
