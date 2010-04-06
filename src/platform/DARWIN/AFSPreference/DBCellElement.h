//
//  DBCellElement.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 14/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
// 
// Is the identification of a cell in he afs cell database

#import <Cocoa/Cocoa.h>
#import "CellIp.h"

@interface DBCellElement : NSObject {
	BOOL userDefaultForToken;
	BOOL userDefaultCell;
	NSString *cellName;
	NSString *cellComment;
	NSMutableArray *ipCellList;
}

/*!
 @method     init
 @abstract   
 @discussion 
 */
-(id) init;

/*!
 @method     dealloc
 @abstract   
 @discussion 
 */
-(void) dealloc;

/*!
 @method     setCellName
 @abstract   Set the cel name
 @discussion Release the old cell name andretain the new one 
 @param      name - Name of the cell
 */
-(void) setCellName:(NSString *)name;

/*!
 @method     getCellName
 @abstract   Return the cell name
 @discussion Return the cell name
 @result     Name of the cell
 */
-(NSString*) getCellName;

/*!
 @method     setCellComment
 @abstract   Set the comment of the cell
 @discussion Release the old cell comment and retain the new one
 @param      comment - Comment of the cell
 */
-(void) setCellComment:(NSString *)comment;

/*!
 @method     getCellComment
 @abstract   Get the comment of the cell
 @discussion Get the comment of the cell
 @result     comment of the cell

 */
-(NSString*) getCellComment;

/*!
 @method     setUserDefaultForToken
 @abstract   Set the userde fault for tokens for the cell
 @discussion If this cell is a cell used by user this flag will be true, 
			 for multi cell authentication will be more cell with this flag on true.
 @param      isDefault - true if the this cell is default which the user want to ge token for

 */
-(void) setUserDefaultForToken:(BOOL)isDefault;

/*!
 @method     userDefault
 @abstract   Return the user request for token flag
 @discussion 
 @result     if true this cell is used to get the tokens
 */
-(BOOL) userDefaultForToken;
/*!
 @method     setUserDefaultCell
 @abstract   set the user default cell state
 @discussion 
 @result     set this cell as default user cell
 */
-(void) setUserDefaultCell:(BOOL)isDefault;
/*!
 @method     userDefaultForCell
 @abstract  Return the user default cell state
 @discussion 
 @result     is true if this cell is the default cell
 */
-(BOOL) userDefaultForCell;
/*!
 @method     addIpToCell
 @abstract   Add an ipcell description to this cell
 @discussion Add a new IpCell class to this cell to specify the server ip
 @param      ip - CellIP class representing the ip of one server of the cell
 */
-(void) addIpToCell:(CellIp*)ip;

/*!
 @method     getIp
 @abstract   Return the array containing all ip for thi cell
 @discussion 
 @result     The array containing all the cell ip decription class
 */
-(NSMutableArray*) getIp;

/*!
 @method     description
 @abstract   Return the description of this cell
 @discussion The description is the same stile used in CellServDB file(for a single cell)
			 so calling thi method for all cell will be reconstructed the entire afs configuration file for CellSerDB
 @result     The string description of the cell with all the ip
 */
-(NSString*) description;

/*!
 @method     isEqual
 @abstract   Compare this object with anoter of the same type 
 @discussion Compare this object with the ine passed to the function
 @param      anObject - An object to compare with this.
 @result     true if the two object are the same.
 */
- (BOOL)isEqual:(id)anObject;

- (BOOL)isEqualToString:(NSString *)aString;
@end
