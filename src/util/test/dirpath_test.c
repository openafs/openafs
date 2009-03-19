/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <stdio.h>
#include <afs/afsutil.h>

main(int argc, char *argv[])
{
    char *pbuf;
    unsigned dirpathStatus;

    /* Initialize dirpaths */

    dirpathStatus = initAFSDirPath();

    if (!(dirpathStatus & AFSDIR_CLIENT_PATHS_OK)) {
	printf
	    ("\n%s: Unable to obtain AFS client configuration directory...using temp.\n",
	     argv[0]);
    }

    if (!(dirpathStatus & AFSDIR_SERVER_PATHS_OK)) {
	printf
	    ("\n%s: Unable to obtain AFS server configuration directory...using temp.\n",
	     argv[0]);
    }

    /* Now print out all dir paths */

    printf("\n");
    printf("AFSDIR_USR_DIRPATH = %s\n", AFSDIR_USR_DIRPATH);
    printf("\n");
    printf("\n");
    printf("AFSDIR_SERVER_AFS_DIRPATH = %s\n", AFSDIR_SERVER_AFS_DIRPATH);
    printf("AFSDIR_SERVER_ETC_DIRPATH = %s\n", AFSDIR_SERVER_ETC_DIRPATH);
    printf("AFSDIR_SERVER_BIN_DIRPATH = %s\n", AFSDIR_SERVER_BIN_DIRPATH);
    printf("AFSDIR_SERVER_CORES_DIRPATH  = %s\n",
	   AFSDIR_SERVER_CORES_DIRPATH);
    printf("AFSDIR_SERVER_DB_DIRPATH  = %s\n", AFSDIR_SERVER_DB_DIRPATH);
    printf("AFSDIR_SERVER_LOGS_DIRPATH  = %s\n", AFSDIR_SERVER_LOGS_DIRPATH);
    printf("AFSDIR_SERVER_LOCAL_DIRPATH  = %s\n",
	   AFSDIR_SERVER_LOCAL_DIRPATH);
    printf("AFSDIR_SERVER_BACKUP_DIRPATH = %s\n",
	   AFSDIR_SERVER_BACKUP_DIRPATH);
    printf("\n");
    printf("\n");
    printf("AFSDIR_CLIENT_VICE_DIRPATH  = %s\n", AFSDIR_CLIENT_VICE_DIRPATH);
    printf("AFSDIR_CLIENT_ETC_DIRPATH  = %s\n", AFSDIR_CLIENT_ETC_DIRPATH);
    printf("\n");
    printf("\n");
    printf("AFSDIR_SERVER_THISCELL_FILEPATH  = %s\n",
	   AFSDIR_SERVER_THISCELL_FILEPATH);
    printf("AFSDIR_SERVER_CELLSERVDB_FILEPATH  = %s\n",
	   AFSDIR_SERVER_CELLSERVDB_FILEPATH);
    printf("AFSDIR_SERVER_KEY_FILEPATH AFSDIR = %s\n",
	   AFSDIR_SERVER_KEY_FILEPATH);
    printf("AFSDIR_SERVER_ULIST_FILEPATH = %s\n",
	   AFSDIR_SERVER_ULIST_FILEPATH);
    printf("AFSDIR_SERVER_NOAUTH_FILEPATH = %s\n",
	   AFSDIR_SERVER_NOAUTH_FILEPATH);
    printf("AFSDIR_SERVER_BUDBLOG_FILEPATH = %s\n",
	   AFSDIR_SERVER_BUDBLOG_FILEPATH);
    printf("AFSDIR_SERVER_TAPECONFIG_FILEPATH  = %s\n",
	   AFSDIR_SERVER_TAPECONFIG_FILEPATH);
    printf("AFSDIR_SERVER_KALOGDB_FILEPATH  = %s\n",
	   AFSDIR_SERVER_KALOGDB_FILEPATH);
    printf("AFSDIR_SERVER_KADB_FILEPATH  = %s\n",
	   AFSDIR_SERVER_KADB_FILEPATH);
    printf("AFSDIR_SERVER_KALOG_FILEPATH  = %s\n",
	   AFSDIR_SERVER_KALOG_FILEPATH);
    printf("AFSDIR_SERVER_NTPD_FILEPATH  = %s\n",
	   AFSDIR_SERVER_NTPD_FILEPATH);
    printf("AFSDIR_SERVER_PRDB_FILEPATH  = %s\n",
	   AFSDIR_SERVER_PRDB_FILEPATH);
    printf("AFSDIR_SERVER_PTLOG_FILEPATH  = %s\n",
	   AFSDIR_SERVER_PTLOG_FILEPATH);
    printf("AFSDIR_SERVER_KCONF_FILEPATH  = %s\n",
	   AFSDIR_SERVER_KCONF_FILEPATH);
    printf("AFSDIR_SERVER_VLDB_FILEPATH  = %s\n",
	   AFSDIR_SERVER_VLDB_FILEPATH);
    printf("AFSDIR_SERVER_VLOG_FILEPATH  = %s\n",
	   AFSDIR_SERVER_VLOG_FILEPATH);
    printf("AFSDIR_SERVER_CORELOG_FILEPATH  = %s\n",
	   AFSDIR_SERVER_CORELOG_FILEPATH);
    printf("AFSDIR_SERVER_SLVGLOG_FILEPATH  = %s\n",
	   AFSDIR_SERVER_SLVGLOG_FILEPATH);
    printf("AFSDIR_SERVER_SALVAGER_FILEPATH  = %s\n",
	   AFSDIR_SERVER_SALVAGER_FILEPATH);
    printf("AFSDIR_SERVER_BOZCONF_FILEPATH = %s\n",
	   AFSDIR_SERVER_BOZCONF_FILEPATH);
    printf("AFSDIR_SERVER_BOZINIT_FILEPATH = %s\n",
	   AFSDIR_SERVER_BOZINIT_FILEPATH);
    printf("AFSDIR_SERVER_BOZLOG_FILEPATH = %s\n",
	   AFSDIR_SERVER_BOZLOG_FILEPATH);
    printf("AFSDIR_SERVER_BOSVR_FILEPATH = %s\n",
	   AFSDIR_SERVER_BOSVR_FILEPATH);
    printf("AFSDIR_SERVER_VOLSERLOG_FILEPATH = %s\n",
	   AFSDIR_SERVER_VOLSERLOG_FILEPATH);
    printf("AFSDIR_SERVER_ROOTVOL_FILEPATH = %s\n",
	   AFSDIR_SERVER_ROOTVOL_FILEPATH);
    printf("AFSDIR_SERVER_HOSTDUMP_FILEPATH = %s\n",
	   AFSDIR_SERVER_HOSTDUMP_FILEPATH);
    printf("AFSDIR_SERVER_CLNTDUMP_FILEPATH = %s\n",
	   AFSDIR_SERVER_CLNTDUMP_FILEPATH);
    printf("AFSDIR_SERVER_CBKDUMP_FILEPATH = %s\n",
	   AFSDIR_SERVER_CBKDUMP_FILEPATH);
    printf("AFSDIR_SERVER_OLDSYSID_FILEPATH = %s\n",
	   AFSDIR_SERVER_OLDSYSID_FILEPATH);
    printf("AFSDIR_SERVER_SYSID_FILEPATH = %s\n",
	   AFSDIR_SERVER_SYSID_FILEPATH);
    printf("AFSDIR_SERVER_FILELOG_FILEPATH = %s\n",
	   AFSDIR_SERVER_FILELOG_FILEPATH);
    printf("AFSDIR_SERVER_AUDIT_FILEPATH = %s\n",
	   AFSDIR_SERVER_AUDIT_FILEPATH);
    printf("AFSDIR_SERVER_KRB_EXCL_FILEPATH  = %s\n",
	   AFSDIR_SERVER_KRB_EXCL_FILEPATH);
    printf("\n");
    printf("\n");
    printf("AFSDIR_CLIENT_THISCELL_FILEPATH = %s\n",
	   AFSDIR_CLIENT_THISCELL_FILEPATH);
    printf("AFSDIR_CLIENT_CELLSERVDB_FILEPATH = %s\n",
	   AFSDIR_CLIENT_CELLSERVDB_FILEPATH);
    printf("\n");
    printf("\n");

    /* test local path construction functions */

#ifdef AFS_NT40_ENV
    printf("ConstructLocalPath(\"C:/fred\", \"/reldir\", &pbuf) = ");
    ConstructLocalPath("C:/fred", "/reldir", &pbuf);
    printf("%s\n", pbuf);
    free(pbuf);
    printf("\n");
#endif

    printf("ConstructLocalPath(\"/fred\", \"/reldir\", &pbuf) = ");
    ConstructLocalPath("/fred", "/reldir", &pbuf);
    printf("%s\n", pbuf);
    free(pbuf);
    printf("\n");

    printf("ConstructLocalPath(\"fred\", \"/reldir\", &pbuf) = ");
    ConstructLocalPath("fred", "/reldir", &pbuf);
    printf("%s\n", pbuf);
    free(pbuf);
    printf("\n");

    printf("ConstructLocalBinPath(\"/fred\", &pbuf) = ");
    ConstructLocalBinPath("/fred", &pbuf);
    printf("%s\n", pbuf);
    free(pbuf);
    printf("\n");

    printf("ConstructLocalBinPath(\"fred\", &pbuf) = ");
    ConstructLocalBinPath("fred", &pbuf);
    printf("%s\n", pbuf);
    free(pbuf);
    printf("\n");

    printf("ConstructLocalLogPath(\"/fred\", &pbuf) = ");
    ConstructLocalLogPath("/fred", &pbuf);
    printf("%s\n", pbuf);
    free(pbuf);
    printf("\n");

    printf("ConstructLocalLogPath(\"fred\", &pbuf) = ");
    ConstructLocalLogPath("fred", &pbuf);
    printf("%s\n", pbuf);
    free(pbuf);
    printf("\n");

    printf("gettmpdir() = %s\n", gettmpdir());
    printf("\n");

    printf("That's all folks!\n");

    return (0);
}
