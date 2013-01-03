//
//  AFSPropertyManager.h
//  AFSCommander
//
//  Created by Claudio Bisegni on 21/05/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "DBCellElement.h"
#import "FileUtil.h"


#define AFS_DAEMON_STARTUPSCRIPT	"/Library/OpenAFS/Tools/root.client/usr/vice/etc/afs.rc"
#define AFS_DAEMON_PATH				"/Library/LaunchDaemons/org.openafs.filesystems.afs.plist"
#define AFS_FS_MOUNT                "AFS"
#define AFS_LAUNCHCTL_GREP_STR		"org.openafs.filesystems.afs"
/*!
    @class		AFSPropertyManager
    @abstract   AFS Manage Class
    @discussion This class manage the openafs param for celldbserv, cache param, group creation, get and release token
*/

@interface AFSPropertyManager : NSObject {
	NSString *installationPath;
	NSString *cellName;
	NSMutableArray *cellList;
	NSArray *userDefaultCellArray;
	NSString *afsRootMountPoint;
	int statCacheEntry;
	int dCacheDim;
	int cacheDimension;
	int daemonNumber;
	int nVolEntry;
	bool dynRoot;
	bool afsDB;
	bool verbose;
	
	//-------------------
	FileUtil *futil;
	BOOL useAfsdConfVersion;
}

/*!
    @method     
    @abstract   (brief description)
    @discussion (comprehensive description)
*/
-(id) init;

/*!
@function	initWithAfsPath
 @abstract   (description)
 @discussion (description)
 @param      path Path di installazione afs
 @result     Istanza della classe di gestione delle propieta' afs
 */
- (id)initWithAfsPath:(NSString*)path;

/*!
    @method     
    @abstract   (brief description)
    @discussion (comprehensive description)
*/
-(void) dealloc;
/*!
 @method     
 @abstract   (brief description)
 @discussion (comprehensive description)
 */
-(NSMutableArray*) getCellList;


/*!
 @method     
 @abstract   (brief description)
 @discussion (comprehensive description)
 */
-(NSArray*) getAllCellsName;
/*!
 @method     
 @abstract   (brief description)
 @discussion (comprehensive description)
 */
-(NSArray*) getUserDefaultForTokenCells;
/*!
 @method     
 @abstract   (brief description)
 @discussion (comprehensive description)
 */
-(NSArray*) getDefaultForTokenCellsName;
/*!
 @method     
 @abstract   (brief description)
 @discussion (comprehensive description)
 */
-(NSString*) getDefaultCellName;

/*!
 @method     setDefaultCellByName
 @abstract   set the cell named "name" to be se user default
 @discussion first clean the last one selected as default and then  set the cell named "name" as default user cell
 */
-(void) setDefaultCellByName:(NSString*)name;

/*!
 @method     
 @abstract   (brief description)
 @discussion (comprehensive description)
 */- (void) setCellName:(NSString*)cell;

/*!
    @method     setPath
    @abstract   Imposta Path
    @discussion Imposta il path dove e' installato afs, in modo da leggere e scrivere le configurazioni
*/
-(void) setPath:(NSString*)path;

/*!
    @method     path
    @abstract   Return the afs base
    @discussion Return the Afs base installation path
*/
-(NSString*) path;

/*!
 @function	 statCacheEntry
 @abstract   get the afs number of state cache entry
 @discussion 
 @result     Number of stat cache entry
 */
-(int) statCacheEntry;


/*!
 @function	 setStatCacheEntry
 @abstract   Set the afs number of state cache entry
 @discussion 
 @result     Number of stat cache entry
 */
-(void) setStatCacheEntry:(int)statEntry;

/*!
@function	dCacheDim
@abstract   return the dCacheDim param value for cache manager
@result     dCacheDim value
*/
-(int) dCacheDim;

/*!
 @function	 setDCacheDim
 @abstract   set the setDCacheDim value for cache manager param
 @discussion <#(description)#>
 @param      dcacheDim cache param value
 */
-(void) setDCacheDim:(int)dcacheDim;

/*!
 @function	 cacheDimension
 @abstract   return the max size of the cache 
 @result     Cache  dimension
 */
-(int) cacheDimension;

/*!
 @function	setCacheDimension
 @abstract   Set the max cache dimension
 @discussion 
 @param      cacheDim MAx Chace dimension
 */
-(void) setCacheDimension:(int)cacheDim;

/*!
 @function	 daemonNumber
 @abstract   Return the number of daemon for the cache manager
 @result     Number of daemon
 */
-(int) daemonNumber;

/*!
 @function	 setDaemonNumber
 @abstract   Set the daemon numbero for the cache manager
 @param      dNumber number of daemon
 */
-(void) setDaemonNumber:(int)dNumber;

/*!
 @function	 afsRootMountPoint
 @abstract   Return the path where afs root volume will be mounted
 @result     AFS mount point
 */
-(NSString*) afsRootMountPoint;

/*!
 @function	 setAfsRootMountPoint
 @abstract   Set the AFS mount point
 @param      mountPoint AFS mount point
 */
-(void) setAfsRootMountPoint:(NSString*)mountPoint;

/*!
 @function	 nVolEntry
 @abstract   Return the nVolEntry for cache manager
 @result     value for nVolEntry cache parameter
 */
-(int) nVolEntry;

/*!
 @function	 setNVolEntry
 @abstract   Set the nVolEntry parameter for cache manager
 @param      entry value for nVolEntry cache manager parameter
 */
-(void) setNVolEntry:(int)entry;

/*!
 @function	 dynRoot
 @abstract   Return the DynRoot parametr for cache manager
 @result     dynRoot parameter value
 */
-(bool) dynRoot;

/*!
 @function	 setDynRoot
 @abstract   Set the DynRoot flag value for cache manager
 @param      flag dynRoot state (enable/disable)
 */
-(void) setDynRoot:(bool)flag;
/*!
 @function	 afsDB
 @abstract   Get the afsdb flag value for cache manager
 @result     Return the value of the flag
 */
-(bool) afsDB;

/*!
 @function	 setAfsDB
 @abstract   Set the flag value for afsdb cache manager
 @param      flag AfsDB state (enable/disable)
 */
-(void) setAfsDB:(bool)flag;
/*!
 @function	 verbose
 */
-(bool) verbose;

/*!
 @function	 setVerbose
 */
-(void) setVerbose:(bool)flag;
/*!
 @function	 readCacheInfo
 @abstract   Read the cache info
 @discussion The cache info is read from the file pointed by filePath param
 @param      filePath file location for the CacheInfo
 */
-(int) readCacheInfo:(NSString*)filePath;

/*!
 @function	 writeCacheInfo
 @abstract   Write the cache info down the file
 @param      filePath where to write the CacheInfo
 @result     return the execution error
 */
-(int) writeCacheInfo:(NSString*)filePath;

/*!
 @function	 readAfsdOption
 @abstract   Read the afs option
 @discussion Read the afs option checking firt the afs version so it can read the old afsd.option or afs.conf file. If any error accour an NSException wil be trown
 @param      filePath file where the parameter are store for default afsd.option or afs.conf
 */
-(void) readAfsdOption:(NSString*)filePath;

/*!
 @function	 readOldAfsdOption
 @abstract   Read the afsd.option file
 @discussion Read the old afsd.option style afsd option file. If any error accour an NSException wil be trown22
 @param      filePath file path to afsd.option like file
 @result     <#(description)#>
 */
-(int) readOldAfsdOption:(NSString*)filePath;
/*!
 @function	 readAFSDParamLineContent
 @abstract   Try to decode one line of afsd.option or afs.conf
 @param      paramLine one line of file afsd.option(the only one that is present) os afs.conf
 */
-(int) readAFSDParamLineContent:(NSString*)paramLine;
/*!
 @function	 readNewAfsdOption
 @abstract   Read the new afs.conf file format
 @discussion Scann every line f the afs.conf file ad for each one call the readAFSDParamLineContent with it's content
 @param      filePath path of the new file with afs.conf file format
 */
-(int) readNewAfsdOption:(NSString*)filePath;
/*!
 @function	 writeAfsdOption
 @abstract   <#(description)#>
 @discussion <#(description)#>
 @param      <#(name) (description)#>
 @result     <#(description)#>
 */
-(int) writeAfsdOption:(NSString*)filePath;
/*!
 @function	 writeOldAfsdOption
 @abstract   Write the cache manager parameter to file
 @discussion First chech the version of afs installed then choice to save old o new file version(afsd.option or afs.conf)
 @param      filePath file path for file to write into
 @result     <#(description)#>
 */
-(int) writeOldAfsdOption:(NSString*)filePath;
/*!
 @function	 writeNewAfsdOption
 @abstract   Write the cache parameter to a file with the new format
 @param      filePath file path where write into
 */
-(int) writeNewAfsdOption:(NSString*)filePath;
/*!
 @function	 getAfsVersion
 @abstract   Return the string representing the afs version
 @result     The Enteir string returned from the call of fs -version
 */
-(NSString*) getAfsVersion;
/*!
 @function	 getAfsMajorVersionVersion
 @abstract   Return the major version of afs Major.x.x
 @result     
 */
-(int) getAfsMajorVersionVersion;
/*!
 @function	 getAfsMinorVersionVersion
 @abstract   Return the major version of afs x.Minor.x
 @result     
 */
-(int) getAfsMinorVersionVersion;

/*!
 @function	 getAfsPatchVersionVersion
 @abstract   Return the major version of afs x.x.Patch
 @result     
 */
-(int) getAfsPatchVersionVersion;

/*!
 @method     clearConfiguration
 @abstract   Clear all structure or array that contain value for afs client setting(cells cache param etch)
 @discussion 
	 */
-(void) clearConfiguration;

/*!
 @function	 exceptionOnInvalidPath
 @abstract   Check the validity of afs path
 @discussion If the installationPath variable don't point to a valid path, will be fired an NSException
 */
-(void) exceptionOnInvalidPath;

/*!
    @method     loadConfiguration
    @abstract   load the all afs configuration (ThisCell & CellServDB & cache parameter)
*/
-(void) loadConfiguration;

/*!
    @method     readCellInfo
    @abstract   Read the Main Cell Name
*/
-(void) readCellInfo:(NSString*) configFile;
/*!
    @method     readCellDB
    @abstract   Read the CellServDB File
    @discussion Read the file of all cellservbd ad make the NSArray containing a DBCellElement for aech cell found
*/
-(void) readCellDB:(NSString*) configFile;
/*!
    @function	readTheseCell
    @abstract   Read the "These Cell"
    @discussion Read the "These Cell" file that contains all the cell from which the user want to get the token
    @param      configFile TheseCell file path
    @result     NSAray with the cells name
*/
-(NSArray*) readTheseCell:(NSString*) configFile;
/*!
    @method     shutdown
    @abstract   Stop The AFS
*/
-(void) shutdown;
-(void) startup;
-(void) scanIpForCell:(DBCellElement*) cellElement allIP:(NSString*)allIP;
-(void) backupConfigurationFiles;
-(void) backupFile:(NSString*)localAfsFilePath;
-(void) saveConfigurationFiles:(BOOL) makeBackup;
-(void) saveCacheConfigurationFiles:(BOOL) makeBackup;
-(void) installConfigurationFile:(NSString*)srcConfFile destPath:(NSString*) destPath;
-(NSArray*) getTokenList;
-(BOOL) checkAfsStatus;
-(BOOL) checkAfsStatusForStartup;
-(void) klog:(NSString*)uName uPwd:(NSString*)uPwd  cell:(NSString*)theCell;
-(void) aklog:(NSString*)theCell noKerberosCall:(BOOL)krb5CallEnable;
-(void) getTokens:(BOOL)klogAklogFlag usr:(NSString*)usr pwd:(NSString*)pwd;
-(void) unlog:(NSString*)cell;
-(NSString*) makeChaceParamString;
-(BOOL) useAfsdConfConfigFile;
@end
