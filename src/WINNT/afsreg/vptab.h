/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef VPTAB_H_
#define VPTAB_H_

/* Functions and definitions for accessing the vice partition table */

/* Sizes of strings */

#define	VPTABSIZE_NAME   32
#define	VPTABSIZE_DEV    32

/* Entry structure */

struct vptab {
    char    vp_name[VPTABSIZE_NAME];      /* vice partition name */
    char    vp_dev[VPTABSIZE_DEV];        /* device path */
};

struct vpt_iter {
    char *last;
    char *vpenum;
};


#ifdef __cplusplus
extern "C" {
#endif

extern int
vpt_Start(struct vpt_iter *iterp);

extern int
vpt_NextEntry(struct vpt_iter *iterp,
	      struct vptab *vptabp);

extern int
vpt_Finish(struct vpt_iter *iterp);

extern int
vpt_AddEntry(const struct vptab *vptabp);

extern int
vpt_RemoveEntry(const char *vpname);

extern int
vpt_PartitionNameValid(const char *vpname);

extern int
vpt_DeviceNameValid(const char *devname);

#ifdef __cplusplus
}
#endif

#endif /* VPTAB_H_ */
