//
//  PListManager.h
//  OpenAFS
//
//  Created by Claudio Bisegni LNF - INFN on 25/04/08.
//  Copyright 2008 Infn. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#define DELETE_IN_10_4	@"authinternal"
#define DELETE_IN_10_5	@"builtin:authenticate,privileged"
#define ADD_IN_PLIST	@"builtin:krb5authnoverify,privileged"

#define AUTH_FILE_DIR	@"/etc"
#define AUTH_FILE		@"/etc/authorization"
#define AUTH_FILE_BK	@"/etc/authorization_bk"
#define TMP_FILE		@"/tmp/authorization"

#define BACKGROUNDER_P_FILE						@"it.infn.lnf.network.AFSBackgrounder"
#define HOME_LAUNCHD_AGENT_FOLDER				@"~/Library/LaunchAgents"
#define BACKGROUNDER_LAUNCHD_CONTROL_FILE		@"~/Library/LaunchAgents/it.infn.lnf.network.AFSBackgrounder.plist"
#define BACKGROUNDER_LAUNCHD_TMP_CONTROL_FILE	@"/tmp/it.infn.lnf.network.AFSBackgrounder.plist"

#define LAUNCHD_DAEMON_FOLDER			@"/Library/LaunchDaemons"
#define AFS_STARTUP_TMP_CONTROL_FILE	@"/tmp/it.infn.lnf.network.afsstartup.plist"
#define AFS_STARTUP_CONTROL_FILE		@"/Library/LaunchDaemons/it.infn.lnf.network.afsstartup.plist"

/*!
    @class		 PListManager
    @abstract    Utility class for modify the plist used by sistem for make login
    @discussion  <#(comprehensive description)#>
*/

@interface PListManager : NSObject {

}
/*!
 @method     krb5TiketAtLoginTime
 @abstract   Enable or disable the system to get kerberos ticket at login time
 @discussion <#(comprehensive description)#>
 */
+(void) krb5TiketAtLoginTime:(BOOL)enable helper:(NSString *)helper;

/*!
 @method     checkKrb5AtLoginTimeLaunchdEnable
 @abstract   check if the system is configured to ket krb5 ticket at login
 @discussion <#(comprehensive description)#>
 */
+(BOOL) checkKrb5AtLoginTimeLaunchdEnable;

/*!
 @method     installLaunchdFile
 @abstract   Install the afs agent launchd config file
 @discussion <#(comprehensive description)#>
 */
+(void) installBackgrounderLaunchdFile:(BOOL)install resourcePath:(NSString*) rsrcPath;
/*!
 @method     checkAklogAtLoginTimeLaunchdEnable
 @abstract   check if the user has installed  or enabled the afs agent
 @discussion <#(comprehensive description)#>
 */
+(BOOL) checkLoginTimeLaunchdBackgrounder;

/*!
 @method     installAfsStartupLaunchdFile
 @abstract   Install the afs daemon launchd config file for startup afs at boot
 @discussion <#(comprehensive description)#>
 */
+(void) manageAfsStartupLaunchdFile:(BOOL)enable afsStartupScript:(NSString*)afsStartupScript afsBasePath:(NSString*)afsBasePath  afsdPath:(NSString*)afsdPath;

/*!
 @method     launchctlCommand
 @abstract   exec the launchctl command on a particular plist job
 @discussion <#(comprehensive description)#>
*/ 
+(void) launchctlCommand:(BOOL)enable userDomain:(BOOL)userDomain option:(NSArray*)option plistName:(NSString*)plistName;
/*!
 @method     launchctlCommand
 @abstract   exec the launchctl command on a particular plist job
 @discussion <#(comprehensive description)#>
 */
+(void) launchctlStringCommandAuth:(NSString*)operation option:(NSArray*)option plistName:(NSString*)plistName helper:(NSString *)helper withAuthRef:(AuthorizationRef)authRef;
/*!
 @method     launchdJobState
 @abstract   check is a job has been submitted to launchd
 @discussion <#(comprehensive description)#>
 */ 
+(BOOL) launchdJobState:(NSString*)jobName;
@end
