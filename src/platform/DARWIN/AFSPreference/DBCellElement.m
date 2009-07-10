//
//  DBCellElement.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 14/06/07.
//  Copyright 2007 INFN. All rights reserved.
//

#import "DBCellElement.h"


@implementation DBCellElement

// -------------------------------------------------------------------------------
//  init:
// -------------------------------------------------------------------------------
-(id) init{
	[super init];
	userDefaultForToken = false;
	userDefaultCell = false;
	ipCellList = [[NSMutableArray alloc] init];
	return self;
}

// -------------------------------------------------------------------------------
//  dealloc:
// -------------------------------------------------------------------------------
-(void) dealloc{
	if(ipCellList) [ipCellList release];
	if(cellName) [cellName release];
	if(cellComment) [cellComment release];
	[super dealloc];
}

// -------------------------------------------------------------------------------
//  setCellName:
// -------------------------------------------------------------------------------
-(void) setCellName:(NSString *)name
{
	if(cellName){
		[cellName release];
	}
	cellName = [name retain];
}

// -------------------------------------------------------------------------------
//  getCellName:
// -------------------------------------------------------------------------------
-(NSString*) getCellName{
	return cellName;
}

// -------------------------------------------------------------------------------
//  setCellComment:
// -------------------------------------------------------------------------------
-(void) setCellComment:(NSString *)comment{
	if(cellComment){
		[cellComment release];
	}
	cellComment = [comment retain];
}

// -------------------------------------------------------------------------------
//  getCellComment:
// -------------------------------------------------------------------------------
-(NSString*) getCellComment{
	return cellComment;
}

// -------------------------------------------------------------------------------
//  setCellComment:
// -------------------------------------------------------------------------------
-(void) setUserDefaultForToken:(BOOL)isDefault {
	userDefaultForToken = isDefault;
}

// -------------------------------------------------------------------------------
//  getCellComment:
// -------------------------------------------------------------------------------
-(BOOL) userDefaultForToken {
	return userDefaultForToken;
}

// -------------------------------------------------------------------------------
//  setCellComment:
// -------------------------------------------------------------------------------
-(void) setUserDefaultCell:(BOOL)isDefault {
	userDefaultCell = isDefault;
}

// -------------------------------------------------------------------------------
//  getCellComment:
// -------------------------------------------------------------------------------
-(BOOL) userDefaultForCell {
	return userDefaultCell;
}

// -------------------------------------------------------------------------------
//  addIpToCell:
// -------------------------------------------------------------------------------
-(void) addIpToCell:(CellIp*)ip{
	[ipCellList addObject:ip];
}

// -------------------------------------------------------------------------------
//  getIp:
// -------------------------------------------------------------------------------
-(NSMutableArray*) getIp{
	return ipCellList;
}

// -------------------------------------------------------------------------------
//  description:
// -------------------------------------------------------------------------------
-(NSString*) description{
	NSMutableString *desc = [[NSMutableString alloc] init];
	// write the description according to CellServDB semantic
	// write cell name
	[desc appendString:@">"];[desc appendString:cellName];[desc appendString:@" "];[desc appendString:@"#"];[desc appendString:cellComment];[desc appendString:@"\n"];
	
	// write all ip
	for(int idx = 0, count = [ipCellList count]; idx < count; idx++){
		CellIp *cellIP = (CellIp*)[ipCellList objectAtIndex:idx];
		[desc appendString:[cellIP description]];
	}
	
	return desc;
}

// -------------------------------------------------------------------------------
//  description:
// -------------------------------------------------------------------------------
- (BOOL)isEqual:(id)anObject
{
	if(!anObject) return NO;
	else return [cellName isEqual:[anObject getCellName]] && [cellComment isEqual:[anObject getCellComment]];
}

- (BOOL)isEqualToString:(NSString *)aString {
	if(!aString) return NO;
	else return [aString isEqualToString:[self getCellName]];
}
@end
