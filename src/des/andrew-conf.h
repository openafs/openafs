/*
 * Andrew configuration.
 */

#ifdef vax
#include "conf-bsdvax.h"
#else
#if mips && !defined(sgi)
#include "conf-mips.h"
#else
#if	defined(sun) && !defined(AFS_X86_ENV)
#include "conf-bsd-sun.h"
#else
#ifdef	AFS_AIX_ENV
#include "conf-aix-ibm.h"
#else
#ifdef mac2
#include "conf-bsd-mac.h"
#else
#ifdef AFS_HPUX_ENV
#ifdef	hp9000s300
#include "conf-hp9000s300.h"
#else
#include "conf-hp9000s700.h"
#endif
#else
#ifdef NeXT
#include "conf-next.h"
#else
#if defined(sgi)
#include "conf-sgi.h"
#else
#ifdef AFS_OSF_ENV
#if defined(__alpha)
#include "conf-bsd-alpha.h"
#else
#error unknown osf
#endif
#else
#if	defined(AFS_X86_ENV) && !defined(AFS_DARWIN_ENV)
#include "conf-bsd-ncr.h"
#else
#ifdef AFS_NT40_ENV
#include "conf-winnt.h"
#else

#ifdef AFS_XBSD_ENV
#ifdef AFS_X86_XBSD_ENV
#include "conf-i386-obsd.h"
#elif defined(AFS_ALPHA_ENV)
#include "conf-alpha-bsd.h"
#else
#error unknown bsd
#endif
#else /* AFS_XBSD_ENV */

#if defined(AFS_LINUX20_ENV)
#ifdef AFS_PARISC_LINUX20_ENV
#include "conf-parisc-linux.h"
#else
#ifdef AFS_PPC_LINUX20_ENV
#include "conf-ppc-linux.h"
#else
#ifdef AFS_SPARC_LINUX20_ENV
#include "conf-sparc-linux.h"
#else
#ifdef AFS_SPARC64_LINUX20_ENV
#include "conf-sparc64-linux.h"
#else
#ifdef AFS_S390_LINUX20_ENV
#include "conf-s390-linux.h"
#else
#ifdef AFS_ALPHA_LINUX20_ENV
#include "conf-alpha-linux.h"
#else
#ifdef AFS_IA64_LINUX20_ENV
#include "conf-ia64-linux.h"
#else
#ifdef AFS_AMD64_LINUX20_ENV
#include "conf-amd64-linux.h"
#else
#ifdef AFS_PPC64_LINUX20_ENV
#include "conf-ppc64-linux.h"
#else
#include "conf-i386-linux.h"
#endif /* AFS_PPC64_LINUX20_ENV */
#endif /* AFS_AMD64_LINUX20_ENV */
#endif /* AFS_IA64_LINUX20_ENV */
#endif /* AFS_ALPHA_LINUX20_ENV */
#endif /* AFS_S390_LINUX20_ENV */
#endif /* AFS_SPARC64_LINUX20_ENV */
#endif /* AFS_SPARC_LINUX20_ENV */
#endif /* AFS_PPC_LINUX20_ENV */
#endif /* AFS_PARISC_LINUX24_ENV */
#else
#if defined(AFS_DARWIN_ENV)
#include "conf-darwin.h"
#else
Sorry,
    you lose.
    Figure out what the machine looks like and fix this file to include it.
#endif
#endif /* AFS_LINUX20_ENV */
#endif /* AFS_NT40_ENV */
#endif /* AFS_XBSD_ENV */
#endif /* NCR || X86 */
#endif /* __alpha */
#endif /* SGI */
#endif /* NeXT */
#endif /* HP/UX */
#endif /* mac */
#endif /* aix */
#endif /* sun */
#endif /* mips */
#endif /* not vax */
