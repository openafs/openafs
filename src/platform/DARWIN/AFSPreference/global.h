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
#define PREFERENCE_AFS_SYS_PAT										@"PREFERENCE_AFS_SYS_PAT"
#define PREFERENCE_AFS_SYS_PAT_STATIC								@"/var/db/openafs"
#define PREFERENCE_USE_AKLOG										@"PREFERENCE_USE_AKLOG"
#define PREFERENCE_START_AFS_AT_STARTUP								@"PREFERENCE_START_AFS_AT_STARTUP"
#define PREFERENCE_SHOW_STATUS_MENU									@"PREFERENCE_SHOW_STATUS_MENU"
#define PREFERENCE_AKLOG_TOKEN_AT_LOGIN								@"PREFERENCE_AKLOG_TOKEN_AT_LOGIN"
#define PREFERENCE_USE_LINK											@"PREFERENCE_USE_LINK"
#define PREFERENCE_LINK_CONFIGURATION								@"PREFERENCE_LINK_CONFIGURATION"
#define PREFERENCE_KRB5_RENEW_TIME									@"PREFERENCE_KRB5_RENEW_TIME"
#define PREFERENCE_KRB5_RENEW_TIME_DEFAULT_VALUE					3600
#define PREFERENCE_KRB5_SEC_TO_EXPIRE_TIME_FOR_RENEW				@"PREFERENCE_KRB5_SEC_TO_EXPIRE_TIME_FOR_RENEW"
#define PREFERENCE_KRB5_SEC_TO_EXPIRE_TIME_FOR_RENEW_DEFAULT_VALUE	3600
#define PREFERENCE_KRB5_RENEW_CHECK_TIME_INTERVALL					@"PREFERENCE_KRB5_RENEW_CHECK_TIME_INTERVALL"
#define PREFERENCE_KRB5_RENEW_CHECK_TIME_INTERVALL_DEFAULT_VALUE	3600
#define PREFERENCE_KRB5_CHECK_ENABLE								@"PREFERENCE_KRB5_CHECK_ENABLE"

// AFSMENUEXTRA INFO
#define kAFSMenuExtra			[NSURL fileURLWithPath:[[self bundle] pathForResource:@"AFSBackgrounder" ofType:@"app" inDirectory:@""]]
#define kMenuCrakerMenuExtra	[NSURL fileURLWithPath:[[self bundle] pathForResource:@"MenuCracker" ofType:@"menu" inDirectory:@""]]

//notification id

#define kMenuCrakerMenuExtraID	@"net.sourceforge.menucracker2"


//Application id
#define kAFSMenuExtraID			@"it.infn.lnf.network.AFSBackgrounder"
#define kAfsCommanderID			@"it.infn.lnf.network.openafs"
// Changed preference notification key
#define kPrefChangeNotification @"preference_changed"
//KLog menuextra window close
#define kLogWindowClosed @"klog_window_closed"
//Fired when some work as appened in AFSMenuExtra
#define kMenuExtraEventOccured @"menu_extra_event_occured"
// Changed preference notification key
#define kMExtraClosedNotification @"preference_changed"
// Changed preference notification key
#define kMExtraTokenOperation @"kMExtraTokenOperation"
// Update MenuExtra AfsState notification key
#define kMExtraAFSStateChange @"menu_extra_afs_state_change"
// Update MenuExtra for show menu notification key
#define kMExtraAFSMenuChangeState @"kMExtraAFSMenuChangeState"
