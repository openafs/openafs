/* This file is at least partly a bridge between the washtool and 
   autoconf worlds. It will likely be replaced later. */

#define ERR_RETURN(X) return X

#ifdef AFS_LINUX20_ENV
#define ERR_RETURN(X) return -X
#endif

#ifdef AFS_SGI62_ENV
#define VTOINUM_VNODETOINO
#define VTODEV_VNODETODEV
#endif

#ifdef AFS_SUN5_ENV
#define VTOINUM_VNODETOINO
#define VTODEV_VNODETODEV
#endif

#ifdef AFS_DARWIN_ENV
#define VTOINUM_VNODETOINO
#define VTODEV_VNODETODEV
#endif

#ifdef AFS_DECOSF_ENV
#define VTOINUM_VTOINUM
#define VTODEV_VTODEV
#endif


