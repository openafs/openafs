/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

RCSID
    ("$Header$");

#include "afs/sysincludes.h"    /* Standard vendor system headers */
#include "afsincludes.h"        /* Afs-based standard headers */
#include "afs/afs_stats.h"

#ifdef DISCONN
#include "discon.h"

#include "discon_stats.h"
#include "discon_log.h"
#define lockedPutDCache(ad) ((ad)->refCount--)


extern afs_int32 afs_dvhashTbl[DVHASHSIZE]; /*Data cache hash table*/
extern afs_int32 afs_dchashTbl[DCHASHSIZE]; /*Data cache hash table*/
extern afs_int32 afs_mariner;
extern struct osi_vfs *afs_globalVFS;
extern discon_modes_t discon_state;
extern struct vcache *freeVSList;
extern struct vcache **afs_indexVTable;
extern short freeDVCList;
extern afs_int32 afs_indexVCounter;
extern afs_int32 CacheFetchProc();
extern struct dcache *OB_GetDCache();
extern afs_int32 Free_OBDcache();
extern struct vcache *afs_NewVCache();
extern char *afs_VindexFlags;
afs_int32 afs_num_vslots;
extern struct llist *cur_llist;
extern afs_int32 llist_done;
extern struct afs_lock afs_xvcache;
extern struct afs_lock afs_xdcache;
extern struct afs_lock afs_xvolume;
extern struct afs_lock afs_xserver;
extern struct afs_lock afs_xcell;
extern afs_int32 *afs_indexVTimes;
extern afs_int32 afs_vchashTable[DVHASHSIZE];
extern struct volume *afs_volumes[NVOLS];
extern struct DirEntry *dir_GetBlob();
extern afs_int32 BlobScan();

afs_uint32 cur_dvnode = 0x0000ffff;
afs_uint32 cur_fvnode = 0x0000fffe;
extern struct afs_q VLRU;

extern struct vcache *LowVSrange;
extern struct vcache *HighVSrange;


struct translations*  translate_table[VCSIZE];

struct volume *slot_to_Volume[REALLY_BIG];
struct cell *slot_to_Cell[REALLY_BIG];
struct server *slot_to_Server[REALLY_BIG];

backing_store_t	vc_data;
backing_store_t	vol_data;
backing_store_t	serv_data;
backing_store_t	cell_data;
backing_store_t	rlog_data;
backing_store_t	log_data;
backing_store_t	name_info;
backing_store_t	trans_data;
backing_store_t	trans_names;


afs_int32 freeVScount = 0;
struct afs_lock op_no_lock;
afs_int32 current_op_no;


extern struct server *afs_servers[NSERVERS];
extern struct cell *afs_cells;
extern afs_int32 freeDCList;
extern afs_int32 freeDCCount;

extern afs_int32 disconnected_servindex;
extern afs_int32 disconnected_volindex;
extern afs_int32 disconnected_cellindex;
extern afs_int32 Log_is_open;
extern int cacheDiskType;

extern int reconcile_rename();

/* Forward function declarations */
void merge_backup_log(void);
int PReplayOp(struct vcache *avc, int afun, struct vrequest *areq, char *ain, char *aout, afs_int32 ainSize, afs_int32 *aoutSize);
int PDisconnect(struct vcache *avc, int afun, struct vrequest *areq, char *ain, char *aout, afs_int32 ainSize, afs_int32 *aoutSize);
int PSetLog(struct vcache *avc, int afun, struct vrequest *areq, char *ain, char *aout, afs_int32 ainSize, afs_int32 *aoutSize);
int disconnected_init(int reinit);
void extend_file(struct osi_file *tfile, int size);
int afs_InitVCacheInfo(char *afile);
static int file_init(char *fname, backing_store_t *, int keep_open);
int afs_InitDBLog(char *afile);
int afs_InitDLog(char *afile);
int afs_InitDCellInfo(char *afile);
int map_init(backing_store_t *, int);
int afs_InitDTransFile(char *afile);
int afs_InitDTransData(char *afile);
int afs_InitDServerInfo(char *afile);
int afs_InitDNameInfo(char *afile);
int afs_InitDVolumeInfo(char *afile);
void init_vcache_entries(void);
void afs_init_data(void);
void afs_ReadServer(void);
void afs_ReadVolume(void);
void afs_ReadCell(void);
void afs_ReconnectPointers(void);
struct dcache *get_newDCache(struct vcache *avc);
void afs_GetDownV(int anumber);
static void afs_SaveVolume(struct volume *vol);
void afs_SaveCell(struct cell *cell);
void afs_SaveServer(struct server *serv);
int afs_SaveVCache(struct vcache *avc, int atime);
void generateDFID(struct vcache *adp, struct VenusFid *newfid);
void store_dirty_vcaches(void);
void generateFFID(struct vcache *adp, struct VenusFid *newfid);
int fid_in_cache(struct VenusFid *afid);
int afs_InitReplayLog(char *afile);
void afs_InitStatFile(char *afile);
void StartRLog(void);
void CloseRLog(void);
int dprint(char *buf, int len);
int parse_log(void);
int replay_operation(log_ent_t *log_ent);
void un_pin_vcaches(void);
void un_pin_dcaches(void);
void copy_backuplog(void);
char *get_UniqueName(struct dcache *tdc, char *fname, char *extension);
struct translations *allocate_translation(struct VenusFid *afid);
struct translations *get_translation(struct VenusFid *afid);
void store_name_translation(struct name_trans *ntrans);
void store_translation(struct translations *trans);
void free_translation(struct translations *trans);
void free_ftrans(int slot);
void free_ntrans(int slot);
void free_name(int slot);
struct vcache *afs_GetVSlot(int aslot, struct vcache *tmpvc);
void save_info(struct afs_lock *lock, int op);
void merge_backup_log();
int afs_FlushVS(struct vcache *tvc);
static void afs_GetDownVS(int anumber);
int read_ntrans(int slot, struct name_trans *ntrans);
int read_trans(int slot, struct translations *trans);
int read_name(int slot, char *name);
int find_file_name(afs_int32 *dir, struct VenusFid *fid, char *fname);
int move_dcache(struct vcache *ovc, struct VenusFid *nfid);
void unalloc_trans(void);
void afs_load_translations(void);
int fix_dirents(struct vcache *ovc, char *name, struct VenusFid *nfid);
int determine_file_name(struct vcache *avc, char *fname);
int save_lost_data(struct vcache *avc, struct vrequest *areq);
int save_in_VRoot(struct vcache *avc, struct vrequest *areq);
int add_to_orphanage(struct vcache *avc, struct vcache *orphvc,
    struct dcache *orphdc, struct vrequest *areq);
struct vcache *procure_vc(struct VenusFid *afid, struct vrequest *areq);
void give_up_cbs(void);
int save_in_homedir(struct vcache *avc);
int save_in_tmp(struct vcache *avc);
int un_string_vc(struct vcache *avc);
int string_vc(struct vcache *avc);
void extend_map_storage(backing_store_t *, int);
int allocate_map_ent(backing_store_t *, int);
int allocate_name(char *name);
struct vcache *Lookup_VC(struct VenusFid *afid);
void rel_readlock(struct afs_lock *lock);
void ob_readlock(struct afs_lock *lock);
void ob_writelock(struct afs_lock *lock);
void ob_sharedlock(struct afs_lock *lock);
void up_stowlock(struct afs_lock *lock);
void conv_wtoslock(struct afs_lock *lock);
void conv_wtorlock(struct afs_lock *lock);
void conv_storlock(struct afs_lock *lock);
void rel_writelock(struct afs_lock *lock);
void rel_sharedlock(struct afs_lock *lock);
void print_info(struct vcache *);

#ifdef LHUSTON
#include "proto.h"
#endif
int afs_running = 0;
afs_int32	integrity_check = 0x04;

int doing_init= 0;
int dlog_mod=0;
int l_flag=0;

/* flags for the logging levels */
afs_int32 log_user_level = DISCON_ERR;
afs_int32 log_file_level = DISCON_INFO;
afs_int32 log_non_mutating_ops = 0;

int do_cold_cache = 0;


int
PReplayOp(avc, afun, areq, ain, aout, ainSize, aoutSize)
	struct vcache *avc;
	int afun;
	struct vrequest *areq;
	char *ain, *aout;
	afs_int32 ainSize;
	afs_int32 *aoutSize;     /* set this */
{
	int code;

	code = replay_operation((log_ent_t *) ain);

	memcpy(aout, &code, sizeof(afs_int32));
	*aoutSize = sizeof(afs_int32);

	return 0;
}


/* The main pioctl interface to enable, disable connected mode.  */

int
PDisconnect(avc, afun, areq, ain, aout, ainSize, aoutSize)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;     /* set this */
{
    afs_int32 operation;
    int code = 0;

    /* use the index table as an indicator whether AFS is running */
    if (!afs_indexTable)
	return ENOENT;

    memcpy(&operation, ain, sizeof(afs_int32));

    switch(operation) {
    case AFS_DIS_DISCON:
	give_up_cbs();
	/* fall through */

    case AFS_DIS_DISCONTOSS:
	if (IS_CONNECTED(discon_state))
	    disconnected_init(0);
	afs_RemoveAllConns();
	discon_state = DISCONNECTED;
	break;

    case AFS_DIS_RECON:
	/*
	 * If we were disconnected, some servers may have come back up without
	 * our knowledge, and it would be a shame for the replay to fail,
	 * so mark them all up.  If they really are still down we'll find out
	 * the hard way.
	 * XXX Maybe should call CheckServers(1, 0) instead?
	 */
	if (USE_OPTIMISTIC(discon_state)) {
	    afs_RemoveAllConns();
	    afs_MarkAllServersUp();
	}
	/*
	 * if we are in disconnected mode, put the CM in
	 * fetch only mode first
	 */
	if (IS_DISCONNECTED(discon_state) || IS_PARTIAL(discon_state))
	    discon_state = FETCH_ONLY;
	if (IS_FETCHONLY(discon_state)) {
	    code = parse_log();
	    if(code) {
		dlog_mod = 1;
	    } else {
		dlog_mod = 1;
		discon_state = CONNECTED;
	    }
	}
	break;

    case AFS_DIS_RECONTOSS:
	/* reconnect without playing the log */

	if (!IS_CONNECTED(discon_state)) {
	    /* clear the log file */
	    Log_is_open = 0;
	    osi_Truncate(log_data.tfile,(afs_int32) 0);
	    osi_UFSClose(log_data.tfile);

	    /* truncate the backup file */
	    discon_state = CONNECTED;
	    dlog_mod = 1;
	    un_pin_vcaches();
	    un_pin_dcaches();
	    unalloc_trans();
	}
	break;

    case AFS_DIS_QUERY:
	/*
	 * query the current mode, we just break out
	 * because we always return the current mode.
	 */
	break;

    case AFS_DIS_FETCHONLY:
	give_up_cbs();
	if (IS_CONNECTED(discon_state))
	    disconnected_init(0);
	StartRLog();
	/* be optimistic */
	afs_MarkAllServersUp();
	discon_state = FETCH_ONLY;
	break;

    case AFS_DIS_PARTIAL:
	/*
	 * If we were in connected
	 * mode, then we need to init some stuff,
	 * otherwise we can just change the state.
	 */
	if (IS_CONNECTED(discon_state))
	    disconnected_init(0);
	else
	    afs_MarkAllServersUp();
	StartRLog();
	discon_state = PARTIALLY_CONNECTED;
	break;

    default:
	code = EINVAL;
	break;
    }

    /* make sure the mode change makes it out to disk */
    store_dirty_vcaches();

    /* we always return the current mode in the out data */

    *((discon_modes_t *) aout) = discon_state;
    *aoutSize = sizeof(discon_modes_t);

    return code;
}

int
PSetDOps(avc, afun, areq, ain, aout, ainSize, aoutSize)
struct vcache *avc;
int afun;
struct vrequest *areq;
char *ain, *aout;
afs_int32 ainSize;
afs_int32 *aoutSize;     /* set this */
{
    int code = 0;
    afs_int32 last_op;
    afs_int32 server;
    afs_int32 *lptr;

    dis_setop_info_t *op_info = (dis_setop_info_t *) ain;
    dis_setopt_op_t	 op = op_info->op;

    if (ainSize != sizeof(dis_setop_info_t))
	return EINVAL;

    switch(op) {
    case GET_LOG_NAME:
	strcpy(aout, log_data.bs_name);
	*aoutSize = strlen(aout) + 1;
	break;

	/* logging level to the user */
    case SET_USERLOG_LEVEL:
	log_user_level = *((afs_int32 *)op_info->data);
	*aout = 0;
	*aoutSize = sizeof(int);
	break;

	/* logging level to the replay log */
    case SET_FILELOG_LEVEL:
	log_file_level = *((afs_int32 *)op_info->data);
	*aout = 0;
	*aoutSize = sizeof(int);
	break;

	/* update the flags on a log entry */
    case UPDATE_FLAGS:
	lptr = (afs_int32 *) &op_info->data;
	update_log_ent(lptr[0], lptr[1]);
	break;

    case PING_SERVER:
	server = *((afs_int32 *) op_info->data);
	code = ping_server(server);
	*aout = 0;
	*aoutSize = sizeof(int);
	break;

    case GIVEUP_CBS:
	give_up_cbs();
	*aout = 0;
	*aoutSize = sizeof(int);
	break;

    case PRINT_INFO:
	print_info(avc);
	*aout = 0;
	*aoutSize = sizeof(int);
	break;

    case VERIFY_VCACHE:
	code = verify_vcache(op_info->data, aout, aoutSize);
	break;

    default:
	*aout = EINVAL;
	*aoutSize = sizeof(int);
	code = EINVAL;
	break;
    }
    return (code);
}


/* This is a hack for the locking code, to save records of each lock event */

void
save_info(lock, op)

	struct afs_lock *lock;
	int op;

{
	if(llist_done) {
		cur_llist = cur_llist->next;
		cur_llist->lk = lock;
		cur_llist->operation = op;
		cur_llist->who = osi_curpid();
	}
}



/* This initializes all the data for disconnected mode */

int
disconnected_init(int reinit)
{
    struct osi_file *tfile;
    struct osi_stat astat;

    Lock_Init(&log_data.bs_lock);
    Lock_Init(&op_no_lock);

    tfile = osi_UFSOpen(&log_data.bs_dev, log_data.bs_inode);
    if (!tfile)
	return(-1);

    ObtainWriteLock(&log_data.bs_lock);

    Log_is_open = 1;
    log_data.tfile = tfile;

    if (reinit) {
	/* seek to the end of the file */
        afs_osi_Stat(tfile, &astat);
	tfile.offset = astat.size;
    } else {
	osi_Truncate(tfile, 0L);
	current_op_no = 1;
    }
    dlog_mod = 1;
    cur_fvnode = 0x0000fffe;
    cur_dvnode = 0x0000ffff;

    ReleaseWriteLock(&log_data.bs_lock);
    return 0;
}


/*
 * this extends the size of a file to the size passed in
 * the variable size.  To extend the file bogus data is written
 * into it.
 */

void
extend_file(tfile, size)
    struct osi_file *tfile;
    int size;
{
    struct osi_stat astat;
    int to_go;
    int block_size = 1024;
    int current_write;
    char * buffer;

    /* seek to the end of the file */
    afs_osi_Stat(tfile, &astat);
    tfile.offset=astat.size;

    to_go = size - astat.size;
    buffer = (char *) afs_osi_Alloc(block_size);
    memset(buffer, 0, block_size);

    while (to_go > 0) {
	if (to_go < block_size)
	    current_write = to_go;
	else
	    current_write = block_size;

	if (afs_osi_Write(tfile, -1, buffer, current_write) < 0)
	    panic("extend_file afs_osi_Write");
	to_go -= current_write;
    }
    afs_osi_Free(buffer, block_size);
}

int
afs_InitVCacheInfo(afile)
    char *afile;

{
    struct afs_dheader theader;
    afs_int32 code;
    int goodFile = 0;

    init_cblist();

    code = file_init(afile, &vc_data, 1);
    if (code)
	panic("afs_InitVCacheInfo");

    code = afs_osi_Read(vc_data.tfile, -1, (char *) &theader, sizeof(theader));

    if (code == sizeof(theader)) {
	/* read the header correctly */
	if (theader.magic == AFS_FHMAGIC &&
	    theader.firstCSize == AFS_FIRSTCSIZE &&
	    theader.otherCSize == AFS_OTHERCSIZE)
	    goodFile = 1;
    }

    if (!goodFile) {
	/* write out a good file label */
	theader.magic = AFS_FHMAGIC;
	theader.firstCSize = AFS_FIRSTCSIZE;
	theader.otherCSize = AFS_OTHERCSIZE;
	theader.mode = discon_state;
	afs_osi_Write(vc_data.tfile, 0, (char *) &theader, sizeof(theader));
	/*
	 * Truncate the rest of the file, since it may be arbitrarily
	 * wrong
	 */
	osi_Truncate(vc_data.tfile, sizeof(struct afs_dheader));
    } else {
	discon_state = theader.mode;
	if (!IS_CONNECTED(discon_state)) {
	    /*
	     * If we are in recon or fetch only mode, start
	     * fully disconnected
	     */
	    discon_state = DISCONNECTED;
	    printf("disconnected...");
	    /*
	     * we add 1000 just to make there were no
	     * more ops before the last sync
	     */
	    current_op_no = theader.current_op + 1000;
	    disconnected_init(1);
	}
    }

    osi_Wakeup(&vc_data.bs_inode);
    return 0;
}

static int
file_init(fname, bsp, keep_open)
char *fname;
backing_store_t	*bsp;
int keep_open;
{
    struct osi_file *tfile;
    struct vnode *filevp;
    int	code;

    if (cacheDiskType != AFS_FCACHE_TYPE_UFS)
	panic("file_init --- called for non-ufs cache!");

    bsp->tfile = NULL;

    code = gop_lookupname(fname, AFS_UIOSYS, 0, (struct vnode *) 0,
			  &filevp);

    if (code)
	return code;

    bsp->bs_inode = VTOI(filevp)->i_number;
#ifdef AFS_NETBSD_ENV
    bsp->bs_dev.mp = filevp->v_mount;
    bsp->bs_dev.held_vnode = filevp;
#else  /* AFS_NETBSD_ENV */
    bsp->bs_dev.dev = VTOI(filevp)->i_dev;
    VN_RELE(filevp);
#endif /* AFS_NETBSD_ENV */

    /* Make sure you can open the file */
    tfile = osi_UFSOpen(&(bsp->bs_dev), bsp->bs_inode);

    if (!tfile)
	panic("file_init");

    /*
     * if do_cold_cache flag is set we truncate this file.  This
     * flag will be set by afs_InitDNameInfo
     */

    if (do_cold_cache)
	osi_Truncate(tfile, (afs_int32) 0);

    /* save the file name */
    bsp->bs_name = (char *) afs_osi_Alloc(strlen(fname) + 1);
    strcpy(bsp->bs_name, fname);

    if (keep_open)
	bsp->tfile = tfile;
    else
	osi_UFSClose(tfile);

    return 0;
}


int
afs_InitDBLog(afile)
	char *afile;
{
	return 0;
}


int
afs_InitDLog(afile)
	char *afile;

{
	int code;

	code = file_init(afile, &log_data, 0);
	return code;
}


int
afs_InitDCellInfo(afile)
    char *afile;

{
    int code;

    code = file_init(afile, &cell_data, 0);
    return code;
}


/*
 * the layout of the map file is
 *	map header
 *	data
 *	map
 *
 * the map file is extended by updating the header to show the new
 * number of entries, extending the file to the correct length, writing
 * out the data, then writing out the extended map.  by putting the map
 * at the end, we don't change the old data offsets.
 */

int
map_init(bsp, obj_size)
backing_store_t *bsp;
int obj_size;
{
    int map_offset;
    struct osi_file *tfile;
    int code;

    /* open the data file */
    tfile = osi_UFSOpen(&(bsp->bs_dev), bsp->bs_inode);
    if (!tfile)
	panic("map init");


    bsp->bs_header = (struct map_header *)
	afs_osi_Alloc(sizeof(struct map_header));
    code = afs_osi_Read(tfile, -1, (char *) bsp->bs_header, sizeof(map_header_t));

    /* if we failed to read file assume it is empty and reinitialize */

    if ((code != sizeof(map_header_t)) || (bsp->bs_header->magic != AFS_DHMAGIC)) {
	do_cold_cache = 1;
	osi_Truncate(tfile, (afs_int32) 0);

	bsp->bs_header->magic = AFS_DHMAGIC;
	bsp->bs_header->num_ents = MAP_ENTS;
	bsp->bs_map = (char *) afs_osi_Alloc(MAP_ENTS);
	memset((char *) bsp->bs_map, 0, (MAP_ENTS * sizeof(char)));

	map_offset = sizeof(map_header_t) + (bsp->bs_header->num_ents * obj_size);

	extend_file(tfile, map_offset);
	afs_osi_Write(tfile, 0, (char *) bsp->bs_header, sizeof(map_header_t));
	afs_osi_Write(tfile, map_offset, (char *) bsp->bs_map, bsp->bs_header->num_ents);
    } else {
	bsp->bs_map = (char *) afs_osi_Alloc(bsp->bs_header->num_ents);
	map_offset = sizeof(map_header_t) + (bsp->bs_header->num_ents * obj_size);
	code = afs_osi_Read(tfile, map_offset, (char *) bsp->bs_map, bsp->bs_header->num_ents);
	if (code != bsp->bs_header->num_ents)
	    panic("failed to init name info ");
    }

    osi_UFSClose(tfile);
    return 0;
}

int
afs_InitDTransFile(afile)
	char *afile;
{
	afs_int32 code;

	Lock_Init(&trans_data.bs_lock);

	ObtainWriteLock(&trans_data.bs_lock);

	code = file_init(afile, &trans_data, 0);

	code = map_init( &trans_data, sizeof(struct translations));
	ReleaseWriteLock(&trans_data.bs_lock);

	return code;

}

int
afs_InitDTransData(afile)
	char *afile;
{
	afs_int32 code;

	Lock_Init(&trans_names.bs_lock);

	ObtainWriteLock(&trans_names.bs_lock);
	code = file_init(afile, &trans_names, 0);

	code = map_init(&trans_names, sizeof(struct name_trans));
	ReleaseWriteLock(&trans_names.bs_lock);
	return code;
}


/* Initialize the server information file */
int
afs_InitDServerInfo(afile)
	char *afile;

{
	afs_int32 code;
	code = file_init(afile, &serv_data, 0);
	return code;
}


int
afs_InitDNameInfo(afile)
	char *afile;
{
	afs_int32 code;

	Lock_Init(&name_info.bs_lock);

	ObtainWriteLock(&name_info.bs_lock);

	code = file_init(afile, &name_info, 0);

	code = map_init( &name_info, NAME_UNIT);

	ReleaseWriteLock(&name_info.bs_lock);
	return code;
}


/* initialize the volume information */
int
afs_InitDVolumeInfo(afile)
char *afile;
{
    afs_int32 code;
    code = file_init(afile, &vol_data, 0);
    afs_init_data();
    return code;
}


/*
 * This function initializes all of the vcache entries.  It is called
 * when AFS is first started, so clean up any state that may still
 * exist from the last time AFS was run.
 */
void
init_vcache_entries()

{
    int  i, j;
    struct vcache *tvc, tempvc;
    afs_int32 hash;
    int freecount = 0, usedcount = 0, inmemcount;
    afs_uint32 vn;

    if (afs_num_vslots == 0)
	panic("no vslots: wrong afsd?");

    ObtainWriteLock(&afs_xvcache);
    doing_init = 1;
    inmemcount = freeVScount;

    printf("Starting vcache scan...");

    for (i = 0; i < afs_num_vslots; i++) {
	tvc = afs_GetVSlot(i, &tempvc);
#ifdef AFS_OBSD_ENV
	tvc->v = NULL;
	lockinit(&tvc->rwlock, PINOD, "vcache", 0, 0);
#endif
	/* if this is a bad entry put on free list */
	if (tvc->fid.Fid.Volume == 0) {
	    tvc->dhnext = freeDVCList;
	    freeDVCList = tvc->index;
	    afs_VindexFlags[tvc->index] |= VC_FREE;
	    freecount++;
	} else {
	    /* else on a good entry put on good list */

	    hash = VCHash(&tvc->fid);
	    tvc->dhnext = afs_vchashTable[hash];
	    afs_vchashTable[hash] = tvc->index;

#ifndef AFS_NETBSD_ENV
#if (!defined(MACH))
	    /* XXX is this the right thing to do? */
	    VCTOV(tvc)->v_text = (struct text *) 0;
#endif
	    VCTOV(tvc)->v_vfsmountedhere = (struct vfs *) 0;
	    VCTOV(tvc)->v_socket = (struct socket *) 0;
	    VCTOV(tvc)->v_flag &= VROOT;
	    SetAfsVnode(VCTOV(tvc));
	    vSetVfsp(tvc,afs_globalVFS);
#endif
	    usedcount++;
	    tvc->execsOrWriters = 0;
	    tvc->opens = 0;

	    /* set the lruq times */
	    afs_indexVTimes[tvc->index] = tvc->ref_time;
	    if(tvc->ref_time >  afs_indexVCounter)
		afs_indexVCounter = tvc->ref_time;

	    if (tvc->dflags & KEEP_VC)
		afs_VindexFlags[tvc->index] |= KEEP_VC;

	    vn = tvc->fid.Fid.Vnode;
	    if ((vn & 0xfffff000) == 0x0000f000
		&& (tvc->fid.Fid.Unique & 0xfffff000) == 0x0000f000) {
		if ((vn & 1) && vn <= cur_dvnode)
		    cur_dvnode = vn - 2;
		else if (!(vn & 1) && vn <= cur_fvnode)
		    cur_fvnode = vn - 2;
	    }
	}

	tvc->Access = NULL;
	tvc->callback = 0;
	tvc->states &= ~(CStatd | CUnique);
	tvc->quick.stamp = 0;
	tvc->dchint = NULL;	/* invalidate hints */
	afs_VindexFlags[tvc->index] &= ~VC_HAVECB;

	afs_SaveVCache(tvc, 1);

	if(tvc->dhnext == tvc->index)
	    panic("init_vcache_entries: index == dhnext");
    }

    printf("found %d out of %d (%d in mem)\n", usedcount, afs_num_vslots, inmemcount);

    doing_init = 0;

    afs_running = 1;
    ReleaseWriteLock(&afs_xvcache);

    return;
}


/* this loads 'persistent' into memory */
void
afs_init_data()

{
    int i;

    for (i = 0; i < REALLY_BIG; i++) {
	slot_to_Volume[i] = 0;
	slot_to_Cell[i] = 0;
	slot_to_Server[i] = 0;
    }

    afs_ReadServer();
    afs_ReadCell();
    afs_ReadVolume();
    afs_ReconnectPointers();

    afs_load_translations();
}


void
afs_ReadServer()

{
    struct save_server ss;
    struct server *serv;
    struct osi_file *tfile;
    afs_int32 code;
    int i,j=0;


    /* open the backing file for the server information */
    tfile = osi_UFSOpen(&serv_data.bs_dev, serv_data.bs_inode);
    if(!tfile)
	panic("afs_ReadServer: failed UFSOpen");

    ObtainWriteLock(&afs_xserver);


    /* read all of the server structures */

    do {
	code=afs_osi_Read(tfile, j*sizeof(struct save_server), (char *) &ss, sizeof(struct save_server));	
	j++;

	if (!code) 
	    break;

	if (code != sizeof(struct save_server))
	    panic("afs_ReadServer: bad size");

	if (disconnected_servindex != ss.dindex)
	    panic("afs_ReadServer: wrong slot");

	/*
	 * See if this server is already in our in memory list of
	 *  servers.  (Ie. it was loaded from CellservDB)
	 */

	i = SHash(ss.host);
	for (serv = afs_servers[i]; serv; serv=serv->next) {
	    if (serv->host == ss.host && serv->portal == ss.portal)
		break;
	}

	/* if it wasn't in our list of servers, then add it */

	if (!serv) {
	    serv = (struct server *) afs_osi_Alloc(sizeof(struct server));
	    memset(serv, 0, sizeof(struct server));

	    serv->host = ss.host;
	    serv->portal = ss.portal;
	    serv->random = ss.random;
	    /*
	     * This looks strange, but it gets fixed up in afs_ReconnectPointers()
	     */
	    serv->cell = (struct cell *) ss.cell;
	    /*
	     *  XXX removed this code, do I need to replace,
	     * now do something about the vcbs
	     */
	    serv->isDown = 0;
	    serv->conns = (struct conn *) 0;

	    /* now put the server on the afs_server list */
	    i = SHash(serv->host);
	    serv->next = afs_servers[i];
	    afs_servers[i] = serv;
	} else {
	    if (serv->dindex != 0) {
		uprintf("orig dindex %d new %d \n", serv->dindex, disconnected_servindex);
		panic("afs_ReadServer: bad dindex");
	    }
	}
	serv->dindex = disconnected_servindex;

	/* make sure the index hasn't grown past our bound */

	if(serv->dindex >= REALLY_BIG)
	    panic("server index to big ");

	/* add the mapping from server index to pointer to the table */
	slot_to_Server[serv->dindex] = serv;
	disconnected_servindex++;
    } while(1);

    ReleaseWriteLock(&afs_xserver);
    osi_UFSClose(tfile);
}


extern struct volume * afs_UFSGetVolSlot();

void
afs_ReadVolume()

{
    struct save_volume sv;
    struct volume *vol;
    struct osi_file *tfile;
    int i,j=0;
    int len;
    afs_int32 code;

    /* open the backing store for the cell information */

    tfile = osi_UFSOpen(&vol_data.bs_dev, vol_data.bs_inode);
    if(!tfile)
	panic("failed UFSOpen in ReadVolume");

    ObtainWriteLock(&afs_xvolume);

    do {
	code = afs_osi_Read(tfile, j*sizeof(struct save_volume), (char *) &sv, sizeof(struct save_volume));
	j++;

	if (code == 0) 
	    break;
	if (code != sizeof(struct save_volume)) {
	    panic("afs_ReadVolume: bad ss");
	}
	if (disconnected_volindex != sv.dindex)
	    panic("afs_ReadVolume: bad ss");
	disconnected_volindex++;

	/* look to see if this volume information already exists.  if
	 * it does then we don't add it again
	 */

	i = VHash(sv.volume);
	for (vol = afs_volumes[i]; vol; vol = vol->next)
	    if (vol->volume == sv.volume && vol->cell == sv.cell)
		break;

	if (!vol) {
	    vol = afs_UFSGetVolSlot();
	    memset(vol, 0, sizeof (struct volume));

	    vol->cell = sv.cell;
	    Lock_Init(&vol->lock);
	    vol->volume = sv.volume;
	    vol->dotdot = sv.dotdot;
	    vol->mtpoint = sv.mtpoint;
	    vol->rootVnode = sv.rootVnode;
	    vol->rootUnique = sv.rootUnique;
	    vol->roVol = sv.roVol;
	    vol->backVol = sv.backVol;
	    vol->rwVol = sv.rwVol;
	    vol->accessTime = sv.accessTime;
	    vol->copyDate = sv.copyDate;
	    vol->states = sv.states | VRecheck;
	    vol->dindex = sv.dindex;
	    vol->vtix = -1;

	    /* now copy the volume name */
	    if (sv.name[0]) {
		len = strlen(sv.name);
		if (len >= sizeof(sv.name))
		    panic("afs_ReadVolume: afs_int32 string");
		vol->name = (char *) afs_osi_Alloc(len + 1);
		strcpy(vol->name, sv.name);
	    } else
		vol->name = (char *) 0;

	    /*  now do something about the serverhost pointers */

	    for (i = 0; i < MAXHOSTS; i++) {
		if (sv.serverHost[i])
		    vol->serverHost[i] = slot_to_Server[sv.serverHost[i]];
		else
		    vol->serverHost[i] = (struct server *)0;
	    }
	    i = VHash(vol->volume);
	    vol->next = afs_volumes[i];
	    afs_volumes[i] = vol;
	}
	slot_to_Volume[vol->dindex] = vol;

    } while (1);
    osi_UFSClose(tfile);
    ReleaseWriteLock(&afs_xvolume);

}


/* Reads the cell information */

void
afs_ReadCell()
{
    struct save_cell sc;
    struct cell *tc;
    struct osi_file *tfile;
    int i,j=0;
    int len;
    afs_int32 code;

    /* open the backing store for all the cell information */

    tfile = osi_UFSOpen(&cell_data.bs_dev, cell_data.bs_inode);
    if(!tfile)
	panic("afs_ReadCell: failed UFSOpen");

    /* get the write lock to make sure none of the cell information
     * changes on us, and make sure noone else is trying to access this
     * information while we are changing it.
     */

    ObtainWriteLock(&afs_xcell);

    do {
	code = afs_osi_Read(tfile, j*sizeof(struct save_cell), (char *) &sc, sizeof(struct save_cell));
	if (code == 0) 
	    break;
	if (code != sizeof(struct save_cell)) {
	    panic("afs_ReadCell: wrong index");
	}
	if(disconnected_cellindex!= sc.dindex) {
	    panic("afs_ReadCell: wrong index");
	}
	disconnected_cellindex++;
	/*
	 *  Look at already loaded cell list, and see if we can go
	 *  if this is new, or if we add the appropriate info
	 */

	for (tc = afs_cells; tc; tc = tc->next)
	    if (!strcmp(tc->cellName, (char *) &sc.cellName))
		break;


	/* if cell structure doesn't exist, create one */

	if (!tc) {
	    tc = (struct cell *) afs_osi_Alloc(sizeof(struct cell));
	    memset(tc, 0, sizeof(struct cell));

	    /* copy all the easy stuff to cell struct */

	    tc->cell = sc.cell;
	    /* lcellp gets turned back into a pointer in ReconnectPointers() */
	    tc->lcellp = (struct cell *) sc.lcell;
	    tc->fsport = sc.fsport;
	    tc->vlport = sc.vlport;
	    tc->states = sc.states;
	    tc->cellIndex = sc.cellIndex;

	    /* now copy the cell name */

	    len = strlen((char *) &sc.cellName);
	    if (len >= sizeof (sc.cellName))
		panic("afs_ReadCell: cell name too afs_int32");
	    tc->cellName = (char *) afs_osi_Alloc(len+1);
	    strcpy(tc->cellName, sc.cellName);

	    /*  now do something about the cellHosts pointers */

	    for (i=0; i < MAXHOSTS; i++) {
		if (sc.cellHosts[i])
		    tc->cellHosts[i] = slot_to_Server[sc.cellHosts[i]];
		else
		    tc->cellHosts[i] = (struct server *) 0;
	    }
	    tc->next = afs_cells;
	    afs_cells = tc->next;
	}

	/*
	 * we now set the dindex of the cell structure, if it already
	 * existed now we have it.  Also set slot_to_cell, so we know
	 * where to make the new pointers point.
	 */

	tc->dindex = sc.dindex;
	slot_to_Cell[tc->dindex] = tc;
    }
    osi_UFSClose(tfile);
    ReleaseWriteLock(&afs_xcell);
}


/*
 * this goes through the server structure and relinks the pointers
 * which currently are just dindex values and makes the pointers
 * point to the correct values
 */
void
afs_ReconnectPointers()
{
    int i;
    struct server *ts;
    struct cell *tc;

    ObtainWriteLock(&afs_xserver);

    for (i=0; i < NSERVERS; i++) {
	for (ts = afs_servers[i]; ts; ts = ts->next) {
	    if ((((int) ts->cell) < REALLY_BIG) && (((int) ts->cell) > 0))
		ts->cell = slot_to_Cell[(int) ts->cell];
	}
    }
    ReleaseWriteLock(&afs_xserver);

    ObtainWriteLock(&afs_xcell);

    for (tc = afs_cells; tc; tc = tc->next) {
	if (tc->lcellp) {
	    i = (int) tc->lcellp;
	    if (i > 0 && i < REALLY_BIG)
		tc->lcellp = slot_to_Cell[i];
	}
    }
    ReleaseWriteLock(&afs_xcell);
}


/*
 * This gets a plain new dcache for a given vcache entry.  This
 * is neccessary to find new
 */


struct dcache *
get_newDCache(avc)
struct vcache *avc;
{
    afs_int32 i;
    afs_int32 index;
    afs_int32 chunk=0;
    struct dcache *tdc=0;

    i = DCHash(&avc->fid, chunk);
    afs_CheckSize(0);		/* check to make sure our space is fine */
    ObtainWriteLock(&afs_xdcache);
    for(index = afs_dchashTbl[i]; index != NULLIDX;) {
	tdc = afs_GetDSlot(index, (struct dcache *)0);
	if (!FidCmp(&tdc->f.fid, &avc->fid) && chunk == tdc->f.chunk) {
	    ReleaseWriteLock(&afs_xdcache);
	    break;		/* leaving refCount high for caller */
	}
	index = tdc->f.hcNextp;
	lockedPutDCache(tdc);
    }

    /* If we didn't find the entry, we'll create one. */
    if (index == NULLIDX) {
	if (freeDCList == NULLIDX)
	    afs_GetDownD(5, 0);	/* just need slots */
	if (freeDCList == NULLIDX) {
	    uprintf( "panic/getdcache\n" );
	    panic("getdcache");
	}
	afs_indexFlags[freeDCList] &= ~IFFree;
	tdc = afs_GetDSlot(freeDCList, 0);
	freeDCList = tdc->f.hvNextp;
	freeDCCount--;

	/* Fill in the newly-allocated dcache record. */
	tdc->f.fid = avc->fid;
	tdc->f.versionNo = avc->m.DataVersion;
	tdc->f.chunk = chunk;
	/*
	 * Now add to the two hash chains - note that i is still set
	 * from the above DCHash call.
	 */
	tdc->f.hcNextp = afs_dchashTbl[i];
	afs_dchashTbl[i] = tdc->index;
	i = DVHash(&avc->fid);
	tdc->f.hvNextp = afs_dvhashTbl[i];
	afs_dvhashTbl[i] = tdc->index;
	tdc->flags = DFEntryMod;
	tdc->f.states = 0;
	ReleaseWriteLock(&afs_xdcache);
	return tdc;
    } else {
	uprintf("Oops file dcache already exists \n");
	return tdc;
    }
}

/*
 * There is some terminology confusion here:
 *
 * In-memory vcaches:
 *   number is determined by -stat flag to afsd
 *   kept on freeVSList, counted by freeVScount
 *   when we run out, freed up by GetDownVS
 *   afs_FlushVS gets rid of in-memory vcache, but leaves on-disk alone
 *
 * On-disk vcaches:
 *   number is determined by -num_vslots flag to afsd, which sets afs_num_vslots
 *   if it's also in memory, can be looked up in afs_indexVTable[]
 *   can always be found by calling afs_GetVSlot(), which will read it into mem if necessary
 *   when we run out, freed up by GetDownV
 *   afs_FlushVCache() frees both on-disk and in-mem vcache
 *
 * NetBSD:
 *   If a vnode is inactive (on the free list) but not reclaimed (still points
 *   to a vcache) its vcache must be kept in memory, so that when it is reclaimed,
 *   the vcache pointer will be good, so it can be locked and flushed.
 *
 *   This means GetDownV and GetDownVS must refuse to flush a vcache with a
 *   non-null vnode pointer.  This means your in-memory vcache pool (as set by
 *   the -stat flag to afsd) must be at least as big as your vnode pool, or you'll
 *   panic in GetDownVS when you can't flush any vcaches.
 *
 *   Note that we can't just check for non-null vnode in FlushVS, because this gets
 *   called from reclaim() to clean (remove the vcache pointer from) a vnode.
 *
 *   It might be better to try to reclaim the vnode in GetDownVS.
 *   I'll try this when I have some more confidence in this code.
 */

/*
 * afs_FlushVS
 *
 * Flush a vcache from memory, presumably because we've run out or we're
 * shutting down, but keep it in the DVCacheItems file for later use.
 */

int
afs_FlushVS(struct vcache *tvc)
{
    if (
#ifdef AFS_NETBSD_ENV
	(tvc->v && tvc->vrefCount) ||
#else
	tvc->vrefCount ||
#endif
	tvc->lock.readers_reading ||
	tvc->lock.excl_locked ||
	tvc->lock.num_waiting ||
	tvc->opens)

	return EBUSY;

    /* pull the entry off the lruq and put on free list */
    QRemove(&tvc->lruq);

    /* free all access info */
    /* XXX lhuston, may want to do something else */
    afs_FreeAllAxs(&(tvc->Access));

    /* drop callback and remove from callback list */
    /* XXX lhuston we should do this be need to rewrite
     * the cbqueue code to handle for non resident vc entries
     */

    afs_DequeueCallback(tvc);
    tvc->callback = 0;
    tvc->states &= ~(CStatd | CUnique);
    tvc->quick.stamp = 0;
    tvc->dchint = NULL;		/* invalidate hints */
    tvc->callsort.prev = tvc->callsort.next = NULL;
#ifdef AFS_OBSD_ENV
    if (tvc->v) {
	tvc->vflag = VCTOV(tvc)->v_flag;
	tvc->vtype = vType(tvc);
	tvc->v->v_data = 0;	/* remove from vnode */
	tvc->v = NULL;
	lockinit(&tvc->rwlock, PINOD, "vcache", 0, 0);
    }
#endif

    /* write-through modified data */
    tvc->dflags &= ~VC_DIRTY;
    afs_VindexFlags[tvc->index] &= ~VC_DATAMOD;
    afs_SaveVCache(tvc, 1);

    /*
     * free any associated memory that has been allocated
     * to this vcache.  Note this only frees the
     * in core vcache data not the slot info which
     * was already written to disk.
     */

    if (tvc->linkData) {
	osi_FreeSmallSpace(tvc->linkData);
	tvc->linkData = (char *) 0;
	tvc->name_idx = 0;
    }

    /* finally put the entry in the free list */
    afs_indexVTable[tvc->index] = (struct vcache *) 0;

    tvc->index = NULLIDX;
    Lock_Init(&tvc->lock);
    tvc->lruq.next = (struct afs_q *) freeVSList;
    freeVSList = tvc;
    freeVScount++;

    return 0;
}


static void
afs_GetDownVS(anumber)
int anumber;
{
    struct afs_q *tq, *nq;
    struct vcache *tvc;
    int	lock_type = 0;

    /*
     * figure out what type of lock we are holding on afs_xvcache.  If
     * it isn't a write lock, get a write lock.
     */

    if (have_shared_lock(&afs_xvcache)) {
	UpgradeSToWLock(&afs_xvcache);
	lock_type = SHARED_LOCK;
    } else if (!(have_write_lock(&afs_xvcache))) {
	ObtainWriteLock(&afs_xvcache);
	lock_type = WRITE_LOCK;
    }


    /*
     * In case some more slots have been added to the free list before
     * we are run, decrement the count for each one currently on the
     * the free list.
     */

    for (tvc = freeVSList; tvc; tvc = (struct vcache *)tvc->lruq.next)
	anumber--;

    /* if we have enough then exit. */

    if (anumber <= 0)
	goto done;

    /*
     * Scan backward through the LRU list looking for vcaches to flush.
     */

    for (tq = VLRU.prev; tq != &VLRU && anumber > 0; tq = nq) {
	tvc = QTOV(tq);
	nq = QPrev(tq);		/* remember prev in case we remove tq */
#ifdef AFS_NETBSD_ENV
	if (tvc->v)
	    continue;
#endif
	if (!afs_FlushVS(tvc))
	    anumber--;
    }

 done:
    /* put the locks back the way you found them */
    if (lock_type == SHARED_LOCK)
	ConvertWToSLock(&afs_xvcache);
    else if (lock_type == WRITE_LOCK)
	ReleaseWriteLock(&afs_xvcache);

} /* afs_GetDownVS */


/*
 * afs_GetDownV
 *
 * Description:
 *      This is responsible for freeing up slots for different vcaches
 *	to use.
 *
 * Parameters:
 *      anumber    : Number of entries that should ideally be moved.
 *
 */

#define MAXATONCE   8           /* max we can obtain at once */

void
afs_GetDownV(anumber)
    int anumber;
{
    struct vcache *tvc;
    int i;
    afs_int32 j;
    afs_int32 vtime;
    int phase;
    int victims[MAXATONCE];
    afs_int32 victimTimes[MAXATONCE]; /* youngest (largest LRU time) first */
    afs_int32 maxVictim;		/* youngest (largest LRU time) victim */
    int victimPtr;		/* next free item in victim arrays */
    int lock_type = 0;
    int num_freed = 0;

    /* make sure that we have a write lock on afs_xvcache */

    if (have_shared_lock(&afs_xvcache)) {
	UpgradeSToWLock(&afs_xvcache);
	lock_type = SHARED_LOCK;
    } else if (!have_write_lock(&afs_xvcache)) {
	ObtainWriteLock(&afs_xvcache);
	lock_type = WRITE_LOCK;
    }

    /* bounds check parameter */
    if (anumber > MAXATONCE)
	anumber = MAXATONCE;	/* all we can do */

    /*
     * The phase variable manages reclaims.  Set to 0, the first pass,
     * we don't reclaim active entries.  Set to 1, we reclaim even active
     * ones.
     */

    phase = 0;
    /* turn off all VC_FLAGS */
    for (i = 0; i < afs_num_vslots; i++)
	afs_VindexFlags[i] &= ~VC_FLAG;

    while (anumber > 0) {

	/* find oldest entries for reclamation */
	victimPtr = 0;
	maxVictim = 0;

	/* select victims from access time array */
	for (i = 0; i < afs_num_vslots; i++) {

	    /* Check to see if data not yet written, or if
	     * already in free list.
	     */

	    if (afs_VindexFlags[i] & (KEEP_VC | VC_DATAMOD | VC_FREE))
		continue;

	    tvc = afs_indexVTable[i];
	    vtime = afs_indexVTimes[i];

	    /*
	     * If we need data, we may have all non-empty files in
	     * active region.
	     */

	    if (tvc) {
		/* active guy, but may be necessary to use */
#ifdef AFS_NETBSD_ENV
		if (tvc->v) {
		    /*
		     * Can't use it, because the vnode hasn't been reclaimed.  If we
		     * flush it now, reclaim will come aafs_int32 later and find a null
		     * vcache, and panic trying to lock and free it.
		     */
#else
		if (tvc->vrefCount != 0) {
		    /* Referenced; can't use it! */
#endif
		    continue;
		}

		/* try not to use this guy */
		vtime = afs_indexVCounter+1;
	    }

	    /* if this one is in use (open or running) skip it */
	    if (phase == 0 && (afs_VindexFlags[i] & VC_FLAG)) {
		continue;
	    }
	    if (victimPtr < MAXATONCE) {
		/* if there's at least one victim slot left */
		victims[victimPtr] = i;
		victimTimes[victimPtr] = vtime;
		if (vtime > maxVictim) maxVictim = vtime;
		victimPtr++;
	    } else if (vtime < maxVictim) {
		/*
		 * We're older than youngest victim, so we
		 * replace at least one victim
		 */

		/* find youngest (largest LRU) victim */
		for (j = 0; j < victimPtr; j++) {
		    if (victimTimes[j] == maxVictim)
			break;
		}

		/* make sure we found one */
		if (j == victimPtr)
		    panic("getdownv local");

		victims[j] = i;
		victimTimes[j] = vtime;

		/* recompute maxVictim */
		maxVictim = 0;
		for (j = 0; j < victimPtr; j++) {
		    if (maxVictim < victimTimes[j])
			maxVictim = victimTimes[j];
		}
	    }
	} /* big for loop */

	/* now really reclaim the victims */
	for (i = j = 0; i < victimPtr; i++) {
	    tvc = afs_GetVSlot(victims[i], 0);
	    /* No need to check vrefCount, afs_FlushVCache will do that */
	    if (!afs_FlushVCache(tvc)) {
		num_freed++;
		anumber--;
		j++;
	    }
	}

	if (phase == 0) {
	    if (j == 0)
		phase = 1;
	} else {
	    /* found no one in phase 1, we're hosed */
	    if (victimPtr == 0)
		break;
	}
    } /* big while loop */

    if (num_freed == 0)
	panic("afs_GetDownV: none freed");

    /* put the locks back the way they were found */
    if (lock_type == SHARED_LOCK)
	ConvertWToSLock(&afs_xvcache);
    else if (lock_type == WRITE_LOCK)
	ReleaseWriteLock(&afs_xvcache);

} /* afs_GetDownV */


/*
 * afs_ReclaimVCache
 *
 * Reclaim the (already released) vnode and flush the vcache.
 * Currently, vgone calls nbsd_reclaim, which calls afs_FlushVS without
 * acquiring the xvcache lock, so it might be best to call this with the
 * xvcache lock held.
 */

afs_ReclaimVCache(tvc)
struct vcache *tvc;
{
    int code;

#ifdef AFS_NETBSD_ENV
    if (tvc->v) {
	if (tvc->vrefCount || osi_Active(tvc))
	    return EBUSY;
	vgone(tvc->v);
	code = 0;
    } else
	code = afs_FlushVCache(tvc);
#else
    code = afs_FlushVCache(tvc);
#endif
    return code;
}


static void
afs_SaveVolume(vol)
struct volume *vol;
{
    struct save_volume sv;
    struct osi_file *tfile;
    afs_int32 code;
    int i;
    int not_good = 0;

    if (vol->dindex == 0)
	vol->dindex = disconnected_volindex++;

    /* copy all the easy stuff to the savevolume */
    memset(&sv, 0, sizeof sv);
    sv.cell = vol->cell;
    sv.volume = vol->volume;
    sv.dotdot = vol->dotdot;
    sv.mtpoint = vol->mtpoint;
    sv.rootVnode = vol->rootVnode;
    sv.rootUnique = vol->rootUnique;
    sv.roVol = vol->roVol;
    sv.backVol = vol->backVol;
    sv.rwVol = vol->rwVol;
    sv.accessTime = vol->accessTime;
    sv.copyDate = vol->copyDate;
    sv.states = vol->states;
    sv.dindex = vol->dindex;

    /* now copy the volume name */

    if (vol->name) {
	if (strlen(vol->name) >= sizeof(sv.name) - 1)
	    panic("afs_SaveVolume: afs_int32 string");
	strcpy(sv.name, vol->name);
    }

    /*  now do something about the serverhost pointers */

    for (i = 0; i < MAXHOSTS; i++) {
	if (vol->serverHost[i]) {
	    sv.serverHost[i] = vol->serverHost[i]->dindex;
	    if (!sv.serverHost[i]) {
		not_good = 1;
		vol->serverHost[i]->dflags |= SERV_DIRTY;
	    }
	} else
	    sv.serverHost[i] = 0;
    }

    tfile = osi_UFSOpen(&vol_data.bs_dev, vol_data.bs_inode);
    if (!tfile)
	panic("afs_SaveVolume: failed UFSOpen");

    /* if we know that the data we are saving is incomplete, then
     * don't clear the dirty bit
     */
    if(!not_good) {
	vol->dflags &= ~VOL_DIRTY;
    }
    code = afs_osi_Write(tfile, sizeof(struct save_volume) * (vol->dindex-1), (char *)(&sv), sizeof(struct save_volume));
    if (code != sizeof(struct save_volume))
	panic("failed to write volume ");

    osi_UFSClose(tfile);
}


void
afs_SaveCell(cell)
struct cell *cell;
{
    struct save_cell sc;
    struct osi_file *tfile;
    afs_int32 code;
    int i;
    int not_good = 0;

    if(cell->dindex == 0)
	cell->dindex = disconnected_cellindex++;

    /* copy all the easy stuff */
    memset(&sc, 0, sizeof sc);
    sc.cell = cell->cell;
    if (cell->lcellp)
	sc.lcell = cell->lcellp->dindex;
    sc.fsport = cell->fsport;
    sc.vlport = cell->vlport;
    sc.states = cell->states;
    sc.cellIndex = cell->cellIndex;
    sc.dindex = cell->dindex;

    /* now copy the cell name */
    if (cell->cellName) {
	if (strlen(cell->cellName) >= sizeof(sc.cellName) - 1)
	    panic("afs_SaveCell: name too afs_int32");
	strcpy(sc.cellName, cell->cellName);
    }

    /*  now do something about the cellHosts pointers */
    for (i = 0; i < MAXHOSTS; i++) {
	if (cell->cellHosts[i]) {
	    sc.cellHosts[i] = cell->cellHosts[i]->dindex;
	    if (!sc.cellHosts[i]) {
		not_good = 1;
		cell->cellHosts[i]->dflags |= SERV_DIRTY;
	    }
	} else
	    sc.cellHosts[i] = 0;
    }

    tfile = osi_UFSOpen(&cell_data.bs_dev, cell_data.bs_inode);
    if (!tfile)
	panic("failed UFSOpen in SaveCell");

    /*
     * if we know that the data we have saved is not complete, don't
     * clear the dirty bit.
     */
    if(!not_good)
	cell->dflags &= ~CELL_DIRTY;

    code = afs_osi_Write(tfile, sizeof(struct save_cell) * (cell->dindex - 1), (char *)(&sc), sizeof(struct save_cell));
    if (code != sizeof(struct save_cell))
	panic("failed to write cell ");

    osi_UFSClose(tfile);
}



/*
 * This save the server structure out to its backing store on
 * the local disk.  It assigns a slot number if one is not already
 * assigned.
 */
void
afs_SaveServer(serv)
	struct server *serv;


{
    struct save_server ss;
    struct osi_file *tfile;
    afs_int32 code;
    int   not_good = 0;

    if(serv->dindex == 0)
	serv->dindex = disconnected_servindex++;

    /* copy all the easy stuff to the savevolume */
    memset(&ss, 0, sizeof(ss));
    ss.cell = serv->cell->dindex;

    /* if the cell struct has not been properly indexed yet */
    if(!ss.cell) {
	not_good = 1;
	serv->cell->dflags |= CELL_DIRTY;
    }

    ss.host = serv->host;
    ss.portal = serv->portal;
    ss.random = serv->random;
    ss.isDown = serv->isDown;
    ss.dindex = serv->dindex;


    /*  XXX now do something about the vcbs  */


    tfile = osi_UFSOpen(&serv_data.bs_dev, serv_data.bs_inode);
    if(!tfile) panic("failed UFSOpen in SaveServer");

    /*
     * if the cell index has not been set yet, we need to make sure
     * that we will try to save this again later.
     */

    if(!not_good)
	serv->dflags &= ~SERV_DIRTY;

    code = afs_osi_Write(tfile, sizeof(struct save_server) * (serv->dindex-1), (char *)(&ss), sizeof(struct save_server));
    if (code != sizeof(struct save_server))
	panic("failed to write server ");


    osi_UFSClose(tfile);
}


afs_int32 num_saves_tovc=0;

int
afs_SaveVCache(avc, atime)
    struct vcache *avc;
    int atime;

{
    afs_int32 code;

    switch(cacheDiskType) {
    case AFS_FCACHE_TYPE_UFS:
	if (avc->index < 0 || avc->index >= afs_num_vslots)
	    panic("SaveVCache: bogus slot");

	/*
	 * Look to see if link data is associated with this
	 * vache structure and a slot has not been
	 * allocated yet. If it is, then save the data to
	 * disk and mark the structure on an appropriate
	 * way to find it.  Also remember to free up
	 * the storage allocated to the link data.
	 */

	if ((avc->linkData) && (avc->name_idx == 0)) {
	    int len = strlen(avc->linkData)+1;
	    if(len > MAX_NAME)
		panic("afs_int32 string: afs_SaveVCache");
	    avc->name_idx = allocate_name(avc->linkData);
	}

	/* save the reference time */
	avc->ref_time = afs_indexVTimes[avc->index];

#if defined(AFS_NETBSD_ENV)
	if (avc->v) {
	    avc->vflag = avc->v->v_flag;
	    avc->vtype = vType(avc);
	}
#endif

	/*
	 * Seek to the right vcache slot and write the
	 * in-memory image out to disk.
	 */
	code = afs_osi_Write(vc_data.tfile, sizeof(struct vcache) * avc->index + sizeof(struct afs_dheader), (char *) avc, sizeof(struct vcache));
	num_saves_tovc++;

	if (code != sizeof(struct vcache))
	    return EIO;
	break;

    case AFS_FCACHE_TYPE_MEM:
	panic("afs_SaveVCache: Mem cache not supported ");
	break;

    case AFS_FCACHE_TYPE_NFS:
	panic("afs_SaveVCache: NFS cache not supported");
	break;

    case AFS_FCACHE_TYPE_EPI:
	panic("afs_SaveVCache: Episode cache not supported");
	break;

    default:
	panic("afs_SaveVCache: unknown cache file type");
	break;
    }
    return 0;

} /*afs_SaveVCache*/



/*
 * afs_GetVSlot
 *
 * Description:
 *      Return vcache for a given slot in the DVCacheItems file
 *
 * Parameters:
 *      aslot : vcache slot to look at.
 *      tmpvc : optional temp vcache storage (to avoid using a real in-memory vcache)
 *
 * Environment:
 *      Nothing interesting.
 */
/*
 * There are two cases.  Either the vcache is in memory, in which case we just
 * return it, or it isn't, in which case we read it in from the file.  If it's
 * in memory it should have a vnode attached, otherwise it shouldn't.
 *
 * If you pass in a temp vc, you either get the in-memory vcache if there is one,
 * or you get the on-disk one read in to your buffer.  Either way, the lru doesn't
 * get updated, you don't get linkdata, and if you change anything, you must call
 * afs_SaveVCache.
 */

struct vcache *
afs_GetVSlot(aslot, tmpvc)
int aslot;
struct vcache *tmpvc;
{				/*afs_GetVSlot*/
    afs_int32 code;
    struct vcache *tvc;
    int lock_type = 0;
    int j;
#if defined(AFS_MACH_ENV) && !defined(MACH30)
    struct vm_info *vmp;
#endif


    /* ensure that we have a write lock on the xvcache */
    if (have_shared_lock(&afs_xvcache)) {
	UpgradeSToWLock(&afs_xvcache);
	lock_type = SHARED_LOCK;
    } else if (!have_write_lock(&afs_xvcache)) {
	ObtainWriteLock(&afs_xvcache);
	lock_type = WRITE_LOCK;
    }

    /*
     * Sometimes we get here before the vcache info is initialized,
     * if that is the case, then we need to sleep.
     */
    while (vc_data.bs_inode == 0) {
	ReleaseWriteLock(&afs_xvcache);
	osi_Sleep(&vc_data.bs_inode);
	ObtainWriteLock(&afs_xvcache);
    }

    if (aslot < 0 || aslot >= afs_num_vslots)
	panic("GetVSlot: bogus slot");

    tvc = afs_indexVTable[aslot];
    if (tvc) {
	if (!tmpvc) {
	    QRemove(&tvc->lruq);	/* move to queue head */
	    QAdd(&VLRU, &tvc->lruq);
	}
	/* set a flag to help with performance measurements */
	tvc->dflags &= ~READ_DISK;
	goto done;
    }

    /*
     * If we weren't passed an in-memory region to place the file info,
     * we have to allocate one.
     */
    if (tmpvc)
	tvc = tmpvc;
    else {
	if (!freeVSList)
	    afs_GetDownVS(5);
	if (!freeVSList)
	    panic("GetVSlot: null freeVSList");
	tvc = freeVSList;
	freeVSList = (struct vcache *) tvc->lruq.next;
	freeVScount--;
    }

#if defined(AFS_MACH_ENV) && !defined(MACH30)
    /* XXX it's not clear this is the correct thing to do */
    vmp  = VCTOV(tvc)->v_vm_info;
#endif

    /* Seek to the aslot'th entry and read it in.  */

    code = afs_osi_Read(vc_data.tfile, sizeof(struct vcache) * aslot + sizeof(struct afs_dheader), (char *) tvc, sizeof(struct vcache));

    if (code != sizeof(struct vcache)) {
	/*
	 * If the read failed to get enough data, then initialize the vcache
	 * Should only happen on cold cache
	 */
	memset((char *)tvc, 0, sizeof(struct vcache));
	tvc->dhnext = NULLIDX;
	SetAfsVnode(VCTOV(tvc));
	tvc->dflags = VC_DIRTY;
	tvc->truncPos = AFS_NOTRUNC;
    }

    /* Set the debugging flags */
    tvc->dflags |= READ_DISK;

    /* Setup all of the mount info */
    if (tvc->mvid)
	tvc->mvid = &tvc->dmvid;

    /* Setup all of the link information */

    if (!tmpvc && tvc->name_idx != 0) {
	tvc->linkData = (char *) osi_AllocSmallSpace(MAX_NAME);
	code = read_name(tvc->name_idx, tvc->linkData);
	if (code) {
	    /* we couldn't read the name, so punt */
	    osi_FreeSmallSpace(tvc->linkData);
	    tvc->name_idx = 0;
	    tvc->linkData = (char *) 0;
	}
    } else
	tvc->linkData = (char *) 0;

    Lock_Init(&tvc->lock);
    tvc->index = aslot;

    /*
     * we may have saved these earlier on a flush with the vrefCount
     * and open set on.  But we know they can't have been removed from
     * memory with these values non-zero.  Therefore instead of
     * marking the vcache dirty every time we change the refcount etc.
     * we just initialize it to 0.
     */

#ifdef AFS_OBSD_ENV
    tvc->v = NULL;
    lockinit(&tvc->rwlock, PINOD, "vcache", 0, 0);
#else
#ifndef MACH
    /* XXX is this the right thing to do? */
    VCTOV(tvc)->v_text = (struct text *) 0;
#endif
    VCTOV(tvc)->v_socket = (struct socket *) 0;
    tvc->vrefCount = 0;
    vSetVfsp(tvc, afs_globalVFS);
#endif
    tvc->opens = 0;
    tvc->dchint = (struct dcache *) 0;

    /* XXX I think I need to do this, I get panics if I don't ? */
    tvc->states &= ~CCore;
    afs_VindexFlags[aslot] &= ~HAS_CCORE;

    /* Make sure the access pointer is null */
    /*
     * XXX lhuston do something to save some
     * access info across stores and retrieves
     */
    tvc->Access = NULL;
    tvc->callsort.prev = tvc->callsort.next = NULL;

    /*
     * If we didn't read into a temporary vcache region, update the
     * slot pointer table.
     */

    if (!tmpvc) {
	QAdd(&VLRU, &tvc->lruq);
	afs_indexVTable[aslot] = tvc;
	tvc->dflags |= VC_DIRTY;
#if defined(AFS_MACH_ENV) && !defined(MACH30)
	/* If mach, initialize the vm information */
	VCTOV(tvc)->v_vm_info = vmp;
	vm_info_init(&tvc->v);
#endif /* AFS_MACH_ENV */
    }

 done:
    /* put the locks back in their original state */
    if (lock_type == SHARED_LOCK)
	ConvertWToSLock(&afs_xvcache);
    else if (lock_type == WRITE_LOCK)
	ReleaseWriteLock(&afs_xvcache);

    return tvc;

} /* afs_GetVSlot */

void
generateDFID(adp,newfid)
	struct vcache *adp;
	struct VenusFid *newfid;

{
	afs_uint32 cur_unique = 0x0000ffff;

	newfid->Fid.Volume = adp->fid.Fid.Volume;
	newfid->Fid.Vnode = cur_dvnode;
	newfid->Fid.Unique = cur_unique--;
	newfid->Cell = adp->fid.Cell;

	while(fid_in_cache(newfid)) {
		newfid->Fid.Unique = cur_unique;
		cur_unique -= 2;
	}

    	cur_dvnode -= 2;
}

void
store_dirty_vcaches()

{
    afs_int32 code;
    struct osi_stat tstat;
    struct vcache *tvc;
    afs_int32 i;
    struct osi_file *tfile;
    struct afs_dheader theader;
    struct volume *cur_vol;
    struct cell *tc;
    struct server *ts;



    ObtainWriteLock(&afs_xvcache);
    for(i = 0; i < afs_num_vslots; i++) {
	tvc = afs_indexVTable[i];
	if (tvc && (tvc->dflags & VC_DIRTY)) {
	    tvc->dflags &= ~VC_DIRTY;
	    afs_SaveVCache(tvc, 1);
	}
    }
    ReleaseWriteLock(&afs_xvcache);

    /*
     * now ensure all the dslots are written to disk
     */
    afs_WriteThroughDSlots();


    ObtainReadLock(&afs_xserver);
    for(i=0;i<NSERVERS;i++){
	for(ts = afs_servers[i];ts;ts=ts->next){
	    if(ts->dflags & SERV_DIRTY)
		afs_SaveServer(ts);
	}
    }
    ReleaseReadLock(&afs_xserver);


    /*
     * Now look through all the volume structure to see if
     * any of them have not been saved to disk.  If they havn't,
     * then save them
     */

    ObtainReadLock(&afs_xvolume);
    for (i=0; i < NVOLS; i++) {
	for (cur_vol = afs_volumes[i]; cur_vol; cur_vol = cur_vol->next) {
	    if(cur_vol->dflags & VOL_DIRTY)
		afs_SaveVolume(cur_vol);
	}
    }
    ReleaseReadLock(&afs_xvolume);


    ObtainReadLock(&afs_xcell);
    /* now look at all the cell information */
    for (tc = afs_cells; tc; tc = tc->next) {
	if (tc->dflags & CELL_DIRTY)
	    afs_SaveCell(tc);
    }

    ReleaseReadLock(&afs_xcell);

    /* Store header information for name and mount info */

    if (name_info.bs_header->flags & NAME_DIRTY) {
	struct osi_file *tfile;
	int map_offset;

	ObtainWriteLock(&name_info.bs_lock);
	tfile = osi_UFSOpen(&name_info.bs_dev, name_info.bs_inode);
	if(!tfile)
	    panic("failed UFSOpen saving name header");

	map_offset = sizeof(map_header_t) +
	    (name_info.bs_header->num_ents * NAME_UNIT);

	afs_osi_Write(tfile, 0, (char *) name_info.bs_header,
		  sizeof(map_header_t));
	afs_osi_Write(tfile, map_offset, (char *) name_info.bs_map,
		  name_info.bs_header->num_ents);

	name_info.bs_header->flags &= ~NAME_DIRTY;
	osi_UFSClose(tfile);
	ReleaseWriteLock(&name_info.bs_lock);
    }
    if (trans_data.bs_header->flags & NAME_DIRTY) {
	struct osi_file *tfile;
	int map_offset;

	ObtainWriteLock(&trans_data.bs_lock);
	tfile = osi_UFSOpen(&trans_data.bs_dev, trans_data.bs_inode);
	if(!tfile)
	    panic("failed UFSOpen saving trans header ");

	map_offset = sizeof(struct map_header) +
	    (trans_data.bs_header->num_ents *
	     sizeof(struct translations));

	afs_osi_Write(tfile, 0, (char *) trans_data.bs_header,
		  sizeof(struct map_header));
	afs_osi_Write(tfile, map_offset, (char *) trans_data.bs_map,
		  trans_data.bs_header->num_ents);

	trans_data.bs_header->flags &= ~NAME_DIRTY;
	osi_UFSClose(tfile);
	ReleaseWriteLock(&trans_data.bs_lock);
    }
    if (trans_names.bs_header->flags & NAME_DIRTY) {
	struct osi_file *tfile;
	int map_offset;

	ObtainWriteLock(&trans_names.bs_lock);
	tfile = osi_UFSOpen(&trans_names.bs_dev, trans_names.bs_inode);
	if(!tfile)
	    panic("failed UFSOpen saving trans name header ");

	map_offset = sizeof(struct map_header) +
	    (trans_names.bs_header->num_ents *
	     sizeof(struct name_trans));

	afs_osi_Write(tfile, 0, (char *) trans_names.bs_header,
		  sizeof(struct map_header));
	afs_osi_Write(tfile, map_offset, (char *) trans_names.bs_map,
		  trans_names.bs_header->num_ents);

	trans_names.bs_header->flags &= ~NAME_DIRTY;
	osi_UFSClose(tfile);
	ReleaseWriteLock(&trans_names.bs_lock);
    }

    /* Make sure all the directory buffers are flushed.  */

    DFlush();


    /*
     * Make sure the header of the vcachinfo has the
     * correct information about the state of afs_disconnected
     */

    if(dlog_mod) {
	code = afs_osi_Read(vc_data.tfile, 0, (char *) &theader, sizeof(theader));

	theader.current_op = current_op_no;
	theader.mode = discon_state;
	afs_osi_Write(vc_data.tfile, 0, (char *) &theader, sizeof(theader));

	dlog_mod = 0;
    }

} /*store_dirty_vcaches*/



void
generateFFID(adp,newfid)
	struct vcache *adp;
	struct VenusFid *newfid;

{
	afs_uint32 cur_unique = 0x0000ffff;

	newfid->Fid.Volume = adp->fid.Fid.Volume;
	newfid->Fid.Vnode = cur_fvnode;
	newfid->Fid.Unique = cur_unique--;
	newfid->Cell = adp->fid.Cell;

	while(fid_in_cache(newfid))
		newfid->Fid.Unique = cur_unique--;

	cur_fvnode -= 2;
}


/*
 *  this looks to see if a dummy FID that was generated actually
 *  already exists in the cache.  XXX this is probably really
 *  slow since it will read vcaches off the disk in all
 *  probablility.
 */

int
fid_in_cache(afid)
struct VenusFid *afid;
{
    struct vcache *tvc;

    tvc = Lookup_VC(afid);
    return (tvc ? 1 : 0);
}


shutdown_discon()
{
    if (Log_is_open)
	osi_UFSClose(log_data.tfile);
    CloseRLog();
    osi_UFSClose(vc_data.tfile);
}


/*
 * This is the code for saving all the information for the replay
 * when the replay agent decides some information is critical to
 * the user.
 */

int Replay_LogFileInUse = 0;
struct osi_file *RLogtfile;

int
afs_InitReplayLog(afile)
	char *afile;
{
	afs_int32 code;

	code = file_init(afile, &rlog_data, 0);
	return code;
}

void
afs_InitStatFile(afile)
	char *afile;
{
	/* if we do something with this, we need to support cold cache */
}

void
StartRLog()
{
    struct osi_stat astat;
    if (Replay_LogFileInUse)
        osi_UFSClose(RLogtfile);

    RLogtfile = osi_UFSOpen(&rlog_data.bs_dev, rlog_data.bs_inode);
    if (!RLogtfile) panic("open RLogFile");
    afs_osi_Stat(RLogtfile,&astat);
    RLogtfile.offset=astat.size;
    Replay_LogFileInUse = 1;
}

#define LOGBUFSIZE      4096
static char afs_Dbuffer[LOGBUFSIZE], *afs_dcurptr = afs_Dbuffer;
static afs_int32 afs_dcurcnt = 0;
#ifndef LHUSTON
int	use_Dbuffer = 0;
#else
int	use_Dbuffer = 1;
#endif

void
CloseRLog()

{
	int code;

	if (Replay_LogFileInUse) {
		code = afs_osi_Write(RLogtfile, -1, afs_Dbuffer, afs_dcurcnt);
		afs_dcurptr = afs_Dbuffer;
		afs_dcurcnt = 0;
		osi_UFSClose(RLogtfile);
		Replay_LogFileInUse = 0;
	}
}


/* output strings associated with all logging levels */

char	*level_strings[] = {
	"D_ERR   : ",	/* 0 */
	"D_NOTICE: ",	/* 1 */
	"D_INFO  : ",	/* 2 */
	"D_DEBUG : "};	/* 3 */

int
discon_log(level, fmt, va_alist)
	int level;
	char *fmt;
	unsigned va_alist;
{
	char *ap = (char *) &va_alist;
	int len;
	int level_len;
	/* XXX lhuston, big for an automatic */
	char buffer[200];
	char *bp;

	strcpy(buffer, level_strings[level]);
	level_len = strlen(level_strings[level]);

	bp = buffer + level_len;

	/* format the string and put in buffer */
	len = fprf(bp, fmt, (u_int *) ap);

	/* check the return values */
	if (!len) {
		printf("bad len in fprint: bailing out \n");
		return -1;
	}

	len += level_len;

	if (len > 200)
		panic("discon_log: formated string overran buffer");


	if (log_user_level >= level) {
		uprintf("%s", buffer);
	}


	if (log_file_level >= level) {
		if (Replay_LogFileInUse)
			dprint(buffer, len);
	}
	return 0;
}


int
dprint(buf, len)
	char *buf;
	int len;
{
	int code;
	/*
	 * lhuston, should there be a lock around RLogtfile to
	 * ensure sequential writes ?
	 */

	/* If we don't want to use the Dbuffer flush the data now. */
	if (!use_Dbuffer) {
		code = afs_osi_Write(RLogtfile, -1, buf, len);
		if(code)
			return (-1);
	} else {

		/* if there is not enough room on the buffer, put in
		 * flush the buffer
		 */

		if (afs_dcurcnt + len > LOGBUFSIZE) {
			code = afs_osi_Write(RLogtfile, -1, afs_Dbuffer, afs_dcurcnt);
			if (code != afs_dcurcnt) {
				afs_dcurptr = afs_Dbuffer;
				afs_dcurcnt = 0;
				return -1;
			}
			afs_dcurptr = afs_Dbuffer;
			afs_dcurcnt = 0;
		}

		/* put the formated string on the buffer */

		memcpy(afs_dcurptr, buf, len);
		afs_dcurptr += len;
		afs_dcurcnt += len;
	}

	return 0;
}

/*
** This function reads back the log file and tries to recreate
** all the actions the user tried to perform while disconnected
*/


int
parse_log()
{
    struct osi_file *tfile;
    int len;
    int code;
    int failed = 0;
    struct osi_stat astat;
    log_ent_t *in_log;
    char *bp;

    /* close log before we re-open it */
    ObtainWriteLock(&log_data.bs_lock);
    Log_is_open = 0;
    osi_UFSClose(log_data.tfile);

    /* make sure there aren't remaining ops in the backup log file */
    in_log = (log_ent_t *) afs_osi_Alloc(sizeof(log_ent_t));

    /* open the log file */
    tfile = osi_UFSOpen(&log_data.bs_dev, log_data.bs_inode);
    if (!tfile)
	panic("ParseLog: Failed to open Log file");

    /*
     *  read the elements in the Log file and carry out the appropriate
     *  actions based on the elements
     */
    StartRLog();
    discon_log(DISCON_INFO,"\n\nStarting to Replay the log %d.%d \n",
	       time.tv_sec, time.tv_usec);

    /* read each operation and replay it */
    while((len = afs_osi_Read(tfile, -1, (char *) in_log, sizeof (int))) ==
	  sizeof(int)) {
	len = in_log->log_len - sizeof(int);
	bp = (char *) in_log + sizeof(int);

	if (afs_osi_Read(tfile, -1, bp, len) != len) {
	    discon_log(DISCON_ERR,"parse log: bad read \n");
	    failed = 1;
	    break;
	}

	/* check to see if this op is still valid */
	if (in_log->log_flags & LOG_INACTIVE) {
	    continue;
	}
	code = replay_operation(in_log);
    }

    if (failed) {
	osi_UFSClose(tfile);
	/* now re-open the log file */
	tfile = osi_UFSOpen(&log_data.bs_dev, log_data.bs_inode);
	if (!tfile)
	    panic("parse_log: can't reopen log file ");

	/* seek to the end of the file */
	osi_Stat(tfile, &astat);
	tfile.offset=astat.size;

	log_data.tfile = tfile;
	Log_is_open = 1;
	dlog_mod = 1;
    } else {
	osi_Truncate(tfile,(afs_int32) 0);
	osi_UFSClose(tfile);
	un_pin_vcaches();
	un_pin_dcaches();
    }
    dump_replay_stats();
    discon_log(DISCON_INFO,
	       "Done Replaying the log at %d.%d\n", time.tv_sec, time.tv_usec);

    CloseRLog();
    afs_osi_Free((char *) in_log, sizeof(log_ent_t));

    unalloc_trans();
    ReleaseWriteLock(&log_data.bs_lock);
    return failed;
}

int
replay_operation(log)
log_ent_t *log;
{
    int code = 0;
    log_ops_t op;
    struct	timeval	start;
    struct	timeval end;
    int  current_stat;


    /* get the log type */

    op = log->log_op;

    start = time;

    switch(op) {

    case DIS_STORE:
	current_stat = TIME_RECON_STORE;
	code = reconcile_store(log);
	break;

    case DIS_CREATE:
	current_stat = TIME_RECON_CREATE;
	code = reconcile_create(log);
	break;

    case DIS_MKDIR:
	current_stat = TIME_RECON_MKDIR;
	code = reconcile_mkdir(log);
	break;

    case DIS_REMOVE:
	current_stat = TIME_RECON_REMOVE;
	code = reconcile_remove(log);
	break;

    case DIS_RMDIR:
	current_stat = TIME_RECON_RMDIR;
	code = reconcile_rmdir(log);
	break;

    case DIS_RENAME:
	current_stat = TIME_RECON_RENAME;
	code = reconcile_rename(log);
	break;

    case DIS_LINK:
	current_stat = TIME_RECON_LINK;
	code = reconcile_link(log);
	break;

    case DIS_SYMLINK:
	current_stat = TIME_RECON_SYMLINK;
	code = reconcile_symlink(log);
	break;

    case DIS_SETATTR:
	current_stat = TIME_RECON_SETATTR;
	code = reconcile_setattr(log);
	break;

    case DIS_FSYNC:
    case DIS_ACCESS:
    case DIS_READDIR:
    case DIS_READLINK:
    case DIS_INFO:
	current_stat = TIME_RECON_ERROR;
	break;

    default:
	current_stat = TIME_RECON_ERROR;
	discon_log(DISCON_ERR,"INVALID OP: %d\n", op);
	break;
    }
    end = time;
    if (code)
	current_stat = TIME_RECON_ERROR;

    /* if this is generated entry there are no flags to update */
    if (current_stat != TIME_RECON_ERROR && !(log->log_flags & LOG_GENERATED)) {
	/* if successful mark the entry as replayed */
	update_log_ent(log->log_offset,
		       (LOG_INACTIVE | LOG_REPLAYED));
    }

    update_stats(start, end, current_stat);

    return code;
}


/*
 * Go through all of the vcaches and remove the flags that prevent them
 * from being flushed from the cache.
 */
void
un_pin_vcaches()

{
    int i;
    struct vcache *tvc;

    /*
     * we need to un-pin all of the vcaches that we have locked
     * into the cache
     */

    ObtainWriteLock(&afs_xvcache);

    for (i = 0; i < afs_num_vslots; i++) {
	/*
	 * if the vcache is marked as a keeper, this means we haven't
	 * replayed all the local changes we have made, so we must
	 * throw out the local copy.
	 */

	if (afs_VindexFlags[i] & KEEP_VC) {

	    /* clear the index flags array */
	    afs_VindexFlags[i] &= ~KEEP_VC;

	    /* clear the on disk copy */

	    /* can't use a tempvc because we will flush */
	    tvc = afs_GetVSlot(i, (struct vcache *) 0);

	    /* if we don't find it */
	    if (!tvc)
		continue;

	    tvc->dflags &= ~KEEP_VC;

	    /*
	     * any items that were pinned need to be flushed
	     * because we don't know about their current state
	     * The caveat is to make sure we clear
	     * this flag on all items we care about.
	     */

	    afs_ReclaimVCache(tvc);
	}
    }

    ReleaseWriteLock(&afs_xvcache);
}

/*
 * Go through all of the dcaches and remove the flags that prevent them
 * from being flushed from the cache.
 */

void
un_pin_dcaches()
{
    int i;
    struct dcache *tdc;

    ObtainWriteLock(&afs_xdcache);

    /* We need to un-pin all pinned dcaches */
    for (i = 0; i < afs_cacheFiles; i++) {

	if (afs_indexFlags[i] & IFFKeep_DC) {

	    afs_indexFlags[i] &= ~IFFKeep_DC;

	    tdc = afs_GetDSlot(i, (struct dcache *) 0);
	    if (!tdc)
		continue;

	    tdc->f.dflags &= ~KEEP_DC;
	    tdc->f.states &= ~DWriting;
	    tdc->flags &= ~DFEntryMod;

	    /* Flush the dcache */
	    afs_FlushDCache(tdc);
	}
    }
    ReleaseWriteLock(&afs_xdcache);
}


/*
 * This generates a file (or directory) name which is unique in the
 * directory referenced by tdc.  The name is generated by repeatedly
 * appending the extension that is passed in onto the basename.
 *
 * This functions returns a pointer to the newfile name if a new
 * name was generated, otherwise it returns 0.
 */


char *
get_UniqueName(tdc, fname, extension)

	struct dcache *tdc;
	char *fname;
	char *extension;

{
	struct  AFSFid tempfid;
	int flag = 0;
	int cur_len;
	int ext_len;
	char *new_name;

	/* get some string lengths */

	cur_len = strlen(fname);
	ext_len = strlen(extension);

	new_name = afs_osi_Alloc (MAX_NAME);

	/* copy the name to the new storage space */
	strcpy(new_name, fname);

	/* create the new name */

	while ((dir_Lookup(&tdc->f.inode, new_name, &tempfid) )==0) {
		strcat(new_name, extension);
		flag = 1;

		cur_len += ext_len;
		if(cur_len>MAX_NAME) panic("get_UniqueName:name is too big ");
	}

	if (!flag) {
		afs_osi_Free(new_name, MAX_NAME);
		new_name = (char *) 0;
	}

	return new_name;
}

#ifdef LHUSTON
/* XXX lhuston debug */
int num_active_trans = 0;
#endif

/*
 *  This allocates a new translation structure and fills out the
 *  appropriate fields.
 */

struct translations *
allocate_translation(afid)

	struct VenusFid *afid;

{
	struct  translations *trans;
	int i;

	trans = (struct translations *) afs_osi_Alloc(sizeof(struct translations));
	memset(trans, 0, (sizeof(struct translations)));

#ifdef LHUSTON
	/* XXX lhuston debug */
	num_active_trans++;
#endif

   	trans->oldfid = *afid;

	hset64(trans->validDV, -1, -1);
	trans->name_list = (struct name_trans *) 0;

   	/* put it on the appropriate hash list */

	i = VCHash(afid);

   	trans->next = translate_table[i];
   	translate_table[i] = trans;

	ObtainWriteLock(&trans_data.bs_lock);
	trans->trans_idx = allocate_map_ent(&trans_data,
	    sizeof(struct translations));
	ReleaseWriteLock(&trans_data.bs_lock);

	trans->nl_idx = NULLIDX;

	store_translation(trans);

	return (trans);
}

/*
 * we are using this translation and want to reclaim the
 * space that it uses.  The first thing to do is to remove it
 * from the hash lists, then free its data using
 * free_translation().
 */

void
remove_translation(trans)
	struct translations *trans;
{
	struct  translations *ltrans;
	int i;

	i = VCHash(&trans->oldfid);

	/* see if it is the first entry */
	if (translate_table[i] == trans) {
		translate_table[i] = trans->next;

	} else {
		ltrans = translate_table[i];

		while (ltrans->next != NULL) {
			if (ltrans->next == trans)
				break;
			ltrans = ltrans->next;
		}

		/* make sure we found something */
		if (ltrans->next == NULL) {
			printf("Failed to find translation %x \n", trans);
			return;
		}

		/* remove it from the list */
		ltrans->next = trans->next;
		store_translation(ltrans);
	}

	/* free the entry */
	free_translation(trans);

	return;
}

/*
 *  given a fid, this will either return a pointer to a translation
 *  structure that tells what this fid was translated into, or
 *  it will allocate a new structure, to be filled in later
 */

struct translations *
get_translation(afid)
struct VenusFid *afid;
{
    int i;
    struct translations *trans;

    i = VCHash(afid);

    /* see if a translation already exists */

    for (trans = translate_table[i]; trans; trans=trans->next)
	if (!FidCmp(&trans->oldfid, afid))
	    break;

    /* we didn't find one, so allocate a new one */
    if (!trans)
	trans = allocate_translation(afid);

    return trans;
}

void
store_name_translation(ntrans)
struct  name_trans *ntrans;
{
	int slot;
	struct osi_file *tfile;

	slot = ntrans->ntrans_idx;

	/* we have a new slot number, now open the file and write the name */
	ObtainWriteLock(&trans_names.bs_lock);
	tfile = osi_UFSOpen(&trans_names.bs_dev, trans_names.bs_inode);

	if(!tfile)
		panic("failed UFSOpen in store name trans");
        afs_osi_Write(tfile, sizeof(struct name_trans) * slot +  sizeof(struct map_header), (char *) ntrans, sizeof(struct name_trans));
    	osi_UFSClose(tfile);
	ReleaseWriteLock(&trans_names.bs_lock);
}

int
read_ntrans(int slot, struct name_trans *ntrans)
{
    struct osi_file *tfile;
    int code;

    /* make sure this slot is allocated */
    if (trans_names.bs_map[slot] != 1)
	return (-1);

    ObtainWriteLock(&trans_names.bs_lock);
    tfile = osi_UFSOpen(&trans_names.bs_dev, trans_names.bs_inode);
    if(!tfile)
	panic("read_ntrans: UFSOpen failed");

    code = afs_osi_Read(tfile, sizeof(struct name_trans)  * slot + sizeof(struct map_header), (char *) ntrans, sizeof(struct name_trans));

    osi_UFSClose(tfile);
    ReleaseWriteLock(&trans_names.bs_lock);
    if (code != sizeof(struct name_trans)) {
	printf("failed to read enough read_ntrans <%d> \n", code);
	return (-1);
    }

    /* now read in the names associated with the translation */

    ntrans->old_name = (char *) afs_osi_Alloc(MAX_NAME);
    ntrans->new_name = (char *) afs_osi_Alloc(MAX_NAME);

    read_name(ntrans->oname_idx, ntrans->old_name);
    read_name(ntrans->nname_idx, ntrans->new_name);

    return (0);
}

void
store_translation(trans)
struct translations *trans;
{
    int slot;
    struct osi_file *tfile;

    slot = trans->trans_idx;

    /* we have a new slot number, now open the file and write the name */
    ObtainWriteLock(&trans_data.bs_lock);
    tfile = osi_UFSOpen(&trans_data.bs_dev, trans_data.bs_inode);

    if (!tfile)
	panic("store_translation: failed UFSOpen");
    afs_osi_Write(tfile, sizeof(struct translations) * slot + sizeof(struct map_header), (char *) trans, sizeof(struct translations));
    osi_UFSClose(tfile);
    ReleaseWriteLock(&trans_data.bs_lock);
}



/*
 * this frees all the associated memory and persistant data storage
 * associated with a translation struct
 */

void
free_translation(trans)
	struct translations *trans;
{
	struct name_trans *ntrans;
	struct name_trans *next_trans;
	ntrans = trans->name_list;

	while (ntrans != NULL) {
		next_trans = ntrans->next;

		/* free name storage */
		free_name(ntrans->oname_idx);
		free_name(ntrans->nname_idx);
		afs_osi_Free(ntrans->old_name, MAX_NAME);
		afs_osi_Free(ntrans->new_name, MAX_NAME);

		/* free the name trans */
		free_ntrans(ntrans->ntrans_idx);
		afs_osi_Free((char *) ntrans, sizeof(struct name_trans));
		ntrans = next_trans;
	}

	free_ftrans(trans->trans_idx);
	afs_osi_Free((char *) trans, sizeof(struct translations));
#ifdef LHUSTON
	/* XXX lhuston debug */
	num_active_trans--;
#endif

}

void
free_ftrans(slot)
	int	slot;
{
	trans_data.bs_map[slot] = 0;
	trans_data.bs_header->flags |= NAME_DIRTY;
}

void
free_ntrans(slot)
	int	slot;
{
	trans_names.bs_map[slot] = 0;
	trans_names.bs_header->flags |= NAME_DIRTY;
}


int
read_trans(int slot, struct translations *trans)
{
    struct osi_file *tfile;
    struct name_trans *ntrans, *ltrans;
    int code;

    /* make sure this slot is allocated */
    if (trans_data.bs_map[slot] != 1) {
	return (-1);
    }

    ObtainWriteLock(&trans_data.bs_lock);
    tfile = osi_UFSOpen(&trans_data.bs_dev, trans_data.bs_inode);
    if(!tfile)
	panic("failed UFSOpen in read name trans");

    code = afs_osi_Read(tfile, sizeof(struct translations)  * slot + sizeof(struct map_header), (char *) trans, sizeof(struct translations));
    osi_UFSClose(tfile);
    ReleaseWriteLock(&trans_data.bs_lock);
    if (code != sizeof(struct translations)) {
	printf("failed to read enough read_trans <%d> \n", code);
	return (-1);
    }

    /* now we need to read in the name translations */

    if (trans->nl_idx == NULLIDX)  {
	trans->name_list = NULL;
	return (0);
    }

    ntrans = (struct name_trans *) afs_osi_Alloc(sizeof (struct name_trans));
    code = read_ntrans(trans->nl_idx, ntrans);
    trans->name_list = ntrans;
    ltrans = ntrans;

    while (ltrans->next_nt_idx != NULLIDX) {
	ntrans = (struct name_trans *) afs_osi_Alloc(sizeof (struct name_trans));
	code = read_ntrans(ltrans->next_nt_idx, ntrans);
	ntrans->next = NULL;
	ltrans->next = ntrans;
	ltrans = ntrans;
    }

    return (0);
}

/*
 * Free any remaing translation structures, in theory there probably
 * should be none.
 */

void
unalloc_trans()

{
    int i;
    struct translations *current;
    struct translations *next;

    for(i=0;i<VCSIZE;i++) {
	current =  translate_table[i];
	while(current != (struct translations *) 0) {
	    next = current->next;
	    free_translation(current);
	    current = next;
	}
	translate_table[i] = (struct translations *)0;
    }
}

/*
 * this reads the persistent translation structures off
 * the disk and sets them up in memory
 */

void
afs_load_translations()

{
    int t_slot;
    int code;
    int i;
    struct translations *trans;

    for(i=0;i<VCSIZE;i++)
	translate_table[i] = (struct translations *)0;

    for(t_slot = 1; t_slot < trans_data.bs_header->num_ents; t_slot++) {
	if (trans_data.bs_map[t_slot] > 0) {
	    trans = (struct translations *)
		afs_osi_Alloc(sizeof(struct translations));
	    code = read_trans(t_slot, trans);

	    i = VCHash(&trans->oldfid);
	    trans->next = translate_table[i];
	    translate_table[i] = trans;
	}
    }
}


/*
 * This patches up the local copies of the directories to reflect
 * the correct name to fid mappings.
 */

int
fix_dirents(ovc, name, nfid)
struct vcache *ovc;
char *name;
struct VenusFid *nfid;
{
    struct vcache *pvc;
    struct dcache *pdc;
    struct VenusFid pfid;
    char *fname;
    int code;
    struct  translations *trans;

    /* get the parent directory fid, translation, vcache, and dcache */

    pfid.Cell = ovc->fid.Cell;
    pfid.Fid.Volume = ovc->fid.Fid.Volume;
    pfid.Fid.Vnode = ovc->parentVnode;
    pfid.Fid.Unique = ovc->parentUnique;

    trans =  get_translation(&pfid);

    pvc = Lookup_VC(trans->newfid.Fid.Volume ? &trans->newfid : &pfid);

    /* if we didn't get the vcache then fail */

    if (!pvc) {
	discon_log(DISCON_INFO,"fix_dirents: no pvc \n");
	discon_log(DISCON_INFO," <%x %x %x %x> \n",pfid.Cell,
		   pfid.Fid.Volume, pfid.Fid.Vnode, pfid.Fid.Unique);
	return -1;
    }

    pdc = afs_FindDCache(pvc, 0);

    if (!pdc) {
	discon_log(DISCON_INFO,
		   "fix_dirents: failed to get pdc <%x %x %x>\n",
		   pvc->fid.Cell, pvc->fid.Fid.Volume, pvc->fid.Fid.Vnode);
	return -1;
    }

    fname = (char *) afs_osi_Alloc(MAX_NAME);

    /* get the name */
    if (name)
	strcpy(fname, name);
    else {
	/* name isn't known; look it up by fid */
	code = find_file_name(&pdc->f.inode, &ovc->fid, fname);
	if (code) {
	    discon_log(DISCON_INFO, "fix_dirents: find_file_name failed\n");
	    /* name isn't here, hopefully it was removed later */
	    afs_PutDCache(pdc);
	    afs_osi_Free(fname, MAX_NAME);
	    return code;
	}
    }

    /* delete the old name if it exists */
    code = dir_Delete(&pdc->f.inode, fname);

    /* add new name, but only if the old one was already there */
    if (!code) {
	code = dir_Create(&pdc->f.inode, fname, &nfid->Fid);
	DFlush();
	if (code) {
	    discon_log(DISCON_INFO,
		       "fix_dirents: failed to add directory entry: %s\n",fname);
	    afs_PutDCache(pdc);
	    afs_osi_Free(fname, MAX_NAME);
	    return code;
	}
    } else
	code = 0;

    afs_PutDCache(pdc);
    afs_osi_Free(fname, MAX_NAME);

    /* if this entry is a directory we need to make
     * sure .. has the right fid and . points to the new fid
     */

    if (vType(ovc) == VDIR) {
	pdc = afs_FindDCache(ovc, 0);
	if (!pdc) {
	    printf("fix_dirents: couldn't get local vc\n");
	    return -1;
	}

	/* delete the old file name */
	code = dir_Delete(&pdc->f.inode,"..");

	/* add new file name */
	code = dir_Create(&pdc->f.inode, "..", &pvc->fid.Fid);

	/* delete the old file name */
	code = dir_Delete(&pdc->f.inode,".");

	/* add new file name */
	code = dir_Create(&pdc->f.inode, ".", &nfid->Fid);

	DFlush();
	afs_PutDCache(pdc);
    }

    ovc->parentVnode = pvc->fid.Fid.Vnode;
    ovc->parentUnique = pvc->fid.Fid.Unique;
    ovc->dflags |= VC_DIRTY;

    return code;
}


/*
** for now move old dcache entries to new dcache entries by reading
** and copying.  This is a bad, but for now I didn't want to worry
** about moving the items from the hash chains. XXX check above statement
*/


int
move_dcache(ovc, nfid)
struct vcache *ovc;
struct VenusFid *nfid;
{
    struct dcache *tdc;
    int chunk = 0;
    int max_chunk;
    struct dcache *udc;
    afs_int32 i;
    int us;
    struct dcache tmpdc;
    afs_int32 index;

    /* find out how many chunks this file covers. */
    max_chunk = AFS_CHUNK(ovc->m.Length);

    for(chunk = 0;chunk<=max_chunk;chunk++) {

	i = DCHash(&ovc->fid, chunk);

	afs_CheckSize(0);	/* check to make sure our space is fine */
	ObtainWriteLock(&afs_xdcache);

	/* try to find the old dcache entry */

	for(index = afs_dchashTbl[i]; index != NULLIDX;) {
	    tdc = afs_GetDSlot(index, (struct dcache *)0);
	    if (!FidCmp(&tdc->f.fid, &ovc->fid) &&
		chunk == tdc->f.chunk) {
		break;		/* leaving refCount high for caller */
	    }
	    index = tdc->f.hcNextp;
	    lockedPutDCache(tdc);
	}

	if (index == NULLIDX) {
	    ReleaseWriteLock(&afs_xdcache);
	    discon_log(DISCON_INFO,
		       "bailing on movedcache: can't find dcache \n");
	    discon_log(DISCON_INFO,
		       "ofid <%x %x %x %x> nfid <%x %x %x %x>\n",
		       ovc->fid.Cell, ovc->fid.Fid.Volume,
		       ovc->fid.Fid.Vnode, ovc->fid.Fid.Unique,
		       nfid->Cell, nfid->Fid.Volume,
		       nfid->Fid.Vnode, nfid->Fid.Unique);
	    return -1;
	}



    	/** the following is from afs_FlushDCache */


    	/* if this guy is in the hash table, pull him out */


    	/* remove entry from first hash chains */

	i = DCHash(&tdc->f.fid, tdc->f.chunk);
	us = afs_dchashTbl[i];

        if (us == tdc->index) {
       	    /* first dude in the list */
	    afs_dchashTbl[i] = tdc->f.hcNextp;
	}
	else {
	    /* somewhere on the chain */
	    while (us != NULLIDX) {
	    	/*
	     	 * Supply a temporary dcache structure to use in looking
  	     	 * up the next slot in case it's not already in memory -
	     	 * we're here because there's a shortage of them!
	     	 */
	    	udc = afs_GetDSlot(us, &tmpdc);
	    	if (udc->f.hcNextp == tdc->index) {
		    /*item pointing at the one to delete */
		    udc->f.hcNextp = tdc->f.hcNextp;
		    afs_WriteDCache(udc, 1);
		    lockedPutDCache(udc); /* fix refCount*/
		    break;
		}
	    	us = udc->f.hcNextp;
	    	lockedPutDCache(udc);
	    }
	    if (us == NULLIDX) panic("move_dcache ");
	}

        /* remove entry from *other* hash chain */
        i = DVHash(&tdc->f.fid);
        us = afs_dvhashTbl[i];
        if (us == tdc->index) {
            /* first dude in the list */
            afs_dvhashTbl[i] = tdc->f.hvNextp;
        }
        else {
            /* somewhere on the chain */
            while (us != NULLIDX) {
                /*
                 * Same as above: don't ask the slot lookup to grab an
                 * in-memory dcache structure - we can't spare one.
                 */
                udc = afs_GetDSlot(us, &tmpdc);
                if (udc->f.hvNextp == tdc->index) {
                    /* found item pointing at the one to delete */
                    udc->f.hvNextp = tdc->f.hvNextp;
                    afs_WriteDCache(udc, 1);
                    lockedPutDCache(udc); /* fix refCount */
                    break;
                }
                us = udc->f.hvNextp;
                lockedPutDCache(udc);
	    }
            if (us == NULLIDX) panic("dcache hv");
	}


	/* set the new fid  */
     	tdc->f.fid.Fid = nfid->Fid;

        /*  Now add to the two hash chains */
     	i = DCHash(nfid, chunk);
        tdc->f.hcNextp = afs_dchashTbl[i];
        afs_dchashTbl[i] = tdc->index;


        i = DVHash(nfid);
        tdc->f.hvNextp = afs_dvhashTbl[i];
        afs_dvhashTbl[i] = tdc->index;

	/* set the dirty flag, and put dcache */
        tdc->flags |= DFEntryMod;
	lockedPutDCache(tdc);
        ReleaseWriteLock(&afs_xdcache);

    }
    return 0;
}


/*
 * given a vcache pointing to a file, determine a name it is
 * called by.
 */

int
determine_file_name(avc,fname)
struct vcache *avc;
char *fname;
{
    struct vcache *pvc;
    struct dcache *pdc;
    struct VenusFid pfid;
    int code;

    /* set the parent fid */

    pfid.Cell = avc->fid.Cell;
    pfid.Fid.Volume = avc->fid.Fid.Volume;
    pfid.Fid.Vnode = avc->parentVnode;
    pfid.Fid.Unique = avc->parentUnique;

    /* lookup the parent vcache */
    pvc = Lookup_VC(&pfid);
    if(!pvc)
	return -1;

    /* find the parent dcache */
    pdc = afs_FindDCache(pvc, 0);
    if (!pdc)
	return -1;

    /* lookup the name in the parent directory */

    ObtainReadLock(&pvc->lock);
    code = find_file_name(&pdc->f.inode, &avc->fid, fname);
    ReleaseReadLock(&pvc->lock);

    afs_PutDCache(pdc);
    return code;
}


int
find_file_name(afs_int32 *dir, struct VenusFid *fid, char *fname)
{
    afs_int32 cur_blob = 0;
    struct DirEntry *tp;

    while (tp = dir_GetBlob(dir, cur_blob)) {
	if ((ntohl(tp->fid.vnode) == fid->Fid.Vnode) &&
	    (ntohl(tp->fid.vunique) == fid->Fid.Unique)) {
	    /* we found it */
	    strcpy(fname, tp->name);
	    DRelease((struct buffer *) tp, 0);
	    return 0;
	}
	DRelease((struct buffer *) tp, 0);
	cur_blob++;
    }
    return ENOENT;
}


/*
 * For some reason or another we have a cache file containing
 * some data, but we can't perform the replay operations on this
 * file.  We want to make sure that we don't lose this data, so we
 * do the following.  We have a three step process in trying to
 * store the data.
 *
 *	1:  first we try to store the file in a directory called
 *	    orphanage in the root of the file's volume.
 *
 *	2:  If this fails then we try to store the file
 *	    in the directory called orphanage in the user's
 *	    home directory.
 *
 *	3:  As a last resort we copy the data into /tmp
 */

int
save_lost_data(avc,areq)
struct vcache *avc;
struct vrequest *areq;
{
    if(!save_in_VRoot(avc,areq))
   	return 1;
    else {
	/* uprintf("failed saving in VROOT \n"); */
    }

    if(!save_in_homedir(avc))
	return 1;
    if(!save_in_tmp(avc))
	return 1;

    return 0;
}


/*
** 	This tries to save the data associated with the named
** 	vcache into a file in the root of the volume.
*/

int
save_in_VRoot(avc,areq)
struct vcache *avc;
struct vrequest *areq;
{
    struct VenusFid vrootfid;
    struct  vcache *vrootvc;
    int code;
    struct dcache *vrootdc, *orphdc;
    struct vcache *orphvc;

    struct  VenusFid tempfid;
    struct vattr attrs;


    /* Set the fid for the root of the volume  */

    vrootfid.Cell = avc->fid.Cell;
    vrootfid.Fid.Volume = avc->fid.Fid.Volume;
    vrootfid.Fid.Vnode = 1;
    vrootfid.Fid.Unique = 1;


    /* get the vcache for the root of the volume */
    vrootvc = procure_vc(&vrootfid,areq);

    /* if we didn't get it, then we won't be able to, so return a failure */
    if(!vrootvc)
	return -1;


    /*
     ** Find the directory called orphanage in the root of the volume,
     ** or try to create it.
     */

    vrootdc = OB_GetDCache(vrootvc, &vrootfid, 0, areq, 1);

    if(!vrootdc){
	/* uprintf("failed to get vrootdc \n"); */
	return -1;
    }

    /*
     * now we have the current version of the vcache, lets look for
     * the orphanage
     */

    code = dir_Lookup(&vrootdc->f.inode, "orphanage", &tempfid.Fid);

    /* we're done with the dcache for the vroot, put it back */
    Free_OBDcache(vrootdc);

    /* if the orphanage did not exist, then we will try and create one */

    if(code) {

	/* we'll set the attributes the same as our parent */
	code = afs_getattr(vrootvc, &attrs, &osi_cred);

	if(code){
	    /* uprintf("failed to stat  vrootvc: code %d\n",code); */
	    return -1;
	}


	/* create the directory called orphanage */

	/* XXX lhuston, fix this */
	panic("should create orphanage \n");

	if(code) {
	    /* uprintf("failed to create orphanage, code %d \n",code); */
	    return -1;
	}
    }

    /*
     ** otherwise it exists, so we just need to look up the vcache
     ** associated with it
     */

    else {
	tempfid.Cell =  vrootvc->fid.Cell;
	tempfid.Fid.Volume =  vrootvc->fid.Fid.Volume;

	orphvc = procure_vc(&tempfid,areq);

	/* if we don't have it, then we can't get it */
	if(!orphvc) {
	    /* uprintf("failed to get orphvc \n"); */
	    return -1;
	}
    }


    /*
     * at this point we have a vcache for the orphanage, we need to
     * get the associated dcache from the server so that we can
     * look up the files currently in it so we don't overwrite anything
     */

    orphdc = OB_GetDCache(orphvc, &tempfid, 0, areq, 1);

    if(!orphdc) {
	/* uprintf("failed to get orphdc \n"); */
	return -1;
    }

    code = add_to_orphanage(avc,orphvc,orphdc,areq);


    /* put the borrowed dcache back on the free list */

    Free_OBDcache(orphdc);

    return code;
}


/*
**   This takes a file referenced by avc and tries to insert
**   it into the orphanage directory reference by orphvc and
**   orphdc.
*/


int
add_to_orphanage(avc,orphvc,orphdc,areq)
struct vcache *avc;
struct vcache *orphvc;
struct dcache *orphdc;
struct vrequest *areq;
{
    int code;
    char  *fname;
    char *new_name;
    struct  VenusFid pfid;
    struct vcache *pvc;
    struct dcache *pdc;
    struct vattr attrs;

    /*
     * we need to find the parent vcache so that we can
     * look up the name of the file we are trying to write.
     */

    pfid.Cell = avc->fid.Cell;
    pfid.Fid.Volume = avc->fid.Fid.Volume;
    pfid.Fid.Vnode = avc->parentVnode;
    pfid.Fid.Unique = avc->parentUnique;

    pvc = Lookup_VC(&pfid);


    /* if we didn't get the vcache then fail */

    /* XXX maybe we should try asking for it from the server */
    if (!pvc)
	return -1;



    /* we want to get the dcache pointing the version in our cache */


    pdc = afs_FindDCache(pvc, 0);

    if (!pdc) {
	discon_log(DISCON_INFO,
		   "add to orphanage: failed on get dcache \n");
	return -1;
    }

    /* allocate some space for this file name */
    fname = afs_osi_Alloc(MAX_NAME);

    /* lookup the name out of the parent directory */
    ObtainReadLock(&pvc->lock);
    code = find_file_name(&pdc->f.inode, &avc->fid, fname);
    ReleaseReadLock(&pvc->lock);

    /* done with the pdc */
    afs_PutDCache(pdc);

    if (code < 0) {
	discon_log(DISCON_ERR," failed on find_file_name \n");
	afs_osi_Free(fname, MAX_NAME);
	return -1;
    }


    /*
     * now that we know the name of the file, we want to find
     * a unique name to assign to it in the current directory
     */

    if (new_name = get_UniqueName(orphdc,fname,ORPH_EXT)) {
	afs_osi_Free(fname, MAX_NAME);
	fname = new_name;
    }

    code = afs_getattr(avc, &attrs, &osi_cred);

    /* try to create the file */
    /* XXX lhuston fix this */
    panic("should save file in orphanage ");

    if(code) {
	afs_osi_Free(fname, MAX_NAME);
	return -1;
    }

    /* move the old contents of the file to the newly created file */
    code = move_dcache(avc, &avc->fid);

    uprintf("saved file %s in orphanage \n",fname);
    afs_osi_Free(fname, MAX_NAME);
    return 0;
}

struct vcache *procure_vc(afid, areq)
struct VenusFid *afid;
struct vrequest *areq;
{
    afs_int32 code=0;
    struct conn *tc;
    struct vcache *tvc=0;
    struct AFSFetchStatus OutStatus;
    struct AFSCallBack CallBack;
    struct AFSVolSync tsync;
    struct volume *tvp;
    int i;

    ObtainSharedLock(&afs_xvcache);

    /* if the vcache exists, then pass it back */

    tvc = Lookup_VC(afid);

    if(tvc) {
	ReleaseSharedLock(&afs_xvcache);
	return tvc;
    }

    /* we didn't have the appropriate vcache, set one up */

    UpgradeSToWLock(&afs_xvcache);
    tvc = afs_NewVCache(afid, 0, 0, 0);
    ConvertWToSLock(&afs_xvcache);


    /* Make sure Access pointer is clean */

    tvc->Access = NULL;

    ReleaseSharedLock(&afs_xvcache);


    /* copy useful per-volume info */
    /* XXX lhuston, do I need fingrained lock support here? */
    tvp = afs_GetVolume(afid, areq, READ_LOCK);

    if (tvp) {
	if (tvp->states & VRO) tvc->states |= CRO;
	if (tvp->states & VBackup) tvc->states |= CBackup;
	/* now copy ".." entry back out of volume structure, if necessary */
	if (tvc->mvstat == 2  && tvp->dotdot.Fid.Volume != 0) {
	    if (!tvc->mvid)
		tvc->mvid = &tvc->dmvid;
	    *tvc->mvid = tvp->dotdot;
	}
	afs_PutVolume(tvp, READ_LOCK);
    }

    ObtainWriteLock(&tvc->lock);
    code = afs_FetchStatus(tvc, afid, areq, &OutStatus);

    if (code) {
	tvc->states &= ~(CStatd | CUnique);
	ReleaseWriteLock(&tvc->lock);
	ObtainReadLock(&afs_xvcache);
#if defined(AFS_NETBSD_ENV) || defined(AFS_LINUX_ENV)
	AFS_RELE(VCTOV(tvc));
#else
	tvc->vrefCount--;
#endif
	ReleaseReadLock(&afs_xvcache);

	return (struct vcache *) 0;
    }
    tvc->states |= CStatd;
    afs_ProcessFS(tvc, &OutStatus, areq);
    tvc->dflags |= VC_DIRTY;

    ReleaseWriteLock(&tvc->lock);
    return tvc;
}


/* This is called at disconnection time.  It goes through all of the
** the vcaches in the cache and notifies the server that we are removing
** these callbacks.
*/

/*
 * TODO:
 * provide some feedback to the user about the number of callbacks
 * being relinquished.
 *
 * check to see if the callback has expired before giving it up.
 *
 * make sure way more than one callback is given up at a time.  it looks
 * like that is the case.
 *
 * make sure honey understands this.
 */
void
give_up_cbs()

{
    int i, nq = 0;
    struct vcache *tvc;
    struct vcache tempvc;

    if (IS_DISCONNECTED(discon_state))
	return;

    ObtainWriteLock(&afs_xvcache);

    for (i = 0; i < afs_num_vslots; i++) {
	/* if there is no callback for this entry, skip it */
	if (!(afs_VindexFlags[i] & VC_HAVECB))
	    continue;

	tvc = afs_GetVSlot(i, &tempvc);

	if ((tvc->states & CRO) == 0 && tvc->callback) {
	    afs_QueueVCB(&tvc->fid);
	    /* mark the callback as removed */
	    tvc->callback = 0;
	    tvc->states &= ~(CStatd | CUnique);
	    tvc->dflags |= VC_DIRTY;
	    afs_SaveVCache(tvc, 1);
	    /* clear callback flag */
	    afs_VindexFlags[tvc->index] &= ~VC_HAVECB;
	    nq++;
	}
    }

    uprintf("%d callbacks to be discarded.  queued...", nq);
    afs_FlushVCBs(0);

    ReleaseWriteLock(&afs_xvcache);
    uprintf("gone\n");
}



int	save_in_homedir(avc)
struct vcache *avc;
{
    return -1;
}

int	save_in_tmp(avc)
struct vcache *avc;
{
    return -1;
}


/* This takes a given vcache and removes it from all of the hash
** chains
*/

int un_string_vc(avc)
struct vcache *avc;
{
    int i;
    struct vcache *wvc;
    int us;
    struct vcache tempvc;

    /* remove entry from the hash chain */
    i = (int) VCHash(&avc->fid);

    /* remove entry from first hash chains */
    us = afs_vchashTable[i];
    if (us == avc->index) {
	afs_vchashTable[i] = avc->dhnext;
    } else {
	/* somewhere on the chain */
	while (us != NULLIDX) {
	    /*
	     * Supply a temporary dcache structure to use in looking
	     * up the next slot in case it's not already in memory -
	     * we're here because there's a shortage of them!
	     */
	    wvc = afs_GetVSlot(us, &tempvc);
	    if (wvc->dhnext == avc->index) {
		/* found item pointing at the one to delete */
		wvc->dhnext = avc->dhnext;
		/* we modified wvc so we need to save it */
		afs_SaveVCache(wvc, 1);
		avc->dhnext = NULLIDX;
		break;
	    }
	    us = wvc->dhnext;
	}

	if (us == NULLIDX)
	    panic("un_string_vc ");
    }
    return 0;
}



int
string_vc(avc)
	struct vcache *avc;

{
    int i;

    i = VCHash(&avc->fid);
    avc->dhnext = afs_vchashTable[i];
    afs_vchashTable[i] = avc->index;
    avc->dflags |=VC_DIRTY;
    return 0;
}



void
extend_map_storage(bsp, ent_size)
backing_store_t *bsp;
int ent_size;
{
    struct osi_file *tfile;
    char *new_map;
    int old_size;
    int map_offset;

    tfile = osi_UFSOpen(&bsp->bs_dev, bsp->bs_inode);
    if(!tfile)
	panic("extend_map_storage: failed to open file ");

    old_size = bsp->bs_header->num_ents;

    bsp->bs_header->num_ents += MAP_ENTS;

    new_map = (char *) afs_osi_Alloc(bsp->bs_header->num_ents);
    memset((char *) new_map, 0, (bsp->bs_header->num_ents * sizeof(char)));

    /* copy the map into the new, larger map */

    memcpy(new_map, bsp->bs_map, old_size);
    afs_osi_Free(bsp->bs_map, old_size);
    bsp->bs_map = new_map;

    map_offset = sizeof(map_header_t) + (bsp->bs_header->num_ents * ent_size);
    extend_file(tfile, map_offset);

    afs_osi_Write(tfile, 0, (char *) bsp->bs_header, sizeof(map_header_t));
    afs_osi_Write(tfile, map_offset, (char *) bsp->bs_map, bsp->bs_header->num_ents);
    osi_UFSClose(tfile);
}


int
allocate_map_ent(bsp, ent_size)
backing_store_t *bsp;
int ent_size;
{
    int slot;

    /* find an empty slot */

    for (slot = 1; slot < bsp->bs_header->num_ents; slot++) {
	if (bsp->bs_map[slot] == 0)
	    break;
    }

    if (slot >= bsp->bs_header->num_ents) {
	extend_map_storage(bsp, ent_size);
    }

    bsp->bs_map[slot] = 1;
    bsp->bs_header->flags |= NAME_DIRTY;

    return (slot);
}


int
allocate_name(name)
char *name;
{
    int slot;
    struct osi_file *tfile;

    ObtainWriteLock(&name_info.bs_lock);
    slot = allocate_map_ent(&name_info, NAME_UNIT);

    tfile = osi_UFSOpen(&name_info.bs_dev, name_info.bs_inode);
    if (!tfile)
	panic("allocate_name: failed UFSOpen");

    /* write header */
    afs_osi_Write(tfile, 0, (char *) name_info.bs_header,
	      sizeof(map_header_t));

    /* write name */
    afs_osi_Write(tfile, NAME_UNIT * slot + sizeof(map_header_t), name, NAME_UNIT);

    /* write map */
    afs_osi_Write(tfile, sizeof(map_header_t) + (name_info.bs_header->num_ents * NAME_UNIT, (char *) name_info.bs_map, name_info.bs_header->num_ents);

    name_info.bs_header->flags &= ~NAME_DIRTY;
    osi_UFSClose(tfile);
    ReleaseWriteLock(&name_info.bs_lock);
    return (slot);
}

/*
 * read a name map entry into memory,  the paramaters are the
 * slot number to read, and a chunk of memory to store the name
 * into.
 */

int
read_name(int slot, char *name)
{
    struct osi_file *tfile;
    int code;

    ObtainWriteLock(&name_info.bs_lock);

    if (slot < 1 || slot >= name_info.bs_header->num_ents) {
	ReleaseWriteLock(&name_info.bs_lock);
	printf("read_name: slot %d out of range\n", slot);
	return (-1);
    }

    /* make sure this slot is allocated */
    if (name_info.bs_map[slot] != 1) {
	ReleaseWriteLock(&name_info.bs_lock);
	return (-1);
    }

    tfile = osi_UFSOpen(&name_info.bs_dev, name_info.bs_inode);
    if (!tfile)
	panic("read_name: failed UFSOpen");
    code = afs_osi_Read(tfile, NAME_UNIT * slot + sizeof(map_header_t), name, NAME_UNIT);
    osi_UFSClose(tfile);
    ReleaseWriteLock(&name_info.bs_lock);

    if (code != NAME_UNIT) {
	printf("read_name: short read %d\n", code);
	return (-1);
    } else
	return (0);
}

/* mark a name map entry as free to use */

void
free_name(slot)
int	slot;
{
    name_info.bs_map[slot] = 0;
    name_info.bs_header->flags |= NAME_DIRTY;
}


/*
 * This functions returns a vcache entries for the
 * file designated by the fid afid if it is in the
 * cache.  If the vcache is not cached, it will
 * return null.
 *
 */

struct vcache *
Lookup_VC(afid)
    struct VenusFid *afid;
{
    afs_int32 i;
    int index;
    struct vcache *tvc;
    struct vcache *prev = 0;
    int	lock_type = 0;

    /* ensure that we have a write lock on the xvcache */

    /*
     * XXX lhuston if we are called with read lock on xvcache we
     * are screwed, but we would also be screwed in afs_GetVSlot
     */

    if (have_shared_lock(&afs_xvcache)) {
	UpgradeSToWLock(&afs_xvcache);
	lock_type = SHARED_LOCK;
    } else if(!have_write_lock(&afs_xvcache)) {
	ObtainWriteLock(&afs_xvcache);
	lock_type = WRITE_LOCK;
    }

    i = VCHash(afid);

    for (index = afs_vchashTable[i]; index != NULLIDX; index = tvc->dhnext) {
	tvc = afs_GetVSlot(index, (struct vcache *) 0);
	if (!FidCmp(&(tvc->fid), afid))
	    break;
	prev = tvc;
    }

    /* If we didn't find the entry return null */
    if (index == NULLIDX) {
	tvc = 0;
	goto done;
    }

    /* if this vcache is marked bad, flush it and return null */

    /* XXX lhuston do we always want to do this? */
    if (tvc->dflags & DBAD_VC) {
	/* if we successfully flushed it, otherwise we continue using */
	if (!afs_ReclaimVCache(tvc)) {
	    tvc = 0;
	    goto done;
	}
    }

    /*
     * if we found a vcache, move it to the front of the chain,
     * ie, implement move to front.  I have measured the results
     * and it makes a big difference.  -lhuston
     */

    if (prev) {
	prev->dhnext = tvc->dhnext;
	tvc->dhnext = afs_vchashTable[i];
	afs_vchashTable[i]= tvc->index;
	tvc->dflags |= VC_DIRTY;
	prev->dflags |= VC_DIRTY;
    }

#ifdef AFS_OBSD_ENV
    if (tvc->v == 0) {
	/* This happens if this vcache was read from DVCacheItems file */
	afs_nbsd_getnewvnode(tvc);	/* includes one refcount */
	lockinit(&tvc->rwlock, PINOD, "vcache", 0, 0);
	VCTOV(tvc)->v_socket = (struct socket *) 0;
	VCTOV(tvc)->v_flag = tvc->vflag & VROOT;
	vSetType(tvc, tvc->vtype);
	SetAfsVnode(VCTOV(tvc));
	vSetVfsp(tvc, afs_globalVFS);
	AFS_RELE(VCTOV(tvc));
    }
#endif

 done:
    if (lock_type == SHARED_LOCK)
	ConvertWToSLock(&afs_xvcache);
    else if (lock_type == WRITE_LOCK)
	ReleaseWriteLock(&afs_xvcache);

    return tvc;
}


/*
 * these are the locking primitives, i've turned them into functions to
 * reduce the code size instead of having them inlined everywhere
 * in the code
 */

void
rel_readlock(lock)
	struct afs_lock *lock;

{
	save_info(lock,REL_READ);
	if (!--(lock)->readers_reading && (lock)->wait_states)
		Lock_ReleaseW(lock);
}

void
ob_readlock(lock)
	struct afs_lock *lock;

{
        save_info(lock, GET_READ);
        if (!(lock->excl_locked & WRITE_LOCK)){
		lock->readers_reading++;
	} else
            Lock_Obtain(lock, READ_LOCK);
}

void
ob_writelock(lock)
	struct afs_lock *lock;

{

        save_info(lock,GET_WRITE);
        if (!lock->excl_locked && !lock->readers_reading) {
		lock->excl_locked = WRITE_LOCK;
		lock->proc = osi_curpid();
		lock->num_xlocked++;
	} else
		Lock_Obtain(lock, WRITE_LOCK);
}


void
ob_sharedlock(lock)
	struct afs_lock *lock;

{

        save_info(lock,GET_SHARED);
        if (!lock->excl_locked) {
		lock->excl_locked = SHARED_LOCK;
		lock->proc = osi_curpid();
		lock->num_xlocked++;
	} else
		Lock_Obtain(lock, SHARED_LOCK);
}


void
up_stowlock(lock)
	struct afs_lock *lock;

{
        save_info(lock,S_TO_W);
#ifdef LHUSTON
	if (lock->num_xlocked>1) {
		printf("stow num xl %d xlocked %x \n", lock->num_xlocked,
		    lock->excl_locked);
	}
#endif

        if (lock->excl_locked != SHARED_LOCK || lock->proc != osi_curpid())
		 panic("bad upgrade on lock");
        if (!lock->readers_reading) {
		lock->excl_locked = WRITE_LOCK;
		lock->proc = osi_curpid();
	} else
            Lock_Obtain(lock, BOOSTED_LOCK);
}


void
conv_wtoslock(lock)
	struct afs_lock *lock;

{
        save_info(lock,W_TO_S);
	if (lock->proc != osi_curpid())
		panic("converWtoSlock");

	if(lock->num_xlocked < 0)
		panic("CWTS bad ");

	lock->excl_locked = SHARED_LOCK;

	if(lock->wait_states)
		Lock_ReleaseR(lock);
}

void
conv_wtorlock(lock)
	struct afs_lock *lock;
{
        save_info(lock,W_TO_R);

	if (lock->num_xlocked != 1)
		panic("converWtoRlock");

	lock->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);
	lock->num_xlocked = 0;
	lock->readers_reading++;

	lock->proc = -1;
	Lock_ReleaseR(lock);
}

void
conv_storlock(lock)
	struct afs_lock *lock;

{
        save_info(lock,S_TO_R);

	if (lock->num_xlocked != 1)
		panic("converStoRlock");

	lock->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);
	lock->num_xlocked = 0;

	lock->readers_reading++;
	lock->proc = -1;
	Lock_ReleaseR(lock);
}

void
rel_writelock(lock)
	struct afs_lock *lock;

{
	save_info(lock,REL_WRITE);

#ifdef LHUSTON
	/* debug checking */

	/* make sure we own the lock */
	if (lock->proc != u.u_procp->p_pid)
		panic("ReleaseWLock");

	/* make sure the lock was write locked */
	if (lock->excl_locked != WRITE_LOCK)
		panic("rel_WLock: not write locked");
#endif
	if (!--(lock->num_xlocked)) {
		lock->proc = -1;
		lock->excl_locked &= ~WRITE_LOCK;
		if (lock->wait_states)
			Lock_ReleaseR(lock);
	}

	if(lock->num_xlocked<0)
		panic("relWLk bad release");
}

void
rel_sharedlock(lock)
	struct afs_lock *lock;

{
        save_info(lock,REL_SHARED);
	if (!--lock->num_xlocked) {
		lock->excl_locked &= ~(SHARED_LOCK | WRITE_LOCK);
		lock->proc = -1;
		if (lock->wait_states)
			Lock_ReleaseR(lock);
	}
	if(lock->num_xlocked < 0)
		panic("relSLk bad release");

}


int
ping_server(server)
	afs_int32 server;
{
	struct vrequest treq;
	struct server *ts;
	struct conn *tc;
	afs_int32 i;
	afs_int32 code = 1;
	struct timeval tv;

	afs_InitReq(&treq, &osi_cred);
	ObtainReadLock(&afs_xserver);


	i = SHash(server);
	for (ts = afs_servers[i]; ts; ts=ts->next) {
		/* see if this is the right host */
		if (ts->host != server) {
			continue;
		}
		/* get a connection */
		if (ts->cell)
			tc = afs_ConnByHost(ts, AFS_FSPORT,
			    ts->cell->cell, &treq, 1, 0);
		else
			continue;

		/* can't do anything without a connection */
		if (!tc)
			continue;

		rx_SetConnDeadTime(tc->id, 3);

		/* try a get time */
                code = RXAFS_GetTime(tc->id, &tv.tv_sec, &tv.tv_usec);

		afs_PutConn(tc, 0);    /* done with it now */
       	}   /* for each server loop */

   	ReleaseReadLock(&afs_xserver);
	return code;

}


/* print out info about the file in question */

void
print_info(struct vcache *avc)
{

	struct dcache *pdc;

	uprintf("fid <%x %x %x %x> \n", avc->fid.Cell, avc->fid.Fid.Volume,
	   avc->fid.Fid.Vnode, avc->fid.Fid.Unique);
	uprintf("dataversion %d  callback %x  cbexpires %d \n",
	   avc->m.DataVersion, avc->callback, avc->cbExpires);
	uprintf("dflags %x  index %x  last_mod_op %d last repl op %d \n",
	   avc->dflags, avc->index, avc->last_mod_op, avc->last_replay_op);
	uprintf("ref count %d \n", avc->vrefCount);

	pdc = afs_FindDCache(avc, 0);
	if (!pdc) {
		uprintf("no pdc \n");
		return;
	}
	uprintf("dcache info \n");
	uprintf("flags %x index %d \n", pdc->flags, pdc->index);
	uprintf("vno %d chunk %d inode %d dflags %x \n", pdc->f.versionNo,
	   pdc->f.chunk, pdc->f.inode, pdc->f.dflags);

	afs_PutDCache(pdc);
}

int
verify_vcache(data, msg, msglenp)
char *data, *msg;
afs_int32 *msglenp;
{
    int index, type, arg, aslot, i, code = 0, oidx;
    struct vrequest treq;
    struct vcache *tvc, tmpvc;
    char *old = 0;
    static char *oname, *bitmap;
    static unsigned short *map;

    index = *((afs_int32 *) data);
    type = data[8];
    arg = data[9];

    if (index == afs_num_vslots) {
	/* End of scan.  Free up some stuff. */
	afs_osi_Free(map, afs_num_vslots * sizeof (unsigned short));
	afs_osi_Free(oname, NAME_UNIT);
	afs_osi_Free(bitmap, name_info.bs_header->num_ents);
	map = 0;
    }

    if (index < 0 || index >= afs_num_vslots)
	return EINVAL;

    ObtainWriteLock(&afs_xvcache);

    if (index == 0) {
	int gap, ig, j, k;

	/* Begin scan.  Sort lru list and do them in that order. */
	if (!map) {
	    map = (unsigned short *) afs_osi_Alloc(afs_num_vslots * sizeof (unsigned short));
	    oname = (char *) afs_osi_Alloc(NAME_UNIT);
	    bitmap = (char *) afs_osi_Alloc(name_info.bs_header->num_ents);
	    memset(bitmap, 0, name_info.bs_header->num_ents);
	}
	for (i = 0; i < afs_num_vslots; i++)
	    map[i] = i;

	/* silly little bubble sort */
	for (gap = afs_num_vslots / 2; gap > 0; gap /= 2) {
	    for (j = gap; j < afs_num_vslots; j++)
		for (i = j - gap; i >= 0; i -= gap) {
		    ig = i + gap;
		    if (afs_indexVTimes[map[i]] <= afs_indexVTimes[map[ig]])
			break;
		    /* exchange */
		    k = map[i];
		    map[i] = map[ig];
		    map[ig] = k;
		}
	}
    }
    if (!map) {
	ReleaseWriteLock(&afs_xvcache);
	return EINVAL;
    }

    aslot = map[index];

    if (afs_VindexFlags[aslot] & VC_FREE) {
	ReleaseWriteLock(&afs_xvcache);
	sprintf(msg, "not in use slot %d", aslot);
	goto done;
    }

    /* First read it in to a temp vc.  This allows us to find out whether it's
       a link without thrashing the in-memory vcaches. */

    tvc = afs_GetVSlot(aslot, &tmpvc);
    ReleaseWriteLock(&afs_xvcache);

    if (!tvc || tvc->fid.Fid.Volume == 0) {
	sprintf(msg, "null slot %d", aslot);
	goto done;
    }

    /* if the type is right, check it */
    if (type == 'a' ||
	type == 'f' && !(tvc->fid.Fid.Vnode & 0x1) ||
	type == 'd' &&  (tvc->fid.Fid.Vnode & 0x1) ||
	type == 's' &&  (tvc->linkData || tvc->name_idx)) {

	/* get vcache into memory with a real vnode attached */
	tvc = Lookup_VC(&tvc->fid);
	if (!tvc) {
	    sprintf(msg, "can't look up vcache?!\n");
	    goto done;
	}

	if (code = afs_InitReq(&treq, osi_curcred()))
	    goto done;

	old = tvc->linkData;
	if (!old) {
	    old = osi_AllocSmallSpace(5);
	    strcpy(old, "NULL");
	}

	if (type == 's') {
	    ObtainWriteLock(&tvc->lock);

	    /*
	     * save old symlink text and index for later corruption detection
	     * old is the old linkData
	     * oidx is the old name_idx
	     * oname is the DNameFile entry for oidx
	     */

	    oidx = tvc->name_idx;

	    /* clear link info */
	    tvc->linkData = (char *) 0;
	    if (oidx) {
		if (read_name(oidx, oname) >= 0) {
		    /* don't free it if it is multiply allocated */
		    if (!bitmap[oidx])
			free_name(oidx);
		} else
		    strcpy(oname, "UNALLOC");
		bitmap[oidx]++;
		tvc->name_idx = 0;
	    } else
		strcpy(oname, "NONE");

	    ReleaseWriteLock(&tvc->lock);
	}

	code = afs_VerifyVCache2(tvc, &treq);

	if (code == ENOENT) {
	    /* It's gone from the server.  Flush, refresh, or keep, depending on arg. */
	    code = 0;
	    if (arg == 'f') {
		ObtainWriteLock(&tvc->lock);
		afs_TryToSmush(tvc, &treq);
		ReleaseWriteLock(&tvc->lock);

		/* ...and the vcache. */
		ObtainWriteLock(&afs_xvcache);
		afs_ReclaimVCache(tvc);
		ReleaseWriteLock(&afs_xvcache);
		sprintf(msg, "flushed stale data slot %d: %s", aslot, old);
	    } else if (arg == 'r') {
		sprintf(msg, "refresh not implemented yet slot %d: %s", aslot, old);
	    } else if (arg == 'k') {
		sprintf(msg, "kept stale data slot %d: %s", aslot, old);
	    }
	    goto done;
	}
	if (code)
	    goto done;

	if (type == 's') {
	    ObtainWriteLock(&tvc->lock);
	    afs_HandleLink(tvc, &treq);
	    if (!tvc->linkData)
		sprintf(msg, "BAD LINK %s slot %d: %d %s (%s)",
			(tvc == &tmpvc) ? "disk" : "mem", aslot, oidx, old, oname);
	    else if (strcmp(old, oname) || strcmp(oname, tvc->linkData))
		sprintf(msg, "WRONG linkdata %s slot %d alloc %d: %d %s (%s) -> %d %s",
			(tvc == &tmpvc) ? "disk" : "mem", aslot, bitmap[oidx], oidx, old, oname, tvc->name_idx, tvc->linkData);
	    else
		sprintf(msg, "ok slot %d: %s", aslot, old);
	    ReleaseWriteLock(&tvc->lock);
	} else
	    sprintf(msg, "ok slot %d type %c", aslot, type);
    } else
	strcpy(msg, "not my type");

 done:
    if (old)
	osi_FreeSmallSpace(old);
    if (!code)
	*msglenp = strlen(msg) + 1;
    return code;
}

#endif
