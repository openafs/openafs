/*
 * (C) COPYRIGHT IBM CORPORATION 1988
 * LICENSED MATERIALS - PROPERTY OF IBM
 */
/*
 *  Module:	    Vol.h
 *  System:	    Volser
 *  Instituition:   ITC, CMU
 *  Date:	    December, 88
 */

/* pick up the declaration of Inode */
#include <afs/afssyscalls.h>

typedef struct DirHandle {
    int		dirh_volume;
    int		dirh_device;
    Inode	dirh_inode;
    afs_int32	dirh_cacheCheck;
    IHandle_t	*dirh_handle;
} DirHandle;
