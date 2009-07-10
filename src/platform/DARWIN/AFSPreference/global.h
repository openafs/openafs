/*
 *  global.h
 *  AFSCommander
 *
 *  Created by Claudio on 13/07/07.
 *  Copyright 2007 INFN. All rights reserved.
 *
 */

#define kMenuBarHeight				22
#define TOKENS_REFRESH_TIME_IN_SEC	60

// Localized string
#define kDevelopInfo					NSLocalizedStringFromTableInBundle(@"DevelopInfo",nil,[NSBundle bundleForClass:[self class]],@"DevelopInfo")
#define kConfigurationSaved				NSLocalizedStringFromTableInBundle(@"ConfigurationSaved",nil,[NSBundle bundleForClass:[self class]],@"ConfigurationSaved")
#define kSavedCacheConfiguration		NSLocalizedStringFromTableInBundle(@"SavedCacheConfiguration",nil,[NSBundle bundleForClass:[self class]],@"SavedCacheConfiguration")
#define kAfsOn							NSLocalizedStringFromTableInBundle(@"AfsOn",nil,[NSBundle bundleForClass:[self class]],@"AfsOn")
#define kAfsOff							NSLocalizedStringFromTableInBundle(@"AfsOff",nil,[NSBundle bundleForClass:[self class]],@"AfsOff")
#define kAfsButtonShutdown				NSLocalizedStringFromTableInBundle(@"AfsButtonShutdown",nil,[NSBundle bundleForClass:[self class]],@"AfsButtonShutdown")
#define kAfsButtonStartup				NSLocalizedStringFromTableInBundle(@"AfsButtonStartup",nil,[NSBundle bundleForClass:[self class]],@"AfsButtonStartup")
#define kNewCellName					NSLocalizedStringFromTableInBundle(@"NewCellName",nil,[NSBundle bundleForClass:[self class]],@"NewCellName")
#define kNewCellComment					NSLocalizedStringFromTableInBundle(@"NewCellComment",nil,[NSBundle bundleForClass:[self class]],@"NewCellComment")
#define kMenuLogin						NSLocalizedStringFromTableInBundle(@"MenuLogin",nil,[NSBundle bundleForClass:[self class]],@"MenuLogin")
#define kMenuUnlog						NSLocalizedStringFromTableInBundle(@"MenuUnlog",nil,[NSBundle bundleForClass:[self class]],@"MenuUnlog")
#define kBadAfsRootMountPoint			NSLocalizedStringFromTableInBundle(@"BadAfsRootMountPoint",nil,[NSBundle bundleForClass:[self class]],@"BadAfsRootMountPoint")
#define kDoYouWantCreateTheDirectory	NSLocalizedStringFromTableInBundle(@"DoYouWantCreateTheDirectory",nil,[NSBundle bundleForClass:[self class]],@"DoYouWantCreateTheDirectory")
#define kDirectoryCreated				NSLocalizedStringFromTableInBundle(@"DirectoryCreated",nil,[NSBundle bundleForClass:[self class]],@"DirectoryCreated")
#define kErrorCreatingDirectory			NSLocalizedStringFromTableInBundle(@"ErrorCreatingDirectory",nil,[NSBundle bundleForClass:[self class]],@"ErrorCreatingDirectory")
#define kErrorGettongSystemVersion		NSLocalizedString(@"ErrorGettongSystemVersion",@"ErrorGettongSystemVersion");


// PREFERENCE KEY
#define PREFERENCE_AFS_SYS_PAT			@"PREFERENCE_AFS_SYS_PAT"
#define PREFERENCE_AFS_SYS_PAT_STATIC	@"/var/db/openafs"
#define PREFERENCE_USE_AKLOG			@"PREFERENCE_USE_AKLOG"
#define PREFERENCE_START_AFS_AT_STARTUP	@"PREFERENCE_START_AFS_AT_STARTUP"

// AFSMENUEXTRA INFO
#define kAFSMenuExtra			[NSURL fileURLWithPath:[[self bundle] pathForResource:@"AFSMenuExtra" ofType:@"menu" inDirectory:@""]]
#define kMenuCrakerMenuExtra	[NSURL fileURLWithPath:[[self bundle] pathForResource:@"MenuCracker" ofType:@"menu" inDirectory:@""]]
#define kAFSMenuExtraID			@"it.infn.lnf.network.AFSMenuExtra"
#define kMenuCrakerMenuExtraID	@"net.sourceforge.menucracker2"


//Application id
#define afsCommanderID @"it.infn.lnf.network.openafs"
// Changed preference notification key
#define kPrefChangeNotification @"preference_changed"
//KLog menuextra window close
#define kLogWindowClosed @"klog_window_closed"
//Fired when some work as appened in AFSMenuExtra
#define kMenuExtraEventOccured @"menu_extra_event_occured"
// Changed preference notification key
#define kMExtraClosedNotification @"preference_changed"
// Update MenuExtra AfsState notification key
#define kMExtraAFSStateChange @"menu_extra_afs_state_change"

