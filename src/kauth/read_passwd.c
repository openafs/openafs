/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * This routine prints the supplied string to standard
 * output as a prompt, and reads a password string without
 * echoing.
 */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <mit-cpyright.h>
#include <des.h>

#include <stdio.h>
#ifdef	BSDUNIX
#include <strings.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <setjmp.h>
#else
char *strcpy();
int strcmp();
#endif
#if defined(AFS_AIX_ENV)
#include <signal.h>
#endif
#if defined(AFS_SGI_ENV)
#include <signal.h>
#endif
#include <string.h>

#if	defined	(AFS_AIX_ENV) || defined(AFS_SGI_ENV)
/* Just temp till we figure out the aix stuff */
#undef	BSDUNIX
static int intrupt;
#endif

#ifdef	BSDUNIX
static jmp_buf env;
#endif

#ifdef BSDUNIX
static void sig_restore();
static push_signals(), pop_signals();
int read_pw_string();
#endif

/*** Routines ****************************************************** */
int
des_read_password(k, prompt, verify)
     C_Block *k;
     char *prompt;
     int verify;
{
    int ok;
    char key_string[BUFSIZ];

#ifdef BSDUNIX
    if (setjmp(env)) {
	ok = -1;
	goto lose;
    }
#endif

    ok = read_pw_string(key_string, BUFSIZ, prompt, verify);
    if (ok == 0)
	string_to_key(key_string, k);

  lose:
    memset(key_string, 0, sizeof(key_string));
    return ok;
}

/* good_gets is like gets except that it take a max string length and won't
   write past the end of its input buffer.  It returns a variety of negative
   numbers in case of errors and zero if there was no characters read (a blank
   line for instance).  Otherwise it returns the length of the string read in.
   */

static int
good_gets(s, max)
     char *s;
     int max;
{
    int l;			/* length of string read */
    if (!fgets(s, max, stdin)) {
	if (feof(stdin))
	    return EOF;		/* EOF on input, nothing read */
	else
	    return -2;		/* I don't think this can happen */
    }
    l = strlen(s);
    if (l && (s[l - 1] == '\n'))
	s[--l] = 0;
    return l;
}

/*
 * This version just returns the string, doesn't map to key.
 *
 * Returns 0 on success, non-zero on failure.
 */

#if	!defined(BSDUNIX) && (defined(AFS_AIX_ENV) || defined(AFS_SGI_ENV))
#include <termio.h>
#endif

int
read_pw_string(s, max, prompt, verify)
     char *s;
     int max;
     char *prompt;
     int verify;
{
    int ok = 0;
    int len;			/* password length */

#ifdef	BSDUNIX
    jmp_buf old_env;
    struct sgttyb tty_state;
#else
#if	defined(AFS_AIX_ENV) || defined(AFS_SGI_ENV)
    struct termio ttyb;
    FILE *fi;
    char savel, flags;
    int (*sig) (), catch();
    extern void setbuf();
    extern int kill(), fclose();
#endif
#endif
    char key_string[BUFSIZ];

    if (max > BUFSIZ) {
	return -1;
    }
#ifdef	BSDUNIX
    memcpy(old_env, env, sizeof(env));
    if (setjmp(env))
	goto lose;

    /* save terminal state */
    if (ioctl(0, TIOCGETP, &tty_state) == -1)
	return -1;

    push_signals();
    /* Turn off echo */
    tty_state.sg_flags &= ~ECHO;
    if (ioctl(0, TIOCSETP, &tty_state) == -1) {
	pop_signals();
	return -1;
    }
#else
#if	defined(AFS_AIX_ENV) || defined(AFS_SGI_ENV)
    if ((fi = fopen("/dev/tty", "r+")) == NULL)
	return (-1);
    else
	setbuf(fi, (char *)NULL);
    sig = signal(SIGINT, catch);
    intrupt = 0;
    (void)ioctl(fileno(fi), TCGETA, &ttyb);
    savel = ttyb.c_line;
    ttyb.c_line = 0;
    flags = ttyb.c_lflag;
    ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    (void)ioctl(fileno(fi), TCSETAF, &ttyb);
#endif
#endif

    while (!ok) {
	printf(prompt);
	fflush(stdout);
#ifdef	CROSSMSDOS
	h19line(s, sizeof(s), 0);
	if (!strlen(s))
	    continue;
#else
	if (good_gets(s, max) <= 0) {
	    if (feof(stdin))
		break;		/* just give up */
	    else
		continue;	/* try again: blank line */
	}
#endif
	if (verify) {
	    printf("\nVerifying, please re-enter %s", prompt);
	    fflush(stdout);
#ifdef CROSSMSDOS
	    h19line(key_string, sizeof(key_string), 0);
	    if (!strlen(key_string))
		continue;
#else
	    if (good_gets(key_string, sizeof(key_string)) <= 0)
		continue;
#endif
	    if (strcmp(s, key_string)) {
		printf("\n\07\07Mismatch - try again\n");
		fflush(stdout);
		continue;
	    }
	}
	ok = 1;
    }

  lose:
    if (!ok)
	memset(s, 0, max);
#ifdef	BSDUNIX
    /* turn echo back on */
    tty_state.sg_flags |= ECHO;
    if (ioctl(0, TIOCSETP, &tty_state))
	ok = 0;
    pop_signals();
    memcpy(env, old_env, sizeof(env));
#else
#if	defined(AFS_AIX_ENV) || defined(AFS_SGI_ENV)
    ttyb.c_lflag = flags;
    ttyb.c_line = savel;
    (void)ioctl(fileno(fi), TCSETAW, &ttyb);
    (void)signal(SIGINT, sig);
    if (fi != stdin)
	(void)fclose(fi);
    if (intrupt)
	(void)kill(getpid(), SIGINT);
#endif
#endif
    if (verify)
	memset(key_string, 0, sizeof(key_string));
    s[max - 1] = 0;		/* force termination */
    return !ok;			/* return nonzero if not okay */
}

#ifdef	BSDUNIX
/*
 * this can be static since we should never have more than
 * one set saved....
 */
static int (*old_sigfunc[NSIG]) ();

static
push_signals()
{
    register i;
    for (i = 0; i < NSIG; i++)
	old_sigfunc[i] = signal(i, sig_restore);
}

static
pop_signals()
{
    register i;
    for (i = 0; i < NSIG; i++)
	signal(i, old_sigfunc[i]);
}

static void
sig_restore(sig, code, scp)
     int sig, code;
     struct sigcontext *scp;
{
    longjmp(env, 1);
}
#endif

#if	defined(AFS_AIX_ENV) || defined(AFS_SGI_ENV)
static int
catch()
{
    ++intrupt;
}
#endif
