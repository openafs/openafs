/* Copyright 1998 - Transarc Corporation */


#ifndef _RX_XMIT_NT_H_
#define _RX_XMIT_NT_H_


typedef struct iovec {
	void *iov_base;
	int iov_len;
} iovec_t;

struct msghdr {
    char * msg_name;
    int msg_namelen;
    iovec_t *msg_iov;
    int msg_iovlen;
    caddr_t msg_accrights;
    int msg_accrightslen;
};

extern int rxi_sendmsg(int socket, struct msghdr *msgP, int flags);
#define sendmsg rxi_sendmsg
extern int rxi_recvmsg(int socket, struct msghdr *msgP, int flags);
#define recvmsg rxi_recvmsg


#endif /* _RX_XMIT_NT_H_ */
