/*
 Copyright (c) The Growl Project, 2004-2005
 All rights reserved.


 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:


 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 3. Neither the name of Growl nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.


 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import "GrowlDefines.h"
#import "GrowlPathway.h"

#include <mach-o/dyld.h>
#include <unistd.h>
#include <getopt.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/param.h>

#define VMON_SOCKET     2106

#define STORESTR "store$"
#define FETCHSTR "fetch$"
#define WARNSTR  "warn$"  
#define STOREOFFT 6
#define FETCHOFFT 6
#define WARNOFFT  5

#define NOTIFICATION_NAME CFSTR("OpenAFS Venus Monitor")
#define APPLICATION_NAME CFSTR("afshelper")

#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x

static void notificationDismissed(CFNotificationCenterRef center,
								  void *observer,
								  CFStringRef name,
								  const void *object,
								  CFDictionaryRef userInfo) {
#pragma unused(center,observer,name,object,userInfo)
	/*CFRunLoopStop(CFRunLoopGetCurrent());*/
}

void getPath(char **selfPathPtr) 
{
	uint32_t selfPathSize = MAXPATHLEN;
	if(!(*selfPathPtr = malloc(selfPathSize)))
	{
		exit(-1);
	}
	if(_NSGetExecutablePath(*selfPathPtr, &selfPathSize) == -1)
	{
		// Try reallocating selfPath with the size returned by the func
		if(!(*selfPathPtr = realloc(*selfPathPtr, selfPathSize + 1)))
		{
			NSLog(@"Could not allocate memory to hold executable path.");
			exit(-1);
		}
		if(_NSGetExecutablePath(*selfPathPtr, &selfPathSize) != 0)
		{
			NSLog(@"Could not get executable path.");
			exit(-1);
		}
	}
}

static void
MyTransmit(CFNotificationCenterRef distCenter, CFDictionaryRef registerInfo, CFMutableDictionaryRef notificationInfo)
{
	NSConnection *connection = [NSConnection connectionWithRegisteredName:@"GrowlApplicationBridgePathway" host:nil];
	if (connection) {
		//Post to Growl via GrowlApplicationBridgePathway
		@try {
			NSDistantObject *theProxy = [connection rootProxy];
			[theProxy setProtocolForProxy:@protocol(GrowlNotificationProtocol)];
			id<GrowlNotificationProtocol> growlProxy = (id)theProxy;
			[growlProxy registerApplicationWithDictionary:(NSDictionary *)registerInfo];
			[growlProxy postNotificationWithDictionary:(NSDictionary *)notificationInfo];
		} @catch(NSException *e) {
			NSLog(@"exception while sending notification: %@", e);
		}
	} else {
		//Post to Growl via NSDistributedNotificationCenter
		NSLog(@"could not find local GrowlApplicationBridgePathway, falling back to NSDNC");
		CFNotificationCenterPostNotificationWithOptions(distCenter, (CFStringRef)GROWL_APP_REGISTRATION, NULL, registerInfo, kCFNotificationPostToAllSessions);
		CFNotificationCenterPostNotificationWithOptions(distCenter, (CFStringRef)GROWL_NOTIFICATION, NULL, notificationInfo, kCFNotificationPostToAllSessions);
	}
}

static void
BuildNotificationInfo(char *recvbuf, CFNotificationCenterRef dcref, CFDictionaryRef regref, CFDataRef icon)
{
	int priority;
	CFNumberRef priorityNumber;
	CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
	CFStringRef clickContext = CFUUIDCreateString(kCFAllocatorDefault, uuid);
	CFMutableDictionaryRef notificationInfo = CFDictionaryCreateMutable(kCFAllocatorDefault ,9, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_NAME, NOTIFICATION_NAME);
	CFDictionarySetValue(notificationInfo, GROWL_APP_NAME, APPLICATION_NAME);
	CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_STICKY, kCFBooleanFalse);
	CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_ICON, icon);
	CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_CLICK_CONTEXT, clickContext);
	CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_TITLE, CFSTR("OpenAFS")/*title*/);
	//CFRelease(title);
#if 0
	/* if fetching ever provides more data we could use this */
	CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_PROGRESS, progressNumber);
#endif
	if (!strncmp(recvbuf, FETCHSTR, sizeof(FETCHSTR)-1)) {
		CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_DESCRIPTION, CFStringCreateWithCString(kCFAllocatorDefault, recvbuf+FETCHOFFT, kCFStringEncodingUTF8));
		priority = -1;
		CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_IDENTIFIER, CFSTR("AFSFetch"));
	} else if (!strncmp(recvbuf, STORESTR, sizeof(STORESTR)-1)) {
		CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_DESCRIPTION, CFStringCreateWithCString(kCFAllocatorDefault, recvbuf+STOREOFFT, kCFStringEncodingUTF8));
		priority = -1;
		CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_IDENTIFIER, CFSTR("AFSStore"));
	} else if (!strncmp(recvbuf, WARNSTR, sizeof(WARNSTR)-1)) {
		CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_DESCRIPTION, CFStringCreateWithCString(kCFAllocatorDefault, recvbuf+WARNOFFT, kCFStringEncodingUTF8));
		priority = +1;
		CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_IDENTIFIER, CFSTR("AFSWarn"));
	} else {
		CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_DESCRIPTION, CFStringCreateWithCString(kCFAllocatorDefault, recvbuf, kCFStringEncodingUTF8));
		priority = -2;
		CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_IDENTIFIER, CFSTR("AFS"));
	}
	priorityNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &priority);
	CFDictionarySetValue(notificationInfo, GROWL_NOTIFICATION_PRIORITY, priorityNumber);
	CFRelease(priorityNumber);	
	CFRelease(clickContext);
	MyTransmit(dcref, regref, notificationInfo);
}

struct GrowlCBContext {
	CFNotificationCenterRef dcref;
	CFDictionaryRef regref;
	CFDataRef icon;
};

static void MySocketReadCallBack(CFSocketRef socket, CFSocketCallBackType callbackType, CFDataRef address, const void *data, void *info)
{
	struct GrowlCBContext* callbackInfo = (struct GrowlCBContext * )info;
	uint8_t recvBuffer[1024];
	int result;
	int recvSocket = CFSocketGetNative( socket );
	if (!info) return;
	if ( callbackType != kCFSocketReadCallBack ) return;

	result = recvfrom( recvSocket, recvBuffer, sizeof(recvBuffer), 0, NULL, NULL );
	if (result) {
		recvBuffer[result-1] = '\0';
		BuildNotificationInfo((char *)recvBuffer, callbackInfo->dcref, callbackInfo->regref, callbackInfo->icon);
	}
}

CFDataRef readFile(const char *filename)
{
    CFDataRef data;
    // read the file into a CFDataRef
    FILE *fp = fopen(filename, "r");
    if (fp) {
	fseek(fp, 0, SEEK_END);
	long dataLength = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	unsigned char *fileData = malloc(dataLength);
	fread(fileData, 1, dataLength, fp);
	fclose(fp);
	data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, fileData, dataLength, kCFAllocatorMalloc);
    } else
	data = NULL;
    
    return data;
}

int main(int argc, const char **argv) {
	BOOL         wait = NO;
	int          code = EXIT_SUCCESS;
	CFDataRef icon = NULL;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSString* myImage = [[NSBundle mainBundle] pathForResource:@"Andy" ofType:@"icns"];
	icon = (CFDataRef) readFile([myImage UTF8String]);

	// Register with Growl
	CFStringRef name = NOTIFICATION_NAME;
	CFArrayRef defaultAndAllNotifications = CFArrayCreate(kCFAllocatorDefault, (const void **)&name, 1, &kCFTypeArrayCallBacks);
	CFTypeRef registerKeys[4] = {
		GROWL_APP_NAME,
		GROWL_NOTIFICATIONS_ALL,
		GROWL_NOTIFICATIONS_DEFAULT,
		GROWL_APP_ICON
	};
	CFTypeRef registerValues[4] = {
		APPLICATION_NAME,
		defaultAndAllNotifications,
		defaultAndAllNotifications,
		icon
	};
	CFDictionaryRef registerInfo = CFDictionaryCreate(kCFAllocatorDefault, registerKeys, registerValues, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFRelease(defaultAndAllNotifications);
	CFRelease(icon);

	CFNotificationCenterRef distCenter;
	{
		distCenter = CFNotificationCenterGetDistributedCenter();
		if (wait) {
			CFMutableStringRef notificationName = CFStringCreateMutable(kCFAllocatorDefault, 0);
			CFStringRef applicationName1 = APPLICATION_NAME;
			CFStringAppend(notificationName, applicationName1);
			CFStringAppend(notificationName, (CFStringRef)GROWL_NOTIFICATION_CLICKED);
			CFNotificationCenterAddObserver(distCenter, "openafsgrowl", notificationDismissed, notificationName, NULL, CFNotificationSuspensionBehaviorCoalesce);
			CFStringReplaceAll(notificationName, applicationName1);
			CFStringAppend(notificationName, (CFStringRef)GROWL_NOTIFICATION_TIMED_OUT);
			CFNotificationCenterAddObserver(distCenter, "openafsgrowl", notificationDismissed, notificationName, NULL, CFNotificationSuspensionBehaviorCoalesce);
			CFRelease(notificationName);
		}
	}
	struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons(VMON_SOCKET), .sin_addr.s_addr = INADDR_ANY };
	CFDataRef theAddress = CFDataCreateWithBytesNoCopy(NULL, (UInt8 *)&sin, sizeof(struct sockaddr_in), kCFAllocatorNull);
	CFSocketSignature MarinerSignature = { PF_INET, SOCK_DGRAM, IPPROTO_UDP, theAddress };
	struct GrowlCBContext growlContext = {distCenter, registerInfo, icon};
	CFSocketContext MarinerSocketContext = {0, (void *)&growlContext, NULL, NULL, NULL };
	
	CFSocketRef MarinerSocket = CFSocketCreateWithSocketSignature(kCFAllocatorDefault, &MarinerSignature, kCFSocketReadCallBack, &MySocketReadCallBack, &MarinerSocketContext);

	if (!MarinerSocket)
		goto fail;
	
	CFSocketSetSocketFlags(MarinerSocket, kCFSocketCloseOnInvalidate|kCFSocketAutomaticallyReenableReadCallBack);
	
	CFRunLoopSourceRef MarinerRunLoopSource = CFSocketCreateRunLoopSource(NULL, MarinerSocket, 0);
	if (!MarinerRunLoopSource)
		goto fail;

	CFRunLoopAddSource(CFRunLoopGetCurrent(), MarinerRunLoopSource, kCFRunLoopCommonModes);

	/* Run the run loop until it is manually cancelled */
	CFRunLoopRun();

	CFRelease(registerInfo);
	CFRelease(MarinerRunLoopSource);
	CFSocketInvalidate(MarinerSocket);
	CFRelease(MarinerSocket);
fail:
	/* CFRelease(notificationInfo); */
	[pool release];

	return code;
}
