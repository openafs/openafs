/*
 *  Copyright (c) 2008 - Secure Endpoints Inc.
 */

/*
 *  The performance module when activated collects
 *  data necessary to analyze the usage of the cache
 *  manager and establish recommendations for future
 *  cache manager configuration changes.
 *
 *  As a starting point, the package will collect
 *  a list of all FIDs accessed during the session
 *  which will be used to periodically analyzed the
 *  contents of the cm_buf_t, cm_scache_t, cm_volume_t
 *  and cm_cell_t pools.

 */

#ifndef CM_PERFORMANCE_H
#define CM_PERFORMANCE_H
typedef struct cm_fid_stats {
    cm_fid_t    fid;
    afs_uint32  fileType;
    osi_hyper_t  fileLength;
    afs_uint32  flags;
    afs_uint32  buffers;
    struct cm_fid_stats * nextp;
} cm_fid_stats_t;

#define CM_FIDSTATS_FLAG_HAVE_SCACHE 0x01  /* set if cm_scache_t present */
#define CM_FIDSTATS_FLAG_HAVE_VOLUME 0x02  /* set on (vnode = 1) if cm_vol_t present */
#define CM_FIDSTATS_FLAG_RO          0x04
#define CM_FIDSTATS_FLAG_PURERO      0x08
#define CM_FIDSTATS_FLAG_CALLBACK    0x10

extern void cm_PerformanceTuningInit(void);
extern void cm_PerformanceTuningCheck(void);
extern void cm_PerformancePrintReport(void);

#endif /* CM_PERFORMANCE_H */

