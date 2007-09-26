/*
 * Copyright (c) 1995 Marcus D. Watts  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Marcus D. Watts.
 * 4. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * MARCUS D. WATTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cputime.h"

int cpu_flag;	/* 1 = elapsed, not CPU time */

#ifdef __svr4__
# ifdef HI_RES_CLOCK
#  define USER_FRACTBASE	1000000000
#  define ELAP_FRACTBASE        1000000000
# else
#  define USER_FRACTBASE	1000000000
#  define ELAP_FRACTBASE        1000000
# endif
void *bsdsignal(int signo, void *alarm_catcher)
{
    struct sigaction act;
    act.sa_handler = alarm_catcher;
    sigemptyset(&act.sa_mask);
/*    act.sa_flags = SA_RESTART; */
    if(sigaction(signo, &act,NULL) == -1) {
        perror("signal:");
        exit(1);
    }
}
#else
# define USER_FRACTBASE 1000000
# define ELAP_FRACTBASE 1000000
# define bsdsignal      signal
#endif

void
mark_time(CPU_TIME *cp)
{
#ifdef HAVE_PRUSAGE
        int fd;
        char proc[2048];        /* arbitrary length */
        sprintf(proc, "/proc/%d", getpid());
        if ((fd = open(proc, O_RDONLY)) == -1)
        {
            perror("open");
            exit(1);
        }
        if (ioctl(fd, PIOCUSAGE, &cp->rus) == -1)
        {
            perror("ioctl");
            exit(1);
        }
#if 0
        gettimeofday(&cp->rue); /* BSD incompatible */
#else
	/* solaris 2.5.1? */
        gettimeofday(&cp->rue, 0); /* BSD incompatible */
#endif
#endif /* ! HAVE_PRUSAGE */

#ifdef HAVE_RUSAGE
	getrusage(RUSAGE_SELF, &cp->rus);
        gettimeofday(&cp->rue, (struct timezone *) 0);
#endif /* HAVE_PRUSAGE */

#if !defined(HAVE_RUSAGE) && !defined(HAVE_PRUSAGE)
	/* V7 doesn't return elapsed ticks since system startup.
	 * then again, who runs V7 anymore?
	 */
	cp->te = times(&cp->ts);
#endif
}

void
sub_time(CPU_TIME *cps, CPU_TIME *cpe)
{
#if defined(bsd42) || defined(HAVE_PRUSAGE)
	long t, d;
	if (cpu_flag)
	{
		t = ELAP_SEC(cpe) - ELAP_SEC(cps);
		d = ELAP_FRAC(cpe) - ELAP_FRAC(cps);
                if (d < 0)
                    d += ELAP_FRACTBASE, --t;
                ELAP_SEC(cpe) = t;
                ELAP_FRAC(cpe) = d;
	}
	else {
		t = USER_SEC(cpe) - USER_SEC(cps);
		d = USER_FRAC(cpe) - USER_FRAC(cps);
                if (d < 0)
                    d += USER_FRACTBASE, --t;
		USER_SEC(cpe) = t;
		USER_FRAC(cpe) = d;
	}
#else
	if (cpu_flag)
		cpe->ts.tms_utime -= cps->ts.tms_utime;
	else
		cpe->te -= cps->te;
#endif
}

void
add_time(CPU_TIME *cps, CPU_TIME *cpe)
{
#if defined(bsd42) || defined(HAVE_PRUSAGE)
	long t, d;
	if (cpu_flag)
	{
		t = ELAP_SEC(cpe) + ELAP_SEC(cps);
		d = ELAP_FRAC(cpe) + ELAP_FRAC(cps);
                if (d >= ELAP_FRACTBASE)
                    d -= ELAP_FRACTBASE, ++t;
		ELAP_SEC(cpe) = t;
		ELAP_FRAC(cpe) = d;
	}
	else {
		t = USER_SEC(cpe) + USER_SEC(cps);
		d = USER_FRAC(cpe) + USER_FRAC(cps);
                if (d >= USER_FRACTBASE)
                    d -= USER_FRACTBASE, ++t;
		USER_SEC(cpe) = t;
		USER_FRAC(cpe) = d;
	}
#else
	if (cpu_flag)
		cpe->ts.tms_utime += cps->ts.tms_utime;
	else
		cpe->te += cps->te;
#endif
}

#ifdef notdef

char * sftime(cps, cpe)
	CPU_TIME *cps, *cpe;
{
	return sfclock(cps, cpe, 1000L, "milliseconds");
}
#endif

char * sfclock(CPU_TIME *cps, CPU_TIME *cpe, long divs, char *msg)
{
	LONGLONG t;
	static char result[30];
	double r1, r2, r3;

#if defined(bsd42) || defined(HAVE_PRUSAGE)
	long d;
#if USER_FRACTBASE != 1000000
	long fractbase;
#endif

	if (!divs) return "(nil)";
	if (cpu_flag)
	{
		t = ELAP_SEC(cpe) - ELAP_SEC(cps);
		d = ELAP_FRAC(cpe) - ELAP_FRAC(cps);
#if USER_FRACTBASE != 1000000
		fractbase = ELAP_FRACTBASE;
#endif
                t *= ELAP_FRACTBASE;
                t += d;
	}
	else {
                t = USER_SEC(cpe) - USER_SEC(cps);
		d = USER_FRAC(cpe) - USER_FRAC(cps);
#if USER_FRACTBASE != 1000000
		fractbase = USER_FRACTBASE;
#endif
                t *= USER_FRACTBASE;
                t += d;
	}
#else
	if (cpu_flag)
		t = cpe->te - cps->te;
	else
		t = cpe->ts.tms_utime - cps->ts.tms_utime;
	if (t > 1000)
		t *= (1000000/cpuhz());
	else {
		t *= 1000000;
		t /= cpuhz();
	}
	if (t < 0 || divs <= 0)
	{
		puts("sfclock trouble");
		printf (" t = %d\n", (int)t);
		printf (" cps %ld cpe %ld\n",
			cps->ts.tms_utime,
			cpe->ts.tms_utime);
		printf (" divs %ld\n", divs);
	}
#endif
	/* this float stuff done this way so that even on the
	 * Altos 68000, where the float stuff has been always
	 * broken, things will work.
	 */
	r1 = t;
	r2 = divs;
#if defined(bsd42) || defined(HAVE_PRUSAGE)
#if USER_FRACTBASE != 1000000
	r2 *= fractbase;
	r1 *= 1000000;
#endif
#endif
	r3 = r1 / r2;
	sprintf (result, ": %.3f %s", r3, msg);
	return result;
}

VOLATILE int went_off;

mysig_return_type
onalarm(n)
{
	went_off = 1;
}

char *fromenv(what, def)
	char *what;
	char *def;
{
	char *result;
	result = getenv(what);
	if (!result) result = def;
	return result;
}

int set_alarm(void)
{
	return nset_alarm(0);
}

int nset_alarm(int f)
{
	char *cp = 0;
	static int delay = 0;

	cpu_flag = f;
	if (!delay)
	{
		/* on the SUN,
		 * 5 seconds seems to give about 3 digits accuracy,
		 * which is plenty for interactive tests...
		 *
		 * the "runthemall" script overrides this default
		 * to 30 seconds for greatest accuracy in the real test.
		 */
		delay = atoi(cp = fromenv("HOWLONG", "5"));
	}
	if (delay <= 0)
	{
		fprintf (stderr, "Bad HOWLONG value: %s\n", cp);
		exit(1);
	}
	went_off = 0;
	signal(SIGALRM, onalarm);
	alarm(delay);
	return delay;
}

#ifdef CPUHZ
int cpuhz(void)
{
	static int result = 0;
	char *cp;
	if (!result)
	{
		if ((cp = getenv("CPUHZ")))
			result = atoi(cp);
		else result = CPUHZ;
	}
	return result;
}
#endif
