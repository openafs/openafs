#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/inode.h>
#include <netinet/in.h>
/*
*/
#include "lock.h"
#include "afsint.h"

#include "venus.h"
#define KERNEL
#include "afs.h"
#include "discon.h"
#include "discon_log.h"

#define MAX_NAME  255
#define CELL_DIRTY   0x01

main()
{
        struct ViceIoctl blob;
        dis_setop_info_t info;
        int code;
        int out_code = 0;

        info.op = MERGE_LOG_FILES;

        blob.in_size = sizeof(dis_setop_info_t);
        blob.in = (char *) &info;
        blob.out_size = sizeof(int);
        blob.out = (char *) &out_code;

        code = pioctl(0, VIOCSETDOPS, &blob, 1);
        if (code) {
                perror("bad pioctl");
        } else if (out_code) {
                printf("merge_logs: outcode %d \n", out_code);
        }
}

