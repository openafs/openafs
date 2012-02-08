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

#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>


#include <security/pam_appl.h>
#include <security/pam_modules.h>


#include <sys/param.h>
#include <afs/kautils.h>
#include <stdarg.h>
#include "afs_message.h"
#include "afs_util.h"

static char *fallback_messages[] = {
    "AFS Invalid message requested!",	/* 0: none              */
    "AFS Unknown option: %s",	/* 1: UNKNOWNOPT        */
    "AFS Username unavailable: code = %d",	/* 2: NOUSER            */
    "AFS Username = `%s'",	/* 3: USERNAMEDEBUG     */
    "AFS Password required but not supplied by user %s\n",
    /* 4: PASSWD_REQ        */
    "AFS Password: ",		/* 5: PWD_PROMPT        */
    "AFS Cannot issue prompt",	/* 6: CANNOT_PROMPT     */
    "AFS Trying first password for user %s",	/* 7: GOTPASS           */
    "AFS No first password for user %s\n",	/* 8: NOFIRSTPASS       */
    "AFS Couldn't get passwd via prompt",	/* 9: GETPASS_FAILED    */
    "AFS not available",	/* 10: AFS_UNAVAIL      */
    "AFS error code 0x%x",	/* 11: AFS_ERROR        */
    "AFS Authentication succeeded.\n",	/* 12: LOGIN_OK         */
    "AFS Authentication failed for user %s. %s\n",
    /* 13: LOGIN_FAILED     */
    "AFS PAM error, code=%d",	/* 14: PAMERROR         */
    "AFS uid exceeds OS bounds.\n",	/* 15: UID_OVERFLOW     */
    "The AFS PAM module may not be used from a non-MT program.\n",
    /* 16: NON_MT_PROG      */
    "AFS Options: nowarn=%d, use_first_pass=%d, try_first_pass=%d, ignore_uid = %d, ignore_uid_id = %d, refresh_token=%d, set_token=%d, dont_fork=%d, use_klog=%d",
    /* 17: OPTIONS          */
    "AFS No pam_conv conversation structure found; no user interaction",
    /* 18: NO_USER_INT      */
    "AFS Deleting creds for user %s\n",	/* 19: DELCRED          */
    "AFS Refreshing creds",	/* 20: REFRESHCRED      */
    "AFS Won't use illegal password for user %s",
    /* 21: NILPASSWORD      */
    "AFS Establishing creds for user %s\n",	/* 22: ESTABCRED        */
    "AFS credentials established.\n",	/* 23: PUT_CREDS        */
    "AFS Couldn't find local cell name",	/* 24: NOCELLNAME       */
    "AFS Foreign users are not valid for login.\n",
    /* 25: XENOPHOBIA       */
    "AFS Ignoring superuser %s",	/* 26: IGNORINGROOT     */
    "AFS/local UIDs do not match",	/* 27: UID_MISMATCH     */
    "AFS Rejected foreign user %s",	/* 28: REJ_FOREIGN      */
    "AFS Deleting leftover creds from previous attempt",
    /* 29: LEGACYCREDS      */
    "You have no AFS credentials.\n",	/* 30: NO_CREDS         */
    "AFS ReInitializing creds for user %s\n",	/* 31: REINITCRED       */
    "AFS Failed to set PASSWORD_EXPIRES for user %s\n",
    /* 32: PASSEXPFAIL      */
    "AFS Failed to chown krb ticketfile\n",	/* 33: CHOWNKRB         */
    "AFS Failed to set KRBTKTFILE\n",	/* 34: KRBFAIL          */
    "AFS Unknown remaining lifetime %s using default %d seconds\n",
    /* 35: REMAINLIFETIME   */
    "AFS Session closed",	/* 36: SESSIONCLOSED1   */
    "AFS Session closed, Tokens destroyed\n",	/* 37: SESSIONCLOSED2   */
    "AFS Option conflict dont_fork and use_klog: %s\n",
    /* 38: CONFLICTOPT      */
    "AFS Unknown uid: %s, option ignored\n",
    /* 39: IGNOREUID        */
    "New AFS Password: ",	/* 40: NEW_PWD_PROMPT   */
    "New AFS Password (again): ",	/* 41: VERIFY_PWD_PROMPT */
    "Failed to change AFS password",	/* 42: KRBPASS_FAIL     */
    "Missing PAM flag: %s",	/* 43: FLAGS            */
    "ka error, code=%d",	/* 44: KAERROR          */
    "Passwords are not equal",	/* 45: NE_PASSWORD      */
    "AFS ignoring unregistered user %s\n"	/* 46: IGNORE_UNREG     */
	"Alternate cell name: %s\n",	/* 47: OTHERCELL        */
};

static int num_fallbacks = sizeof(fallback_messages) / sizeof(char *);


char *
pam_afs_message(int msgnum, int *freeit)
{
    /*
     * This really should try to get an NLS message from the message catalog.
     * For now, just return a fallback message.
     */

    if (msgnum > num_fallbacks || msgnum < 1)
	msgnum = 0;

    if (freeit != NULL)
	*freeit = 0;
    return fallback_messages[msgnum];
}


void
pam_afs_syslog(int priority, int msgid, ...)
{
    char *msg = NULL;
    int freeit;
    va_list args;

    msg = pam_afs_message(msgid, &freeit);
    va_start(args, msgid);
    vsyslog(priority, msg, args);
    va_end(args);
    if (freeit)
	free(msg);
}
