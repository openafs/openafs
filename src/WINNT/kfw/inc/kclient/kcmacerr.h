
/* Error codes from Mac version of Project Mandarin Kerberos Client */
/* Copyright © 1995 Project Mandarin, Inc. */

typedef signed short OSErr;

enum {
	cKrbCorruptedFile = -1024,	/* couldn't find a needed resource */
	cKrbNoKillIO,				/* can't killIO because all calls sync */
	cKrbBadSelector,			/* csCode passed doesn't select a recognized function */
	cKrbCantClose,				/* we must always remain open */
	cKrbMapDoesntExist,			/* tried to access a map that doesn't exist (index too large,
									or criteria doesn't match anything) */
	cKrbSessDoesntExist,			/* tried to access a session that doesn't exist */
	cKrbCredsDontExist,			/* tried to access credentials that don't exist */
	cKrbTCPunavailable,			/* couldn't open MacTCP driver */
	cKrbUserCancelled,			/* user cancelled a log in operation */
	cKrbConfigurationErr,		/* Kerberos Preference file is not configured properly */
	cKrbServerRejected,			/* A server rejected our ticket */
	cKrbServerImposter,			/* Server appears to be a phoney */
	cKrbServerRespIncomplete,	/* Server response is not complete */
	cKrbNotLoggedIn,			/* Returned by cKrbGetUserName if user is not logged in */
	cKrbOldDriver,				/* old version of the driver */
	cKrbDriverInUse,			/* driver is not reentrant */
	cKrbAppInBkgnd,				/* driver won't put up password dialog when in background */
	cKrbInvalidSession,			/* invalid structure passed to KClient/KServer routine */
	
	cKrbKerberosErrBlock = -20000	/* start of block of 256 kerberos error numbers */
};


/* the following are borrowed from MacTCPCommonTypes.h */
/* MacTCP return Codes in the range -23000 through -23049 */
#define ipBadLapErr				-23000			/* bad network configuration */
#define ipBadCnfgErr			-23001			/* bad IP configuration error */
#define ipNoCnfgErr				-23002			/* missing IP or LAP configuration error */
#define ipLoadErr				-23003			/* error in MacTCP load */
#define ipBadAddr				-23004			/* error in getting address */
#define connectionClosing		-23005			/* connection is closing */
#define invalidLength			-23006
#define connectionExists		-23007			/* request conflicts with existing connection */
#define connectionDoesntExist	-23008			/* connection does not exist */
#define insufficientResources	-23009			/* insufficient resources to perform request */
#define invalidStreamPtr		-23010
#define streamAlreadyOpen		-23011
#define connectionTerminated	-23012
#define invalidBufPtr			-23013
#define invalidRDS				-23014
#define invalidWDS				-23014
#define openFailed				-23015
#define commandTimeout			-23016
#define duplicateSocket			-23017

/* Error codes from internal IP functions */
#define ipDontFragErr			-23032			/* Packet too large to send w/o fragmenting */
#define ipDestDeadErr			-23033			/* destination not responding */
#define ipNoFragMemErr			-23036			/* no memory to send fragmented pkt */
#define ipRouteErr				-23037			/* can't route packet off-net */

#define nameSyntaxErr 			-23041		
#define cacheFault				-23042
#define noResultProc			-23043
#define noNameServer			-23044
#define authNameErr				-23045
#define noAnsErr				-23046
#define dnrErr					-23047
#define	outOfMemory				-23048

