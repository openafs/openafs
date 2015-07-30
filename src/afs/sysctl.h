#ifndef AFS_SYSCTL_H
#define AFS_SYSCTL_H

#define AFS_SC_ALL                              0
#define AFS_SC_DARWIN                           1
#define AFS_SC_AIX				2
#define AFS_SC_DFBSD				3
#define AFS_SC_FBSD				4
#define AFS_SC_LINUX				5
#define AFS_SC_HPUX				6
#define AFS_SC_IRIX				7
#define AFS_SC_NBSD				8
#define AFS_SC_OBSD				9
#define AFS_SC_SOLARIS				10
#define AFS_SC_UKERNEL				11

/* AFS_SC_ALL: platform-independent sysctls */

/* AFS_SC_DARWIN: darwin platforms */
#define AFS_SC_DARWIN_ALL                       0
#define AFS_SC_DARWIN_60                        4
#define AFS_SC_DARWIN_70                        5
#define AFS_SC_DARWIN_80                        6
#define AFS_SC_DARWIN_90                        7
#define AFS_SC_DARWIN_100                       8
#define AFS_SC_DARWIN_110                       9
#define AFS_SC_DARWIN_120                      10
#define AFS_SC_DARWIN_130                      11
#define AFS_SC_DARWIN_140                      12

/* AFS_SC_DARWIN_ALL sysctls */
#define AFS_SC_DARWIN_ALL_REALMODES             1
#define AFS_SC_DARWIN_ALL_FSEVENTS		2

/* AFS_SC_AIX: aix platforms */
#define AFS_SC_AIX_ALL				0
#define AFS_SC_AIX_43				1
#define AFS_SC_AIX_51				2
#define AFS_SC_AIX_52				3
#define AFS_SC_AIX_53				4
#define AFS_SC_AIX_61				5

/* AFS_SC_FBSD: freebsd platforms */
#define AFS_SC_FBSD_ALL				0
#define AFS_SC_FBSD_70				1
#define AFS_SC_FBSD_71				2
#define AFS_SC_FBSD_72				3
#define AFS_SC_FBSD_80				4

/* AFS_SC_LINUX: linux platforms */
#define AFS_SC_LINUX_ALL			0
#define AFS_SC_LINUX_22				1
#define AFS_SC_LINUX_24				2
#define AFS_SC_LINUX_26				3

#endif
