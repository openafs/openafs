/*
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-cpyright.h>.
 *
 * This routine prints the supplied string to standard
 * output as a prompt, and reads a password string without
 * echoing.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include "mit-cpyright.h"
#include "des.h"
#include "conf.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef	BSDUNIX
#ifdef	AFS_SUN5_ENV
#define BSD_COMP
#endif
#include <sys/ioctl.h>
#include <signal.h>
#include <setjmp.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef	AFS_HPUX_ENV
#include <bsdtty.h>
#include <sys/ttold.h>
#include <termios.h>
static int intrupt;
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

static int intrupt;
#if defined(AFS_SGI_ENV) || defined (AFS_AIX_ENV) || defined(AFS_XBSD_ENV)	/*|| defined (AFS_HPUX_ENV) || defined(AFS_SUN5_ENV) */
#undef	BSDUNIX
#endif

#ifdef	BSDUNIX
static jmp_buf env;
#endif

#ifdef BSDUNIX
#define POSIX
#ifdef POSIX
typedef void sigtype;
#else
typedef int sigtype;
#endif
static sigtype sig_restore();
static push_signals(), pop_signals();
#endif

#include "des_prototypes.h"

/*** Routines ****************************************************** */
int
des_read_password(des_cblock * k, char *prompt, int verify)
{
    int ok;
    char key_string[BUFSIZ];

#ifdef BSDUNIX
    if (setjmp(env)) {
	ok = -1;
	goto lose;
    }
#endif

    ok = des_read_pw_string(key_string, BUFSIZ, prompt, verify);
    if (ok == 0)
	des_string_to_key(key_string, k);

#ifdef BSDUNIX
  lose:
#endif
    memset(key_string, 0, sizeof(key_string));
    return ok;
}

#if	defined	(AFS_AIX_ENV) || defined (AFS_HPUX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
static void catch(int);
#endif

#if	!defined(BSDUNIX) && (defined(AFS_AIX_ENV) || defined (AFS_HPUX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV))
#include <termio.h>
#endif

/*
 * This version just returns the string, doesn't map to key.
 *
 * Returns 0 on success, non-zero on failure.
 */
int
des_read_pw_string(char *s, int maxa, char *prompt, int verify)
{
    int ok = 0, cnt1 = 0;
    char *ptr;
#if defined(AFS_HPUX_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    int fno;
    struct sigaction newsig, oldsig;
    struct termios save_ttyb, ttyb;
#endif
#if defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    FILE *fi;
#endif
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
    struct termios ttyb;
    struct sigaction osa, sa;
#endif
#ifdef BSDUNIX
    jmp_buf old_env;
    unsigned long flags;
    struct sgttyb tty_state, echo_off_tty_state;
    FILE *fi;
#else
#if	defined	(AFS_AIX_ENV) || defined (AFS_HPUX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV)
    struct termio ttyb;
    FILE *fi;
    char savel, flags;
    void (*sig) (int);
#endif
#endif
#ifdef AFS_NT40_ENV
    HANDLE hConStdin;
    DWORD oldConMode, newConMode;
    BOOL resetConMode = FALSE;
#endif
    char key_string[BUFSIZ];

    if (maxa > BUFSIZ) {
	return -1;
    }
#if defined(AFS_HPUX_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
    if ((fi = fopen("/dev/tty", "r")) == NULL)
	return -1;
    setbuf(fi, (char *)NULL);	/* We don't want any buffering for our i/o. */
    /*
     * Install signal handler for SIGINT so that we can restore
     * the tty settings after we change them.  The handler merely
     * increments the variable "intrupt" to tell us that an
     * interrupt signal was received.
     */
    newsig.sa_handler = catch;
    sigemptyset(&newsig.sa_mask);
    newsig.sa_flags = 0;
    sigaction(SIGINT, &newsig, &oldsig);
    intrupt = 0;

    /*
     * Get the terminal characters (save for later restoration) and
     * reset them so that echo is off
     */
    fno = fileno(fi);
    tcgetattr(fno, &ttyb);
    save_ttyb = ttyb;
    ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    tcsetattr(fno, TCSAFLUSH, &ttyb);
#else
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
    if ((fi = fopen("/dev/tty", "r")) == NULL) {
	return (-1);
    } else
	setbuf(fi, (char *)NULL);
    sa.sa_handler = catch;
    sa.sa_mask = 0;
    sa.sa_flags = SA_INTERRUPT;
    (void)sigaction(SIGINT, &sa, &osa);
    intrupt = 0;
    (void)ioctl(fileno(fi), TCGETS, &ttyb);
    flags = ttyb.c_lflag;
    ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    (void)ioctl(fileno(fi), TCSETSF, &ttyb);
#else
#ifdef	BSDUNIX
    /* XXX assume jmp_buf is typedef'ed to an array */
    memcpy((char *)env, (char *)old_env, sizeof(env));
    if (setjmp(env))
	goto lose;
    /* save terminal state */
    if (ioctl(0, TIOCGETP, (char *)&tty_state) == -1)
	return -1;
    push_signals();
    /* Turn off echo */
    memcpy(&echo_off_tty_state, &tty_state, sizeof(tty_state));
    echo_off_tty_state.sg_flags &= ~ECHO;
    if (ioctl(0, TIOCSETP, (char *)&echo_off_tty_state) == -1)
	return -1;
#else
#if	defined	(AFS_AIX_ENV) || defined (AFS_HPUX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV)
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
#else
#ifdef AFS_NT40_ENV
    /* turn off console input echoing */
    if ((hConStdin = GetStdHandle(STD_INPUT_HANDLE)) != INVALID_HANDLE_VALUE) {
	if (GetConsoleMode(hConStdin, &oldConMode)) {
	    newConMode = (oldConMode & ~(ENABLE_ECHO_INPUT));
	    if (SetConsoleMode(hConStdin, newConMode)) {
		resetConMode = TRUE;
	    }
	}
    }
#endif
#endif
#endif
#endif
#endif
    while (!ok) {
	(void)printf("%s", prompt);
	(void)fflush(stdout);
#ifdef	CROSSMSDOS
	h19line(s, sizeof(s), 0);
	if (!strlen(s))
	    continue;
#else
	if (!fgets(s, maxa, stdin)) {
	    clearerr(stdin);
	    printf("\n");
	    if (cnt1++ > 1) {
		/*
		 * Otherwise hitting ctrl-d will always leave us inside this loop forever!
		 */
		break;
	    }
	    continue;
	}
	if ((ptr = strchr(s, '\n')))
	    *ptr = '\0';
#endif
	if (verify) {
	    printf("\nVerifying, please re-enter %s", prompt);
	    (void)fflush(stdout);
#ifdef CROSSMSDOS
	    h19line(key_string, sizeof(key_string), 0);
	    if (!strlen(key_string))
		continue;
#else
	    if (!fgets(key_string, sizeof(key_string), stdin)) {
		clearerr(stdin);
		continue;
	    }
	    if ((ptr = strchr(key_string, '\n')))
		*ptr = '\0';
#endif
	    if (strcmp(s, key_string)) {
		printf("\n\07\07Mismatch - try again\n");
		(void)fflush(stdout);
		continue;
	    }
	}
	ok = 1;
    }

#ifdef BSDUNIX
  lose:
#endif
    if (!ok)
	memset(s, 0, maxa);
    printf("\n");
#if defined(AFS_HPUX_ENV) || defined(AFS_XBSD_ENV) || defined(AFS_DARWIN_ENV)
    /*
     * Restore the terminal to its previous characteristics.
     * Restore the old signal handler for SIGINT.
     */
    tcsetattr(fno, TCSANOW, &save_ttyb);
    sigaction(SIGINT, &oldsig, NULL);
    if (fi != stdin)
	fclose(fi);

    /*
     * If we got a SIGINT while we were doing things, send the SIGINT
     * to ourselves so that the calling program receives it (since we
     * were intercepting it for a period of time.)
     */
    if (intrupt)
	kill(getpid(), SIGINT);
#else
#if	defined(AFS_SUN_ENV) && !defined(AFS_SUN5_ENV)
    ttyb.c_lflag = flags;
    (void)ioctl(fileno(fi), TCSETSW, &ttyb);
    (void)sigaction(SIGINT, &osa, (struct sigaction *)NULL);
    if (fi != stdin)
	(void)fclose(fi);
#else
#ifdef	BSDUNIX
    if (ioctl(0, TIOCSETP, (char *)&tty_state))
	ok = 0;
    pop_signals();
    memcpy((char *)old_env, (char *)env, sizeof(env));
#else
#if	defined	(AFS_AIX_ENV) /*|| defined (AFS_HPUX_ENV)*/ || defined(AFS_SGI_ENV) || defined(AFS_LINUX20_ENV)
    ttyb.c_lflag = flags;
    ttyb.c_line = savel;
    (void)ioctl(fileno(fi), TCSETAW, &ttyb);
    (void)signal(SIGINT, sig);
    if (fi != stdin)
	(void)fclose(fi);
    if (intrupt)
	(void)kill(getpid(), SIGINT);
#else
#ifdef AFS_NT40_ENV
    /* restore console to original mode settings */
    if (resetConMode) {
	(void)SetConsoleMode(hConStdin, oldConMode);
    }
#endif
#endif
#endif
#endif
#endif
    if (verify)
	memset(key_string, 0, sizeof(key_string));
    s[maxa - 1] = 0;		/* force termination */
    return !ok;			/* return nonzero if not okay */
}

#ifdef	BSDUNIX
/*
 * this can be static since we should never have more than
 * one set saved....
 */
#ifdef mips
void static (*old_sigfunc[NSIG]) ();
#else
static sigtype(*old_sigfunc[NSIG]) ();
#endif

static
push_signals()
{
    int i;
    for (i = 0; i < NSIG; i++)
	old_sigfunc[i] = signal(i, sig_restore);
}

static
pop_signals()
{
    int i;
    for (i = 0; i < NSIG; i++)
	(void)signal(i, old_sigfunc[i]);
}

static sigtype
sig_restore()
{
    longjmp(env, 1);
}
#endif


#if	defined	(AFS_AIX_ENV) || defined (AFS_HPUX_ENV) || defined(AFS_SGI_ENV) || defined(AFS_SUN_ENV) || defined(AFS_LINUX20_ENV) || defined(AFS_DARWIN_ENV) || defined(AFS_XBSD_ENV)
static void
catch(int junk)
{
    ++intrupt;
}
#endif
