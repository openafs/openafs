/*
 *
 * ka_util: Program to dump the AFS authentication server database
 *         into an ascii file.
 *
 *	Assumptions: We *cheat* here and read the datafile directly, ie.
 *	             not going through the ubik distributed data manager.
 *		     therefore the database must be quiescent for the
 *		     output of this program to be valid.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/file.h>

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <lock.h>
#include <netinet/in.h>
#define UBIK_INTERNALS
#include <ubik.h>
#include <rx/xdr.h>
#include <rx/rx.h>
#include <rx/rxkad.h>
#include "kauth.h"
#include "kaserver.h"
#include "kautils.h"

#define IDHash(x) (abs(x) % HASHSIZE)
#define print_id(x) ( ((flags&DO_SYS)==0 && (x<-32767 || x>97536)) || \
		      ((flags&DO_OTR)==0 && (x>-32768 && x<97537)))

extern char *optarg;
extern int optind;
extern int errno;

int display_entry();

static struct kaheader kah;
static struct ubik_version uv;
struct kadstats dynamic_statistics;

char buffer[1024];
int dbase_fd;
FILE *dfp;

int nflag = 0;
int wflag = 0;
int flags = 0;

afs_int32
es_Report()
{
}

struct afsconf_dir *KA_conf;
struct ubik_dbase *KA_dbase;
int MinHours = 0;
int npwSums = KA_NPWSUMS;
afs_int32 verbose_track = 1;
afs_int32 myHost = 0;

main(argc, argv)
     int argc;
     char **argv;
{
    register int i;
    register long code;
    long cc, upos = 0, gpos;
    struct ubik_hdr *uh;
    char *dfile = 0;
    char *pfile = "/usr/afs/db/kaserver.DB0";

    while ((cc = getopt(argc, argv, "wugmxsnp:d:")) != EOF) {
	switch (cc) {
	case 'p':
	    pfile = optarg;
	    break;
	case 'd':
	    dfile = optarg;
	    break;
	case 'n':
	    nflag++;
	    break;
	case 'w':
	    wflag++;
	    break;
	default:
	    fprintf(stderr, "Usage: ka_util [options] [-d data] [-p prdb]\n");
	    fputs("  Options:\n", stderr);
	    fputs("    -w  Update prdb with contents of data file\n", stderr);
	    fputs("    -u  Display users\n", stderr);
	    fputs("    -g  Display groups\n", stderr);
	    fputs("    -m  Display group members\n", stderr);
	    fputs("    -n  Follow name hash chains (not id hashes)\n",
		  stderr);
	    fputs("    -s  Display only system data\n", stderr);
	    fputs("    -x  Display extra users/groups\n", stderr);
	    exit(1);
	}
    }
    if ((dbase_fd = open(pfile, (wflag ? O_RDWR : O_RDONLY) | O_CREAT, 0600))
	< 0) {
	fprintf(stderr, "ka_util: cannot open %s: %s\n", pfile,
		strerror(errno));
	exit(1);
    }
    if (read(dbase_fd, buffer, HDRSIZE) < 0) {
	fprintf(stderr, "ka_util: error reading %s: %s\n", pfile,
		strerror(errno));
	exit(1);
    }

    if (dfile) {
	if ((dfp = fopen(dfile, wflag ? "r" : "w")) == 0) {
	    fprintf(stderr, "ka_util: error opening %s: %s\n", dfile,
		    strerror(errno));
	    exit(1);
	}
    } else
	dfp = (wflag ? stdin : stdout);

    uh = (struct ubik_hdr *)buffer;
    if (ntohl(uh->magic) != UBIK_MAGIC)
	fprintf(stderr, "ka_util: %s: Bad UBIK_MAGIC. Is %x should be %x\n",
		pfile, ntohl(uh->magic), UBIK_MAGIC);
    memcpy(&uv, &uh->version, sizeof(struct ubik_version));
    if (wflag && uv.epoch == 0 && uv.counter == 0) {
	uv.epoch = 2;		/* a ubik version of 0 or 1 has special meaning */
	memcpy(&uh->version, &uv, sizeof(struct ubik_version));
	lseek(dbase_fd, 0, SEEK_SET);
	if (write(dbase_fd, buffer, HDRSIZE) < 0) {
	    fprintf(stderr, "ka_util: error writing ubik version to %s: %s\n",
		    pfile, strerror(errno));
	    exit(1);
	}
    }
    fprintf(stderr, "Ubik Version is: %d.%d\n", uv.epoch, uv.counter);
    if (read(dbase_fd, &kah, sizeof(struct kaheader)) < 0) {
	fprintf(stderr, "ka_util: error reading %s: %s\n", pfile,
		strerror(errno));
	exit(1);
    }

    initialize_KA_error_table();
    initialize_rx_error_table();

    if (wflag) {
	struct kaheader header;
	afs_int32 ltime = time(0);
	memset(&header, 0, sizeof(header));
	header.version = htonl(KADBVERSION);
	header.headerSize = htonl(sizeof(header));
	header.freePtr = 0;
	header.eofPtr = htonl(sizeof(header));
	header.kvnoPtr = 0;
	header.stats.allocs = 0;
	header.stats.frees = 0;
	header.stats.cpws = 0;
	header.admin_accounts = 0;
	header.specialKeysVersion = htonl(ltime);
	header.hashsize = htonl(HASHSIZE);
	header.checkVersion = htonl(KADBVERSION);

	write(dbase_fd, &header, sizeof(header));
	while (fgets(buffer, sizeof(buffer), dfp)) {
	    struct kaentry tentry;
	    int flags, exp, modtime, modid, cpwtime, maxlife, kvno;
	    char kaname[64 + 64 + 2], key[33], name[64], instance[64],
		rlm[64];
	    afs_int32 maxLifetime;

	    sscanf(buffer, "%s %d %d %d %d %d %d %d %s", kaname, &flags, &exp,
		   &modtime, &modid, &cpwtime, &maxlife, &kvno, key);

	    printf("%s %d %d %d %d %d %d %d %s", kaname, flags, exp, modtime,
		   modid, cpwtime, maxlife, kvno, key);
	    memset(name, 0, sizeof(name));
	    memset(instance, 0, sizeof(instance));
	    ka_ParseLoginName(&kaname, &name, &instance, &rlm);
	    printf("%s %s %s\n", kaname, name, instance);
	    strncpy(tentry.userID.name, name, sizeof(tentry.userID.name));
	    strncpy(tentry.userID.instance, instance,
		    sizeof(tentry.userID.instance));
	    tentry.flags = htonl(flags);
	    memcpy(&tentry.key, key, sizeof(tentry.key));
	    tentry.key_version = htonl(kvno);

	    tentry.user_expiration = htonl(exp);

	    /* time and addr of entry for guy changing this entry */
	    tentry.modification_time = htonl(modtime);
	    tentry.modification_id = htonl(modid);
	    tentry.change_password_time = htonl(cpwtime);

	    if (strcmp(name, KA_TGS_NAME) == 0)
		maxLifetime = MAXKTCTICKETLIFETIME;
	    else if (strcmp(name, KA_ADMIN_NAME) == 0)
		maxLifetime = 10 * 3600;
	    else if (strcmp(name, AUTH_SUPERUSER) == 0)
		maxLifetime = 100 * 3600;
	    else
		maxLifetime = 25 * 3600;	/* regular users */
	    if (maxlife)
		tentry.max_ticket_lifetime = htonl(maxlife);
	    else
		tentry.max_ticket_lifetime = htonl(maxLifetime);

	    write(dbase_fd, &tentry, sizeof(tentry));
	}
	/*CheckInit(0,0); */
    } else {
	while (1) {
	    gpos = display_entry(upos * sizeof(struct kaentry));
	    if (gpos < 0)
		break;
	    upos++;
	}
    }

    lseek(dbase_fd, 0, L_SET);	/* rewind to beginning of file */
    if (read(dbase_fd, buffer, HDRSIZE) < 0) {
	fprintf(stderr, "ka_util: error reading %s: %s\n", pfile,
		strerror(errno));
	exit(1);
    }
    uh = (struct ubik_hdr *)buffer;
    if ((uh->version.epoch != uv.epoch)
	|| (uh->version.counter != uv.counter)) {
	fprintf(stderr,
		"ka_util: Ubik Version number changed during execution.\n");
	fprintf(stderr, "Old Version = %d.%d, new version = %d.%d\n",
		uv.epoch, uv.counter, uh->version.epoch, uh->version.counter);
    }
    close(dbase_fd);
    exit(0);
}

int
display_entry(offset)
     int offset;
{
    register int i;
    struct kaentry dbentry;
    int count;
    unsigned char x[8];
    char thiskey[33];

    if (lseek(dbase_fd, offset + HDRSIZE + sizeof(struct kaheader), L_SET) <
	0)
	return -1;
    i = read(dbase_fd, &dbentry, sizeof(struct kaentry));
    if (i < sizeof(struct kaentry))
	return -1;
    if (!strcmp(dbentry.userID.name, ""))
	return 1;
    memcpy(x, &dbentry.key, 8);

    fprintf(dfp, "%s%s%s %d %d %d %d %d %d %d ", dbentry.userID.name,
	    ((dbentry.userID.instance && strcmp(dbentry.userID.instance, ""))
	     ? "." : ""), ((dbentry.userID.instance
			    && strcmp(dbentry.userID.instance, ""))
			   ? dbentry.userID.instance : ""), dbentry.flags,
	    dbentry.user_expiration, dbentry.modification_time,
	    dbentry.modification_id, dbentry.change_password_time,
	    dbentry.max_ticket_lifetime, dbentry.key_version);
    for (count = 0; count < 8; count++) {
	fprintf(dfp, "\\%03o", (unsigned char *)x[count]);
    }

    fprintf(dfp, "\n");
    return 0;
}
