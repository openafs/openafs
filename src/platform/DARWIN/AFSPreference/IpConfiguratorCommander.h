//
//  IpConfiguratorCommander.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 18/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "DBCellElement.h"
#import "AFSCommanderPref.h"
@interface IpConfiguratorCommander : NSObject {
	id confPanel;
	id afsCommanderPref;
	id textFieldCellName;
	id textFieldComment;
	id tableViewCellIP;
	
	id modifyButton;
	id createButton;
	id deleteButton;
	
	BOOL hasSaved;
	DBCellElement *cellElement;
	NSMutableArray *bkIPArray;
	NSMutableArray *workIPArray;
	CellIp *currentSelectedIP;
}

- (void) setWorkCell:(DBCellElement*)cell;
- (IBAction) save:(id) sender;
- (IBAction) cancel:(id) sender;
- (IBAction) createNewIP:(id) sender;
- (IBAction) cancelIP:(id) sender;
- (BOOL) saved;

- (id) getPanel;
- (void) commitModify;
- (void) rollbackModify;
- (void) loadValueFromCellIPClass;
- (void) manageTableSelection:(int)row;
@end
