//
//  AFSMenuExtraView.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 11/07/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "SystemUIPlugin.h"
#import "AFSMenuExtra.h"

@interface AFSMenuExtraView : NSMenuExtraView {
	AFSMenuExtra 	*theMenuExtra;

}
- (NSAttributedString*) makeKerberosIndicator:(int*)fontHeight;
@end
