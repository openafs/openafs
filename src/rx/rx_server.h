/* Application server thread management structures and functions */

/* A server puts itself on an idle queue for a service using an
 * instance of the following structure.  When a call arrives, the call
 * structure pointer is placed in "newcall", the routine to execute to
 * service the request is placed in executeRequestProc, and the
 * process is woken up.  The queue entry's address is used for the
 * sleep/wakeup. If socketp is non-null, then this thread is willing
 * to become a listener thread. A thread sets *socketp to -1 before
 * sleeping. If *socketp is not -1 when the thread awakes, it is now
 * the listener thread for *socketp. When socketp is non-null, tno
 * contains the server's threadID, which is used to make decisions
 * in GetCall.
 */

struct rx_serverQueueEntry {
    struct opr_queue entry;
    struct rx_call *newcall;
#ifdef	RX_ENABLE_LOCKS
    afs_kmutex_t lock;
    afs_kcondvar_t cv;
#endif
    int tno;
    osi_socket *socketp;
};
