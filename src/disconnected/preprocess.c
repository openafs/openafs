#ifdef __OpenBSD__
#define __NetBSD__
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
#ifndef __NetBSD__
#include <sys/inode.h>
#endif
#include <netinet/in.h>
#ifdef __NetBSD__
#include "../libafs/afs/lock.h"
#else
#include "lock.h"
#endif
#include "afsint.h" 
#include "stds.h"
/* add the kernel define so we get the fid structure from afs.h */
#define KERNEL

#include "afs.h"

#undef KERNEL

#include "discon.h"
#include "discon_log.h"


/* Forward function declarations */
void 	format_store();
void 	format_create();
void 	format_mkdir();
void 	format_remove();
void 	format_rmdir();
void 	format_rename(log_ent_t *, int *);
void 	format_link();
void 	format_symlink();
void 	format_setattr();
void	print_entries();
void	sort_entries();
int	comp_ents();

char *malloc();

#define MAX_NAME  255
#define CELL_DIRTY   0x01

#define REALLY_BIG 1024


#define DEFAULT_NAME  "/var/log/DisconnectedLog"
extern char    *optarg;

int current_index_name = 100000;
int current_index_file = 10000;
int current_index_dir = 1;

typedef struct name_info {
	struct VenusFid ni_pfid;
	char 		ni_name[MAX_NAME];
	int		ni_index;
	struct name_info *next;
} name_info_t;
	
typedef struct fid_info {
    	struct VenusFid fid;
    	int index;
	int	fi_linked;
	int	fi_new;
    	struct fid_info *next;
} fid_info_t;

typedef struct sort_data {
        log_ops_t op;
	int	index;
	int	op_no;
	char	data[MAX_NAME];
} sort_data_t;

#define	MAX_SORTS 8000
sort_data_t	sorted_data[MAX_SORTS];
int	current_data = 0;


fid_info_t  *afs_vchashTable[VCSIZE];

name_info_t *name_hashT[VCSIZE];

/* returns 1 if this is a file fid, 0 if it is a dir fid.  */

int
is_file(afid)
	struct VenusFid *afid;
{
	int ret;
	u_long mask = 1;
	
	ret = (int) ( afid->Fid.Vnode & mask);

	return (ret);
}

void
dump_names()
{
	int i;
	name_info_t *current;


	for (i=0; i<VCSIZE; i++) {
		for(current = name_hashT[i]; current != (name_info_t *) 0;
		    current=current->next) {
			printf("%d <%s>  \n", current->ni_index,
			    current->ni_name);
		}
	}

}

struct name_info *
lookup_name(pfid, name)
	struct VenusFid *pfid;
	char *name;
{
	int i;
	name_info_t *current;
 
	i = VCHash(pfid);
	for(current = name_hashT[i]; current != (name_info_t *) 0;
	    current=current->next) {
		if (current->ni_pfid.Fid.Unique == pfid->Fid.Unique &&
		    current->ni_pfid.Fid.Volume == pfid->Fid.Volume &&
		    current->ni_pfid.Cell == pfid->Cell &&
		    current->ni_pfid.Fid.Vnode == pfid->Fid.Vnode){
			if(strcmp(name, current->ni_name)) {
				continue;	
			} else {
				break;
			}
		}
	}

	if(current)
		return current;


	/* create a new hash value */

	current = (name_info_t *) malloc(sizeof(name_info_t));
	current->next = name_hashT[i];
	name_hashT[i] = current;
	current->ni_index = current_index_name++;
	current->ni_pfid = *pfid;
	strcpy(current->ni_name, name);
	return current;
}


fid_info_t *
lookup_fid(afid)
	struct VenusFid *afid;
{
	int i;
	fid_info_t *current;
 
	i = VCHash(afid);
	for(current = afs_vchashTable[i];current != (fid_info_t *) 0;
	    current=current->next) {
		if (current->fid.Fid.Unique == afid->Fid.Unique &&
		    current->fid.Fid.Volume == afid->Fid.Volume &&
		    current->fid.Cell == afid->Cell &&
		    current->fid.Fid.Vnode == afid->Fid.Vnode){
			break;
		}
	}

	if(current) {
		current->fi_new = 0;
		return current;
	}


	/* create a new hash value */

	current = (fid_info_t *) malloc(sizeof(fid_info_t));
	current->next = afs_vchashTable[i];
	afs_vchashTable[i] = current;
	if (is_file(afid)) {
		current->index = current_index_file++;
	} else {
		current->index = current_index_dir++;
	}
	current->fi_linked = 0;
	current->fi_new = 1;

	current->fid = *afid;
	return current;

}




main(argc, argv)
	int argc;
	char **argv;
{
	int cc;
	FILE *logfp;
	log_ent_t *cur_ent;
        char *bp;
        int len;
        log_ops_t op;

	char *fname = DEFAULT_NAME;
	int c;
	int vflag = 0;
	int i;
	int current_op = 0;
 	for(i=0;i<VCSIZE;i++) {
		afs_vchashTable[i] = (fid_info_t *) 0;
	}

	/* parse the arguments */

	while ((c = getopt(argc, argv, "?hvf:")) != EOF) {
		switch (c) {
		case 'v':
			vflag++;
			break;


		case 'f':
			fname = optarg;
			break;


		case 'h': case '?':
			printf("Options are: \n");
			printf("\t -f use a non-default file name \n");
			printf("\t -v include read-only ops\n");
			printf("\t -h this help information \n");
			printf("\t -? this help information \n");

		default:
			exit(1);
		}
	}


	/* open the file */
        if ((logfp = fopen(fname, "r")) == 0) {
                perror("failed to open log ");
                exit(-1);
        }

	cur_ent = (log_ent_t *) malloc(sizeof(log_ent_t));

        while(fread(cur_ent, sizeof (int), 1, logfp)) {
               /* check the bounds on the data array */
		if (current_data > MAX_SORTS) {
			printf("MAX_SORTS too small, recompile \n");
			exit (-1);
		}

                len = cur_ent->log_len - sizeof(int);

                bp = (char *) cur_ent + sizeof(int);
                if (!fread(bp, len, 1, logfp)) {
                        printf("parse log: bad read \n");
                        break;
                }

		current_op++;


		switch (cur_ent->log_op) {
			case DIS_STORE:
				format_store(cur_ent, current_op);
				break; 

			case DIS_CREATE:
				format_create(cur_ent, current_op);
				break;

			case DIS_MKDIR:
				format_mkdir(cur_ent, current_op);
				break;
	
			case DIS_REMOVE:
				format_remove(cur_ent, current_op);
				break;
	
			case DIS_RMDIR:
				format_rmdir(cur_ent, current_op);
				break;
	
			case DIS_RENAME:
				format_rename(cur_ent, &current_op);
				break;
	
			case DIS_LINK:
				format_link(cur_ent, current_op);
				break;

			case DIS_SYMLINK:
				format_symlink(cur_ent, current_op);
				break;

			case DIS_SETATTR:
				format_setattr(cur_ent, current_op);
				break;

			case DIS_READDIR:
			case DIS_FSYNC:
			case DIS_READLINK:
				break;

			default:
				break;
	    	}


	}
	fclose(logfp);
	sort_entries();
	print_entries();
	if (vflag)
		dump_names();

	exit(0);
}

void
sort_entries()
{
	qsort((char *) sorted_data, current_data, sizeof(struct sort_data),
		comp_ents);
}

int
comp_ents(ent1, ent2)
	struct sort_data *ent1;
	struct sort_data *ent2;
{

	if ((ent1->op == DIS_CREATE) || (ent1->op == DIS_LINK))
		return (1);

	if ((ent1->op == DIS_MKDIR) || (ent1->op == DIS_RMDIR))
		return (1);

	if ((ent1->op == DIS_RENAME) || (ent1->op == DIS_SYMLINK))
		return (1);
	
	if ((ent2->op == DIS_CREATE) || (ent2->op == DIS_LINK))
		return (-1);

	if ((ent2->op == DIS_MKDIR) || (ent2->op == DIS_RMDIR))
		return (-1);

	if ((ent2->op == DIS_RENAME) || (ent2->op == DIS_SYMLINK))
		return (-1);


	if (ent1->index < ent2->index) {
		return (-1);
	} else if ( ent1->index == ent2->index) {
		if (ent1->op_no < ent2->op_no)
			return (-1);
		else
			return(1);
	} else {
		return (1);
	} 

}

void
print_entries()
{
	int i;
	
	for(i=0; i<current_data; i++) {
		printf("%s", sorted_data[i].data);
	}
}


/* 
 * Format for store operation:
 * 	store fid1 linked op
 */

void
format_store(log_ent, op)
	log_ent_t *log_ent;
	int op;

{
	fid_info_t *svc;
	svc = lookup_fid(&log_ent->st_fid);

	/*
	 * if this is the first ref to this fid, set linked bit because
	 * it may have a link count > 1.  
	 */
	if (svc->fi_new)
		svc->fi_linked = 1;

	sorted_data[current_data].index = svc->index;
	sorted_data[current_data].op = log_ent->log_op;
	sorted_data[current_data].op_no = op;
	sprintf(sorted_data[current_data].data, "%s %d %d %d\n", "store", 
	    svc->index, svc->fi_linked,  op);
	current_data++;
}


/* 
 * Format for create operation:
 * 	create parentfid filefid name operation
 */

void
format_create(log_ent, op)
	log_ent_t *log_ent;
	int op;

{
	fid_info_t *fvc;
	fid_info_t *pvc;
	name_info_t	*name;

	fvc = lookup_fid(&log_ent->cr_filefid);
	pvc = lookup_fid(&log_ent->cr_parentfid);
	name = lookup_name(&log_ent->cr_parentfid, log_ent->cr_name);

	sorted_data[current_data].index = fvc->index;
	sorted_data[current_data].op = log_ent->log_op;
	sorted_data[current_data].op_no = op;
	sprintf(sorted_data[current_data].data,"%s %d %d %d %d\n", 
	    "create", pvc->index, fvc->index, name->ni_index,op);
	current_data++;

}


/* 
 * Format for mkdir operation:
 * 	mkdir parentfid dirfid dirname operation
 */

void
format_mkdir(log_ent, op)

	log_ent_t *log_ent;
	int op;

{
	fid_info_t *svc;
	fid_info_t *pvc;
	name_info_t	*name;

	svc = lookup_fid(&log_ent->md_dirfid);
	pvc = lookup_fid(&log_ent->md_parentfid);
	name = lookup_name(&log_ent->md_parentfid, log_ent->md_name);

	sorted_data[current_data].index = svc->index;
	sorted_data[current_data].op = log_ent->log_op;
	sorted_data[current_data].op_no = op;
	sprintf(sorted_data[current_data].data,"%s %d %d %d %d\n", 
	    "create", pvc->index, svc->index, name->ni_index,op);
	current_data++;
}


/* 
 * Format for remove operation:
 * 	remove parentfid filefid filename linked op
 */

void
format_remove(log_ent, op)

	log_ent_t *log_ent;
	int op;

{
	fid_info_t *svc;
	fid_info_t *pvc;
	name_info_t	*name;

	svc = lookup_fid(&log_ent->rm_filefid);
	pvc = lookup_fid(&log_ent->rm_parentfid);
	name = lookup_name(&log_ent->rm_parentfid, log_ent->rm_name);

	sorted_data[current_data].index = svc->index;
	sorted_data[current_data].op = log_ent->log_op;
	sorted_data[current_data].op_no = op;
	sprintf(sorted_data[current_data].data,"%s %d %d %d %d %d\n", 
	    "remove", pvc->index, svc->index, name->ni_index, svc->fi_linked, op);
	current_data++;
}


/* 
 * Format for rmdir operation:
 * 	rmdir parentfid dirfid dirname operation
 */

void
format_rmdir(log_ent, op)
	log_ent_t *log_ent;
	int op;

{
	fid_info_t *svc;
	fid_info_t *pvc;
	name_info_t *name;

	svc = lookup_fid(&log_ent->rd_dirfid);
	pvc = lookup_fid(&log_ent->rd_parentfid);
	name = lookup_name(&log_ent->rd_parentfid, log_ent->rd_name);

	sorted_data[current_data].index = svc->index;
	sorted_data[current_data].op = log_ent->log_op;
	sorted_data[current_data].op_no = op;
	sprintf(sorted_data[current_data].data,"%s %d %d %d %d\n", 
	    "remove", pvc->index, svc->index, name->ni_index,op);
	current_data++;
}



/* 
 * Format for rename operation:
 * 	rename oparentfid oname nparentfid nname destfid overfid op
 */

void
format_rename(log_ent, op)
	log_ent_t *log_ent;
	int *op;


{
	fid_info_t *spvc;
	fid_info_t *dpvc;
	fid_info_t *destvc;
	fid_info_t *overvc;

	name_info_t *oldname;
	name_info_t *newname;
	char *on;
	char *nn;

	on = log_ent->rn_names;
	nn = on + strlen(on) +1;

	spvc = lookup_fid(&log_ent->rn_oparentfid);
	dpvc = lookup_fid(&log_ent->rn_nparentfid);
	destvc = lookup_fid(&log_ent->rn_renamefid);

	oldname = lookup_name(&log_ent->rn_oparentfid, on);
	newname = lookup_name(&log_ent->rn_nparentfid, nn);

	/* write a remove record if overwritten */
	if (log_ent->rn_overfid.Fid.Volume != 0) {
		overvc = lookup_fid(&log_ent->rn_overfid);

		sorted_data[current_data].index = overvc->index;
		sorted_data[current_data].op = DIS_REMOVE;
		sorted_data[current_data].op_no = *op;
		sprintf(sorted_data[current_data].data, "%s %d %d %d %d\n",
		    "remove", dpvc->index, overvc->index,
		     newname->ni_index, *op);
		current_data++;
		/* we increment the op_no when adding a remove op */
		(*op)++;

	}	

	sorted_data[current_data].index = dpvc->index;
	sorted_data[current_data].op = log_ent->log_op;
	sorted_data[current_data].op_no = *op;
	sprintf(sorted_data[current_data].data, "%s %d %d %d %d %d 0 %d\n",
	    "rename", spvc->index, oldname->ni_index, dpvc->index, 
	    newname->ni_index, destvc->index, *op);
	current_data++;
}


/* 
 * Format for link operation:
 * 	link parentfid linkfid linkname operation
 */

void
format_link(log_ent, op)

	log_ent_t *log_ent;
	int op;

{
	fid_info_t	*lvc;
	fid_info_t	*pvc;
	name_info_t	*name;

	pvc = lookup_fid(&log_ent->ln_parentfid);
	lvc = lookup_fid(&log_ent->ln_linkfid);

	lvc->fi_linked = 1;

	name = lookup_name(&log_ent->ln_parentfid, log_ent->ln_name);

	sorted_data[current_data].index = lvc->index;
	sorted_data[current_data].op = log_ent->log_op;
	sorted_data[current_data].op_no = op;
	sprintf(sorted_data[current_data].data, "%s %d %d %d %d\n", "link", 
	    pvc->index, lvc->index, name->ni_index, op);
	current_data++;


}



/* 
 * Format for symlink operation:
 * 	symlink parentfid linkfid linkname operation
 */


void
format_symlink(log_ent, op)

	log_ent_t *log_ent;
	int op;

{
	fid_info_t *lvc;
	fid_info_t *pvc;
	name_info_t	*name;

	pvc = lookup_fid(&log_ent->sy_parentfid);
	lvc = lookup_fid(&log_ent->sy_linkfid);
	
	name = lookup_name(&log_ent->sy_parentfid, log_ent->sy_names);

	sorted_data[current_data].index = lvc->index;
	sorted_data[current_data].op = log_ent->log_op;
	sorted_data[current_data].op_no = op;
	sprintf(sorted_data[current_data].data, "%s %d %d %d %d\n", "create", 
	    pvc->index, lvc->index, name->ni_index, op);
	current_data++;
}

/* 
 * Format for setattr operation:
 * 	setattr filefid operation
 */

void
format_setattr(log_ent, op)

	log_ent_t *log_ent;
	int op;
{
	int	printed = 0;
	fid_info_t *pvc;

	pvc = lookup_fid(&log_ent->sa_fid);

	/*
	 * if this is the first ref to this fid, set linked bit because
	 * it may have a link count > 1.  
	 */
	if (pvc->fi_new)
		pvc->fi_linked = 1;


	if (log_ent->sa_vattr.va_size != VNOVAL) {
		sorted_data[current_data].index = pvc->index;
		sorted_data[current_data].op = log_ent->log_op;
		sorted_data[current_data].op_no = op;
		sprintf(sorted_data[current_data].data, "%s %d %d %d %d\n",
		     "set_len", pvc->index, (int) log_ent->sa_vattr.va_size, 
		     pvc->fi_linked, op);
		current_data++;

		printed = 1;
	}

	if ((log_ent->sa_vattr.va_mode != 0xffff) || 
	    (log_ent->sa_vattr.va_gid != -1) || 
	    (log_ent->sa_vattr.va_uid != -1)) {

		sorted_data[current_data].index = pvc->index;
		sorted_data[current_data].op = log_ent->log_op;
		sorted_data[current_data].op_no = op;
		sprintf(sorted_data[current_data].data, "%s %d %d %d\n",
		     "set_mode", pvc->index, pvc->fi_linked, op);
		current_data++;

		printed = 1;
	}

	if (!printed) {
		sorted_data[current_data].index = pvc->index;
		sorted_data[current_data].op = log_ent->log_op;
		sorted_data[current_data].op_no = op;
		sprintf(sorted_data[current_data].data, "%s %d %d %d\n",
		     "set_time", pvc->index, pvc->fi_linked, op);
		current_data++;
	}
}


