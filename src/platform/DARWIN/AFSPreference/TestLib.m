
#import "AFSPropertyManager.h"
#import "FileUtil.h"
#import "TaskUtil.h"
#import "global.h"

int CoreMenuExtraGetMenuExtra(CFStringRef identifier, void *menuExtra);
int CoreMenuExtraAddMenuExtra(CFURLRef path, int position, int whoCares, int whoCares2, int whoCares3, int whoCares4);
int CoreMenuExtraRemoveMenuExtra(void *menuExtra, int whoCares);
void printNSArray(NSArray *array);

int main(int argc, char *argv[])
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	/*AFSPropertyManager *propMan=[[AFSPropertyManager alloc] initWithAfsPath:@"/var/db/openafs"];
	[propMan loadConfiguration];
	[propMan saveCacheConfigurationFiles:true];
	[propMan release];*/
	
	//[AFSPropertyManager aklog];
	
	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:@"test"];
	[alert addButtonWithTitle:@"Yes"];
	[alert addButtonWithTitle:@"No"];
	int result = [alert runModal];
	
	[pool release];
}

void printNSArray(NSArray *array) {
	for(int idx = 0; idx < [array count]; idx++) {
		NSLog([[array objectAtIndex:idx] description]);
	}
}