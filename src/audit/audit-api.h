/*
 * Copyright 2009, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _AUDIT_API_H
#define _AUDIT_API_H

#define OSI_AUDIT_MAXMSG 2048    /* Max amount of text in auditmsg */

struct osi_audit_ops {
    /*
     * send_msg - send a formatted message to interface output
     *     Called from osi_audit
     */
    void (*send_msg)(void *rock, const char *message, int msglen, int truncated);

    /*
     * open_file - process file specification parameter
     *     Called from osi_audit_file during commandline processing
     */
    int  (*open_file)(void *rock, const char *fileName);

    /*
     * print_interface_stats - print stats
     *     Called from audit_Printstats
     */
    void (*print_interface_stats)(void *rock, FILE *out);

    /*
     * create_interface - create a new instance of the interface
     *     Called from osi_audit_file during command line processing
     */
    void *(*create_interface)(void);

    /*
     * close_interface - stop the interface and free resources
     *     Called from osi_audit_close during daemon shutdown
     */
    void (*close_interface)(void **rock);

    /* Following are optional functions.  Set to NULL if not implemented */

    /*
     * set_option - process optional parameter
     *     Called from osi_audit_file during command line processing
     */
    int  (*set_option)(void *rock, char *opt, char *val);

    /*
     * open_interface - complete interface initialization
     *     Called from osi_audit_open during daemon initialization
     *            after command line processing
     */
    void (*open_interface)(void *rock);
};

#endif /* _AUDIT_API_H */
