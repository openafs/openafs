//
//  CellIp.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 14/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface CellIp : NSObject {
	NSString *ip;
	NSString *ipComment;
}
-(id)init;
-(void)dealloc;
-(void)setCellIp:(NSString*)newip;
-(NSString*) getCellIp;
-(void)setCellComment:(NSString*)newcomment;
-(NSString*) getCellComment;
-(NSString*) description;
@end
