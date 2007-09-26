#include <signal.h>
#include <sys/types.h>
#ifdef __svr4__
#  define HAVE_PRUSAGE 1
#else
# ifdef bsd42
#  define HAVE_RUSAGE 1
# endif
#endif

#if defined(__GNUC__)
#define VOLATILE /**/
#endif

/* if it can't find these files, put a '#' at the start
 * of the CFLAGS line in the makefile, so that "bsd42"
 * will not be defined.
 */
#ifdef HAVE_RUSAGE
#       include <time.h>
#	include <sys/time.h>
#	include <sys/resource.h>
        typedef struct 
        {
                struct rusage rus;
                struct timeval rue;
        } CPU_TIME;
#       define USER_SEC(X) (X)->rus.ru_utime.tv_sec
#       define USER_FRAC(X) (X)->rus.ru_utime.tv_usec
#       define ELAP_SEC(X) (X)->rue.tv_sec
#       define ELAP_FRAC(X) (X)->rue.tv_usec
#endif /* HAVE_RUSAGE */

#ifdef HAVE_PRUSAGE
#       include <sys/fcntl.h>
#       include <sys/fault.h>
#       include <sys/syscall.h>
#       include <sys/procfs.h>
        typedef struct 
        {
                prusage_t rus;
#       ifdef HI_RES_CLOCK
                timestruc_t rue;
#       else
                struct timeval rue;
#       endif
        } CPU_TIME;
#       define USER_SEC(X) (X)->rus.pr_utime.tv_sec
#       define USER_FRAC(X) (X)->rus.pr_utime.tv_nsec
#       define ELAP_SEC(X) (X)->rue.tv_sec
#       ifdef HI_RES_CLOCK
#               define ELAP_FRAC(X) (X)->rue.tv_nsec
#       else
#               define ELAP_FRAC(X) (X)->rue.tv_usec
#       endif
/* for solaris... */
#	if defined(sparc) && defined(__svr4__)
#		define CPUHZ _sysconf(3)
#	endif
#endif /* HAVE_PRUSAGE */

#if !defined(HAVE_RUSAGE) && !defined(HAVE_PRUSAGE)
#	include <sys/times.h>
	typedef struct {
		struct tms ts;
		unsigned long te;
	}  CPU_TIME;
#	if defined(sparc) && defined(__svr4__)
#		define CPUHZ _sysconf(3)
#	endif
#	if defined(u3b2) || defined(u3b5) || defined(u3b15)
#		define CPUHZ 100
#	endif
#	if M_XENIX && defined(sys5)
#		define CPUHZ atoi(getenv("HZ")) /* login does this */
#	endif
#	ifndef CPUHZ
#		define CPUHZ 60
#	endif
#endif

#ifndef mysig_return_type
#       if 1
#	        define	mysig_return_type	void
#       else
#	        define	mysig_return_type	int
#       endif
#endif

char *sftime();
char *sfclock();
mysig_return_type onalarm();
#ifndef VOLATILE
#define VOLATILE volatile
#else
#endif
#ifndef LONGLONG
#define LONGLONG long long
#else
#endif
VOLATILE extern int went_off;
char *getenv();

extern int cpu_flag;

void mark_time(CPU_TIME *);
int nset_alarm(int);
int set_alarm(void);
int cpuhz(void);
