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

struct osi_audit_ops {
    void (*send_msg)(void);
    void (*append_msg)(const char *format, ...);
    int  (*open_file)(const char *fileName);
    void (*print_interface_stats)(FILE *out);
};

#endif /* _AUDIT_API_H */
