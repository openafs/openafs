#include <afs/param.h>
#ifdef AFS_HPUX_ENV
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>

/* insque/remque moved to timer.c where they are used. */

#ifndef AFS_HPUX102_ENV
utimes(file,tvp)
char *file;
struct timeval tvp[2];
{
	struct utimbuf times;
	
	times.actime = tvp[0].tv_sec;
	times.modtime = tvp[1].tv_sec;
	return (utime(file,&times));
}
#endif

random()
{
	return rand();
}

srandom(seed)
{
	srand(seed);
}
	       
getdtablesize()
{
	return (20);
}

setlinebuf(file)
FILE *file;
{
	setbuf(file,NULL);
}

psignal(sig,s)
unsigned sig;
char *s;
{
	fprintf (stderr,"%s: signal %d\n",s,sig);
}
#endif /* AFS_HPUX_ENV */

