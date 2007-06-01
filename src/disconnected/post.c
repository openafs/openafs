/* This is the post processor for the log optimizer.  */

#ifdef __OpenBSD__
#define __NetBSD__
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
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
#include "post.h"

#define MAX_NAME  255
#define CELL_DIRTY   0x01


/* forward function declarations */
void read_store();
void read_create();
void read_mkdir();
void read_remove();
void read_rmdir();
void read_rename();
void read_link();
void read_set_len();
void read_set_mode();
void read_set_time();
void read_input();
void dump_ents();
void add_ent();
int  comp_ents();
void sort_ents();
void rewrite_log();
void read_log();
log_ent_t *join_ops();

log_ent_t *join_setattrs();
log_ent_t *join_create_rename();
log_ent_t *join_mkdir_rename();
log_ent_t *join_symlink_rename();
log_ent_t *join_link_rename();
log_ent_t *join_rename_remove();
log_ent_t *join_rename_rmdir();
log_ent_t *join_rename_rename();
log_ent_t * gen_remove_ent();


#define DEFAULT_NAME  "/var/log/DisconnectedLog"
extern char    *optarg;

struct vc_info {
	struct VenusFid fid;
	int index;
	struct vc_info *next;
};



struct vc_info *afs_vchashTable[VCSIZE];

#define	MAX_ENTRIES	2048
struct sort_ent	ent[MAX_ENTRIES];
int	cur_ent;


#define MAX_LOG_ENTS	80000
char	want_entry[MAX_LOG_ENTS];
log_ent_t	*log_ents[MAX_LOG_ENTS];

int	debug = 0;
int 	newfd;


main(argc, argv)
	int argc;
	char **argv;
{	

	char *filename = 0;
	char *logname = 0;
	char *newname = 0;

	int cc;
	int c;
	int i;
 	for(i=0;i<VCSIZE;i++) {
		afs_vchashTable[i] = (struct vc_info *) 0;
	}

	/* parse the arguments */

	while ((c = getopt(argc, argv, "?dhl:f:n:")) != EOF) {
		switch (c) {
		case 'd':
			debug++;
			break;

		case 'l':
			logname = optarg;
			break;

		case 'f':
			filename = optarg;
			break;

		case 'n':
			newname = optarg;
			break;

		case 'h':
		case '?':
		default:
			usage();
			exit(1);
		}
	}

	/* make sure all options are set */
	if (!(logname) || (!newname)) {
		usage();
		exit (-1);
	}

	/* zero out storage arrays */
	bzero(want_entry, MAX_LOG_ENTS);
	bzero((char *) log_ents, (sizeof(char *) * MAX_LOG_ENTS));

	/* read the optimized output */

	read_input(filename);

	newfd = open(newname, O_RDWR|O_CREAT|O_TRUNC, 0744);

	if (newfd < 0) {
		perror("failed to output file ");
		exit(-1);
	}

	read_log(logname);	

	rewrite_log();

	cc = close(newfd);
	if (cc)
		perror("closing new log");

	exit (0);
}

/* read the log file and store keep the relevant records */

void
read_log(log_name)
	char *log_name;
{
	log_ent_t cur, *curp = &cur;
	FILE *logfp;
	int cc;
        char *bp;
	log_ent_t *new_ent;
        int len;
	int log_num = 0 ;

	/* open the log_file */

        if ((logfp = fopen(log_name, "r")) == 0) {
                perror("failed to open log ");
                exit(-1);
        }


        while(fread(curp, sizeof (int), 1, logfp)) {
                len = curp->log_len - sizeof(int);
                bp = (char *) curp + sizeof(int);
                if (fread(bp, len, 1, logfp) == 0) {
                        printf("post parse log: bad read \n");
			exit(-1);
                        break;
                }

		log_num++;

		/* check bounds on log_num */

		if (log_num > MAX_LOG_ENTS) {
			printf("increase MAX_LOG_ENTS and recompile \n");
			exit(-1);
		}
		/*
		 * deal with the rename log entries that also remove
		 * files.
		 */

		if (curp->log_op == DIS_RENAME) {
			if (curp->rn_overfid.Fid.Volume != 0) {
				if (want_entry[log_num]) {
					new_ent = gen_remove_ent(curp); 
					log_ents[log_num] = new_ent;
					log_num++;
					/*
					 * we always want the rename in this
					 * this case.
					 */
					new_ent = (log_ent_t *) 
					    malloc(curp->log_len);
					bcopy((char *) curp, (char *) new_ent, 
					    curp->log_len);
					log_ents[log_num] = new_ent;
					continue;
				}
				log_num++;
			}
		}

		if (want_entry[log_num]) {
 		   	/* if we want to keep this log entry */

			new_ent = (log_ent_t *) malloc(curp->log_len);
			bcopy((char *) curp, (char *) new_ent, curp->log_len);
			log_ents[log_num] = new_ent;
		} else {
			/*
			 * if we don't want this log entry
			 * then it has been optimized out, so
			 * we write it to the new log after marking
			 * it a optimized away.  We don't need to 
			 * preserver ordering because its removeval
			 * is independent of any other operations being
			 * replayed.  (this is not true for combined
			 * operations as seen below).
			 */

			
			curp->log_flags |= LOG_OPTIMIZED;

			/* XXX lhuston change to fwrite */	
			cc = write(newfd, curp, curp->log_len);

			if (cc != curp->log_len) {
				perror("failed writing new log");
				exit(-1);
			}

			
		}
	}

	fclose(logfp);
}

/*
 * Take the text equivalent of the log entries and
 * write log entries.
 */

void
rewrite_log()
{
	int cc;
	log_ent_t *new_log;
	log_ent_t *second_log;
	log_ent_t *first_log;
	struct sort_ent *cur_entry;
	struct op_no_ent *cur_nump;
	int i;
	int joined, pass;

	for (i = 0; i < cur_ent; i++) {
		joined = 0;
		cur_entry = &ent[i];
		cur_nump = cur_entry->se_nump;
		new_log = log_ents[cur_nump->num];
		if (new_log == NULL) {
		    fprintf(stderr, "rewrite_log: i=%d cur_ent=%d cur_nump->num=%d\n",
			    i, cur_ent, cur_nump->num);
		    continue;
		}
		cur_nump = cur_nump->next;

		while (cur_nump != NULL) {
			pass = 0;
			joined = 1;
			first_log = new_log;
			second_log = log_ents[cur_nump->num];

			new_log = join_ops(first_log, second_log);
			new_log->log_flags |= LOG_GENERATED;

			/*
			 * after the first pass we want to free the
			 * first log because it was generated
			 */
			if (pass)
				free(first_log);

			cur_nump = cur_nump->next;
			pass++;

		}

		/* XXX lhuston change to fwrite */	
		cc = write(newfd, new_log, new_log->log_len);
		if (cc != new_log->log_len) {
			perror("failed writing new log");
			exit(-1);
		}
		free(new_log);

		/*
		 * if the new log entry was a joined one, then
		 * we write the old log entries with the optimzied
		 * bit set on them.  These need to come after the optimized
		 * entry, because if it doesn't get replayed, then its
		 * changes will be lost.
		 */

		if (joined) {
			cur_nump = cur_entry->se_nump;
			while (cur_nump != NULL) {
				new_log = log_ents[cur_nump->num];
				new_log->log_flags |= LOG_OPTIMIZED;
				/* XXX lhuston change to fwrite */
				cc = write(newfd, new_log, new_log->log_len);
				if (cc != new_log->log_len) {
					perror("failed writing new log");
					exit(-1);
				}

				/* free up these entries */

				free(new_log);
				log_ents[cur_nump->num] = NULL;
				cur_nump = cur_nump->next;
			}
		} else {
			log_ents[cur_entry->se_nump->num] = NULL;
		}
	}

	/*
	 * we need go write all log entries that haven't been written
	 * out yet.  These are leftovers for the remove, rename joining
	 * conditions.  We just scan through the log_ents list,
	 * writing all that we find while setting the LOG_OPTIMIZED flag.
	 */

	for (i=0; i < MAX_LOG_ENTS; i++) {
		if (log_ents[i] != NULL) {
			new_log = log_ents[i];
			new_log->log_flags |= LOG_OPTIMIZED;
			/* XXX lhuston change to fwrite */
			cc = write(newfd, new_log, new_log->log_len);
			if (cc != new_log->log_len) {
				perror("failed writing new log");
				exit(-1);
			}
		}
	}
}


log_ent_t *
join_ops(first, second)
	log_ent_t *first;
	log_ent_t *second;
{
	log_ent_t *new_log = 0;


	if ((first->log_op == DIS_SETATTR) && (second->log_op == DIS_SETATTR))
		new_log = join_setattrs(first, second);

	else if ((first->log_op == DIS_CREATE) &&
	    (second->log_op == DIS_RENAME))
		new_log = join_create_rename(first, second);

	else if ((first->log_op == DIS_MKDIR) && (second->log_op == DIS_RENAME))
		new_log = join_mkdir_rename(first, second);

	else if ((first->log_op == DIS_SYMLINK) &&
	    (second->log_op == DIS_RENAME))
		new_log = join_symlink_rename(first, second);

	else if ((first->log_op == DIS_LINK) &&
	    (second->log_op == DIS_RENAME))
		new_log = join_link_rename(first, second);

	else if ((first->log_op == DIS_RENAME) &&
	    (second->log_op == DIS_REMOVE))
		new_log = join_rename_remove(first, second);

	else if ((first->log_op == DIS_RENAME) && 
	    (second->log_op == DIS_RMDIR))
		new_log = join_rename_rmdir(first, second);

	else if ((first->log_op == DIS_RENAME) &&
	    (second->log_op == DIS_RENAME))
		new_log = join_rename_rename(first, second);
	
	else if ((first->log_op == DIS_REMOVE) &&
	    (second->log_op == DIS_REMOVE)) {

		printf("join_ops: join of removes! \n");
		exit (1);

	} else {
		printf("join_ops: unknown join %d %d \n", first->log_op, 
		    second->log_op);
		exit (1);
	}

	if (debug) {
		printf("join ops /n");
		printf("first op /n");
		display_op(first, 1, 0);
		printf("second op /n");
		display_op(second, 1, 0);
		printf("new op /n");
		display_op(new_log, 1, 0);
	}
		
if (new_log == 0) abort();
	return new_log;
}


log_ent_t *
join_setattrs(sa1, sa2)
	log_ent_t *sa1;
	log_ent_t *sa2;
{
	
	log_ent_t *new_sa;

	/* allocate new log entry space */	
	new_sa = (log_ent_t *) malloc(sizeof(log_ent_t));

	/* copy first setattr data into new entry */	
	bcopy((char *) sa1, (char *) new_sa, sizeof(log_ent_t));


	if (sa2->sa_vattr.va_size != VNOVAL)
		new_sa->sa_vattr.va_size = sa2->sa_vattr.va_size;

	if (sa2->sa_vattr.va_mode != 0xffff)
		new_sa->sa_vattr.va_mode = sa2->sa_vattr.va_mode;

	if (sa2->sa_vattr.va_uid != 0xffffffff)
		new_sa->sa_vattr.va_uid = sa2->sa_vattr.va_uid;

	if (sa2->sa_vattr.va_gid != 0xffffffff)
		new_sa->sa_vattr.va_gid = sa2->sa_vattr.va_gid;


#if defined(__NetBSD__) && !defined(__OpenBSD__)
	if (sa2->sa_vattr.va_mtime.ts_sec != -1)
#else
	if (sa2->sa_vattr.va_mtime.tv_sec != -1)
#endif
		new_sa->sa_vattr.va_mtime = sa2->sa_vattr.va_mtime;

	return (new_sa);	
}


log_ent_t *
join_create_rename(clog, ren_log)
	log_ent_t *clog;
	log_ent_t *ren_log;
{
	log_ent_t *new_cr;
	char *oname, *nname;

	/* get name pointers from rename */
	oname = ren_log->rn_names;
	nname = oname + strlen(oname) + 1;

	/* allocate new log entry space */	
	new_cr = (log_ent_t *) malloc(sizeof(log_ent_t));

	/* copy old create data into new entry */	
	bcopy((char *) clog, (char *) new_cr, sizeof(log_ent_t));

	/* update create data with rename info */
	new_cr->cr_parentfid = ren_log->rn_nparentfid;
	strcpy((char *) new_cr->cr_name, (char *) nname);

	/* calculate the length of this record */
	new_cr->log_len =  ((char *) new_cr->cr_name - (char *) new_cr) +
            strlen(nname) + 1;

	return (new_cr);	
}


log_ent_t *
join_mkdir_rename(md_log, ren_log)
	log_ent_t *md_log;
	log_ent_t *ren_log;
{
	log_ent_t *new_md;
	char *oname, *nname;

	/* get name pointers from rename */
	oname = ren_log->rn_names;
	nname = oname + strlen(oname) + 1;
	
	/* allocate new log entry space */	
	new_md = (log_ent_t *) malloc(sizeof(log_ent_t));

	/* copy old mkdir data into new entry */	
	bcopy((char *) md_log, (char *) new_md, sizeof(log_ent_t));

	/* update create data with rename info */
	new_md->md_parentfid = ren_log->rn_nparentfid;
	strcpy((char *) new_md->md_name, (char *) nname);

	/* calculate the length of this record */
	new_md->log_len = ((char *) new_md->md_name - (char *) new_md)
	    + strlen(nname) + 1;
 
	return (new_md);	
}



log_ent_t *
join_symlink_rename(sym_log, ren_log)
	log_ent_t *sym_log;
	log_ent_t *ren_log;
{
	log_ent_t *new_sy;
	char *oname, *nname;
	char *cp;
	char *target;

	/* get name pointers from rename */
	oname = ren_log->rn_names;
	nname = oname + strlen(oname) + 1;
	
	/* allocate new log entry space */	
	new_sy = (log_ent_t *) malloc(sizeof(log_ent_t));

	/* copy old symlink data into new entry */
	bcopy((char *) sym_log, (char *) new_sy, sizeof(log_ent_t));

	/* update log fields */
	new_sy->sy_parentfid = ren_log->rn_nparentfid;
	strcpy((char *) new_sy->sy_names, (char *) nname);

	/* restore link data */
	target = (char *) sym_log->sy_names + strlen(sym_log->sy_names) + 1;	
	cp =  new_sy->sy_names + sizeof (nname) + 1;

	strcpy(cp, target);
	
	cp += strlen(target) +1;
	new_sy->log_len = cp - (char *) new_sy;

	return (new_sy);	
}


log_ent_t *
join_link_rename(link_log, ren_log)
	log_ent_t *link_log;
	log_ent_t *ren_log;
{
	log_ent_t *new_link;
	char *oname, *nname;

	/* get name pointers from rename */
	oname = ren_log->rn_names;
	nname = oname + strlen(oname) + 1;
	
	/* allocate new log entry space */	
	new_link = (log_ent_t *) malloc(sizeof(log_ent_t));

	/* copy old link data into new entry */	
	bcopy((char *) link_log, (char *) new_link, sizeof(log_ent_t));

	/* update link data with rename info */
	new_link->ln_parentfid = ren_log->rn_nparentfid;
	strcpy((char *) new_link->ln_name, (char *) nname);

	/* calculate the length of this record */
	new_link->log_len = ((char *) new_link->ln_name - (char *) new_link) +
	    strlen(nname) + 1;

	return (new_link);	
}


/* This generates the remove log entry for a given rename */

log_ent_t *
gen_remove_ent(ren_log)
	log_ent_t *ren_log;
{
	log_ent_t *new_rm;
	char *oname, *nname;

	/* get name pointers from rename */
	oname = ren_log->rn_names;
	nname = oname + strlen(oname) + 1;
	
	/* allocate new log entry space */	
	new_rm = (log_ent_t *) malloc(sizeof(log_ent_t));


	new_rm->log_op = DIS_REMOVE;
	new_rm->log_opno = ren_log->log_opno;
	new_rm->rm_filefid = ren_log->rn_overfid;
	new_rm->rm_parentfid = ren_log->rn_nparentfid;
	new_rm->rm_origdv = ren_log->rn_overdv;

	strcpy((char *) new_rm->rm_name, nname);

	new_rm->log_len = ((char *) new_rm->rm_name - (char *) new_rm) +
	    strlen(nname) + 1;

	/* set the generate flag on this one */
	new_rm->log_flags |= LOG_GENERATED;

	return (new_rm);	
}


log_ent_t *
join_rename_remove(ren_log, rm_log)
	log_ent_t *ren_log;
	log_ent_t *rm_log;
{
	log_ent_t *new_rm;
	char *oname;

	/* get name pointers from rename */
	oname = ren_log->rn_names;
	
	/* allocate new log entry space */	
	new_rm = (log_ent_t *) malloc(sizeof(log_ent_t));

	/* copy old remove data into new entry */	
	bcopy((char *) rm_log, (char *) new_rm, sizeof(log_ent_t));

	/* update remove data with rename info */
	new_rm->rm_parentfid = ren_log->rn_oparentfid;
	strcpy((char *) new_rm->rm_name, (char *) oname);

	/* calculate the length of this record */
	new_rm->log_len = ((char *) new_rm->rm_name - (char *) new_rm) +
	    strlen(oname) + 1;

	return (new_rm);	
}

log_ent_t *
join_rename_rmdir(ren_log, rd_log)
	log_ent_t *ren_log;
	log_ent_t *rd_log;
{
	log_ent_t *new_rd;
	char *oname;

	/* get name pointers from rename */
	oname = ren_log->rn_names;
	
	/* allocate new log entry space */	
	new_rd = (log_ent_t *) malloc(sizeof(log_ent_t));

	/* copy old rmdir data into new entry */	
	bcopy((char *) rd_log, (char *) new_rd, sizeof(log_ent_t));

	/* update rmdir data with rename info */
	new_rd->rd_parentfid = ren_log->rn_oparentfid;
	strcpy((char *) new_rd->rd_name, (char *) oname);

	/* calculate the length of this record */
	new_rd->log_len = ((char *) new_rd->rd_name - (char *) new_rd) +
	    strlen(oname) + 1;

	return (new_rd);	
}

log_ent_t *
join_rename_rename(ren_log1, ren_log2)
	log_ent_t *ren_log1;
	log_ent_t *ren_log2;
{
	log_ent_t *new_rn;
	char *oname1, *oname2, *nname2, *cp;

	/* get name pointers from rename */
	oname1 = ren_log1->rn_names;
	oname2 = ren_log2->rn_names;
	nname2 = oname2 + strlen(oname2) + 1;
	
	/* allocate new log entry space */	
	new_rn = (log_ent_t *) malloc(sizeof(log_ent_t));

	/* copy latest rename data into new entry */	
	bcopy((char *) ren_log2, (char *) new_rn, sizeof(log_ent_t));

	/* update old name info data with rename info */
	new_rn->rn_oparentfid = ren_log1->rn_oparentfid;
	strcpy((char *) new_rn->rn_names, (char *) oname1);

	/* restore new info, may have been overwritten above */
	cp =  new_rn->rn_names + strlen (oname1) + 1;
	strcpy(cp, nname2);
	
	cp += strlen(nname2) +1;
	new_rn->log_len = cp - (char *) new_rn;

	return (new_rn);	
}



void
read_input(fname)
	char *fname;
{
	FILE *fp;
	char indata[MAX_NAME];
	char op[MAX_NAME];

	/*
	 * if a filename is given, then open that file, otherwise
	 * use stdin
	 */

	if (fname != 0) {
		fp = fopen(fname, "r");
		if (!fp) {
			perror("failed to open log ");
			exit(-1);
		}
	} else {
		fp = stdin;
	}

	/* read each line from the file */
	while (fgets(indata, MAX_NAME, fp)) {
		sscanf(indata,"%s", op);
		if (!strcmp(op, "store")) {
			read_store(indata);

		} else if (!strcmp(op, "create")) {
			read_create(indata);

		} else if (!strcmp(op, "remove")) {
			read_remove(indata);

		} else if (!strcmp(op, "rename")) {
			read_rename(indata);

		} else if (!strcmp(op, "link")) {
			read_link(indata);

		} else if (!strcmp(op, "set_len")) {
			read_set_len(indata);

		} else if (!strcmp(op, "set_mode")) {
			read_set_mode(indata);

		} else if (!strcmp(op, "set_time")) {
			read_set_time(indata);

		} else {
			printf("unknown op <%s>\n", indata);
		}

	}

	sort_ents();
	if (debug)
		dump_ents();
}

/*
 * parse the string pointed to by opno and build a list of
 * all operations in the string.  The opno's are seperated by
 * :'s.
 */

struct op_no_ent *
parse_opnos(opstr)
	char *opstr;
{
	char *cp;
	struct op_no_ent *first;
	struct op_no_ent *current;
	struct op_no_ent *new;
	int  len;
	int  count = 0;

	len = strlen(opstr);
	
	/* get the first entry */
	new = (struct op_no_ent *) malloc(sizeof (struct op_no_ent));
	new->next = 0;
	new->num = atoi(opstr);
	first = new;
	current = new;

	cp = opstr;

	while (count < len) {
		while ((count < len) && (*cp != ':')) {
			cp++;
			count++;
		}

		if (count < len) {
			cp++;
			count++;
			new = (struct op_no_ent *) malloc(sizeof
			    (struct op_no_ent));
			new->next = 0;
			new->num = atoi(cp);
			current->next = new;
			current = new;		
		}
	}

	return (first);
}

void
add_ent(op, opstr)
	char *op;
	char *opstr;
{
	struct op_no_ent *current;
	

	strcpy(ent[cur_ent].se_op_name, op); 
	ent[cur_ent].se_nump = parse_opnos(opstr);


	/* mark all the log entries we want */
	current =  ent[cur_ent].se_nump;
	while (current != NULL) {
		want_entry[current->num] = 1;
		current = current->next;
	}

	cur_ent++;
	if (cur_ent > MAX_ENTRIES) {
		printf("cur_ent too large \n");
		exit(1);
	}
}


void
dump_ents()
{
	int i;
	struct op_no_ent *current;

	for(i=0; i < cur_ent; i++) {
		printf("<%s> ", ent[i].se_op_name);
		current = ent[i].se_nump;
		while (current != NULL) {
			printf("%d ", current->num);
			current = current->next;
		}
		printf("\n");
	}
}

int
comp_ents(ent1, ent2)
	struct sort_ent *ent1;
	struct sort_ent *ent2;
{

	if (ent1->se_nump->num < ent2->se_nump->num)
		return(-1);
	else if (ent1->se_nump->num == ent2->se_nump->num)
		return(0);
	else
		return(1);
}

	
void
sort_ents()
{
	qsort((char *) ent, cur_ent, sizeof(struct sort_ent), comp_ents);

}

void
read_store(data)
	char *data;
{
	char op[MAX_NAME];
	char opstr[MAX_NAME];
	int fid;
	int linked;

	sscanf(data, "%s %d %d %s\n", op, &fid, &linked, opstr);
	add_ent(op, opstr);
}


void
read_create(data)
	char *data;
{
	char op[MAX_NAME];
	char opstr[MAX_NAME];
	int fid;
	int pfid;
	int name;

	sscanf(data, "%s %d %d %d %s\n", op, &pfid, &fid, &name, opstr);
	add_ent(op, opstr);
}



void
read_remove(data)
	char *data;
{
	char op[MAX_NAME];
	char opstr[MAX_NAME];

	sscanf(data, "%s %*d %*d %*d %*d %s", op, opstr);
	add_ent(op, opstr);
}


void
read_rename(data)
	char *data;
{
	char op[MAX_NAME];
	char opstr[MAX_NAME];
	int fid;
	int spfid;
	int dpfid;
	int ovfid;
	int oname;
	int nname;

	sscanf(data, "%s %d %d %d %d %d %d %s\n", op, &spfid, &oname,
	    &dpfid, &nname, &fid,
	     &ovfid, opstr);
	add_ent(op, opstr);

}


void
read_link(data)
	char *data;
{
	char op[MAX_NAME];
	char opstr[MAX_NAME];
	int fid;
	int pfid;
	int name;

	sscanf(data, "%s %d %d %d %s\n", op, &pfid, &fid, &name, opstr);
	add_ent(op, opstr);


}


void
read_set_len(data)
	char *data;
{
	char op[MAX_NAME];
	char opstr[MAX_NAME];
	int fid;
	int size;
	int linked;

	sscanf(data, "%s %d %d %d %s\n", op, &fid, &size, &linked, opstr);
	add_ent(op, opstr);

}


void
read_set_mode(data)
	char *data;
{
	char op[MAX_NAME];
	char opstr[MAX_NAME];
	int fid;
	int linked;

	sscanf(data, "%s %d %d %s\n", op, &fid, &linked, opstr);
	add_ent(op, opstr);

}


void
read_set_time(data)
	char *data;
{
	char op[MAX_NAME];
	char opstr[MAX_NAME];
	int fid;
	int linked;

	sscanf(data, "%s %d %d %s\n", op, &fid, &linked, opstr);
	add_ent(op, opstr);

}
			
usage()
{

	printf("Options are: \n");
	printf("\t -f file containing processed data \n");
	printf("\t -l log file \n");
	printf("\t -n newlog file \n");
	printf("\t -h this help information \n");
	printf("\t -? this help information \n");
}
