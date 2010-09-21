/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef PAM_AFS_MESSAGE_H
#define PAM_AFS_MESSAGE_H


#define PAMAFS_UNKNOWNOPT	1	/* "Unknown option: %s"              */
#define PAMAFS_NOUSER		2	/* "Username unavailable: code=%d"   */
#define PAMAFS_USERNAMEDEBUG	3	/* "Username = `%s'"                 */
#define PAMAFS_PASSWD_REQ	4	/* "Password required but not supplied" */
#define PAMAFS_PWD_PROMPT	5	/* "AFS Password: "                  */
#define PAMAFS_CANNOT_PROMPT	6	/* "Cannot issue prompt"             */
#define PAMAFS_GOTPASS		7	/* "Trying first password"           */
#define PAMAFS_NOFIRSTPASS	8	/* "No first password"               */
#define PAMAFS_GETPASS_FAILED	9	/* "Couldn't get passwd via prompt"  */
#define PAMAFS_AFS_UNAVAIL	10	/* "AFS not available"               */
#define PAMAFS_AFS_ERROR	11	/* "AFS error code 0x%08x"           */
#define PAMAFS_LOGIN_OK		12	/* "AFS Authentication succeeded.\n" */
#define PAMAFS_LOGIN_FAILED	13	/* "AFS Authentication failed.\n"    */
#define PAMAFS_PAMERROR		14	/* "PAM error, code=%d"              */
#define PAMAFS_UID_OVERFLOW	15	/* "AFS uid exceeds OS bounds.\n"    */
#define PAMAFS_NON_MT_PROG	16	/* "The AFS PAM module may not b..." */
#define PAMAFS_OPTIONS		17	/* "Option: nowarn=%d, use_first..." */
#define PAMAFS_NO_USER_INT	18	/* "No pam_conv conversation str..." */
#define PAMAFS_DELCRED		19	/* "Deleting creds"                  */
#define PAMAFS_REFRESHCRED	20	/* "Refreshing creds"                */
#define PAMAFS_NILPASSWORD	21	/* "Won't use illegal password"      */
#define PAMAFS_ESTABCRED	22	/* "Establishing creds"              */
#define PAMAFS_PUT_CREDS	23	/* "AFS credentials established.\n"  */
#define PAMAFS_NOCELLNAME	24	/* "Couldn't find local cell name"   */
#define PAMAFS_XENOPHOBIA	25	/* "Foreign users are not valid ..." */
#define PAMAFS_IGNORINGROOT	26	/* "Ignoring superuser %s"           */
#define PAMAFS_UID_MISMATCH	27	/* "AFS/local UIDs do not match"     */
#define PAMAFS_REJ_FOREIGN	28	/* "Rejected foreign user %s"        */
#define PAMAFS_LEGACYCREDS	29	/* "Deleting leftover creds from..." */
#define PAMAFS_NO_CREDS		30	/* "You have no AFS credentials.\n"  */
#define PAMAFS_REINITCRED	31	/* "ReInitializing creds"            */
#define PAMAFS_PASSEXPFAIL	32	/* "Failed to set PASSWORD_EXPIRES"  */
#define PAMAFS_CHOWNKRB		33	/* "Failed to chown krb ticketfile"  */
#define PAMAFS_KRBFAIL		34	/* "Failed to set KRBTKTFILE"        */
#define PAMAFS_REMAINLIFETIME   35	/* "Unknown remaining lifetime"      */
#define PAMAFS_SESSIONCLOSED1   36	/* "Session closed"                  */
#define PAMAFS_SESSIONCLOSED2   37	/* "Session closed, Tokens destroyed" */
#define PAMAFS_CONFLICTOPT      38	/* "Option conflict"                 */
#define PAMAFS_IGNOREUID        39	/* "Unknown uid"                     */
#define PAMAFS_NEW_PWD_PROMPT   40	/* "New AFS Password:"               */
#define PAMAFS_VERIFY_PWD_PROMPT  41	/* "New AFS Password (again):"     */
#define PAMAFS_KAPASS_FAIL      42	/* "Failed to change AFS password"   */
#define PAMAFS_FLAGS            43	/* "Missing PAM flag:"               */
#define PAMAFS_KAERROR          44	/* "ka error, code=%d"               */
#define PAMAFS_NE_PASSWORD      45	/* "Passwords are not equal"         */
#define PAMAFS_IGNORE_UNREG     46	/* "AFS ignoring unregistered user"  */
#define PAMAFS_OTHERCELL	47	/* "Alternate cell name"             */

char *pam_afs_message(int msgnum, int *freeit);
void pam_afs_syslog(int priority, int msgid, ...);


#endif
