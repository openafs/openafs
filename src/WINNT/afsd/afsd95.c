/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

/*#include <windows.h>*/
#include <string.h>
/*#include <nb30.h>*/

#include <stdio.h>
#include <stdlib.h>

#include <osi.h>
#include <signal.h>
#include <afs/cmd.h>
/*#include <winsock2.h>*/
#include "afsd.h"
#include "afsd_init.h"


char main_statusText[100];
osi_log_t *afsd_logp;

extern int traceOnPanic;
BOOL InitInstance(struct cmd_syndesc *as, char *arock);
extern int afs_shutdown;
int tried_shutdown=0;

int afs_current_status = AFS_STATUS_INIT;

/*
 * Notifier function for use by osi_panic
 */
void afsd_notifier(char *msgp, char *filep, long line)
{
	char tbuffer[100];
	if (filep)
		sprintf(tbuffer, "Error at file %s, line %d", filep, line);
	else
		strcpy(tbuffer, "Error at unknown location");

	if (!msgp)
		msgp = "Assertion failure";

	/*MessageBox(NULL, tbuffer, msgp, MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);*/

	afsd_ForceTrace(TRUE);
        buf_ForceTrace(TRUE);

	if (traceOnPanic) {
		/*asm("int 3");*/
	}

	afs_exit(AFS_EXITCODE_PANIC);
}

/* Init function called when window application starts.  Inits instance and
 * application together, since in Win32 they're essentially the same.
 *
 * Function then goes into a loop handling user interface messages.  Most are
 * used to handle redrawing the icon.
 */
int main(int argc, char *argv[])
{
    struct cmd_syndesc *ts;
    
    fprintf(stderr, "AFS Client for Windows 95.\n");
    /*fprintf(stderr, "Use Ctrl-C to shut down client.\n\n\n");*/
    ts = cmd_CreateSyntax((char *) 0, (int (*)()) InitInstance, (char *) 0, "start AFS");
    cmd_AddParm(ts, "-lanadapt", CMD_SINGLE, CMD_OPTIONAL, "LAN adapter number");
    cmd_AddParm(ts, "-threads", CMD_SINGLE, CMD_OPTIONAL, "Number of server threads");
    cmd_AddParm(ts, "-rootvol", CMD_SINGLE, CMD_OPTIONAL, "name of AFS root volume");
    cmd_AddParm(ts, "-stat", CMD_SINGLE, CMD_OPTIONAL, "number of stat entries");
    cmd_AddParm(ts, "-memcache", CMD_FLAG, CMD_OPTIONAL, "use memory cache");
    cmd_AddParm(ts, "-cachedir", CMD_SINGLE, CMD_OPTIONAL, "cache directory");
    cmd_AddParm(ts, "-mountdir", CMD_SINGLE, CMD_OPTIONAL, "mount location");
    cmd_AddParm(ts, "-daemons", CMD_SINGLE, CMD_OPTIONAL, "number of daemons to use");
    cmd_AddParm(ts, "-nosettime", CMD_FLAG, CMD_OPTIONAL, "don't set the time");
    cmd_AddParm(ts, "-verbose", CMD_FLAG, CMD_OPTIONAL, "display lots of information");
    cmd_AddParm(ts, "-debug", CMD_FLAG, CMD_OPTIONAL, "display debug info");
    cmd_AddParm(ts, "-chunksize", CMD_SINGLE, CMD_OPTIONAL, "log(2) of chunk size");
    cmd_AddParm(ts, "-dcache", CMD_SINGLE, CMD_OPTIONAL, "number of dcache entries");
    cmd_AddParm(ts, "-confdir", CMD_SINGLE, CMD_OPTIONAL, "configuration directory");
    cmd_AddParm(ts, "-logfile", CMD_SINGLE, CMD_OPTIONAL, "Place to keep the CM log");
    cmd_AddParm(ts, "-waitclose", CMD_FLAG, CMD_OPTIONAL, "make close calls synchronous");
    cmd_AddParm(ts, "-shutdown", CMD_FLAG, CMD_OPTIONAL, "Shutdown all afs state");
    cmd_AddParm(ts, "-sysname", CMD_SINGLE, CMD_OPTIONAL, "System name (@sys value)");
    cmd_AddParm(ts, "-gateway", CMD_FLAG, CMD_OPTIONAL, "machine is a gateway");
    cmd_AddParm(ts, "-tracebuf", CMD_SINGLE, CMD_OPTIONAL, "trace buffer size");
    cmd_AddParm(ts, "-startup", CMD_FLAG, CMD_OPTIONAL, "start AFS client");
    cmd_AddParm(ts, "-diskcache", CMD_SINGLE, CMD_OPTIONAL, "diskcache size");
    cmd_AddParm(ts, "-afsdb", CMD_FLAG, CMD_OPTIONAL, "use DNS for cell server resolution");
    cmd_AddParm(ts, "-freelance", CMD_FLAG, CMD_OPTIONAL, "virtual AFS root");

    return (cmd_Dispatch(argc, argv));
}

/* initialize the process.  Reads the init files to get the appropriate
 * information. */
void vxd_Shutdown(void);
int afsd_shutdown(int);
int shutdown_handler(int);

BOOL InitInstance(struct cmd_syndesc *as, char *arock)
{
        long code;
	char *reason;

#ifdef DJGPP
	osi_Init();
#endif
 
#ifndef DJGPP
	osi_InitPanic(afsd_notifier);
#endif

        /*sleep(10);*/
        
	afsi_start();

        code = afsMsg_Init();
	if (code != 0)
		osi_panic("socket failure", __FILE__, __LINE__);
        
        code = afsd_InitCM(&reason, as, arock);
	if (code != 0)
		osi_panic(reason, __FILE__, __LINE__);

	code = afsd_InitDaemons(&reason);
	if (code != 0)
		osi_panic(reason, __FILE__, __LINE__);

        code = afsd_InitSMB(&reason);
	if (code != 0)
		osi_panic(reason, __FILE__, __LINE__);

        signal(SIGINT, shutdown_handler);

        thrd_Yield();   /* give new threads a chance to run */
        
        /* send message to GUI caller indicating successful init */
        afs_current_status = AFS_STATUS_RUNNING;
        afsMsg_StatusChange(afs_current_status, 0, NULL);

#ifdef DJGPP
	/* Keep the process from just terminating */
	while(afs_shutdown == 0)
        {
        /*IOMGR_Sleep(180);*/
          IOMGR_Sleep(8);
		/* workaround: WaitForKeystroke(nonzero num) calls 
		   IOMGR_Select, though Win95 select works only on sockets */
		/* so, we poll instead */
		/*if (LWP_WaitForKeystroke(0))
                  break;*/
        }
        afsd_shutdown(0);
#endif
        afs_exit(0);
        
	return (TRUE);
}

int shutdown_handler(int x)
{
  if (!tried_shutdown)
  {
    fprintf(stderr, "This program should not be shut down manually.  It should "
           "be shut down by the\nWindows AFS Client Control Center.  Press Ctrl-C "
            "again if you really want to do this.\n");
    fflush(stderr);
    tried_shutdown = 1;
  }
  else
  {
    fprintf(stderr, "Shutting down AFSD...\n");
    fflush(stderr);
    afs_shutdown = 1;
  }
}

int afsd_shutdown(int x)
{
#ifdef AFS_VXD
  vxd_Shutdown();
#else
  smb_Shutdown();
#endif
  
  fprintf(stderr, "AFSD shutdown complete.\n");
  /*exit(0);*/
}

void afs_exit(int exitCode)
{
  afs_current_status = AFS_STATUS_EXITING;
  afsMsg_StatusChange(afs_current_status,
                      exitCode, NULL);
  afsMsg_Shutdown();
  exit(exitCode);
}
