/*
 * Andrew configuration.
 */

#include <afs/param.h>
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
#ifdef	__alpha
#include "conf-bsd-alpha.h"
#else
#if	defined(AFS_X86_ENV)
#include "conf-bsd-ncr.h"
#else
#ifdef AFS_NT40_ENV
#include "conf-winnt.h"
#else
#ifdef AFS_LINUX20_ENV
#include "conf-i386-linux.h"
#else
Sorry, you lose.
Figure out what the machine looks like and fix this file to include it.
#endif /* AFS_LINUX20_ENV */
#endif /* AFS_NT40_ENV */
#endif /* NCR || X86 */
#endif /* __alpha */
#endif /* SGI */
#endif /* NeXT */
#endif	/* HP/UX */
#endif /* mac */
#endif	/* aix */
#endif /* sun */
#endif /* mips */
#endif /* not vax */
