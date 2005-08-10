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
    ("$Header: /cvs/openafs/src/pam/test_pam.c,v 1.7.2.1 2005/05/30 03:37:48 shadow Exp $");

#include <stdio.h>
#include <security/pam_appl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


static int my_conv(int num_msg, struct pam_message **msg,
		   struct pam_response **response, void *appdata_ptr);


static struct pam_conv pam_conv = { &my_conv, NULL };


static pam_handle_t *pamh;


static const char *service = "afstest";
static const char *new_envstring = "GOTHEREVIATESTPAM=1";
static const char *new_homestring = "HOME=/tmp";

#if defined(AFS_LINUX20_ENV) || defined(AFS_FBSD_ENV) || defined(AFS_NBSD_ENV)
#define getpassphrase getpass
#endif


void
main(int argc, char *argv[])
{
    int authenticated = 0;
    int retcode;
    char *username;
    int setcred = 1;

    if (argc < 2 || argc > 3) {
	fprintf(stderr, "Usage: %s [-u] <user>\n", argv[0]);
	exit(1);
    }
    if (argc == 3) {
	if (strcmp(argv[1], "-u") != 0) {
	    fprintf(stderr, "Usage: %s [-u] <user>\n", argv[0]);
	    exit(1);
	}
	/* service = "unixtest"; */
	setcred = 0;
	username = argv[2];
    } else {
	username = argv[1];
    }

    if ((retcode =
	 pam_start(service, username, &pam_conv, &pamh)) != PAM_SUCCESS) {
	fprintf(stderr, "PAM error %d\n", retcode);
	exit(1);
    }

    authenticated = ((retcode = pam_authenticate(pamh, 0)) == PAM_SUCCESS);

    if (!authenticated) {
	fprintf(stderr, "PAM couldn't authenticate you.\n");
	pam_end(pamh, PAM_ABORT);
	exit(1);
    }

    if ((retcode = pam_acct_mgmt(pamh, 0)) != PAM_SUCCESS) {
	fprintf(stderr, "pam_acct_mgmt returned %d.\n", retcode);
	pam_end(pamh, PAM_ABORT);
	exit(1);
    }

    /* pam_open_session */

    if (setcred)
	if ((retcode = pam_setcred(pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS) {
	    fprintf(stderr, "pam_setcred returned %d.\n", retcode);
	    pam_end(pamh, PAM_ABORT);
	    exit(1);
	}

    if ((retcode = pam_open_session(pamh, PAM_SILENT)) != PAM_SUCCESS) {
	fprintf(stderr, "pam_open_session returned %d.\n", retcode);
	pam_end(pamh, PAM_ABORT);
	exit(1);
    }
    pam_end(pamh, PAM_SUCCESS);

    putenv(new_envstring);
    putenv(new_homestring);
    chdir("/tmp");
    printf("Type exit to back out.\n");
    execl("/bin/csh", "/bin/csh", NULL);
}


static int
my_conv(int num_msg, struct pam_message **msg, struct pam_response **response,
	void *appdata_ptr)
{
    struct pam_message *m;
    struct pam_response *r;
    char *p;

    m = *msg;
    if (response) {
	*response = calloc(num_msg, sizeof(struct pam_response));
	if (*response == NULL)
	    return PAM_BUF_ERR;
	r = *response;
    } else {
	r = NULL;
    }

    while (num_msg--) {
	switch (m->msg_style) {
	case PAM_PROMPT_ECHO_OFF:
#ifdef __hpux
	    /* ON HP's we still read 8 chars */
	    if (r)
		r->resp = strdup(getpass(m->msg));
#else
	    if (r)
		r->resp = strdup(getpassphrase(m->msg));
#endif
	    break;
	case PAM_PROMPT_ECHO_ON:
	    fputs(m->msg, stdout);
	    if (r) {
		r->resp = malloc(PAM_MAX_RESP_SIZE);
		fgets(r->resp, PAM_MAX_RESP_SIZE, stdin);
		r->resp[PAM_MAX_RESP_SIZE - 1] = '\0';
		p = &r->resp[strlen(r->resp) - 1];
		while (*p == '\n' && p >= r->resp)
		    *(p--) = '\0';
	    }
	    break;
	case PAM_ERROR_MSG:
	    fputs(m->msg, stderr);
	    break;
	case PAM_TEXT_INFO:
	    fputs(m->msg, stdout);
	    break;
	default:
	    break;
	}
	m++;
	if (r)
	    r++;
    }
    return PAM_SUCCESS;
}
