//
//  FileUtil.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 21/06/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include "AuthUtil.h"

@interface FileUtil : NSObject {
	AuthUtil *autorization;
}

-(id) init;
-(void) dealloc;
-(OSStatus) startAutorization;
-(OSStatus) autorizedMoveFile:(NSString*)srcPath toPath:(NSString*)dstPath;
-(OSStatus) autorizedChown:(NSString*)filePath owner:(NSString*)owner group:(NSString*)group;
-(OSStatus) autorizedCopy:(NSString*)srcPath toPath:(NSString*)dstPath;
-(OSStatus) autorizedDelete:(NSString*)destFilePath;
-(void) endAutorization;
@end
