//
//  IpConfiguratorCommander.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 18/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "IpConfiguratorCommander.h"
#import "AFSCommanderPref.h"

@implementation IpConfiguratorCommander

// -------------------------------------------------------------------------------
//  awakeFromNib:
// -------------------------------------------------------------------------------
- (void)awakeFromNib
{
	[tableViewCellIP setDelegate:self];
	[tableViewCellIP setDataSource:self];
}

// -------------------------------------------------------------------------------
//  initWhitCell:
// -------------------------------------------------------------------------------
- (void) setWorkCell:(DBCellElement*)cell {
	cellElement = cell;
	currentSelectedIP = nil;
}

// -------------------------------------------------------------------------------
//  save:
// -------------------------------------------------------------------------------
- (IBAction) save:(id) sender
{
	hasSaved = YES;
	[self commitModify];
	[NSApp endSheet:confPanel];
}

// -------------------------------------------------------------------------------
//  commitModify:
// -------------------------------------------------------------------------------
- (void) commitModify
{
	//store the cell name
	[cellElement setCellName:[textFieldCellName stringValue]];
	[cellElement setCellComment:[textFieldComment stringValue]];
	[bkIPArray removeAllObjects];
	[bkIPArray setArray:workIPArray];
	[workIPArray release];
}

// -------------------------------------------------------------------------------
//  cancel:
// -------------------------------------------------------------------------------
- (IBAction) cancel:(id) sender
{
	hasSaved = NO;
	[self rollbackModify];
	[NSApp endSheet:confPanel];
}


// -------------------------------------------------------------------------------
//  rollbackModify:
// -------------------------------------------------------------------------------
- (void) rollbackModify
{
	// take bkarray
	[workIPArray release];
	workIPArray = nil;
}

// -------------------------------------------------------------------------------
//  createNewIP:
// -------------------------------------------------------------------------------
- (IBAction) createNewIP:(id) sender
{
	CellIp *ip = [[CellIp alloc] init];
	[workIPArray addObject:ip];
	[ip release];
	currentSelectedIP = ip;
	[tableViewCellIP reloadData];
	[tableViewCellIP scrollRowToVisible:[[cellElement getIp] count]-1];
}

// -------------------------------------------------------------------------------
//  cancelIP:
// -------------------------------------------------------------------------------
- (IBAction) cancelIP:(id) sender
{
	[workIPArray removeObjectAtIndex:[tableViewCellIP selectedRow]];
	[tableViewCellIP deselectAll:nil];
	[self manageTableSelection:-1];
	[tableViewCellIP reloadData];

}

// -------------------------------------------------------------------------------
//  hasSaved:
// -------------------------------------------------------------------------------
- (BOOL)saved
{
	return hasSaved;
}

// -------------------------------------------------------------------------------
//  loadValueFromCellIPClass:
// -------------------------------------------------------------------------------
- (void) loadValueFromCellIPClass
{
	[textFieldCellName setStringValue:[cellElement getCellName]];
	[textFieldComment setStringValue:[cellElement getCellComment]];
	[tableViewCellIP reloadData];
}

// -------------------------------------------------------------------------------
//  manageTableSelection:
// -------------------------------------------------------------------------------
- (void) manageTableSelection:(int)row
{
	//[((NSControl*) modifyButton) setEnabled:row>=0];
	[deleteButton setEnabled:row>=0];
}

- (id) getPanel
{
	return confPanel;
}
@end

//Windows Delegator
@implementation IpConfiguratorCommander(PanelDelegator)
// -------------------------------------------------------------------------------
//  windowDidBecomeKey:
// -------------------------------------------------------------------------------
- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
	if(!cellElement){
		[NSApp endSheet:confPanel];
		return;
	}
	
	bkIPArray = [cellElement getIp];
	workIPArray = [[NSMutableArray alloc] initWithArray:bkIPArray];
	[self loadValueFromCellIPClass];
}

// -------------------------------------------------------------------------------
//  windowWillClose:
// -------------------------------------------------------------------------------
- (void)windowWillClose:(NSNotification *)aNotification 
{
}
@end

//Table datasource
@implementation IpConfiguratorCommander (NSTableDataSource)
// -------------------------------------------------------------------------------
//  tableView:
// -------------------------------------------------------------------------------
- (id)	tableView:(NSTableView *) aTableView
	objectValueForTableColumn:(NSTableColumn *) aTableColumn
						  row:(int) rowIndex
{  
	
	NSString *result = nil;
	//NSMutableArray *cellArray = [cellElement getIp];
	CellIp *ipElement =  (CellIp*)[workIPArray objectAtIndex:rowIndex];
	NSString *identifier = (NSString*)[aTableColumn identifier];
	switch([identifier intValue]){
		case 1:
			result = [ipElement getCellIp];
			break;
			
		case 2:
			result = [ipElement getCellComment];
			break;
			
	}
	return result;  
}

// -------------------------------------------------------------------------------
//  numberOfRowsInTableView:
// -------------------------------------------------------------------------------
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
	return [workIPArray count];  
}

- (void)tableView:(NSTableView *)aTable setObjectValue:(id)aData 
   forTableColumn:(NSTableColumn *)aCol row:(int)aRow
{
	CellIp *ipElement =  (CellIp*)[workIPArray objectAtIndex:aRow];
	switch([[aCol identifier] intValue])
	{
		case 1:
			[ipElement setCellIp:[aData description]];
			break;
			
		case 2:
			[ipElement setCellComment:[aData description]];
			break;
	}
}
@end

// Table delegator
@implementation IpConfiguratorCommander (TableDelegate)
// -------------------------------------------------------------------------------
//  selectionShouldChangeInTableView:
// -------------------------------------------------------------------------------
- (BOOL)selectionShouldChangeInTableView:(NSTableView *)aTable
{
	[self manageTableSelection:[aTable selectedRow]];
	return YES;
}

// -------------------------------------------------------------------------------
//  tableView:
// -------------------------------------------------------------------------------
- (BOOL)tableView:(NSTableView *)aTable shouldSelectRow:(int)aRow
{
	[self manageTableSelection:aRow];
	return YES;
}
@end
