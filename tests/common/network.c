#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/cellconfig.h>
#include <rx/rx.h>

#include <tests/tap/basic.h>
#include "common.h"

/*!
 * Check if the current machine's hostname resolves to the loopback
 * network.
 */
int
afstest_IsLoopbackNetworkDefault(void)
{
    char hostname[MAXHOSTCHARS];
    afs_uint32 addr;
    struct hostent *host;

    gethostname(hostname, sizeof(hostname));
    host = gethostbyname(hostname);
    if (!host) {
	skip_all("Can't resolve hostname %s\n", hostname);
    }
    memcpy(&addr, host->h_addr, sizeof(addr));

    return(rx_IsLoopbackAddr(ntohl(addr)));
}

/*!
 * Skips all TAP tests if the current machine's hostname resolves to the
 * loopback network.
 */
int
afstest_SkipTestsIfLoopbackNetIsDefault(void)
{
    int retval;

    retval = afstest_IsLoopbackNetworkDefault();
    if (retval == 1) {
	skip_all("Default IP address is on the loopback network!\n");
    }
    return retval;
}

/*!
 * Skips all TAP tests if the current machine's hostname can't be resolved
 * to any IP address.
 */
void
afstest_SkipTestsIfBadHostname(void)
{
    char hostname[MAXHOSTCHARS];
    struct hostent *host;

    gethostname(hostname, sizeof(hostname));
    host = gethostbyname(hostname);
    if (!host)
	skip_all("Can't resolve hostname %s\n", hostname);
}

/*!
 * Skips all TAP tests if a server is already running on this system.
 *
 * \param name[in]  IANA service name, e.g. "afs3-vlserver"
 */
void
afstest_SkipTestsIfServerRunning(char *name)
{
    afs_int32 code;
    osi_socket sock;
    struct sockaddr_in addr;
    afs_int32 service;

    service = afsconf_FindService(name);
    if (service == -1) {
	fprintf(stderr, "Unknown service name: %s\n", name);
	exit(1);
    }
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == OSI_NULLSOCKET) {
	fprintf(stderr, "Failed to get socket file descriptor.\n");
	exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = service; /* Already in network byte order. */
#ifdef STRUCT_SOCKADDR_HAS_SA_LEN
    addr.sin_len = sizeof(addr);
#endif
    code = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (code < 0) {
	if (errno == EADDRINUSE) {
	    skip_all("Service %s is already running.\n", name);
	} else {
	    perror("bind");
	    exit(1);
	}
    }
    close(sock);
}
