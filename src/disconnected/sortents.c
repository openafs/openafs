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
#include "post.h"

#define MAX_NAME  255
#define CELL_DIRTY   0x01

#define REALLY_BIG 1024

/* forward function declarations */
void read_store();
void read_create();
void read_mkdir();
void read_remove();
void read_rmdir();
void read_rename();
void read_link();
void read_symlink();
void read_set_len();
void read_set_mode();
void read_set_time();
void write_ents();
void read_input();
void add_ent();
int  comp_ents();
void sort_ents();

#define DEFAULT_NAME  "sorted"
extern char    *optarg;

int current_index_file = 10000;
int current_index_dir = 1;

struct vc_info {
    struct VenusFid fid;
    int index;
    struct vc_info *next;
    };

struct vc_info *afs_vchashTable[VCSIZE];

#define	MAX_ENTRIES	2048

struct sort_ent	ent[MAX_ENTRIES];
int	cur_ent;


main(argc, argv)
	int argc;
	char **argv;
{	

	char *filename = 0;
	char *newname = DEFAULT_NAME;
	int c;
	int vflag = 0;
	int i;
	int current_op = 0;
 	for(i=0;i<VCSIZE;i++) {
		afs_vchashTable[i] = (struct vc_info *) 0;
	}

	/* parse the arguments */

	while ((c = getopt(argc, argv, "?hvf:n:")) != EOF) {
		switch (c) {
		case 'v':
			vflag++;
			break;

		case 'f':
			filename = optarg;
			break;

		case 'n':
			newname = optarg;
			break;

		case 'h': case '?':
			printf("Options are: \n");
			printf("\t -f file containing processed data \n");
			printf("\t -n newlog file \n");
			printf("\t -h this help information \n");
			printf("\t -? this help information \n");

		default:
			exit(1);
		}
	}

	/* read the optimized output */

	read_input(filename);
	sort_ents();
	write_ents(newname);
	exit (1);
}


void
read_input(fname)
	char *fname;
{
	FILE *fp;
	char indata[MAX_NAME];
	char op[MAX_NAME];

	int fd;

	if (fname == 0) {
		fp = stdin;
	} else {
		/* open the file */
		fp = fopen(fname, "r");

		if (!fp) {
			perror("failed to open log ");
			exit(-1);
		}
	}


	/* read each line from the file */
	while (fgets(indata, MAX_NAME, fp)) {
		sscanf(indata,"%s", op);
		if (!strcmp(op, "store")) {
			read_store(indata);

		} else if (!strcmp(op, "create")) {
			read_create(indata);

		} else if (!strcmp(op, "mkdir")) {
			read_mkdir(indata);

		} else if (!strcmp(op, "remove")) {
			read_remove(indata);

		} else if (!strcmp(op, "rmdir")) {
			read_rmdir(indata);

		} else if (!strcmp(op, "rename")) {
			read_rename(indata);

		} else if (!strcmp(op, "link")) {
			read_link(indata);

		} else if (!strcmp(op, "symlink")) {
			read_symlink(indata);

		} else if (!strcmp(op, "set_len")) {
			read_set_len(indata);

		} else if (!strcmp(op, "set_mode")) {
			read_set_mode(indata);

		} else if (!strcmp(op, "set_time")) {
			read_set_time(indata);

		} else {
			printf("sortents:unknown op <%s>\n", op);
		}

	}

}

void
add_ent(op, fid, opnum, data)
	char *op;
	int fid;
	int opnum;
	char *data;
{

	
	ent[cur_ent].se_num = opnum;
	strcpy(ent[cur_ent].se_op_name, op); 
	strcpy(ent[cur_ent].se_data, data); 

	cur_ent++;
	if (cur_ent > MAX_ENTRIES) {
		printf("cur_ent too large \n");
		exit(1);
	}
}


void
write_ents(filename)
{
	int i;

	for(i=0; i < cur_ent; i++) {
		printf("%s", ent[i].se_data);
	}
}

int
comp_ents(ent1, ent2)
	struct sort_ent *ent1;
	struct sort_ent *ent2;
{

	if (ent1->se_num < ent2->se_num) 
		return(-1);
	else if (ent1->se_num == ent2->se_num)
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
	int fid;
	int opnum;
	int linked;

	sscanf(data, "%s %d %d %d\n", op, &fid, &linked, &opnum);
	add_ent(op, fid, opnum, data);


}

void
read_create(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int pfid;
	int opnum;
	int name;

	sscanf(data, "%s %d %d %d %d\n", op, &pfid, &fid, &name, &opnum);
	add_ent(op, fid, opnum, data);


}

void
read_mkdir(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int pfid;
	int opnum;
	int name;

	sscanf(data, "%s %d %d %d %d\n", op, &pfid, &fid, &name, &opnum);
	add_ent(op, fid, opnum, data);


}

void
read_remove(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int opnum;

	sscanf(data, "%s %*d %d %*d %*d %d\n", op, &fid, &opnum);
	add_ent(op, fid, opnum, data);
}

void
read_rmdir(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int pfid;
	int opnum;
	int name;

	sscanf(data, "%s %d %d %d %d\n", op, &pfid, &fid, &name, &opnum);
	add_ent(op, fid, opnum, data);


}

void
read_rename(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int spfid;
	int dpfid;
	int ovfid;
	int opnum;
	int oname;
	int nname;

	sscanf(data, "%s %d %d %d %d %d %d %d\n", op, &spfid, &oname,
	     &dpfid, &nname, &fid, &ovfid, &opnum);
	add_ent(op, fid, opnum, data);

}

void
read_link(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int pfid;
	int opnum;
	int name;

	sscanf(data, "%s %d %d %d %d\n", op, &pfid, &fid, &name, &opnum);
	add_ent(op, fid, opnum, data);


}

void
read_symlink(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int pfid;
	int opnum;
	int name;

	sscanf(data, "%s %d %d %d %d\n", op, &pfid, &fid, &name, &opnum);
	add_ent(op, fid, opnum, data);

}


void
read_set_len(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int size;
	int opnum;
	int linked;

	sscanf(data, "%s %d %d %d %d\n", op, &fid, &size, &linked, &opnum);
	add_ent(op, fid, opnum, data);
}


void
read_set_mode(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int opnum;
	int linked;

	sscanf(data, "%s %d %d %d\n", op, &fid, &linked, &opnum);
	add_ent(op, fid, opnum, data);

}


void
read_set_time(data)
	char *data;
{
	char op[MAX_NAME];
	int fid;
	int opnum;
	int linked;

	sscanf(data, "%s %d %d %d\n", op, &fid, &linked, &opnum);
	add_ent(op, fid, opnum, data);

}
