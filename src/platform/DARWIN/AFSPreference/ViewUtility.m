//
//  ViewUtility.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 25/12/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "ViewUtility.h"


@implementation ViewUtility
+(void) enbleDisableControlView:(NSView*)parentView 
				   controlState:(BOOL)controlState
{
	if(!parentView) return;
	NSArray *views = [parentView subviews];
	for(int idx = 0; idx < [views count]; idx++)
	{
		NSObject *obj = [views objectAtIndex:idx];
		if([obj isKindOfClass:[NSButton class]])
		{
			[(NSButton*)obj setEnabled:controlState];
			//[obj performSelector:@selector(setEnabled:) withObject:controlState];
		}
	}
	
}
@end
