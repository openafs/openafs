//
//  afscellPane.h
//  afscell
//
//  Created by David Botsch on 10/23/07.
//  Copyright (c) 2007 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <InstallerPlugins/InstallerPlugins.h>

@interface afscellPane : InstallerPane {

	IBOutlet NSTextField * ThisCell;
	IBOutlet NSTextField * CellAlias;

}

@end
