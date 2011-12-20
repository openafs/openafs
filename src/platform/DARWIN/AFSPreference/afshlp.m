//
//  afshlp.m
//  AFSCommander
//
//  Created by Claudio on 28/06/07.
//


#include <sys/types.h>
#include <unistd.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <asl.h>
#include <sys/event.h>
#include <mach-o/dyld.h>
#import "TaskUtil.h"
#import "AuthUtil.h"
#import "PListManager.h"

#define AFS_DAEMON_STARTUPSCRIPT	"/Library/OpenAFS/Tools/root.client/usr/vice/etc/afs.rc"
#define AFS_DAEMON_PATH				"/Library/LaunchDaemons/org.openafs.filesystems.afs.plist"

 void stopAfs(int argc, char *argv[]);
 void getPath(char **selfPathPtr);
 void selfRepair(char *selfPath);
 void runWithSelfRepair(char *selfPath,int argc, char *argv[]);

int main(int argc, char *argv[])
{
	if (argc < 2)
		return 1; // nothing to do
	NSString *cmdString = [NSString stringWithCString:(const char *)argv[1] encoding:NSUTF8StringEncoding];

	if(argc == 2 && [cmdString rangeOfString:@"stop_afs"].location!=NSNotFound ){
		if (setuid(0) == -1)
			return 1;
		const char *stopArgs[] = {"stop", 0L};
		[[AuthUtil shared] execUnixCommand:AFS_DAEMON_STARTUPSCRIPT
									  args:stopArgs
									output:nil];
	} else 	if(argc == 2 && [cmdString rangeOfString:@"start_afs"].location!=NSNotFound){
		if (setuid(0) == -1)
			return 1;
		const char *startArgs[] = {"start", 0L};
		[[AuthUtil shared] execUnixCommand:AFS_DAEMON_STARTUPSCRIPT
									  args:startArgs
									output:nil];
	} else if(argc == 4 && [cmdString rangeOfString:@"enable_krb5_startup"].location!=NSNotFound) {
		int olduid = getuid();
		setuid(0);
		int arg2 = atoi(argv[2]);
		[PListManager krb5TiketAtLoginTime:[[NSNumber numberWithInt:arg2] boolValue]];
	} else if(argc == 3 && [cmdString rangeOfString:@"start_afs_at_startup"].location!=NSNotFound){
		if (setuid(0) == -1)
			return 1;
		BOOL enable = strcmp("enable", argv[2])==0;
		NSLog(@"Manage start_afs_at_startup with option %s from helper", argv[2]);
		[PListManager launchctlStringCommand:enable?@"load":@"unload"
									  option:[NSArray arrayWithObjects:@"-w", nil]
								   plistName:@AFS_DAEMON_PATH];
#if 0
	} else if(argc == 2 && [cmdString rangeOfString:@"check_afs_daemon"].location!=NSNotFound) {
		NSString *fsResult = [TaskUtil executeTaskSearchingPath:@"launchctl" args:[NSArray arrayWithObjects: @"list", nil]];
		BOOL checkAfsDaemon = (fsResult?([fsResult rangeOfString:@"org.openafs.filesystems.afs"].location != NSNotFound):NO);
		printf("afshlp:afs daemon registration result:%d",checkAfsDaemon);
#endif
	}
	return 0;
}

#if 0
void stopAfs(int argc, char *argv[])
{
	const char *umountArgs[] = {"-f", "/afs", 0L};
	[[AuthUtil shared] execUnixCommand:"/sbin/umount" 
								  args:umountArgs
								output:nil];
	
	const char *afsdArgs[] = {"-shutdown", 0L};
	[[AuthUtil shared] execUnixCommand:argv[3]
								  args:afsdArgs
								output:nil];
	
	const char *kernelExtArgs[] = {argv[2], 0L};
	[[AuthUtil shared] execUnixCommand:"/sbin/kextunload"
								  args:kernelExtArgs
								output:nil];
	
	[[AuthUtil shared] deautorize];
}

// Code to get the path to the executable using _NSGetExecutablePath.
void getPath(char **selfPathPtr)
{
    uint32_t selfPathSize = MAXPATHLEN;
    if(!(*selfPathPtr = malloc(selfPathSize)))
    {
        exit(-1);
    }
    if(_NSGetExecutablePath(*selfPathPtr, &selfPathSize) == -1)
    {
        // Try reallocating selfPath with the size returned by the function.
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

int main(int argc, char *argv[])
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    char *selfPath;
	
	NSLog(@"num of arguments %d", argc);
	int status = [[AuthUtil shared] autorize];
	if(status != noErr) exit(-1);
	
    // Get the path to the tool's executable
    getPath(&selfPath);

    //selfRepair(selfPath);
    // All done with the executable path
    if(selfPath) free(selfPath);
	
	// Now do the real work of running the command.
    runCommand(argc, argv);
    [[AuthUtil shared] deautorize];
    [pool release];
	
	return 0;
}

// Self-repair code. Found somehwere in internet
void selfRepair(char *selfPath)
{
    struct stat st;
    int fdTool;
	printf("selfRepair"); 
	
//    [[AuthUtil shared] autorize];
    
    // Open tool exclusively, noone can touch it when we work on it, this idea i kepped somewhere in internet
    fdTool = open(selfPath, O_NONBLOCK | O_RDONLY | O_EXLOCK, 0);
    
    if(fdTool == -1)
    {
        NSLog(@"Open Filed: %d.", errno);
        exit(-1);
    }
    
    if(fstat(fdTool, &st))
    {
        NSLog(@"fstat failed.");
        exit(-1);
    }
    
    // Disable group and world writability and make setuid root.
    if ((st.st_uid != 0) || (st.st_mode & S_IWGRP) || (st.st_mode & S_IWOTH) ||
	!(st.st_mode & S_ISUID))
    {
        fchown(fdTool, 0, st.st_gid);
	fchmod(fdTool, (st.st_mode & (~(S_IWGRP | S_IWOTH))) | S_ISUID);
    } else  NSLog(@"st_uid = 0");
    
    close(fdTool);
    
    NSLog(@"Self-repair done.");
}


// Code to execute the tool in self-repair mode.
void runWithSelfRepair(char *selfPath, int argc, char *argv[])
{
    int status;
    int pid;
	
      
	// Make the qargs array for passing to child the same args of father
    const char *arguments[] = {argv[1], argv[2], argv[3], "--self-repair", 0L};
	
	// Get the privileged AuthorizationRef
    [[AuthUtil shared] autorize];
	[[AuthUtil shared] execUnixCommand:selfPath 
								  args:arguments 
								output:nil];

    pid = wait(&status);
    if(pid == -1 || !WIFEXITED(status))
    {
        NSLog(@"Error returned from wait().");
        exit(-1);
    }
    
    // Exit with the same exit code as the self-repair child
    exit(WEXITSTATUS(status));
}
#endif
