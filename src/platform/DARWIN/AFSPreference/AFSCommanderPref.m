//
//  AFSCommanderPref.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 10/05/07.
//  Copyright (c) 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "AFSCommanderPref.h"
#import "IpConfiguratorCommander.h"
#import "TokenCredentialController.h"
#import "InfoController.h"
#import "TaskUtil.h"
#import "PListManager.h"
#import "DialogUtility.h"
#import "NSString+search.h"
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#import <CoreServices/CoreServices.h>


#define ADD_CELL_CONTROL_TAG 1
#define REMOVE_CELL_CONTROL_TAG 2

#define TABLE_TOKENS_LIST 1
#define TABLE_CELL_LIST 2
#define TABLE_LINK_LIST 3

#define TAB_TOKENS 1
#define TAB_CELL_SERV_DB 2
#define TAB_CACHE 3
#define TAB_LINK 4

//CellServDB table id
#define CELLSRVDB_TABLE_USR_DFLT_CHECK_COLUMN	0
#define CELLSRVDB_TABLE_DFLT_CHECK_COLUMN		1
#define CELLSRVDB_TABLE_NAME_COLUMN				2
#define CELLSRVDB_TABLE_DESCRIPTION_COLUMN		3

//Link Table
#define TABLE_COLUMN_LINK_NAME	0
#define TABLE_COLUMN_LINK_PATH	1


@implementation AFSCommanderPref

// -------------------------------------------------------------------------------
//  initWithBundle:
// -------------------------------------------------------------------------------
- (id)initWithBundle:(NSBundle *)bundle
{
    if ( ( self = [super initWithBundle:bundle] ) != nil ) {
        //appID = kAfsCommanderID;
	prefStartUp = 1;
    }
    return self;
}

// -------------------------------------------------------------------------------
//  mainView:
// -------------------------------------------------------------------------------
- (NSView *) mainView {
    if (prefStartUp == 1){
	SInt32 osxMJVers = 0;
	SInt32 osxMnVers = 0;
	if (Gestalt(gestaltSystemVersionMajor, &osxMJVers) == noErr && Gestalt(gestaltSystemVersionMinor, &osxMnVers) == noErr) {
	    if (osxMJVers == 10 && osxMnVers>= 5) {
		[afsCommanderView  setFrameSize:NSMakeSize(668, [afsCommanderView frame].size.height)];
                prefStartUp = 0;
	    }
	}
    }
	
    return afsCommanderView;
}

// -------------------------------------------------------------------------------
//  mainViewDidLoad:
// -------------------------------------------------------------------------------
- (void) mainViewDidLoad
{
    //CellServDB Table
    [cellList setDelegate:self];
    [cellList setTarget:self];
    [cellList setDoubleAction:@selector(tableDoubleAction:)];

    // Setup security.
    AuthorizationItem items = {kAuthorizationRightExecute, 0, NULL, 0};
    AuthorizationRights rights = {1, &items};
    [authView setAuthorizationRights:&rights];
    authView.delegate = self;
    [authView updateStatus:nil];
}

// -------------------------------------------------------------------------------
//  didSelect:
// -------------------------------------------------------------------------------
- (void) didSelect
{
    //try to install the launchd file for backgrounder
    //Remove launchd ctrl file
    @try {
	[PListManager installBackgrounderLaunchdFile:YES
		      resourcePath:[[self bundle] resourcePath]];
    }
    @catch (NSException * e) {
	NSDictionary *excecptDic = [e userInfo];
	NSNumber *keyNum = [excecptDic objectForKey:@"agent_folder_error"];
	if(keyNum && [keyNum boolValue]) {
	    // the dir HOME_LAUNCHD_AGENT_FOLDER (PListManager.h) must be created
	    NSBeginAlertSheet([[NSString stringWithString:kDoYouWantCreateTheDirectory] stringByAppendingString:HOME_LAUNCHD_AGENT_FOLDER],
			      @"Create", @"Cancel", nil,
			      [[self mainView] window],	self, @selector(credentialAtLoginTimeEventCreationLaunchAgentDir:returnCode:contextInfo:), NULL,
			      nil, @"", nil);
	}
    }
    @finally {
	
    }


    // Set Developer info
    [textFieldDevInfoLabel setStringValue:kDevelopInfo];
    // creating the lock
    tokensLock = [[NSLock alloc] init];

    //Initialization cellservdb and token list
    filteredCellDB = nil;
    tokenList = nil;

    [self readPreferenceFile];

    // alloc the afs property mananger
    afsProperty = [[AFSPropertyManager alloc] init];

    // register preference pane to detect menuextra killed by user
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self
						     selector:@selector(refreshTokensNotify:)
						     name:kAfsCommanderID
						     object:kMExtraTokenOperation];

    [[NSDistributedNotificationCenter defaultCenter] addObserver:self
						     selector:@selector(refreshGui:)
						     name:kAfsCommanderID
						     object:kMenuExtraEventOccured];

    //Register for mount/unmount afs volume
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
							selector:@selector(afsVolumeMountChange:)
							name:NSWorkspaceDidMountNotification object:nil];

    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self
							selector:@selector(afsVolumeMountChange:)
							name:NSWorkspaceDidUnmountNotification object:nil];

    // set self as table data source
    [cellList setDataSource:self];
    [tokensTable setDataSource:self];
    //[tableViewLink setDataSource:self];
    //check the afs state
    [self setAfsStatus];

    // let show the configuration after prefpane is open
    [self refreshConfiguration:nil];

    // refresh the token list
    //[self refreshTokens:nil];

    //refresh table to reflect the NSSearchField contained text
    [self searchCellTextEvent:nil];
}

// -------------------------------------------------------------------------------
//  credentialAtLoginTimeEventCreationLaunchAgentDir:
// -------------------------------------------------------------------------------
- (void) credentialAtLoginTimeEventCreationLaunchAgentDir:(NSWindow*)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo {
    [alert close];
    switch (returnCode) {
    case  1:
	if([[NSFileManager defaultManager] createDirectoryAtPath:[HOME_LAUNCHD_AGENT_FOLDER stringByExpandingTildeInPath]
					   withIntermediateDirectories:NO
					   attributes:nil
					   error:nil]) {

	    //Create the file
	    [PListManager installBackgrounderLaunchdFile:YES
			  resourcePath:[[self bundle] resourcePath]];
	    [self showMessage:kDirectoryCreated];
	} else {
	    [self showMessage:kErrorCreatingDirectory];
	}
	break;
    case 0:
	break;
    }
}


// -------------------------------------------------------------------------------
//  willUnselect:
// -------------------------------------------------------------------------------
- (void)willUnselect
{
    // remove self as datasource
    [cellList setDataSource:nil];
    [tokensTable setDataSource:nil];

    //release the afs property manager
    if(afsProperty) [afsProperty release];
    //release tokens list
    if(tokenList) [tokenList release];
    //Remove the cell temp array
    if(filteredCellDB) [filteredCellDB release];

    [self writePreferenceFile];

    // unregister preference pane to detect menuextra killed by user
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self
						     name:kAfsCommanderID
						     object:kMExtraClosedNotification];
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self
						     name:kAfsCommanderID
						     object:kMExtraTokenOperation];
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self
						     name:kAfsCommanderID
						     object:kMenuExtraEventOccured];
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self
							name:NSWorkspaceDidMountNotification object:nil];
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self
							name:NSWorkspaceDidUnmountNotification object:nil];

    [self stopTimer];
    [tokensLock release];
}


// -------------------------------------------------------------------------------
//  startTimer:
// -------------------------------------------------------------------------------
- (void)startTimer{
    //start the time for check tokens validity
    if(timerForCheckTokensList) return;
    timerForCheckTokensList = [NSTimer scheduledTimerWithTimeInterval:TOKENS_REFRESH_TIME_IN_SEC
				       target:self
				       selector:@selector(refreshTokens:)
				       userInfo:nil
				       repeats:YES];
    [timerForCheckTokensList fire];
}

// -------------------------------------------------------------------------------
//  stopTimer:
// -------------------------------------------------------------------------------
- (void)stopTimer{
    if(!timerForCheckTokensList) return;
    [timerForCheckTokensList invalidate];
    timerForCheckTokensList = nil;
}


// -------------------------------------------------------------------------------
//  readPreferenceFile:
// -------------------------------------------------------------------------------
- (void) readPreferenceFile
{
    // read the preference for aklog use
    NSNumber *useAklogPrefValue = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_USE_AKLOG, (CFStringRef)kAfsCommanderID,
								    kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    NSNumber *aklogTokenAtLogin = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_AKLOG_TOKEN_AT_LOGIN, (CFStringRef)kAfsCommanderID,
								    kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    [useAklogCheck setState:[useAklogPrefValue intValue]];
    [aklogCredentialAtLoginTime setEnabled:useAklogPrefValue && [useAklogPrefValue boolValue]];
    [aklogCredentialAtLoginTime setState:aklogTokenAtLogin && [aklogTokenAtLogin boolValue]];

    //check krb5 at login time
    [installKRB5AuthAtLoginButton setState:[PListManager checkKrb5AtLoginTimeLaunchdEnable]];

    //check for AFS enable at startup
    NSNumber *afsEnableStartupTime = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_START_AFS_AT_STARTUP,
								       (CFStringRef)kAfsCommanderID,  kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    if(afsEnableStartupTime)
	startAFSAtLogin = [afsEnableStartupTime boolValue];
    else
	startAFSAtLogin = false;
    //set the check button state
    [checkButtonAfsAtBootTime setState:startAFSAtLogin];

    NSNumber *showStatusMenu =  (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_SHOW_STATUS_MENU,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    [(NSButton*)afsMenucheckBox setState: [showStatusMenu boolValue]];

    //backgrounder state
    [backgrounderActivationCheck setState:[PListManager launchdJobState:BACKGROUNDER_P_FILE]];

    //link enabled status
    NSNumber *linkEnabledStatus =  (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_USE_LINK,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    [checkEnableLink setState:[linkEnabledStatus boolValue]];

    //check the user preference for manage the renew
    NSNumber *checkRenew =  (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_KRB5_CHECK_ENABLE,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if(checkRenew)[nsButtonEnableDisableKrb5RenewCheck setState:[checkRenew intValue]];

    NSNumber *renewTime = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_KRB5_RENEW_TIME,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if(!renewTime) renewTime = [NSNumber numberWithInt:PREFERENCE_KRB5_RENEW_TIME_DEFAULT_VALUE];

    //update gui
    NSCalendar *gregorian = [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
    NSDateComponents *weekdayComponents = [gregorian components:(NSDayCalendarUnit | NSHourCalendarUnit | NSMinuteCalendarUnit | NSSecondCalendarUnit)
						     fromDate:[NSDate dateWithTimeIntervalSince1970:[renewTime intValue]]];
    [nsTextFieldKrb5RenewTimeD setIntValue:[weekdayComponents day]-1];
    [nsTextFieldKrb5RenewTimeH setIntValue:[weekdayComponents hour]-1];
    [nsTextFieldKrb5RenewTimeM setIntValue:[weekdayComponents minute]];
    [nsTextFieldKrb5RenewTimeS setIntValue:[weekdayComponents second]];
    [nsStepperKrb5RenewTimeD setIntValue:[weekdayComponents day]-1];
    [nsStepperKrb5RenewTimeH setIntValue:[weekdayComponents hour]-1];
    [nsStepperKrb5RenewTimeM setIntValue:[weekdayComponents minute]];
    [nsStepperKrb5RenewTimeS setIntValue:[weekdayComponents second]];

    NSNumber *renewCheckTimeInterval = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_KRB5_RENEW_CHECK_TIME_INTERVALL,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if(renewCheckTimeInterval && [renewCheckTimeInterval intValue])[nsTextFieldKrb5RenewCheckIntervall setIntValue:[renewCheckTimeInterval intValue]];
    else [nsTextFieldKrb5RenewCheckIntervall setIntValue:PREFERENCE_KRB5_RENEW_CHECK_TIME_INTERVALL_DEFAULT_VALUE];

    NSNumber *expireTimeForRenew = (NSNumber*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_KRB5_SEC_TO_EXPIRE_TIME_FOR_RENEW,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if(expireTimeForRenew && [expireTimeForRenew intValue])[nsTextFieldKrb5SecToExpireDateForRenew setIntValue:[expireTimeForRenew intValue]];
    else [nsTextFieldKrb5SecToExpireDateForRenew setIntValue:PREFERENCE_KRB5_SEC_TO_EXPIRE_TIME_FOR_RENEW_DEFAULT_VALUE];

    //link configuration
    NSData *prefData = (NSData*)CFPreferencesCopyValue((CFStringRef)PREFERENCE_LINK_CONFIGURATION,  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    NSDictionary *linkDict = (NSMutableDictionary*)[NSPropertyListSerialization propertyListFromData:prefData
										mutabilityOption:NSPropertyListMutableContainers
										format:nil
										errorDescription:nil];
    linkConfiguration = [linkDict mutableCopy];
}

// -------------------------------------------------------------------------------
//  willUnselect:
// -------------------------------------------------------------------------------
- (void) writePreferenceFile
{
    //Set the preference for afs path
    //Set the preference for aklog use
    CFPreferencesSetValue((CFStringRef)PREFERENCE_USE_AKLOG,
			  (CFNumberRef)[NSNumber numberWithInt:[useAklogCheck state]],
			  (CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    //set AFS enable state at startup
    CFPreferencesSetValue((CFStringRef)PREFERENCE_START_AFS_AT_STARTUP,
			  (CFNumberRef)[NSNumber numberWithBool:startAFSAtLogin],
			  (CFStringRef)kAfsCommanderID, kCFPreferencesAnyUser, kCFPreferencesAnyHost);

    //set aklog at login
    CFPreferencesSetValue((CFStringRef)PREFERENCE_AKLOG_TOKEN_AT_LOGIN,
			  (CFNumberRef)[NSNumber numberWithBool:[aklogCredentialAtLoginTime state]],
			  (CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    //set aklog at login
    CFPreferencesSetValue((CFStringRef)PREFERENCE_SHOW_STATUS_MENU,
			  (CFNumberRef)[NSNumber numberWithBool:[(NSButton*)afsMenucheckBox state]],
			  (CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    //preference for link
    CFPreferencesSetValue((CFStringRef)PREFERENCE_USE_LINK,
			  (CFNumberRef)[NSNumber numberWithBool:[checkEnableLink state]],
			  (CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    //preference for renew time
    //NSLog(@"%d %d %d %d", [nsTextFieldKrb5RenewTimeD intValue],[nsTextFieldKrb5RenewTimeH intValue],[nsTextFieldKrb5RenewTimeM intValue],[nsTextFieldKrb5RenewTimeS intValue]);
    NSInteger totalSeconds =	([nsTextFieldKrb5RenewTimeD intValue]*24*60*60)+
	([nsTextFieldKrb5RenewTimeH intValue]*60*60)+
	([nsTextFieldKrb5RenewTimeM intValue]*60)+
	[nsTextFieldKrb5RenewTimeS intValue];

    CFPreferencesSetValue((CFStringRef)PREFERENCE_KRB5_RENEW_TIME,
			  (CFNumberRef)[NSNumber numberWithInt:totalSeconds],
			  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    //expire time for renew
    CFPreferencesSetValue((CFStringRef)PREFERENCE_KRB5_SEC_TO_EXPIRE_TIME_FOR_RENEW,
			  (CFNumberRef)[NSNumber numberWithInt:[nsTextFieldKrb5SecToExpireDateForRenew intValue]],
			  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    //sec to expiretime for renew job
    CFPreferencesSetValue((CFStringRef)PREFERENCE_KRB5_RENEW_CHECK_TIME_INTERVALL,
			  (CFNumberRef)[NSNumber numberWithInt:[nsTextFieldKrb5RenewCheckIntervall intValue]],
			  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kPrefChangeNotification];
}

// -------------------------------------------------------------------------------
//  saveConfiguration:
// -------------------------------------------------------------------------------
- (IBAction) saveConfiguration:(id) sender
{
    @try{
	[afsProperty setCellName:[afsProperty getDefaultCellName]];

	//save configurations
	[afsProperty saveConfigurationFiles:YES];
		
	//Reload all configuration
	[self refreshConfiguration:nil];

	//refresh table to reflect the NSSearchField contained text
	[self searchCellTextEvent:nil];
	
	//Show dialog for notifity al saving process ar gone ell
	[self showMessage:kConfigurationSaved];
    }@catch(NSException *e){
	[self showMessage:[e reason]];
    } @finally {
	[cellList reloadData];
    }
}

// -------------------------------------------------------------------------------
//  saveCacheManagerParam:
// -------------------------------------------------------------------------------
- (IBAction) saveCacheManagerParam:(id) sender
{
    @try{
	//Update the value form view to afs property manager class
	[self updateCacheParamFromView];
	[afsProperty saveCacheConfigurationFiles:YES];
	[self showMessage:kSavedCacheConfiguration];
    }@catch(NSException *e){
	[self showMessage:[e reason]];
    } @finally {
	[cellList reloadData];
    }
}

// -------------------------------------------------------------------------------
//  refreshConfiguration:
// -------------------------------------------------------------------------------
- (IBAction) refreshConfiguration:(id) sender
{
    NSString *afsBasePath = PREFERENCE_AFS_SYS_PAT_STATIC;
    @try{
	// set the afs path
	[afsProperty setPath:afsBasePath];

	// load configuration
	[afsProperty loadConfiguration];

	//set the afs version label
	[afsVersionLabel setStringValue:[afsProperty getAfsVersion]];

	//set the current default cell
	[afsDefaultCellLabel setStringValue:[afsProperty getDefaultCellName]];

	// Update cache view
	[self fillCacheParamView];

	//Filter the cellServDb and allocate filtered array
	[self filterCellServDB:nil];

    }@catch(NSException *e){
	[self showMessage:[e reason]];
    } @finally {
	[cellList reloadData];
    }
}

// -------------------------------------------------------------------------------
//  fillCacheParamView:
// -------------------------------------------------------------------------------
-(void) fillCacheParamView
{
    [dynRoot setState:[afsProperty dynRoot]?NSOnState:NSOffState];
    [afsDB setState:[afsProperty afsDB]?NSOnState:NSOffState];
    [statCacheEntry setIntValue:[afsProperty statCacheEntry]];
    [dCacheDim setIntValue:[afsProperty dCacheDim]];
    [cacheDimension setIntValue:[afsProperty cacheDimension]];
    [daemonNumber setIntValue:[afsProperty daemonNumber]];
    [afsRootMountPoint setStringValue:[afsProperty afsRootMountPoint]];
    [nVolEntry setIntValue:[afsProperty nVolEntry]];

    //new version property
    //[verbose setEnabled:[afsProperty useAfsdConfConfigFile]];
    [verbose setState:[afsProperty verbose]?NSOnState:NSOffState];

}

// -------------------------------------------------------------------------------
//  updateCacheParamFromView:
// -------------------------------------------------------------------------------
-(void) updateCacheParamFromView
{
    NSString *tmpAfsPath = [afsRootMountPoint stringValue];
    if(!tmpAfsPath || ([tmpAfsPath length] == 0) || ([tmpAfsPath characterAtIndex:0] != '/'))
	@throw [NSException exceptionWithName:@"updateCacheParamFromView"
			    reason:kBadAfsRootMountPoint
			    userInfo:nil];

    [afsProperty setDynRoot:[dynRoot state]==NSOnState];
    [afsProperty setAfsDB:[afsDB state]==NSOnState];
    [afsProperty setStatCacheEntry:[statCacheEntry intValue]];
    [afsProperty setDCacheDim:[dCacheDim intValue]];
    [afsProperty setCacheDimension:[cacheDimension intValue]];
    [afsProperty setDaemonNumber:[daemonNumber intValue]];
    [afsProperty setAfsRootMountPoint:tmpAfsPath];
    [afsProperty setNVolEntry:[nVolEntry intValue]];
    [afsProperty setVerbose:[verbose state]==NSOnState];
}


// -------------------------------------------------------------------------------
//  showCellIP:
// -------------------------------------------------------------------------------
- (IBAction) showCellIP:(id) sender
{
    int rowSelected = [((NSTableView *) cellList) selectedRow];
    [self modifyCellByIDX:rowSelected];
}

// -------------------------------------------------------------------------------
//  modifyCellByIDX:
// -------------------------------------------------------------------------------
-(void) modifyCellByIDX:(int) idx
{
    [self modifyCell:[self getCellByIDX:idx]];
}

// -------------------------------------------------------------------------------
//  modifyCellByIDX:
// -------------------------------------------------------------------------------
-(void) modifyCell:(DBCellElement*) cellElement
{
    [NSBundle loadNibNamed:@"IpPanel" owner:self];
    [((IpConfiguratorCommander*) ipConfControllerCommander) setWorkCell:cellElement];
    [NSApp beginSheet: ipConfigurationSheet
	   modalForWindow: [[self mainView] window]
	   modalDelegate: self
	   didEndSelector: @selector(didEndSheet:returnCode:contextInfo:)
	   contextInfo: nil];
}

// -------------------------------------------------------------------------------
//  addMoifyCell:
// -------------------------------------------------------------------------------
- (IBAction) addRemoveCell:(id) sender
{
    switch([((NSControl*) sender) tag]){
    case ADD_CELL_CONTROL_TAG:
    {
	DBCellElement *newCell = [[DBCellElement alloc] init];
	if(!newCell) break;
	
	[newCell setCellName:kNewCellName];
	[newCell setCellComment:kNewCellComment];
	//cellArray = ;
	[[afsProperty getCellList] addObject:newCell];
	[newCell release];
	
	//Modify new cell
	[self modifyCell:newCell];
    }
    break;
    
    case REMOVE_CELL_CONTROL_TAG:
    {
	int index = 0;
	NSIndexSet *selectedIndex = [(NSTableView*)cellList selectedRowIndexes];
	if( [selectedIndex count] > 0) {
	    index = [selectedIndex firstIndex];
	    do {
		DBCellElement *cellElement =  (DBCellElement*)[filteredCellDB objectAtIndex:index];
		[[afsProperty getCellList] removeObject:cellElement];
	    } while ((index = [selectedIndex indexGreaterThanIndex:index]) != NSNotFound);
	}
    }
    break;
    }
    //Filter the cellServDb and allocate filtered array
    [self searchCellTextEvent:nil];
    [cellList deselectAll:nil];
    [cellList reloadData];
}

// -------------------------------------------------------------------------------
//  startStopAfs:
// -------------------------------------------------------------------------------
- (IBAction) startStopAfs:(id) sender
{
    BOOL currentAfsState = NO;
    @try {
	currentAfsState = [afsProperty checkAfsStatus];
	// make the parameter to call the root helper app
	if(currentAfsState){
	    //shutdown afs
	    NSLog(@"Shutting down afs");
	    [afsProperty shutdown];
	} else {
	    //Start afs
	    NSLog(@"Starting up afs");
	    [afsProperty startup];
	}
	[self refreshGui:nil];
    }
    @catch (NSException * e) {
	[self showMessage:[e reason]];
    }
    @finally {
    }
}

// -------------------------------------------------------------------------------
//  info:
// -------------------------------------------------------------------------------
- (void) refreshGui:(NSNotification *)notification{
    BOOL afsIsUp = [afsProperty checkAfsStatus];
    [self setAfsStatus];
    [tokensButton setEnabled:afsIsUp];
    [unlogButton setEnabled:afsIsUp];

}

// -------------------------------------------------------------------------------
//  -(void) refreshTokensNotify:(NSNotification*)notification
// -------------------------------------------------------------------------------
-(void) refreshTokensNotify:(NSNotification*)notification {
    [self refreshTokens:nil];
}

// -------------------------------------------------------------------------------
//  afsVolumeMountChange: Track the afs volume state change
// -------------------------------------------------------------------------------
// XXX should use mountdir not /afs
- (void) afsVolumeMountChange:(NSNotification *)notification{
    // Check if is mounted or unmounted afs
    if([[[notification userInfo] objectForKey:@"NSDevicePath"] isEqualToString:@"/afs"]){
	[self setAfsStatus];
	[self refreshTokens:nil];
    }
}

// -------------------------------------------------------------------------------
//  info:
// -------------------------------------------------------------------------------
- (IBAction) info:(id) sender
{
    [infoController showHtmlResource:[[self bundle] pathForResource:@"license" ofType:@"rtf"]];

    [NSApp beginSheet: infoSheet
	   modalForWindow: [[self mainView] window]
	   modalDelegate: self
	   didEndSelector:  @selector(didEndInfoSheet:returnCode:contextInfo:)
	   contextInfo: nil];
}

// -------------------------------------------------------------------------------
//  tableDoubleAction:
// -------------------------------------------------------------------------------
- (IBAction) tableDoubleAction:(id) sender
{
    [self showCellIP:nil];
}

// -------------------------------------------------------------------------------
//  getNewToken:
// -------------------------------------------------------------------------------
- (IBAction) getNewToken:(id) sender
{
    BOOL useAklog = [useAklogCheck state] == NSOnState;
    if(useAklog){
	//[AFSPropertyManager aklog];
	[afsProperty getTokens:false
		     usr:nil
		     pwd:nil];
	[self refreshTokens:nil];
	//Inform afs menuextra to updata afs status
	[[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kMExtraAFSStateChange];

    } else {
	[NSBundle loadNibNamed:@"CredentialPanel" owner:self];
	[NSApp beginSheet: credentialSheet
	       modalForWindow: [[self mainView] window]
	       modalDelegate: self
	       didEndSelector: @selector(didEndCredentialSheet:returnCode:contextInfo:)
	       contextInfo: nil];
    }
}


// -------------------------------------------------------------------------------
//  getCurrentCellInDB:
// -------------------------------------------------------------------------------
- (IBAction) unlog:(id) sender
{
    int index = -1;
    NSIndexSet *selectedIndex = [tokensTable selectedRowIndexes];
    if( [selectedIndex count] > 0) {
	index = [selectedIndex firstIndex];
	do {
	    NSString *tokenDesc = [tokenList objectAtIndex:index];
	    NSString *cellToUnlog = [tokenDesc estractTokenByDelimiter:@"afs@"
					       endToken:@" "];
	    [afsProperty unlog:cellToUnlog];
	} while ((index = [selectedIndex indexGreaterThanIndex: index]) != NSNotFound);
    } else {
	[afsProperty unlog:nil];
    }
    [self refreshTokens:nil];
    //Inform afs menuextra to updata afs status
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kMExtraAFSStateChange];
}


// -------------------------------------------------------------------------------
//  aklogSwitchEvent:
// -------------------------------------------------------------------------------
- (IBAction) aklogSwitchEvent:(id) sender
{
    //afs menu extra is loaded inform it to read preference
    @try {
	if(![useAklogCheck state]) {
	    //deselect the checkbox
	    [aklogCredentialAtLoginTime setState:NO];
	}
	
	[self writePreferenceFile];

	//Enable disable aklog at login time checkbox according the useAklog checkbox
	[aklogCredentialAtLoginTime setEnabled:[useAklogCheck state]];

    }
    @catch (NSException * e) {
	[self showMessage:[e reason]];
    }
}

// -------------------------------------------------------------------------------
//  credentialAtLoginTimeEvent:
// -------------------------------------------------------------------------------
- (IBAction) credentialAtLoginTimeEvent:(id) sender {
    [self writePreferenceFile];
}

// -------------------------------------------------------------------------------
//  afsStartupSwitchEvent:
// -------------------------------------------------------------------------------
- (IBAction) afsStartupSwitchEvent:(id) sender {
    NSString *rootHelperApp = [[self bundle] pathForResource:@"afshlp" ofType:@""];
    //get the new state
    startAFSAtLogin = [checkButtonAfsAtBootTime state];
    [PListManager launchctlStringCommandAuth:startAFSAtLogin?@"load":@"unload"
		  option:[NSArray arrayWithObjects:@"-w", nil]
		  plistName:@AFS_DAEMON_PATH
		  helper:rootHelperApp
		  withAuthRef:[[authView authorization] authorizationRef]];
}


// -------------------------------------------------------------------------------
//  afsMenuActivationEvent:
// -------------------------------------------------------------------------------
- (IBAction) krb5KredentialAtLoginTimeEvent:(id) sender {
    NSString *rootHelperApp = [[self bundle] pathForResource:@"afshlp" ofType:@""];
    [PListManager krb5TiketAtLoginTime:[installKRB5AuthAtLoginButton state] helper:rootHelperApp];

    //check if all is gone well
    [installKRB5AuthAtLoginButton setState:[PListManager checkKrb5AtLoginTimeLaunchdEnable]];
}

// -------------------------------------------------------------------------------
//  afsMenuActivationEvent:
// -------------------------------------------------------------------------------
-(IBAction) afsMenuActivationEvent:(id) sender
{
    CFPreferencesSetValue((CFStringRef)PREFERENCE_SHOW_STATUS_MENU,
			  (CFNumberRef)[NSNumber numberWithBool:[(NSButton*)afsMenucheckBox state]],
			  (CFStringRef)kAfsCommanderID, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    //notify the backgrounder
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kMExtraAFSMenuChangeState];
}

// -------------------------------------------------------------------------------
//  searchCellTextEvent:
//		Fileter the CellServDB list according to NSSearch content
// -------------------------------------------------------------------------------
- (IBAction) searchCellTextEvent:(id) sender
{
    NSString *searchText = [[textSearchField stringValue] lowercaseString]; //filter string
    [self filterCellServDB:searchText];
    [((NSTableView*)cellList) reloadData];
}

// -------------------------------------------------------------------------------
//  clearCellServDBFiltering:
//		clear the NSSearchField and showw all CellServDB table
// -------------------------------------------------------------------------------
- (void) clearCellServDBFiltering {
    //Clear the text search
    [textSearchField setStringValue:@""];
    //load the temp array with all cell servdb
    [self searchCellTextEvent:nil];
}
// --------------------------------------o-----------------------------------------
//  filterCellServDB:
//  make the NSMutableArray with all cellservdb or filtered element
// -------------------------------------------------------------------------------
- (void) filterCellServDB:(NSString*)textToFilter {
    DBCellElement *cellElement; //Filtered element
    BOOL doFilter = !(textToFilter == nil || ([textToFilter length] == 0));

    // We can do filtering and make the temp array
    if(filteredCellDB){
	[filteredCellDB release];
    }
    filteredCellDB = [[NSMutableArray alloc] init];
    NSEnumerator *e = [[afsProperty getCellList] objectEnumerator];
    while(cellElement = (DBCellElement*)[e nextObject]) {
	// check if the element can be get
	if(doFilter) {
	    //Get the CellServDB array enumerator
	    NSRange rsltRng = [[[cellElement getCellName] lowercaseString] rangeOfString:textToFilter];
	    if(rsltRng.location != NSNotFound) {
		//we can add this cell to filtered
		[filteredCellDB addObject:[cellElement retain]];
	    }
	} else {
	    [filteredCellDB addObject:[cellElement retain]];

	}
    }
}
		   
// -------------------------------------------------------------------------------
//  getCurrentCellInDB:
// -------------------------------------------------------------------------------
- (DBCellElement*) getCurrentCellInDB
{
    int rowSelected = [cellList selectedRow];
    return [self getCellByIDX:rowSelected];
}

// -------------------------------------------------------------------------------
//  getCurrentCellInDB:
// -------------------------------------------------------------------------------
- (DBCellElement*) getCellByIDX:(int) idx
{
    DBCellElement *cellElement =  (DBCellElement*)[filteredCellDB objectAtIndex:idx];
    return cellElement;
}

// -------------------------------------------------------------------------------
//  showMessage:
// -------------------------------------------------------------------------------
-(void) showMessage:(NSString*) message{
    NSAlert *alert = [[NSAlert alloc] init];

    [alert setMessageText:message];
    [alert beginSheetModalForWindow:[[self mainView] window]
	   modalDelegate:nil
	   didEndSelector:nil
	   contextInfo:nil];
    [alert release];
}

// -------------------------------------------------------------------------------
//  setAfsStatus:
// -------------------------------------------------------------------------------
-(void) setAfsStatus
{
    BOOL afsIsUp = [afsProperty checkAfsStatus];
    BOOL afsEnabledAtStartup = NO;
    NSString *rootHelperApp = [[self bundle] pathForResource:@"afshlp" ofType:@""];

    if ([self isUnlocked]) {
	afsEnabledAtStartup = (
	    [TaskUtil executeTaskWithAuth:@"/bin/launchctl"
		      arguments:[NSArray arrayWithObjects:@"list",
					 @"org.openafs.filesystems.afs", nil]
		      helper:rootHelperApp
		      withAuthRef:[[authView authorization] authorizationRef]
	     ] == noErr)?YES:NO;
	[checkButtonAfsAtBootTime setState:afsEnabledAtStartup];
    }

    [startStopButton setTitle: (afsIsUp?kAfsButtonShutdown:kAfsButtonStartup)];

    NSMutableAttributedString *colorTitle =[[NSMutableAttributedString alloc] initWithAttributedString:[startStopButton attributedTitle]];
    NSRange titleRange = NSMakeRange(0, [colorTitle length]);

    [colorTitle addAttribute:NSForegroundColorAttributeName
		value:(afsIsUp?[NSColor redColor]:[NSColor blackColor])
		range:titleRange];

    [startStopButton setAttributedTitle:colorTitle];
    if(afsIsUp) {
	[self startTimer];
    } else {
	[self stopTimer];
    }
}

// -------------------------------------------------------------------------------
//  refreshToken:
// -------------------------------------------------------------------------------
- (void) refreshTokens:(NSTimer*)theTimer;
{
    if(![tokensLock tryLock]) return;
    if(tokenList){
	[tokenList release];
    }

    tokenList = [afsProperty getTokenList];
    [tokensTable reloadData];
    [tokensLock unlock];
}

// -------------------------------------------------------------------------------
//  removeExtra:
// -------------------------------------------------------------------------------
- (IBAction) addLink:(id) sender {
    [NSBundle loadNibNamed:@"SymLinkEdit" owner:self];

    [NSApp beginSheet: lyncCreationSheet
	   modalForWindow: [[self mainView] window]
	   modalDelegate: self
	   didEndSelector: @selector(didEndSymlinkSheet:returnCode:contextInfo:)
	   contextInfo: nil];
}

// -------------------------------------------------------------------------------
//  removeExtra:
// -------------------------------------------------------------------------------
- (IBAction) removeLink:(id) sender {
    if(!linkConfiguration) return;
    int index = 0;
    NSArray *keys = [linkConfiguration allKeys];
    NSIndexSet *linkToRemove = [tableViewLink selectedRowIndexes];
    if( [linkToRemove count] > 0) {
	index = [linkToRemove firstIndex];
	do {
	    [linkConfiguration removeObjectForKey:[keys objectAtIndex:index]];
	} while ((index = [linkToRemove indexGreaterThanIndex:index]) != -1);
    }

    //write the new configuration
    NSData *prefData = nil;
    if([linkConfiguration count] > 0) {
	prefData = [NSPropertyListSerialization dataWithPropertyList:linkConfiguration
						format:NSPropertyListXMLFormat_v1_0
						options:0
						error:nil];
    }
    CFPreferencesSetValue((CFStringRef)PREFERENCE_LINK_CONFIGURATION,
			  (CFDataRef)prefData,
			  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

    //reload the new data
    [tableViewLink reloadData];
}

// -------------------------------------------------------------------------------
//  removeExtra:
// -------------------------------------------------------------------------------
- (IBAction) enableLink:(id) sender {
    [self writePreferenceFile];
}

// -------------------------------------------------------------------------------
//  removeExtra:
// -------------------------------------------------------------------------------
- (IBAction) manageBackgrounderActivation:(id)sender {
    [PListManager launchctlCommand:[(NSButton*)sender state]
		  userDomain:YES
		  option:[NSArray arrayWithObjects:@"-S", @"Aqua", nil]
		  plistName:[NSString stringWithFormat:@"%@.plist", BACKGROUNDER_P_FILE]];
    //read the status to check that all is gone well
    [backgrounderActivationCheck setState:[PListManager launchdJobState:BACKGROUNDER_P_FILE]];
}

// -------------------------------------------------------------------------------
//  tableViewLinkPerformClick:
// -------------------------------------------------------------------------------
- (IBAction) tableViewLinkPerformClick:(id) sender {
    NSLog(@"tableViewLinkPerformClick");
}

// -------------------------------------------------------------------------------
//  - (void)tabView:(NSTabView *)tabView willSelectTabViewItem: (NSTabViewItem *)tabViewItem
// -------------------------------------------------------------------------------
- (void)tabView:(NSTabView *)tabView willSelectTabViewItem: (NSTabViewItem *)tabViewItem 
{
    //check to see if the cache param tab is the tab that will be selected
    if([((NSString*)[tabViewItem identifier]) intValue] == TAB_LINK)
    {
	[tableViewLink reloadData];
    }
}
// -------------------------------------------------------------------------------
//  tableViewLinkPerformClick:
// -------------------------------------------------------------------------------
- (IBAction) enableDisableKrb5RenewCheck:(id) sender {
    //NSLog(@"enableDisableKrb5RenewCheck");
    CFPreferencesSetValue((CFStringRef)PREFERENCE_KRB5_CHECK_ENABLE,
			  (CFNumberRef) [NSNumber numberWithInt:[(NSButton*)sender intValue]],
			  (CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    CFPreferencesSynchronize((CFStringRef)kAfsCommanderID,  kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    //notify the backgrounder
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kPrefChangeNotification];
}
// -------------------------------------------------------------------------------
//  tableViewLinkPerformClick:
// -------------------------------------------------------------------------------
- (IBAction) krb5RenewParamChange:(id) sender {
}


- (BOOL)isUnlocked {
    return [authView authorizationState] == SFAuthorizationViewUnlockedState;
}

- (void)authorizationViewDidAuthorize:(SFAuthorizationView *)view {
    // enable things
    [startStopButton setEnabled:[self isUnlocked]];
    [checkButtonAfsAtBootTime setEnabled:[self isUnlocked]];
    [self setAfsStatus];
    [installKRB5AuthAtLoginButton setEnabled:[self isUnlocked]];
}

- (void)authorizationViewDidDeauthorize:(SFAuthorizationView *)view {
    // disable things
    [startStopButton setEnabled:[self isUnlocked]];
    [checkButtonAfsAtBootTime setEnabled:[self isUnlocked]];
    [installKRB5AuthAtLoginButton setEnabled:[self isUnlocked]];
}
@end

@implementation AFSCommanderPref (NSTableDataSource)


// -------------------------------------------------------------------------------
//  tableView:
//		Manage the checkbox of CellServDB Table

// -------------------------------------------------------------------------------
- (void)tableView:(NSTableView *)table 
   setObjectValue:(id)data 
   forTableColumn:(NSTableColumn *)col 
	      row:(int)row
{
    NSString *identifier = (NSString*)[col identifier];
    switch([table tag]){
    case TABLE_TOKENS_LIST:
	break;
	
    case TABLE_CELL_LIST:
	// we are editing checkbox for cellservdb table
	if([identifier intValue] == CELLSRVDB_TABLE_USR_DFLT_CHECK_COLUMN) {
	    // set the user default cell
	    DBCellElement *cellElement =  (DBCellElement*)[filteredCellDB objectAtIndex:row];
	    [afsProperty setDefaultCellByName:[cellElement getCellName]];
	    //[afsDefaultCellLabel setStringValue:[afsProperty getDefaultCellName]];
	    [((NSTableView*)cellList) reloadData];
	} else if([identifier intValue] == CELLSRVDB_TABLE_DFLT_CHECK_COLUMN) {
	    // set the cell for wich the user want to get token
	    DBCellElement *cellElement =  (DBCellElement*)[filteredCellDB objectAtIndex:row];
	    [cellElement setUserDefaultForToken:![cellElement userDefaultForToken]];
	}
	break;
    }
}


// -------------------------------------------------------------------------------
//  tableView:
//		refresh delegate method for two AFSCommander table
// -------------------------------------------------------------------------------
- (id) tableView:(NSTableView *) aTableView
objectValueForTableColumn:(NSTableColumn *) aTableColumn
	     row:(int) rowIndex
{  
    id result = nil;
    NSString *identifier = (NSString*)[aTableColumn identifier];
    switch([aTableView tag]){
    case TABLE_TOKENS_LIST:
	//We are refreshing tokens table
	result = [self getTableTokensListValue:[identifier intValue] row:rowIndex];
	break;
	
    case TABLE_CELL_LIST:
	//We are refreshing cell db table
	result = [self getTableCelListValue:[identifier intValue] row:rowIndex];
	break;

    case TABLE_LINK_LIST:
	result = [self getTableLinkValue:[identifier intValue] row:rowIndex];
	break;
    }
    return result;
}


// -------------------------------------------------------------------------------
//  getTableCelListValue:
// -------------------------------------------------------------------------------
- (id)getTableTokensListValue:(int) colId row:(int)row
{
    id result = nil;
    if(!tokenList) return nil;
    switch(colId){
    case 0:
	result = (NSString*)[tokenList objectAtIndex:row];
	break;
    }
    return result;
}


// -------------------------------------------------------------------------------
//  getTableCelListValue:
// -------------------------------------------------------------------------------
- (id)getTableCelListValue:(int) colId row:(int)row
{
    id result = nil;
    //NSMutableArray *cellArray = [afsProperty getCellList];
    DBCellElement *cellElement =  (DBCellElement*)[filteredCellDB objectAtIndex:row];
    switch(colId){
    case CELLSRVDB_TABLE_USR_DFLT_CHECK_COLUMN:
	result = [NSNumber numberWithInt:[cellElement userDefaultForCell]];
	break;

    case CELLSRVDB_TABLE_DFLT_CHECK_COLUMN:
	result = [NSNumber numberWithInt:[cellElement userDefaultForToken]];
	break;
    case CELLSRVDB_TABLE_NAME_COLUMN:
	result = [cellElement getCellName];
	break;

    case CELLSRVDB_TABLE_DESCRIPTION_COLUMN:
	result = [cellElement getCellComment];
	break;
    }
    return result;
}

// -------------------------------------------------------------------------------
//  getTableCelListValue:
// -------------------------------------------------------------------------------
- (id)getTableLinkValue:(int) colId row:(int)row
{
    id result = nil;
    NSArray *allKey = [linkConfiguration allKeys];
    switch(colId){
    case TABLE_COLUMN_LINK_NAME:
	result = [allKey objectAtIndex:row];
	break;

    case TABLE_COLUMN_LINK_PATH:
	result = [linkConfiguration objectForKey:[allKey objectAtIndex:row]];
	break;
    }
    return result;
}


// -------------------------------------------------------------------------------
//  numberOfRowsInTableView:
// -------------------------------------------------------------------------------
- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
    int rowCount = 0;
    switch([aTableView tag]){
    case TABLE_TOKENS_LIST:
	if(tokenList)  rowCount = [tokenList count];
	break;

    case TABLE_CELL_LIST:
	if(filteredCellDB)  rowCount = [filteredCellDB count];
	break;

    case TABLE_LINK_LIST:
	if(linkConfiguration)  rowCount = [linkConfiguration count];
	break;
    }
    return rowCount;
}
@end


@implementation AFSCommanderPref (TableDelegate)
// -------------------------------------------------------------------------------
//  selectionShouldChangeInTableView:
// -------------------------------------------------------------------------------
- (BOOL)selectionShouldChangeInTableView:(NSTableView *)aTable
{
    switch([aTable tag]){
    case TABLE_TOKENS_LIST:
	break;
	
    case TABLE_CELL_LIST:
	[self tableViewCellmanageButtonState:[aTable selectedRow]];
	break;

    case TABLE_LINK_LIST:
	break;


    }

    return YES;
}

// -------------------------------------------------------------------------------
//  tableView:
// -------------------------------------------------------------------------------
- (BOOL)tableView:(NSTableView *)aTable shouldSelectRow:(int)aRow
{
    switch([aTable tag]){
    case TABLE_TOKENS_LIST:
	break;

    case TABLE_CELL_LIST:
	[self tableViewCellmanageButtonState:aRow];
	break;
	
    case TABLE_LINK_LIST:
	break;
    }
    return YES;
}

// -------------------------------------------------------------------------------
//  tableView:
// -------------------------------------------------------------------------------
- (void)tableViewSelectionDidChange:(NSNotification *)aNotification {
    NSTableView *aTable = [aNotification object];
    switch([aTable tag]){
    case TABLE_TOKENS_LIST:
	break;

    case TABLE_CELL_LIST:
	break;

    case TABLE_LINK_LIST:
	[self tableViewLinkmanageButtonState:[aTable selectedRowIndexes]];
	break;
    }
}
// -------------------------------------------------------------------------------
//  manageButtonState:
// -------------------------------------------------------------------------------
-(void) tableViewCellmanageButtonState:(int) rowSelected  {
    [cellIpButton setEnabled:rowSelected >= 0];
    [removeCellButton setEnabled:rowSelected >= 0];
}

// -------------------------------------------------------------------------------
//  manageButtonState:
// -------------------------------------------------------------------------------
-(void) tableViewLinkmanageButtonState:(NSIndexSet *) rowsSelectedIndex {
    [buttonRemoveLink setEnabled:[rowsSelectedIndex count]>0];
}
@end


@implementation AFSCommanderPref (ModalDelegate)
// -------------------------------------------------------------------------------
//  didEndSheet:
// -------------------------------------------------------------------------------
- (void)didEndSheet:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
    //Filter the cellServDb and allocate filtered array
    [self searchCellTextEvent:nil];
    [cellList reloadData];
}

// -------------------------------------------------------------------------------
//  Klog credential request
// -------------------------------------------------------------------------------
- (void)didEndCredentialSheet:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if([((TokenCredentialController*)credentialCommander) takenToken] == YES){
	/*[AFSPropertyManager klog:[((TokenCredentialController*)credentialCommander) uName]
	  uPwd:[((TokenCredentialController*)credentialCommander) uPwd] ];*/
	[afsProperty getTokens:true
		     usr:[((TokenCredentialController*)credentialCommander) uName]
		     pwd:[((TokenCredentialController*)credentialCommander) uPwd]];
    }
    [sheet orderOut:self];
    [self refreshTokens:nil];
    //Inform afs menuextra to updata afs status
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:kAFSMenuExtraID object:kMExtraAFSStateChange];
}

// -------------------------------------------------------------------------------
//  Klog credential request
// -------------------------------------------------------------------------------
- (void)didEndInfoSheet:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
}

// -------------------------------------------------------------------------------
//  symlink edite
// -------------------------------------------------------------------------------
- (void)didEndSymlinkSheet:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [lyncCreationSheet orderOut:self];
    [self readPreferenceFile];
    [tableViewLink reloadData];
}
@end
