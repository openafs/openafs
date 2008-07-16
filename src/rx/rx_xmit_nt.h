/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _RX_XMIT_NT_H_
#define _RX_XMIT_NT_H_


typedef struct iovec
#ifndef AFS_DJGPP_ENV
{
    void *iov_base;
    int iov_len;
}
#endif
iovec_t;

struct msghdr {
    char *msg_name;
    int msg_namelen;
    iovec_t *msg_iov;
    int msg_iovlen;
    caddr_t msg_accrights;
    int msg_accrightslen;
};

extern int rxi_sendmsg(osi_socket socket, struct msghdr *msgP, int flags);
#define sendmsg rxi_sendmsg
extern int rxi_recvmsg(osi_socket socket, struct msghdr *msgP, int flags);
#define recvmsg rxi_recvmsg
#endif /* _RX_XMIT_NT_H_ */
