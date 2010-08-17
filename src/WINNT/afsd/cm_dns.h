/* Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Well-known DNS port is 53 (for both TCP and UDP,
   although UDP is typically the only one used) */

#define DNS_PORT    53

/* this function will continue to return cell server
   names for the given cell, ending in null */
int getAFSServer(const char *service, const char *protocol, const char *cellname,
                 unsigned short afsdbPort,
                 int *cellHostAddrs, char cellHostNames[][MAXHOSTCHARS],
                 unsigned short ports[], unsigned short ipRanks[], int *numServers, int *ttl);

/* Same as above, but using cm_unichar_t.  Note that this functon will
   only be defined for DNSAPI_ENV. */
int getAFSServerW(const cm_unichar_t *service, const cm_unichar_t *protocol, const cm_unichar_t *cellName,
                  unsigned short afsdbPort,
                  int *cellHostAddrs,
                  cm_unichar_t cellHostNames[][MAXHOSTCHARS],
                  unsigned short ports[],
                  unsigned short ipRanks[],
                  int *numServers, int *ttl);

/* a supplement for the DJGPP gethostbyname ... which
   never bothers calling a DNS server ... so this function
   takes care of that. This should be called when you
   failed with gethostbyname (as that WILL check for
   dotted decimal, and local hostfile) */

struct hostent *DNSgetHostByName(char *hostname);



