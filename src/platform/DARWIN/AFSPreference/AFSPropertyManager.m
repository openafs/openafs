//
//  AFSPropertyManager.m
//  AFSCommander
//
//  Created by Claudio Bisegni on 21/05/07.
//  Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import "Krb5Util.h"
#import "AFSPropertyManager.h"
#import "TaskUtil.h"

#define kKerberosAuthError					NSLocalizedStringFromTableInBundle(@"KerberosAuthError",nil,[NSBundle bundleForClass:[self class]],@"KerberosAuthError")
#define kPathNotEmpty						NSLocalizedStringFromTableInBundle(@"PathNotEmpty",nil,[NSBundle bundleForClass:[self class]],@"PathNotEmpty")
#define kPathDontContainAfsInstallation		NSLocalizedStringFromTableInBundle(@"PathDontContainAfsInstallation",nil,[NSBundle bundleForClass:[self class]],@"PathDontContainAfsInstallation")
#define kThisCellFOError					NSLocalizedStringFromTableInBundle(@"ThisCellFOError",nil,[NSBundle bundleForClass:[self class]],@"ThisCellFOError")
#define kConfFileNotExits					NSLocalizedStringFromTableInBundle(@"ConfFileNotExits",nil,[NSBundle bundleForClass:[self class]],@"ConfFileNotExits")
#define kUserNotAuth						NSLocalizedStringFromTableInBundle(@"UserNotAuth",nil,[NSBundle bundleForClass:[self class]],@"UserNotAuth")
#define kBadAfsPath							NSLocalizedStringFromTableInBundle(@"BadAfsPath",nil,[NSBundle bundleForClass:[self class]],@"BadAfsPath")

#define AFS_TOOLS_DIRECTORY /Library/Openafs/
#define AFS_TOOLS_BIN(x) "/Library/Openafs/bin/"#x
#define AFS_CACHE_PATH "/var/db/openafs/cache"

#define AFSD_TMP_OLD_PREFERENCE_FILE @"/tmp/afsd.options"  
#define AFSD_TMP_NEW_PREFERENCE_FILE @"/tmp/afs.conf"
#define AFSD_OLD_PREFERENCE_FILE @"/etc/config/afsd.options" 
#define AFSD_NEW_PREFERENCE_FILE @"/etc/config/afs.conf"


//-afsdb -stat 2000 -dcache 800 -daemons 3 -volumes 70 -dynroot -fakestat-all
#define AFSD_OPTION_AFSDB_KEY	"-afsdb"
#define AFSD_OPTION_VERBOSE_KEY	"-verbose"
#define AFSD_OPTION_STAT_KEY	"-stat"
#define AFSD_OPTION_DCACHE_KEY	"-dcache"
#define AFSD_OPTION_DAEMONS_KEY	"-daemons"
#define AFSD_OPTION_VOLUMES_KEY	"-volumes"
#define AFSD_OPTION_DYNROOT_KEY	"-dynroot"
#define AFSD_OPTION_FKESTAT_ALL "-fakestat-all"

#define AFSD_CONF_VERBOSE		@"VERBOSE"
#define AFSD_CONF_OPTION		@"OPTIONS"
#define AFSD_CONF_SYSNAME		@"AFS_SYSNAME"
#define AFSD_CONF_POST_INI		@"AFS_POST_INIT"
#define AFSD_CONF_PRE_SHUTDOWN	@"AFS_PRE_SHUTDOWN"

#define AFS_CONF_CHANGE_FROM_Mj_VERSION 1
#define AFS_CONF_CHANGE_FROM_Mi_VERSION 4
#define AFS_DEV_CONF_CHANGE_FROM_Mi_VERSION 5
#define AFS_CONF_CHANGE_FROM_Pa_VERSION 7
#define AFS_DEV_CONF_CHANGE_FROM_Pa_VERSION 31

@implementation AFSPropertyManager


// -------------------------------------------------------------------------------
//  init:
// -------------------------------------------------------------------------------
-(id) init
{
	[super init];
	cellList = [[NSMutableArray alloc] init];
	cellName = nil;
	userDefaultCellArray = nil;
	useAfsdConfVersion = NO;
	dynRoot = NO;
	afsDB = NO;
	verbose = NO;
	futil = [[FileUtil alloc] init];
	return self;
}

// -------------------------------------------------------------------------------
//  initWithAfsPath:
// -------------------------------------------------------------------------------
- (id)initWithAfsPath:(NSString*)path
{
	// avvio la creazione standard dell'oggetto
	if([self init]){
		// imposto il path
		[self setPath:path];
	}
	return self;
}

// -------------------------------------------------------------------------------
//  dealloc:
// -------------------------------------------------------------------------------
-(void) dealloc
{
	if(installationPath){ [installationPath release];}
	if(cellList) {[cellList removeAllObjects];[cellList release];}
	if(cellName) {[cellName release];}
	if(futil) {
		[futil release];
		futil = nil; 
	}
	[super dealloc];
}

// -------------------------------------------------------------------------------
//  setPath:
// -------------------------------------------------------------------------------
- (void) setPath:(NSString*)path {
	if(installationPath)[installationPath release];
	
	if(path)
		installationPath = [path retain];
}

// -------------------------------------------------------------------------------
//  path:
// -------------------------------------------------------------------------------
- (NSString*) path{
	return installationPath;
}

// -------------------------------------------------------------------------------
//  statCacheEntry:
// -------------------------------------------------------------------------------
-(int) statCacheEntry
{
	return statCacheEntry;
}

// -------------------------------------------------------------------------------
//  setStatCacheEntry:
// -------------------------------------------------------------------------------
-(void) setStatCacheEntry:(int)statEntry
{
	statCacheEntry = statEntry;
}


// -------------------------------------------------------------------------------
//  dCacheDim:
// -------------------------------------------------------------------------------
-(int) dCacheDim
{
	return dCacheDim;
}

// -------------------------------------------------------------------------------
//  setDCacheDim:
// -------------------------------------------------------------------------------
-(void) setDCacheDim:(int)dcDim
{
	dCacheDim = dcDim;
}

// -------------------------------------------------------------------------------
//  cacheDimension:
// -------------------------------------------------------------------------------
-(int) cacheDimension
{
	return cacheDimension;
}

// -------------------------------------------------------------------------------
//  setCacheDimension:
// -------------------------------------------------------------------------------
-(void) setCacheDimension:(int)cacheDim
{
	cacheDimension = cacheDim;
}

// -------------------------------------------------------------------------------
//  daemonNumber:
// -------------------------------------------------------------------------------
-(int) daemonNumber
{
	return daemonNumber;
}

// -------------------------------------------------------------------------------
//  setDaemonNumber:
// -------------------------------------------------------------------------------
-(void) setDaemonNumber:(int)dNumber
{
	daemonNumber = dNumber;
}

// -------------------------------------------------------------------------------
//  afsRootMountPoint:
// -------------------------------------------------------------------------------
-(NSString*) afsRootMountPoint
{
	return afsRootMountPoint;
}

// -------------------------------------------------------------------------------
//  setAfsRootMountPoint:
// -------------------------------------------------------------------------------
-(void) setAfsRootMountPoint:(NSString*)mountPoint
{
	if(afsRootMountPoint)[afsRootMountPoint release];
	
	if(mountPoint)
		afsRootMountPoint = [mountPoint retain];
}

// -------------------------------------------------------------------------------
//  nVolEntry:
// -------------------------------------------------------------------------------
-(int) nVolEntry
{
	return nVolEntry;
}

// -------------------------------------------------------------------------------
//  setNVolEntry:
// -------------------------------------------------------------------------------
-(void) setNVolEntry:(int)entry
{
	nVolEntry = entry;
}

// -------------------------------------------------------------------------------
//  dynRoot:
// -------------------------------------------------------------------------------
-(bool) dynRoot
{
	return dynRoot;
}

// -------------------------------------------------------------------------------
//  setDynRoot:
// -------------------------------------------------------------------------------
-(void) setDynRoot:(bool)flag
{
	dynRoot = flag;
}

// -------------------------------------------------------------------------------
//  dynRoot:
// -------------------------------------------------------------------------------
-(bool) afsDB
{
	return afsDB;
}

// -------------------------------------------------------------------------------
//  setDynRoot:
// -------------------------------------------------------------------------------
-(void) setAfsDB:(bool)flag
{
	afsDB = flag;
}

// -------------------------------------------------------------------------------
//  setDynRoot:
// -------------------------------------------------------------------------------
-(bool) verbose
{
	return verbose;
}

// -------------------------------------------------------------------------------
//  setDynRoot:
// -------------------------------------------------------------------------------
-(void) setVerbose:(bool)flag
{
	verbose = flag;
}

// -------------------------------------------------------------------------------
//  useAfsdConfVersion:
// -------------------------------------------------------------------------------
-(BOOL) useAfsdConfConfigFile
{
	return useAfsdConfVersion;
}

// -------------------------------------------------------------------------------
//  exceptionOnInvalidPath:
// -------------------------------------------------------------------------------
- (void) exceptionOnInvalidPath
{
	if(!installationPath || ([installationPath length] == 0)) {
		@throw [NSException exceptionWithName:@"AFSPropertyManager:exceptionOnInvalidPath" 
									   reason:kPathNotEmpty 
									 userInfo:nil];			
	}
	if(![[NSFileManager defaultManager] fileExistsAtPath:installationPath]){
		@throw [NSException exceptionWithName:@"AFSPropertyManager:exceptionOnInvalidPath" 
									   reason:kPathDontContainAfsInstallation
									 userInfo:nil];			
	}
	
}

// -------------------------------------------------------------------------------
//  loadConfiguration:
// -------------------------------------------------------------------------------
- (void) loadConfiguration {
	int mjVersion = [self getAfsMajorVersionVersion];
	int miVersion = [self getAfsMinorVersionVersion];
	int paVersion = [self getAfsPatchVersionVersion];
	
	NSMutableString *filePath = [[NSMutableString alloc] initWithCapacity:256];
	@try{
		
		[self exceptionOnInvalidPath];
		[self clearConfiguration];
		
		//chech the afs version for chioce wich afsd conf file usage
		useAfsdConfVersion = mjVersion >= 1 && miVersion==4 && paVersion>=7;
		useAfsdConfVersion = useAfsdConfVersion || (mjVersion >= 1 && miVersion==5 && paVersion>=31);
		useAfsdConfVersion = useAfsdConfVersion || (mjVersion >= 1 && miVersion>=6);
		
		// read thiscell config file
		[filePath setString:installationPath];
		[filePath appendString: @"/etc/ThisCell"];

		[self readCellInfo:filePath];
		if(!cellName){
			@throw [NSException exceptionWithName:@"readCellInfo" 
									reason:kThisCellFOError 
								  userInfo:nil];
		}
		//read TheseCell file
		[filePath setString: installationPath];
		[filePath appendString: @"/etc/TheseCells"];
		userDefaultCellArray = [self readTheseCell:filePath];
		
		//read cell serv db
		[filePath setString: installationPath];
		[filePath appendString: @"/etc/CellServDB"];
		[self readCellDB:filePath];
		
		//Read cacheinfo
		[filePath setString: installationPath];
		[filePath appendString: @"/etc/cacheinfo"];
		[self readCacheInfo:filePath];
		
		//Read config/afsd.options
		[filePath setString: installationPath];
		[filePath appendString: useAfsdConfVersion?AFSD_NEW_PREFERENCE_FILE:AFSD_OLD_PREFERENCE_FILE];
		[self readAfsdOption:filePath];
	} @catch(NSException * e){
		@throw e;
	} @finally {
		[filePath release];
	}
}

// -------------------------------------------------------------------------------
//  readCacheInfo:
//  file template "/afs:/var/db/openafs/cache:30000"
// -------------------------------------------------------------------------------
-(int) readCacheInfo:(NSString*)filePath
{
	int cicle = 0;
	NSString *tmpString = nil;
	NSCharacterSet *fullStopCS = [NSCharacterSet characterSetWithCharactersInString:@":"];
	NSMutableCharacterSet *chunkStartCS = [[NSMutableCharacterSet alloc] init];
	[chunkStartCS formUnionWithCharacterSet:[NSCharacterSet alphanumericCharacterSet]];
	[chunkStartCS formUnionWithCharacterSet:[NSMutableCharacterSet characterSetWithCharactersInString:@"/"]];
	
	NSCharacterSet *returnCS = [NSCharacterSet characterSetWithCharactersInString:@"\n"];
	NSFileHandle *fileH = [NSFileHandle fileHandleForReadingAtPath:filePath];
	if (!fileH) return 0;
	NSData *fileHData = [fileH readDataToEndOfFile];
	NSString *cacheInfoStrData = [[NSString alloc] initWithData:fileHData
													   encoding:NSASCIIStringEncoding];
	NSScanner *cacheInfoS = [NSScanner  scannerWithString:cacheInfoStrData];
	
	@try{
		do {
			cicle++;
			switch(cicle){
				case 1:
					// afs mount path
					[cacheInfoS scanUpToCharactersFromSet:fullStopCS intoString:&tmpString];
					[self setAfsRootMountPoint:tmpString];
					[cacheInfoS scanUpToCharactersFromSet:chunkStartCS intoString:&tmpString];
					break;
					
				case 2:
					//cache path default '/var/db/openafs/cache'
					[cacheInfoS scanUpToCharactersFromSet:fullStopCS intoString:&tmpString];
					[cacheInfoS scanUpToCharactersFromSet:chunkStartCS intoString:&tmpString];
					break;
				
				case 3:
					// cache dimension
					[cacheInfoS scanUpToCharactersFromSet:returnCS intoString:&tmpString];
					[self setCacheDimension:[tmpString intValue]];
					break;
			}
		}while(cicle < 3 && ![cacheInfoS isAtEnd]);
	}@catch(NSException *e){
		@throw e;
	}@finally{
		//if(cacheInfoStrData) [cacheInfoStrData release];
		if(chunkStartCS) [chunkStartCS release];
	}
	return noErr;
}


// -------------------------------------------------------------------------------
//  writeCacheInfo:
// -------------------------------------------------------------------------------
-(int) writeCacheInfo:(NSString*)filePath
{
	NSNumber *tmpNum = nil;
	NSMutableString *cacheInfoContent = [[NSMutableString alloc] init];
	if(!cacheInfoContent) return -1;
	
	//Afs root mount point
	if([[self afsRootMountPoint] rangeOfString:@"/"].location == NSNotFound || [[self afsRootMountPoint] length] == 0) 
		@throw [NSException exceptionWithName:@"AFSPropertyManager:writeCacheInfo"  
									   reason:@"Bad afs path" 
									 userInfo:nil];
	[cacheInfoContent appendString:[self afsRootMountPoint]];
	
	//standard cache path
	
	[cacheInfoContent appendString:@":"]; [cacheInfoContent appendString:@AFS_CACHE_PATH]; [cacheInfoContent appendString:@":"];
	
	//cache dimension
	tmpNum = [NSNumber numberWithInt:[self cacheDimension]];
	if([tmpNum intValue] == 0)
		@throw [NSException exceptionWithName:@"AFSPropertyManager:writeCacheInfo"  
									   reason:@"Bad cache dimension" 
									 userInfo:nil];
	[cacheInfoContent appendString:[tmpNum stringValue]];
	
	[cacheInfoContent writeToFile: [filePath stringByExpandingTildeInPath]
					   atomically: YES 
						 encoding: NSASCIIStringEncoding 
							error: nil];
	
	[cacheInfoContent release];
	return noErr;
}

// -------------------------------------------------------------------------------
//  readCacheInfo:
//  file template "/afs:/var/db/openafs/cache:30000"
// -------------------------------------------------------------------------------
-(void) readAfsdOption:(NSString*)filePath
{
	@try{
		if(useAfsdConfVersion) {
		    if ([self readNewAfsdOption:filePath] != noErr)
			[self readOldAfsdOption:filePath];
		} else {
		    if ([self readOldAfsdOption:filePath] != noErr)
			[self readNewAfsdOption:filePath];
		}
	
	}@catch(NSException *e){
		@throw e;
	}@finally{
		
	}
	
}

// -------------------------------------------------------------------------------
//  readOldAfsdOption:
// -------------------------------------------------------------------------------
-(int) readOldAfsdOption:(NSString*)filePath
{
	if(!filePath) return 0;
	return [self readAFSDParamLineContent:[[NSString stringWithContentsOfFile:filePath 
															  encoding:NSUTF8StringEncoding 
																 error:nil] stringByStandardizingPath]];
}

// -------------------------------------------------------------------------------
//  readAFSDParamLineContent:
// -------------------------------------------------------------------------------
-(int) readAFSDParamLineContent:(NSString*) paramLine{
	if (!paramLine) return 0;

	NSString *tmpString = nil;
	NSCharacterSet *space = [NSCharacterSet characterSetWithCharactersInString:@" "];
	NSScanner *afsdOptionS = [NSScanner  scannerWithString:paramLine];
	
	do{
		[afsdOptionS scanUpToCharactersFromSet:space intoString:&tmpString];
		if(!tmpString) continue;
		//check parameter type
		if([tmpString isEqualToString:@AFSD_OPTION_DAEMONS_KEY])
		{
			// get number of daemon
			[afsdOptionS scanUpToCharactersFromSet:space intoString:&tmpString];
			[self setDaemonNumber:[tmpString intValue]];
		} else				
			//check parameter type
			if([tmpString isEqualToString:@AFSD_OPTION_DCACHE_KEY])
			{
				//get dcache dimension
				[afsdOptionS scanUpToCharactersFromSet:space intoString:&tmpString];
				[self setDCacheDim:[tmpString intValue]];
			} else 
				//check parameter type
				if([tmpString isEqualToString:@AFSD_OPTION_DYNROOT_KEY])
				{
					[self setDynRoot:true];
				} else 
					if([tmpString isEqualToString:@AFSD_OPTION_VERBOSE_KEY])
					{
						[self setVerbose:true];
					} else if([tmpString isEqualToString:@AFSD_OPTION_AFSDB_KEY])
					{
						[self setAfsDB:true];
					} else
						//check parameter type
						if([tmpString isEqualToString:@AFSD_OPTION_STAT_KEY])
						{
							// get fstat entry num
							[afsdOptionS scanUpToCharactersFromSet:space intoString:&tmpString];
							[self setStatCacheEntry:[tmpString intValue]];
						} else
							
							//check parameter type
							if([tmpString isEqualToString:@AFSD_OPTION_VOLUMES_KEY])
							{
								// get fstat entry num
								[afsdOptionS scanUpToCharactersFromSet:space intoString:&tmpString];
								[self setNVolEntry:[tmpString intValue]];
							}
		
		
		
	}while(![afsdOptionS isAtEnd]);
	return noErr;
}

// -------------------------------------------------------------------------------
//  readNewAfsdOption:
// -------------------------------------------------------------------------------
-(int) readNewAfsdOption:(NSString*)filePath
{
	if(!filePath) return 0;
	NSString *currentLines = 0;
	NSString *paramValue = 0;
	NSScanner *lineScanner = 0;
	
	//Get file content
	NSString *newAFSDConfContent = [NSString stringWithContentsOfFile:filePath 
															 encoding:NSUTF8StringEncoding 
																error:nil];
	if (!newAFSDConfContent) return 0;
	
	//get lines in array
	NSArray *confLines = [newAFSDConfContent componentsSeparatedByString:@"\n"];
	
	//Get the lines enumerator
	NSEnumerator *lineEnumerator = [confLines objectEnumerator];
	
	//scann all lines
	while(currentLines = [lineEnumerator nextObject]) {
		if([currentLines rangeOfString:@"#"].location != NSNotFound) continue;
			
		if([currentLines rangeOfString:AFSD_CONF_OPTION].location != NSNotFound) {
			lineScanner = [NSScanner scannerWithString:currentLines];
			if(!lineScanner) continue;
			
			//scann the line
			
			[lineScanner scanUpToString:@"\"" intoString:&paramValue];[lineScanner scanUpToString:@"-" intoString:&paramValue];
			[lineScanner scanUpToString:@"\"" intoString:&paramValue];
			
			// read the asfd option param line
			[self readAFSDParamLineContent:paramValue];
		} else if([currentLines rangeOfString:AFSD_CONF_SYSNAME].location != NSNotFound) {
			
		} else if([currentLines rangeOfString:AFSD_CONF_POST_INI].location != NSNotFound) {
			
		} else if([currentLines rangeOfString:AFSD_CONF_PRE_SHUTDOWN].location != NSNotFound) {
			
		}
	}
	return noErr;
}


// -------------------------------------------------------------------------------
//  writeCacheInfo:
// -------------------------------------------------------------------------------
-(int) writeAfsdOption:(NSString*)filePath
{
	int result = 0;
	@try{
		if(useAfsdConfVersion) {
			result = [self writeNewAfsdOption:filePath];			
		} else {
			result = [self writeOldAfsdOption:filePath];
		}
		
	}@catch(NSException *e){
		@throw e;
	}@finally{
		
	}
	return result;
}

// -------------------------------------------------------------------------------
//  writeOldAfsdOption:
// -------------------------------------------------------------------------------
-(int) writeOldAfsdOption:(NSString*)filePath;
{
	NSMutableString *oldConfFileContent = [[[NSMutableString alloc] init] autorelease];
	//add afsd param
	[oldConfFileContent appendString:[self makeChaceParamString]];
	
	//add cariage return at end of file
	[oldConfFileContent appendString:@"\n"];
	
	//write content on file
	[oldConfFileContent writeToFile: [filePath stringByExpandingTildeInPath]
						 atomically: YES 
						   encoding: NSASCIIStringEncoding 
							  error: nil];
	return noErr;
	
}

// -------------------------------------------------------------------------------
//  writeNewAfsdOption:
//  OPTIONS=
//  AFS_SYSNAME=
//  AFS_POST_INIT=afs_server_prefs
//  AFS_PRE_SHUTDOWN=
// -------------------------------------------------------------------------------
-(int) writeNewAfsdOption:(NSString*)filePath
{
	NSMutableString *newConfFileContent = [[[NSMutableString alloc] init] autorelease];
	
	//Verbose
	[newConfFileContent appendString:AFSD_CONF_VERBOSE]; [newConfFileContent appendString:@"="]; [newConfFileContent appendString:@"\n"];
	
	//AFSD Option
	[newConfFileContent appendString:AFSD_CONF_OPTION];[newConfFileContent appendString:@"=\""]; [newConfFileContent appendString:[self makeChaceParamString]]; [newConfFileContent appendString:@"\""]; [newConfFileContent appendString:@"\n"]; 
	
	//AFS_SYSNAME
	[newConfFileContent appendString:AFSD_CONF_SYSNAME];[newConfFileContent appendString:@"=\""];[newConfFileContent appendString:@"\""]; [newConfFileContent appendString:@"\n"];
	
	//AFS_POST_INIT
	[newConfFileContent appendString:AFSD_CONF_POST_INI];[newConfFileContent appendString:@"="]; [newConfFileContent appendString:@"\n"]; 
	
	//AFS_PRE_SHUTDOWN
	[newConfFileContent appendString:AFSD_CONF_PRE_SHUTDOWN];[newConfFileContent appendString:@"="]; [newConfFileContent appendString:@"\n"];
	
	//add content on file
	[newConfFileContent appendString:@"\n"];
	
	//Write to file
	[newConfFileContent writeToFile: [filePath stringByExpandingTildeInPath]
						 atomically: YES 
						   encoding: NSASCIIStringEncoding 
							  error: nil];
	return noErr;
}

// -------------------------------------------------------------------------------
//  getAfsVersion:
// -------------------------------------------------------------------------------
-(NSString*) getAfsVersion
{
	NSString *tmpString = nil;
	NSString *result = [TaskUtil executeTaskSearchingPath:@"fs" args:[NSArray arrayWithObjects:@"-version", nil]];
	if (!result) return nil;
	NSCharacterSet *endVersionCS = [NSCharacterSet characterSetWithCharactersInString:@"qwertyuiopasdfghjklzxcvbnmMNBVCXZLKJHGFDSAPOIUYTREWQ"];
	NSCharacterSet *spaceCS = [NSCharacterSet characterSetWithCharactersInString:@" "];
	NSScanner *versionS = [NSScanner  scannerWithString:result];
	//go to  start of version
	[versionS scanUpToCharactersFromSet:spaceCS intoString:&tmpString];
	
	//get the total version string
	[versionS scanUpToCharactersFromSet:endVersionCS intoString:&tmpString];

	return tmpString;
}

// -------------------------------------------------------------------------------
//  getAfsMajorVersionVersion:
// -------------------------------------------------------------------------------
-(int) getAfsMajorVersionVersion
{
	NSString *tmpString = nil;
	NSString *totalVersion = [self getAfsVersion];
	if (!totalVersion) return 0;
	NSCharacterSet *pointCS = [NSCharacterSet characterSetWithCharactersInString:@"."];
	NSScanner *versionMS = [NSScanner  scannerWithString:totalVersion];
	[versionMS scanUpToCharactersFromSet:pointCS intoString:&tmpString];
	return [tmpString intValue];
}

// -------------------------------------------------------------------------------
//  getAfsMinorVersionVersion:
// -------------------------------------------------------------------------------
-(int) getAfsMinorVersionVersion
{
	NSString *tmpString = nil;
	NSString *totalVersion = [self getAfsVersion];
	if (!totalVersion) return 0;
	NSCharacterSet *numCS = [NSCharacterSet characterSetWithCharactersInString:@"1234567890"];
	NSCharacterSet *pointCS = [NSCharacterSet characterSetWithCharactersInString:@"."];
	NSScanner *versionMS = [NSScanner  scannerWithString:totalVersion];
	[versionMS scanUpToCharactersFromSet:pointCS intoString:&tmpString];[versionMS scanUpToCharactersFromSet:numCS intoString:&tmpString];[versionMS scanUpToCharactersFromSet:pointCS intoString:&tmpString];
	return [tmpString intValue];
}

// -------------------------------------------------------------------------------
//  getAfsPatchVersionVersion:
// -------------------------------------------------------------------------------
-(int) getAfsPatchVersionVersion
{
	NSString *totalVersion = [self getAfsVersion];
	if (!totalVersion) return 0;
	NSCharacterSet *pointCS = [NSCharacterSet characterSetWithCharactersInString:@"."];
	int lastPointIndex = [totalVersion rangeOfCharacterFromSet:pointCS 
													   options:NSBackwardsSearch].location;
	int patchVersion = [[totalVersion substringFromIndex:lastPointIndex+1] intValue];
	return patchVersion;
}


// -------------------------------------------------------------------------------
//  clearConfiguration:
// -------------------------------------------------------------------------------
- (void) clearConfiguration{
	
	// clear list of cell
	[cellList removeAllObjects];
	
	userDefaultCellArray= nil;
	
	// remove the old cell name
	if(cellName) {
		[cellName release];
		cellName = nil;
	}
}

// -------------------------------------------------------------------------------
//  getCellList:
// -------------------------------------------------------------------------------
-(NSMutableArray*) getCellList
{
	return cellList;
}



// -------------------------------------------------------------------------------
//  getAllCellName:
// -------------------------------------------------------------------------------
-(NSArray*) getAllCellsName {
	NSMutableArray *allCelListName = [[[NSMutableArray alloc] init] autorelease];
	for(int idx = 0; idx < [cellList count]; idx++){
		DBCellElement *elemnt = [cellList objectAtIndex:idx];
		[allCelListName addObject:[elemnt getCellName]];
	}
	return allCelListName;
}

// -------------------------------------------------------------------------------
//  getCellList:
// -------------------------------------------------------------------------------
-(NSArray*) getUserDefaultForTokenCells {
	return userDefaultCellArray;
}

// -------------------------------------------------------------------------------
//  getDefaultCellName:
// -------------------------------------------------------------------------------
-(NSArray*) getDefaultForTokenCellsName {
	NSMutableArray *allCelListName = [[[NSMutableArray alloc] init] autorelease];
	for(int idx = 0; idx < [cellList count]; idx++){
		DBCellElement *elemnt = [cellList objectAtIndex:idx];
		if([elemnt userDefaultForToken]) [allCelListName addObject:[[elemnt getCellName] retain]];
	}
	return allCelListName;
}

// -------------------------------------------------------------------------------
//  getCellName:
// -------------------------------------------------------------------------------
-(NSString*) getDefaultCellName
{
	return cellName;
}

// -------------------------------------------------------------------------------
//  setDefaultCellByName:
// -------------------------------------------------------------------------------
-(void) setDefaultCellByName:(NSString*)name
{
	DBCellElement *elementCell = nil;
	BOOL cellFound = false;
	if(!name) return;
	
	for(int idx = 0; idx < [cellList count]; idx++) {
		// check every cell for delete as old user default cell or selected as neww cell
		elementCell = [cellList objectAtIndex:idx];
		cellFound = [name  compare:[elementCell getCellName]] == NSOrderedSame;
		[elementCell setUserDefaultCell:cellFound];
		if(cellFound) {
			[elementCell setUserDefaultForToken:YES];
			if(cellName)[cellName release];
			cellName = [name retain];
		 }
	}
	
}

// -------------------------------------------------------------------------------
//  setCellName:
// -------------------------------------------------------------------------------
- (void) setCellName:(NSString*)cell {
	[self setDefaultCellByName:cell];
}

// -------------------------------------------------------------------------------
//  readCellInfo:
// -------------------------------------------------------------------------------
-(void) readCellInfo:(NSString*) configFile {
	NSError *error = nil;
	NSString *tmpStr = nil;
	NSString * result = [NSString stringWithContentsOfFile:configFile
												  encoding:NSASCIIStringEncoding
													 error:&error];
	if(!result && error){
		if([error domain] )
		@throw [NSException exceptionWithName:@"readCellInfo" 
									   reason:kConfFileNotExits
									 userInfo:nil];
	} else if (!result)
	    return;
	NSScanner *scanner = [NSScanner scannerWithString:result];
	[scanner scanUpToString:@"\n" 
				 intoString:&tmpStr];
	
	// make a copy of self created string
	cellName = [tmpStr retain];
}

// -------------------------------------------------------------------------------
//  readCellDB:
// -------------------------------------------------------------------------------
-(void) readCellDB:(NSString*) configFile {	
	NSString *tmpString = nil;
	BOOL isInCellDefaultArray = NO; // the cell belong
	BOOL isDefaultCell = NO;
	DBCellElement *afsCellDBElement = nil;
	NSCharacterSet *returnCS = [NSCharacterSet characterSetWithCharactersInString:@"\n"];
	NSCharacterSet *spaceCS = [NSCharacterSet characterSetWithCharactersInString:@" \t"];
	
	NSFileHandle *fileH = [NSFileHandle fileHandleForReadingAtPath:configFile];
	if (!fileH) return;
	NSData *dbCellData = [fileH readDataToEndOfFile];
	NSString *strData = [[NSString alloc] initWithData:dbCellData
											  encoding:NSASCIIStringEncoding];
	NSScanner *cellDBScanner = [NSScanner  scannerWithString:strData];
	
	@try{
	[cellDBScanner scanUpToCharactersFromSet:[NSCharacterSet alphanumericCharacterSet] intoString:&tmpString];
	while([cellDBScanner isAtEnd] == NO) {
		// make new cell
		afsCellDBElement = [[DBCellElement alloc] init];
		
		// get the name of cell
		[cellDBScanner scanUpToCharactersFromSet:spaceCS intoString:&tmpString];
		[afsCellDBElement setCellName: tmpString];
		
		//check if this cells is one of user has selected to get token
		isInCellDefaultArray = [userDefaultCellArray containsObject:tmpString];
		//check if this cell is also the default cell
		isDefaultCell = [cellName compare:tmpString]==NSOrderedSame;
		
		[afsCellDBElement setUserDefaultForToken:isInCellDefaultArray||isDefaultCell];
		[afsCellDBElement setUserDefaultCell:isDefaultCell];
		
		
	
		
		
		// get the cell comment
		[cellDBScanner scanUpToCharactersFromSet:[NSCharacterSet alphanumericCharacterSet] intoString:nil];
		[cellDBScanner scanUpToCharactersFromSet:returnCS intoString:&tmpString];
		[afsCellDBElement setCellComment: tmpString];

		// get all ip
		[cellDBScanner scanUpToString:@">" intoString:&tmpString];
		// scann all ip in list
		[self scanIpForCell:afsCellDBElement allIP:tmpString];	
		
		// add cell to list
		[cellList addObject:afsCellDBElement];
		// release the object becasuse NSMutableArray make a retain on object
		[afsCellDBElement release];
		//repeat
		[cellDBScanner scanUpToCharactersFromSet:[NSCharacterSet alphanumericCharacterSet] intoString:&tmpString];
	}
	}@catch(NSException *e){
		@throw e;
	}@finally{
		//if(strData) [strData release];
	}
}

// -------------------------------------------------------------------------------
//  scanIpForCell:
// -------------------------------------------------------------------------------
-(void) scanIpForCell:(DBCellElement*) cellElement allIP:(NSString*)allIP {
    if (!allIP) return;
	NSScanner *ipScann = [NSScanner scannerWithString:allIP];
	NSCharacterSet *returnCS = [NSCharacterSet characterSetWithCharactersInString:@"\n"];
	NSCharacterSet *spaceCS = [NSCharacterSet characterSetWithCharactersInString:@" \t"];
	NSCharacterSet *startCommentCS = [NSCharacterSet characterSetWithCharactersInString:@"#"];
	NSString *tmpString = nil;
	while([ipScann isAtEnd] == NO){
		CellIp *cellIpDesc = [[CellIp alloc] init];
		
		//ip string
		[ipScann scanUpToCharactersFromSet:spaceCS 
								intoString:&tmpString];
		
		[cellIpDesc setCellIp:tmpString];
		//[tmpString release];
		
		// go to comment
		[ipScann scanUpToCharactersFromSet:startCommentCS intoString:nil];
		// skip comment symbol
		[ipScann scanUpToCharactersFromSet:[NSCharacterSet alphanumericCharacterSet] intoString:nil];
		// get comment
		[ipScann scanUpToCharactersFromSet:returnCS intoString:&tmpString];
		[cellIpDesc setCellComment:tmpString];
		//[tmpString release];
		
		[cellElement addIpToCell:cellIpDesc];
		// release the object becasuse NSMutableArray make a retain on object
		[cellIpDesc release];
	}
}


// -------------------------------------------------------------------------------
//  scanIpForCell:
// -------------------------------------------------------------------------------
-(NSArray*) readTheseCell:(NSString*) configFile {
	
	NSFileHandle *fileH = [NSFileHandle fileHandleForReadingAtPath:configFile];
	NSData *dbCellData = [fileH readDataToEndOfFile];
	NSString *strData = [[NSString alloc] initWithData:dbCellData
											  encoding:NSASCIIStringEncoding];
	
	return [strData componentsSeparatedByString:@"\n"];
}

// -------------------------------------------------------------------------------
//  -(void) getTokenList
// -------------------------------------------------------------------------------
-(NSArray*) getTokenList
{
	int line = 0;
	NSString *tokenLine = nil;
	//NSString *tmpValue = nil;
	NSMutableArray *tokenList = [[NSMutableArray alloc] init];
	NSString *tokensOutput = [TaskUtil executeTaskSearchingPath:@"tokens" args:[NSArray arrayWithObjects:nil]];
	
	if (!tokensOutput) return tokenList;
	// scan the tokens
	NSScanner *tokenScan = [NSScanner scannerWithString:tokensOutput];
	NSCharacterSet *returnCS = [NSCharacterSet characterSetWithCharactersInString:@"\n"];
	
	while([tokenScan isAtEnd] == NO){
		line++;
		// get the next line
		[tokenScan scanUpToCharactersFromSet:returnCS 
								intoString:&tokenLine];
		
		// check for tokens end
		if([tokenLine rangeOfString:@"--End of list--"].location != NSNotFound) break;
		
		
		if(line >= 2){
			// add enteir row to result
			[tokenList addObject:tokenLine];
			// create the line scanner for all the row that contains token info
		}
	}
	//
	return tokenList;
}

// -------------------------------------------------------------------------------
//  +(void) klog:(NSString*)uName uPwd:(NSString*)uPwd
// -------------------------------------------------------------------------------
-(void) klog:(NSString*)uName uPwd:(NSString*)uPwd cell:(NSString*)theCell
{
	if(uName == @"" ||  uPwd == @"") return;
	
	[TaskUtil executeTaskSearchingPath:@"klog" 
								  args:(theCell==nil?[NSArray arrayWithObjects:@"-principal", uName, @"-password", uPwd, nil]:[NSArray arrayWithObjects:@"-principal", uName, @"-password", uPwd, @"-c", theCell, nil])];
	
	
}


// -------------------------------------------------------------------------------
//  +(void) aklog
// -------------------------------------------------------------------------------
-(void) aklog:(NSString*)theCell noKerberosCall:(BOOL)krb5CallEnable {
	KLStatus		kstatus = noErr;
	@try {
		// trying to ket kerberos ticket
		if(krb5CallEnable) {
			kstatus = [Krb5Util getNewTicketIfNotPresent];
		} else kstatus = klNoErr;
		
		//ok to launch aklog
		if(kstatus == klNoErr) [TaskUtil executeTaskSearchingPath:@"aklog" 
															 args:(theCell==nil?[NSArray arrayWithObjects:nil]:[NSArray arrayWithObjects:@"-c", theCell, nil])];
		
	}
	@catch (NSException * e) {
		@throw e;
	}
	@finally {

	}
		
}


// -------------------------------------------------------------------------------
//  getTokens:
// -------------------------------------------------------------------------------
- (void) getTokens:(BOOL)klogAklogFlag usr:(NSString*)usr pwd:(NSString*)pwd {
	
	NSString *celStrName = nil;
	NSArray *tmpDefaultArray = [self getDefaultForTokenCellsName];
	if(tmpDefaultArray && [tmpDefaultArray count] > 1) {
		//there are other cell to autenticate
		for(int idx=0; idx < [tmpDefaultArray count]; idx++){
			celStrName = [tmpDefaultArray objectAtIndex:idx];
			if(klogAklogFlag)
				[self klog:usr 
					  uPwd:pwd  
					  cell:celStrName];
			else
				[self aklog:celStrName noKerberosCall:YES];	
		}
		
	} else {
		//there is only default cell to autenticate
		if(klogAklogFlag)
			[self klog:usr 
				  uPwd:pwd 
				  cell:nil];
		else
			[self aklog:nil noKerberosCall:YES];
	}
	
}

// -------------------------------------------------------------------------------
//  +(void) unlog
// -------------------------------------------------------------------------------
-(void) unlog:(NSString*)cell
{
	[TaskUtil executeTaskSearchingPath:@"unlog" 
								  args:(cell?[NSArray arrayWithObjects:@"-c",cell,nil]:[NSArray arrayWithObjects:nil])];
}

// -------------------------------------------------------------------------------
//  -(void) shutdown
// -------------------------------------------------------------------------------
-(void) shutdown
{
  NSString *rootHelperApp = [[NSBundle bundleForClass:[self class]] pathForResource:@"afshlp" ofType:@""];
    @try {
	const char *stopArgs[] = {AFS_DAEMON_STARTUPSCRIPT, "stop", 0L};
	[[AuthUtil shared] execUnixCommand:[rootHelperApp fileSystemRepresentation]
			   args:stopArgs
			   output:nil];
    }
    @catch (NSException * e) {
	@throw e;
    }
    @finally {
    }
}


// -------------------------------------------------------------------------------
//  -(void) shutdown
// -------------------------------------------------------------------------------
-(void) startup
{
  NSString *rootHelperApp = [[NSBundle bundleForClass:[self class]] pathForResource:@"afshlp" ofType:@""];
    @try {
	const char *startArgs[] = {AFS_DAEMON_STARTUPSCRIPT, "start", 0L};
	[[AuthUtil shared] execUnixCommand:[rootHelperApp fileSystemRepresentation]
			   args:startArgs
			   output:nil];
    }
    @catch (NSException * e) {
	@throw e;
    }
    @finally {
    }
}

// -------------------------------------------------------------------------------
//  -(void) saveConfigurationFiles
// -------------------------------------------------------------------------------
-(void) saveConfigurationFiles:(BOOL) makeBackup
{
	NSError *err;
	NSMutableString *filePath = [[NSMutableString alloc] initWithCapacity:256];
	NSMutableString *cellServDBString = [[NSMutableString alloc] initWithCapacity:256];
	NSMutableString *theseCellString = [[NSMutableString alloc] initWithCapacity:256];
	DBCellElement *cellElement = nil;
	
	// save the configuration file
	@try{
		[self exceptionOnInvalidPath];

		// ThisCell
		[filePath setString: @"/tmp/ThisCell"];
		[cellName writeToFile: [filePath stringByExpandingTildeInPath]
				   atomically:YES 
					 encoding: NSASCIIStringEncoding 
						error:nil];
		// CellServDB
		
		for(int idx = 0; idx < [cellList count]; idx++){
			cellElement = [cellList objectAtIndex:idx];
			[cellServDBString appendString:[cellElement description]];
			if([cellElement userDefaultForToken]) {
				[theseCellString  appendString:[cellElement getCellName]];
				[theseCellString  appendString:@"\n"];
			}
		}
		
		
		[filePath setString: @"/tmp/CellServDB"];
		[cellServDBString writeToFile: [filePath stringByExpandingTildeInPath]
						   atomically:YES 
							 encoding:  NSUTF8StringEncoding 
								error:&err];
		
		[filePath setString: @"/tmp/TheseCells"];
		[theseCellString writeToFile: [filePath stringByExpandingTildeInPath]
						   atomically:YES 
							 encoding:  NSUTF8StringEncoding 
								error:&err];
		
		if(makeBackup) [self backupConfigurationFiles];

		// install ThisCell
		[filePath setString: installationPath];
		[filePath appendString: @"/etc/ThisCell"];
		[self installConfigurationFile:@"/tmp/ThisCell" 
							  destPath:filePath];		
		
		// install CellServDB
		[filePath setString: installationPath];
		[filePath appendString: @"/etc/CellServDB"];
		[self installConfigurationFile:@"/tmp/CellServDB" 
							  destPath:filePath];
		
		// install CellServDB
		[filePath setString: installationPath];
		[filePath appendString: @"/etc/TheseCells"];
		[self installConfigurationFile:@"/tmp/TheseCells" 
							  destPath:filePath];
		
	} @catch (NSException *e) {
		@throw e;
	}@finally {
		// dispose all variable used
		if(filePath) [filePath release];
		if(cellServDBString) [cellServDBString release];
	}
	
}

// -------------------------------------------------------------------------------
//  -(void) saveCacheConfigurationFiles
// -------------------------------------------------------------------------------
-(void) saveCacheConfigurationFiles:(BOOL)makeBackup
{
	NSMutableString *filePath = [[NSMutableString alloc] initWithCapacity:256];	
	// save the configuration file
	@try{
		[self exceptionOnInvalidPath];
		
		// cacheinfo file creation
		[self writeCacheInfo:@"/tmp/cacheinfo"];
		
		//afsd.option or afs.conf file creation
		[self writeAfsdOption:useAfsdConfVersion?AFSD_TMP_NEW_PREFERENCE_FILE:AFSD_TMP_OLD_PREFERENCE_FILE];
		
		// backup original file
		if(makeBackup) {
			//cacheinfo
			[self backupFile:@"/etc/cacheinfo"];
			
			//afsd.options
			[self backupFile:useAfsdConfVersion?AFSD_NEW_PREFERENCE_FILE:AFSD_OLD_PREFERENCE_FILE];	
		}
		
		// install cacheinfo
		[filePath setString:installationPath];
		[filePath appendString: @"/etc/cacheinfo"];
		[self installConfigurationFile:@"/tmp/cacheinfo" 
							  destPath:filePath];		
		
		// install afsd.conf or afs.conf configuration file 
		[filePath setString: installationPath];
		[filePath appendString: useAfsdConfVersion?AFSD_NEW_PREFERENCE_FILE:AFSD_OLD_PREFERENCE_FILE];
		[self installConfigurationFile:useAfsdConfVersion?AFSD_TMP_NEW_PREFERENCE_FILE:AFSD_TMP_OLD_PREFERENCE_FILE
							  destPath:filePath];
		
	} @catch (NSException *e) {
		@throw e;
	}@finally {
		if(filePath) [filePath release];
	}
	
	
}

// -------------------------------------------------------------------------------
//  -(void) installConfigurationFile
// -------------------------------------------------------------------------------
-(void) installConfigurationFile:(NSString*)srcConfFile 
						destPath:(NSString*)destPath
{
	// delete the file original file

	if([futil autorizedDelete:destPath] != noErr){
		@throw [NSException exceptionWithName:@"installConfigurationFile:autorizedDelete" 
									   reason:destPath
									 userInfo:nil];
	}
	
	// move the file
	if([futil autorizedMoveFile:srcConfFile
						 toPath:destPath] != noErr) {
		@throw [NSException exceptionWithName:@"saveConfigurationFiles:autorizedMoveFile" 
									   reason:srcConfFile
									 userInfo:nil];
	}
	
	
	if([futil autorizedChown:destPath 
					   owner:@"root"
					   group:@"wheel"]!= noErr) {
		@throw [NSException exceptionWithName:@"saveConfigurationFiles:autorizedChown" 
									   reason:destPath
									 userInfo:nil];
	}
}

// -------------------------------------------------------------------------------
//  -(void) backupConfigurationFiles
// -------------------------------------------------------------------------------
-(void) backupConfigurationFiles
{	

	@try{
		//This cell
		[self backupFile:@"/etc/ThisCell"];
	
		//CellServDB
		[self backupFile:@"/etc/CellServDB"];
		
		//TheseCell
		[self backupFile:@"/etc/TheseCells"];
		
	} @catch (NSException *e) {
		@throw e;
	} @finally {
	}
}

// -------------------------------------------------------------------------------
//  -(void) backupFile:(NSString*)localAfsFilePath
// -------------------------------------------------------------------------------
-(void) backupFile:(NSString*)localAfsFilePath
{
	NSString *srcString = nil;
	NSMutableString *filePath = [[NSMutableString alloc] initWithCapacity:256];
	OSStatus err = noErr;
	@try{
		[filePath setString: installationPath];
		[filePath appendString: localAfsFilePath];
		
		//Check if the file at path exist
		NSFileManager *fileManager = [NSFileManager defaultManager];
		if(![fileManager fileExistsAtPath:[filePath stringByExpandingTildeInPath]]) return;
		
		// store the source path
		srcString  = [filePath stringByExpandingTildeInPath];
		[filePath appendString: @".afscommander_bk"];
		
		// check for presence of bk file
		if(![[NSFileManager defaultManager] fileExistsAtPath:[filePath stringByExpandingTildeInPath]]){
			// backup the file
			err = [futil autorizedCopy:srcString 
						  toPath:[filePath stringByExpandingTildeInPath]];
		}
	} @catch (NSException *e) {
		@throw e;
	} @finally {
		if(filePath) [filePath release];
	}
}

// -------------------------------------------------------------------------------
//  checkAfsStatus:[NSArray arrayWithObjects:@"checkserver", nil];
// -------------------------------------------------------------------------------
-(BOOL) checkAfsStatus
{
	BOOL result = NO;
	NSString *dfResult = [TaskUtil executeTaskSearchingPath:@"/bin/df" args:[NSArray arrayWithObjects:nil]];
	result = (dfResult?([dfResult rangeOfString:@AFS_FS_MOUNT].location != NSNotFound):NO);
	return result;	
}

// -------------------------------------------------------------------------------
//  checkAfsStatus:[NSArray arrayWithObjects:@"checkserver", nil];
// -------------------------------------------------------------------------------
-(BOOL) checkAfsStatusForStartup {
	BOOL result = NO;
		//NSString *fsResult = [TaskUtil executeTaskSearchingPath:@"launchctl" args:[NSArray arrayWithObjects: @"list", nil]];
		//result = (fsResult?([fsResult rangeOfString:@AFS_LAUNCHCTL_GREP_STR].location != NSNotFound):NO);
	return result;
}

// -------------------------------------------------------------------------------
//  makeChaceParamString
// -------------------------------------------------------------------------------
-(NSString*) makeChaceParamString
{
	NSNumber *tmpNum = nil;
	NSMutableString *afsdOption = [[NSMutableString alloc] init];
	if(!afsdOption) return @"";
	//write the data for afsd config file '-afsdb -stat x -dcache x -daemons x -volumes x -dynroot -fakestat-all'
	//afsdb
	//dynRoot
	if([self afsDB]) {
		[afsdOption appendString:@AFSD_OPTION_AFSDB_KEY];[afsdOption appendString:@" "];
	}
	
	//Verbose
	if([self verbose]) {
		[afsdOption appendString:@AFSD_OPTION_VERBOSE_KEY];[afsdOption appendString:@" "];
	}
	
	//stat entry
	tmpNum = [NSNumber numberWithInt:[self statCacheEntry]];
	if([tmpNum  intValue]) {[afsdOption appendString:@AFSD_OPTION_STAT_KEY];[afsdOption appendString:@" "];[afsdOption appendString:[tmpNum stringValue]];[afsdOption appendString:@" "];}
	
	//dcace
	tmpNum = [NSNumber numberWithInt:[self dCacheDim]];
	if([tmpNum  intValue]) {[afsdOption appendString:@AFSD_OPTION_DCACHE_KEY];[afsdOption appendString:@" "];[afsdOption appendString:[tmpNum stringValue]];[afsdOption appendString:@" "];}
	
	//daemons
	tmpNum = [NSNumber numberWithInt:[self daemonNumber]];
	if([tmpNum  intValue]) {[afsdOption appendString:@AFSD_OPTION_DAEMONS_KEY];[afsdOption appendString:@" "];[afsdOption appendString:[tmpNum stringValue]];[afsdOption appendString:@" "];}
	
	//volumes
	tmpNum = [NSNumber numberWithInt:[self nVolEntry]];
	if([tmpNum  intValue]) {[afsdOption appendString:@AFSD_OPTION_VOLUMES_KEY];[afsdOption appendString:@" "];[afsdOption appendString:[tmpNum stringValue]];[afsdOption appendString:@" "];}
	
	//dynRoot
	if([self dynRoot]) {
		[afsdOption appendString:@AFSD_OPTION_DYNROOT_KEY];[afsdOption appendString:@" "];
	}
	
	//fakestat-all
	[afsdOption appendString:@AFSD_OPTION_FKESTAT_ALL];[afsdOption appendString:@" "];
	
	return [afsdOption autorelease];
}

@end

