/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef AFS_VOL_INFO_H
#define AFS_VOL_INFO_H

#include <afs/afsint.h>

/* scanVolType flags */
#define SCAN_RW  0x01		/**< scan read-write volumes vnodes */
#define SCAN_RO  0x02		/**< scan read-only volume vnodes */
#define SCAN_BK  0x04		/**< scan backup volume vnodes */

/* findVnType flags */
#define FIND_FILE       0x01	/**< find regular files */
#define FIND_DIR        0x02	/**< find directories */
#define FIND_MOUNT      0x04	/**< find afs mount points */
#define FIND_SYMLINK    0x08	/**< find symlinks */
#define FIND_ACL        0x10	/**< find access entiries */


struct VolInfoOpt {
    int checkout;            /**< Use FSSYNC to checkout volumes from the fileserver. */
    int dumpInfo;            /**< Dump volume information */
    int dumpHeader;          /**< Dump volume header files info */
    int dumpVnodes;          /**< Dump vnode info */
    int dumpInodeNumber;     /**< Dump inode numbers with vnodes */
    int dumpDate;            /**< Dump vnode date (server modify date) with vnode */
    int dumpInodeTimes;      /**< Dump some of the dates associated with inodes */
    int dumpFileNames;       /**< Dump vnode and special file name filenames */
    int showOrphaned;        /**< Show "orphaned" vnodes (vnodes with parent of 0) */
    int showSizes;           /**< Show volume size summary */
    int saveInodes;          /**< Save vnode data to files */
    int fixHeader;           /**< Repair header files magic and version fields. */
    char hostname[64];       /**< This hostname, for volscan output. */
    char columnDelim[16];    /**< Column delimiter char(s) */
    char printHeading;       /**< Print column heading */
    int checkMagic;          /**< Check directory vnode magic when looking up paths */
    unsigned int modeMask[64]; /**< unix mode bit pattern for searching for specific modes */
    int scanVolType;	     /**< volume types to scan; zero means do not check */
    int findVnType;	     /**< types of objects to find */
};


/*
 * volscan output columns
 */
#define VOLSCAN_COLUMNS \
    c(host) \
    c(desc) \
    c(vid) \
    c(offset) \
    c(vtype) \
    c(vname) \
    c(part) \
    c(partid) \
    c(fid) \
    c(path) \
    c(target) \
    c(mount) \
    c(mtype) \
    c(mcell) \
    c(mvol) \
    c(aid) \
    c(arights) \
    c(vntype) \
    c(cloned) \
    c(mode) \
    c(links) \
    c(length) \
    c(uniq) \
    c(dv) \
    c(inode) \
    c(namei) \
    c(modtime) \
    c(author) \
    c(owner) \
    c(parent) \
    c(magic) \
    c(lock) \
    c(smodtime) \
    c(group)


struct VnodeDetails;
int volinfo_Init(const char *name);
int volinfo_Options(struct VolInfoOpt **optp);
void volinfo_AddVnodeToSizeTotals(struct VolInfoOpt *opt, struct VnodeDetails *vdp);
void volinfo_SaveInode(struct VolInfoOpt *opt, struct VnodeDetails *vdp);
void volinfo_PrintVnode(struct VolInfoOpt *opt, struct VnodeDetails *vdp);
void volinfo_PrintVnodeDetails(struct VolInfoOpt *opt, struct VnodeDetails *vdp);
void volinfo_ScanAcl(struct VolInfoOpt *opt, struct VnodeDetails *vdp);
void volinfo_AddVnodeHandler(int vnodeClass,
		     void (*proc) (struct VolInfoOpt * opt,
				   struct VnodeDetails * vdp),
		     const char *heading);
int volinfo_AddOutputColumn(char *name);
int volinfo_ScanPartitions(struct VolInfoOpt *opt, char *partNameOrId, VolumeId volumeId);

#endif /* AFS_VOL_INFO_H */
