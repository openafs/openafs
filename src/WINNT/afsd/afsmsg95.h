#ifndef _AFSMSG_H
#define _AFSMSG_H

#define AFS_MSG_PORT 2020

#define AFS_MAX_MSG_LEN 512

typedef struct _afsMsg_hdr {
  int msgtype;
  int length;
} afsMsg_hdr_t;

#define AFS_MSG_STATUS_CHANGE 1
#define AFS_MSG_PRINT 2

typedef struct _afsMsg_statChange {
  afsMsg_hdr_t hdr;
  int oldStatus;
  int newStatus;
  int exitCode;
  char message;
} afsMsg_statChange_t;

#define AFS_STATUS_NOSTATUS -1
#define AFS_STATUS_INIT    1
#define AFS_STATUS_RUNNING 2
#define AFS_STATUS_EXITING 3

#define AFS_EXITCODE_NORMAL 0
#define AFS_EXITCODE_PANIC  2
#define AFS_EXITCODE_NETWORK_FAILURE 3
#define AFS_EXITCODE_GENERAL_FAILURE 100

typedef struct _afsMsg_print {
  afsMsg_hdr_t hdr;
  int debugLevel;
  char message;
} afsMsg_print_t;

#endif
