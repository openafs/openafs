/*
 * (C) COPYRIGHT IBM CORPORATION 1987, 1998
 * LICENSED MATERIALS - PROPERTY OF IBM
 */

#include <afs/param.h>
#include <stdio.h>
#ifndef AFS_NT40_ENV
#include <strings.h>
#else
#include <WINNT/afsevent.h>
#endif
#include <rx/rx.h>
#include <rx/xdr.h>
#include <afs/cellconfig.h>
#include <afs/afsutil.h>
#include "ptclient.h"

int osi_audit()
{
/* OK, this REALLY sucks bigtime, but I can't tell who is calling
 * afsconf_CheckAuth easily, and only *SERVERS* should be calling osi_audit
 * anyway.  It's gonna give somebody fits to debug, I know, I know.
 */
return 0;
}

#include "AFS_component_version_number.c"

main(argc,argv)
afs_int32 argc;
char **argv;
{

    register afs_int32 code;
    char name[PR_MAXNAMELEN];
    afs_int32 id;
    char buf[150];
    FILE *fp;
    char *ptr;
    char *aptr;
    char *tmp;
    char uid[8];
    afs_int32 i;
    afs_int32 verbose = 0;
    char *cellname;

    if (argc < 2) {
	fprintf(stderr,"Usage: readpwd [-v] [-c cellname] passwdfile.\n");
	exit(1);
    }
    cellname = 0;
    for (i = 1;i<argc;i++) {
	if (!strcmp(argv[i],"-v"))
	    verbose = 1;
	else {
	    if (!strcmp(argv[i],"-c")) {
		cellname = (char *)malloc(100);
		strncpy(cellname,argv[++i],100);
	    }
	    else
		strncpy(buf,argv[i],150);
	}
    }
    code = pr_Initialize(2, AFSDIR_CLIENT_ETC_DIRPATH, cellname);
    if (code) {
	fprintf(stderr,"pr_Initialize failed, code %d.\n",code);
	exit(1);
    }


    if ((fp= fopen(buf,"r")) == NULL) {
	fprintf(stderr,"Couldn't open %s.\n",argv[1]);
	exit(2);
    }
    while ((tmp = fgets(buf,150,fp)) != NULL) {
	bzero(name,PR_MAXNAMELEN);
	bzero(uid,8);
	ptr = index(buf,':');
	strncpy(name,buf,ptr-buf);
	aptr = index(++ptr,':');
	ptr = index(++aptr,':');
	strncpy(uid,aptr,ptr-aptr);
	id = atoi(uid);
	if (verbose)
	    printf("Adding %s with id %d.\n",name,id);
	code = pr_CreateUser(name,&id);
	if (code) {
	    fprintf(stderr,"Failed to add user %s with id %d!\n",name,id);
	    fprintf(stderr,"%s (%d).\n",pr_ErrorMsg(code),code);
	}
    }
}
