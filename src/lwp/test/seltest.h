/* seltest.h - Common RPC codes from client and server. */

#ifndef _SELTEST_H_
#define _SELTEST_H_


typedef struct {
#define SC_PROBE 0
#define SC_WRITE 1
#define SC_END   2 /* server terminates with this. */
    int sc_cmd;

    int sc_delay;

#define SC_WAIT_ONLY 0
#define SC_WAIT_OOB  1
    int sc_flags;
    int sc_info; /* stores size of write for SC_WRITE */
} selcmd_t;

#define MIN_D_VALUE 0
#define MAX_D_VALUE 254
#define END_DATA 255

/* Function Declarations. */
void OpenFDs(int);
void assertNullFDSet(int fd, fd_set *);
void Die(int flag, char *);
void Log(char *fmt, ...);
void sendOOB(int);
void recvOOB(int);

#ifdef NEEDS_ALLOCFDSET
/* Include these if testing against 32 bit fd_set IOMGR. */
fd_set *IOMGR_AllocFDSet(void);
void IOMGR_FreeFDSet(fd_set *fds);
#endif

#endif /* _SELTEST_H_ */
