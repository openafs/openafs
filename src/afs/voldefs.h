
#if !defined(lint) && !defined(LOCORE) && defined(RCS_HDRS)
#endif

/*
 * (C) COPYRIGHT IBM CORPORATION 1987
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*

	System:		VICE-TWO
	Module:		voldefs.h
	Institution:	The Information Technology Center, Carnegie-Mellon University

 */

/* If you add volume types here, be sure to check the definition of
   volumeWriteable in volume.h */

#define readwriteVolume		RWVOL
#define readonlyVolume		ROVOL
#define backupVolume		BACKVOL

#define RWVOL			0
#define ROVOL			1
#define BACKVOL			2

/* All volumes will have a volume header name in this format */
#define VFORMAT "V%010lu.vol"
#define VMAXPATHLEN 64		/* Maximum length (including null) of a volume
				   external path name */

/* Values for connect parameter to VInitVolumePackage */
#define CONNECT_FS	1
#define DONT_CONNECT_FS	0
