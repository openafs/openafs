#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <lwp.h>
#include <rx/rx.h>
#include <afs/bubasics.h>
#include "bc.h"

extern struct rx_securityClass *rxnull_NewClientSecurityObject();

#include "AFS_component_version_number.c"

main(argc, argv)
int argc;
char **argv; {
    struct rx_securityClass *rxsc;
    register struct rx_connection *tconn;
    register afs_int32 code;

    rx_Init(0);
    rxsc = rxnull_NewClientSecurityObject();
    tconn = rx_NewConnection(htonl(0x7f000001), htons(BC_MESSAGEPORT), 1, rxsc, 0);
    code = BC_Print(tconn, 1, 2, argv[1]);
    printf("Done, code %d\n", code);
    exit(0);
}
