/*

@doc

@module mitwhich.h |

some defines so that we can figure out which MS OS and subsystem an
application is running under. Also support for finding out which
TCP/IP stack is being used. This is useful when you need to find out
about the domain or the nameservers.

 */

#if !defined( __MIT_WHICH_H )
#define __MIT_WHICH_H

// these should become resources and loaded at run time
#define NT_32 "Winsock 2.0"
#define NT_16 "Windows NT 16-bit Windows Sockets"
#define W95_32 "Microsoft Windows Sockets Version 1.1."
#define W95_16 "Microsoft Windows Sockets Version 1.1."
#define LWP_16 "Novell Winsock version 1.1"
// Note that these are currently in wshelper.h and should be somewhere else
#define MS_NT_32 1
#define MS_NT_16 2
#define MS_95_32 3
#define MS_95_16 4
#define NOVELL_LWP_16 5       
	
#define MS_OS_WIN 1
#define MS_OS_95 2
#define MS_OS_NT 4
#define MS_OS_2000 12
#define MS_OS_XP 28
#define MS_OS_2003 60
#define MS_OS_NT_UNKNOWN 124
#define MS_OS_UNKNOWN 0
	
#define STACK_UNKNOWN 0
#define UNKNOWN_16_UNDER_32 -2
#define UNKNOWN_16_UNDER_16 -3
#define UNKNOWN_32_UNDER_32 -4
#define UNKNOWN_32_UNDER_16 -5


/*
   @comm these are the current MIT DNS servers, the wshelper and
   wshelp32 DLLs will do their best to find the correct DNS servers
   for the local machine however, if all else fails these will be used
   as a last resort. Site administrators outside of the MIT domain
   should change these defaults to their own defaults either by
   editing this file and recompiling or by editing the string tables
   of the binaries. Don't use App Studio to edit the .RC files.
<nl>
	#define DNS1	"18.70.0.160" <nl>
	#define DNS2	"18.71.0.151" <nl>
	#define DNS3	"18.72.0.3"   <nl>
<nl>
	#define DEFAULT_DOMAIN "mit.edu" <nl>
*/

#define DNS1	"18.70.0.160"
#define DNS2	"18.71.0.151"
#define DNS3	"18.72.0.3"

#define DEFAULT_DOMAIN "mit.edu"


#ifndef _PATH_RESCONF
#if !defined(WINDOWS) && !defined(_WINDOWS) && !defined(_WIN32)
#define _PATH_RESCONF  "/etc/resolv.conf"
#else
#define _PATH_RESCONF  "c:/net/tcp/resolv.cfg"
#endif
#endif


/* Microsoft TCP/IP registry values that we care about */
#define NT_TCP_PATH    "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define NT_TCP_PATH_TRANS "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Transient"
#define W95_TCP_PATH   "SYSTEM\\CurrentControlSet\\Services\\VxD\\MSTCP"

#define NT_DOMAIN_KEY  "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Domain"
#define NT_NS_KEY      "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\NameServer"

#define W95_DOMAIN_KEY "SYSTEM\\CurrentControlSet\\Services\\VxD\\MSTCP\\Domain"
#define W95_NS_KEY     "SYSTEM\\CurrentControlSet\\Services\\VxD\\MSTCP\\NameServer"

/*

  @comm Notes on different Winsock stack configuration files.<nl>

  The Microsoft stacks for Windows95 and NT 3.x, 4.x use the registry
  to store the default domain and the IP addresses of the DNS
  servers. The wshelper and wshelp32 will use the registry information
  when possible.<nl>

  Novell's LAN WorkPlace stack and the much older Excelan products use
  a resolv.cfg file to store this information. The resolv.cfg file
  could be located in a number of places over the years. Our DLL will
  try to search for it in:<nl><nl>

  C:\etc\resolv.cfg<nl>
  C:\excelan\tcp\resolv.cfg<nl>
  C:\net\tcp\resolv.cfg<nl>
  %NDIR%\etc\resolv.cfg<nl>
  %NDIR%\tcp\resolv.cfg<nl><nl>

  where %NDIR% is the expansion of the local environment variable
  NDIR. So setting NDIR to be N:\COMMON\NET would mean that we would
  also look in N:\common\net\etc\resolv.cfg and
  N:\common\net\tcp\resolv.cfg for the domain and nameserver
  information.<nl><nl>

  Here is a sample resolv.cfg file<nl>

  ; LAN WorkPlace resolver configuration file<nl>
  domain mit.edu<nl>

  nameserver 18.70.0.160<nl>
  nameserver 18.71.0.151<nl>
  nameserver 18.72.0.3<nl>
  ; end of file<nl><nl>

  The TRUMPET Winsock stack uses a TRUMPETWSK.INI file to store the
  domain and nameserver configuration information. The section tag is
  [Trumpet Winsock]. The domain information is identified by domain=
  and the nameserver information is identified by a single name, dns=,
  multiple nameservers may be specified on the same line and they
  should be space delimited.<nl><nl>

  trupwsk.ini<nl>
  [Trumpet Winsock]<nl>
  dns=18.71.0.151 18.70.0.160 18.72.0.3<nl>
  domain=mit.edu<nl><nl>

  Core Internet-Connect uses a CORE.INI file, nameservers are comma
  delimited.<nl>

  [winsock]<nl>
  domainname=mit.edu<nl>
  nameservers=18.71.0.151, 18.70.0.160, 18.72.0.3<nl><nl>

  FTP software uses a PCTCP.INI file. This file may be located by use
  of the environment variable PCTCP.<nl>

  [pctcp general]<nl>
  domain=mit.edu<nl>
  [pctcp addresses]<nl>
  domain-name-server=18.70.0.160<nl>
  domain-name-server=18.71.0.151<nl>
  domain-name-server=18.72.0.3<nl>

*/
	   
#endif // __MIT_WHICH_H
