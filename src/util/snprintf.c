/* snprintf.c - Formatted, length-limited print to a string */

#include <afsconfig.h>
#include <afs/param.h>

RCSID
    ("$Header$");

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#ifndef AFS_NT40_ENV
#include <netinet/in.h>
#include <netdb.h>
#else
#include <winsock2.h>
#endif
#if defined(AFS_AIX32_ENV) || defined(AFS_SUN_ENV) || defined(AFS_XBSD_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SGI65_ENV)
#include <sys/socket.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#define MAXPREC 100

/* Generate an ASCII representation of an integer <val>, as follows:
 * <base> indicates the base to be used (2-36)
 * <uc> is nonzero if letter digits should be uppercase
 * <prec> is the minimum number of digits
 * The resulting number is stored in <buf>, which must be long enough
 * to receive it.  The minimum length is <prec> or ceil(log{base}(val)),
 * whichever is larger, plus room for a trailing NUL.
 */
static void
mkint(char *buf, afs_uintmax_t val, int base, int uc, unsigned prec)
{
    int len = 0, dig, i;

    while (val) {
	dig = (int) (val % base);
	val = (val - dig) / base;
	if (dig < 10)
	    dig = dig + '0';
	else if (uc)
	    dig = dig + 'A' - 10;
	else
	    dig = dig + 'a' - 10;
	buf[len++] = dig;
    }
    while (len < prec)
	buf[len++] = '0';
    for (i = 0; i < (len + 1) / 2; i++) {
	dig = buf[i];
	buf[i] = buf[len - i - 1];
	buf[len - i - 1] = dig;
    }
    buf[len] = 0;
}


/* This function is a mostly-complete implementation of snprintf,
 * with the following features:
 *
 *   - Actually obeys the length limit, which (unfortunately) many
 *     implementations of snprintf do not.
 *  
 *   - Supports all the standard format specifiers for integers
 *     (d, i, o, u, x, X), floating-point values (f, e, E, g, G),
 *     and strings and characters (c, s, %), plus a few unusual
 *     but useful ones described below.
 *  
 *   - Supports all the standard flags (-, 0, +, space, #).  These
 *     flags are ignored if used when they are not appropriate.
 *  
 *   - Supports the standard size modifiers for short (h), long (h),
 *     and double (L) arguments.  These modifiers are ignored if used
 *     when they are not appropriate.
 *  
 *   - Supports minimum field width and precision, where appropriate,
 *     including the use of '*' to specify a value given as an argument
 *     instead of in the format string.  There is a maximum precision
 *     of 100 digits.
 *  
 *   - At present, the 'p' specifier for printing pointers is not
 *     implemented, because it is inherently non-portable and thus
 *     can be implemented correctly only by the compiler's run-time
 *     library.
 *
 *   - Floating-point specifier (%e, %f, %g) are implemented by
 *     calling the standard sprintf, and thus may be unsafe.
 *  
 *   - The '%...$' notation is used primarily when the format string
 *     is specified by the user, who knows but cannot change the order
 *     of the arguments.  Such usage is inherently dangerous and
 *     insecure; thus, it is not supported.
 *  
 * The custom format specifier '%I' is supported.  This specifier
 * takes as its argument an unsigned long integer containing an
 * IPv4 address in network byte order.  The address is rendered
 * either as a hostname or as a dotted quad, as follows:
 *  
 *   - If precision is nonzero or unspecified, a hostname lookup
 *     is attempted; if it is successful, the hostname is printed.
 *     If the hostname lookup fails, the address is printed in
 *     dotted-quad notation.
 *  
 *   - If precision is explicitly specified as 0, then the hostname
 *     lookup is skipped, and dotted-quad notation is always used.
 *  
 *   - If a hostname is to be printed:
 *     + The precision controls the maximum number of characters
 *       printed, as with %s.
 *     + If the '#' flag is specified, any letters in the hostname
 *       will be forced to lower case before printing.
 *     + If the '+' flag is specified, any letters in the hostname
 *       will be forced to upper case before printing.  If both
 *       '#' and '+' are given, the '+' flag will be ignored.
 *     + The '0' and ' ' flags have no effect.
 *  
 *   - If a dotted quad is to be printed:
 *     + The precision has no effect; dotted quads are always
 *       7 to 12 characters in length, depending on the value
 *       to be printed and the format flags used.
 *     + If the '0' flag is given, each field (byte) of the address
 *       will be padded with '0' on the left to three digits.
 *     + If the ' ' flag is given, each field (byte) of the address
 *       will be padded with spaces on the left to three digits.  If
 *       both '0' and ' ' are given, the ' ' flag will be ignored.
 *     + The '#' and '+' flags have no effect.
 */
int
afs_vsnprintf(char *p, size_t avail, const char *fmt, va_list ap)
{
    unsigned int width, precision, haveprec;
    size_t len;
    int ljust, plsign, spsign, altform, zfill;
    int hflag, lflag, count, *countp, j;
    char *x, *y, xbuf[MAXPREC + 21], fbuf[20];
    struct hostent *he;
    struct in_addr ia;
    afs_uintmax_t UVAL;
    afs_intmax_t SVAL;
    afs_intmax_t *llcountp;
    long *lcountp;
    double FVAL;
    short *hcountp;

    count = 0;
    avail--;
    while (*fmt && avail) {
	if (*fmt != '%') {
	    *p++ = *fmt++;
	    avail--;
	    count++;
	    continue;
	}

    /** Found a format specifier **/
	ljust = plsign = spsign = altform = zfill = 0;
	width = precision = haveprec = 0;
	hflag = lflag = 0;
	fmt++;

	/* parse format flags */
	while (*fmt) {
	    switch (*fmt) {
	    case '-':
		ljust = 1;
		fmt++;
		continue;	/* left justify */
	    case '+':
		plsign = 1;
		fmt++;
		continue;	/* use + or - */
	    case ' ':
		spsign = 1;
		fmt++;
		continue;	/* use space or - */
	    case '#':
		altform = 1;
		fmt++;
		continue;	/* alternate form */
	    case '0':
		zfill = 1;
		fmt++;
		continue;	/* pad with 0 */
	    default:
		break;
	    }
	    break;
	}

	/* parse minimum width */
	if (*fmt == '*') {
	    width = va_arg(ap, int);
	    fmt++;
	} else
	    while (isdigit(*fmt)) {
		width = (width * 10) + (*fmt - '0');
		fmt++;
	    }

	/* parse precision */
	if (*fmt == '.') {
	    fmt++;
	    haveprec = 1;
	    if (*fmt == '*') {
		precision = va_arg(ap, int);
		fmt++;
	    } else
		while (isdigit(*fmt)) {
		    precision = (precision * 10) + (*fmt - '0');
		    fmt++;
		}
	}

	/* parse size flags */
	while (*fmt) {
	    switch (*fmt) {
	    case 'h':
		hflag = 1;
		fmt++;
		continue;	/* short argument */
	    case 'l':
		lflag += 1;
		fmt++;
		continue;	/* long argument */
	    case 'L':
		lflag = 2;
		fmt++;
		continue;	/* long long argument */
	    default:
		break;
	    }
	    break;
	}

	/* parse format specifier */
	if (!*fmt)
	    break;
	switch (*fmt++) {
	case 'e':
	case 'E':
	case 'f':
	case 'g':
	case 'G':
	    FVAL = va_arg(ap, double);
	    sprintf(fbuf, "%%%s%s.*L%c", plsign ? "+" : (spsign ? " " : ""),
		    altform ? "#" : "", fmt[-1]);
	    if (!haveprec)
		precision = 6;
	    if (precision > MAXPREC)
		precision = MAXPREC;
	    sprintf(xbuf, fbuf, precision, FVAL);
	    x = xbuf;
	    len = strlen(x);
	    break;

	case 'i':
	case 'd':		/* signed decimal integer */
	    if (lflag > 1)
		SVAL = va_arg(ap, afs_intmax_t);
	    else if (lflag)
		SVAL = va_arg(ap, long);
	    else if (hflag)
		SVAL = va_arg(ap, int);
	    else
		SVAL = va_arg(ap, int);
	    UVAL = (SVAL < 0) ? -SVAL : SVAL;

	    if (SVAL < 0)
		xbuf[0] = '-';
	    else if (plsign)
		xbuf[0] = '+';
	    else if (spsign)
		xbuf[0] = ' ';
	    else
		xbuf[0] = 0;

	    if (!haveprec) {
		if (zfill && !ljust)
		    precision = width - !!xbuf[0];
		else
		    precision = 1;
		if (precision < 1 + !!xbuf[0])
		    precision = 1 + !!xbuf[0];
	    }
	    if (precision > MAXPREC)
		precision = MAXPREC;

	    mkint(xbuf + 1, UVAL, 10, 0, precision);
	    x = xbuf + !xbuf[0];
	    len = strlen(x);
	    break;


	case 'o':		/* unsigned octal integer */
	    if (lflag > 1)
		UVAL = va_arg(ap, afs_uintmax_t);
	    else if (lflag)
		UVAL = va_arg(ap, unsigned long);
	    else if (hflag)
		UVAL = va_arg(ap, unsigned int);
	    else
		UVAL = va_arg(ap, unsigned int);

	    xbuf[0] = '0';

	    if (!haveprec) {
		if (zfill && !ljust)
		    precision = width;
		else
		    precision = 1;
	    }
	    if (precision > MAXPREC)
		precision = MAXPREC;

	    mkint(xbuf + 1, UVAL, 8, 0, precision);
	    x = xbuf + (xbuf[1] == '0' || !altform);
	    len = strlen(x);
	    break;

	case 'u':		/* unsigned decimal integer */
	    if (lflag > 1)
		UVAL = va_arg(ap, afs_uintmax_t);
	    else if (lflag)
		UVAL = va_arg(ap, unsigned long);
	    else if (hflag)
		UVAL = va_arg(ap, unsigned int);
	    else
		UVAL = va_arg(ap, unsigned int);

	    if (!haveprec) {
		if (zfill && !ljust)
		    precision = width;
		else
		    precision = 1;
	    }
	    if (precision > MAXPREC)
		precision = MAXPREC;

	    mkint(xbuf, UVAL, 10, 0, precision);
	    x = xbuf;
	    len = strlen(x);
	    break;

	case 'x':
	case 'X':		/* unsigned hexadecimal integer */
	    if (lflag > 1)
		UVAL = va_arg(ap, afs_uintmax_t);
	    else if (lflag)
		UVAL = va_arg(ap, unsigned long);
	    else if (hflag)
		UVAL = va_arg(ap, unsigned int);
	    else
		UVAL = va_arg(ap, unsigned int);

	    xbuf[0] = '0';
	    xbuf[1] = 'x';

	    if (!haveprec) {
		if (zfill && !ljust)
		    precision = width;
		else
		    precision = 1;
	    }
	    if (precision > MAXPREC)
		precision = MAXPREC;

	    mkint(xbuf + 2, UVAL, 16, 0, precision);
	    x = xbuf + ((altform && UVAL) ? 0 : 2);
	    len = strlen(x);
	    break;

	case '%':		/* literal % */
	    xbuf[0] = '%';
	    xbuf[1] = 0;
	    x = xbuf;
	    len = 1;
	    break;

	case 'c':		/* character */
	    xbuf[0] = va_arg(ap, int);
	    xbuf[1] = 0;
	    x = xbuf;
	    len = 1;
	    break;

	case 's':		/* string */
	    x = va_arg(ap, char *);
	    if (!x)
		x = "<null>";
	    len = strlen(x);
	    if (haveprec && precision < len)
		len = precision;
	    break;

	case 'I':		/* IP address:
				 * value is provided as a network-order unsigned long integer
				 * precision specifies max hostname length, as for %s
				 * if precision is explicitly 0, no hostname lookup is done
				 * if 0fill specified, IPaddr fields are 0-filled to 3 digits
				 * if spsign specified, IPaddr fields are space-filled to 3 digits
				 */
	    UVAL = va_arg(ap, unsigned long);
	    ia.s_addr = (unsigned long)UVAL;
	    if (haveprec && !precision)
		he = 0;
	    else
		he = gethostbyaddr((char *)&ia, 4, AF_INET);
	    if (he) {
		x = he->h_name;
		len = strlen(x);
		if (haveprec && precision < len)
		    len = precision;
		if (altform) {
		    for (y = x; *y; y++)
			if (isupper(*y))
			    *y = tolower(*y);
		} else if (plsign) {
		    for (y = x; *y; y++)
			if (islower(*y))
			    *y = toupper(*y);
		}
	    } else {
		UVAL = ntohl((unsigned long)UVAL);
		if (zfill) {
		    x = "%03u.%03u.%03u.%03u";
		} else if (spsign) {
		    x = "%3u.%3u.%3u.%3u";
		} else {
		    x = "%u.%u.%u.%u";
		}
		/* typecast to whatever '%u' is! */
		sprintf(xbuf, x, (unsigned int)((UVAL & 0xff000000) >> 24),
			(unsigned int)((UVAL & 0x00ff0000) >> 16), 
			(unsigned int)((UVAL & 0x0000ff00) >> 8),
			(unsigned int)(UVAL & 0x000000ff));
		x = xbuf;
		len = strlen(xbuf);
	    }
	    break;

	case 'n':		/* report count so far */
	    if (lflag > 1) {
		llcountp = va_arg(ap, afs_intmax_t *);
		*llcountp = (afs_intmax_t)count;
	    } else if (lflag) {
		lcountp = va_arg(ap, long *);
		*lcountp = (long)count;
	    } else if (hflag) {
		hcountp = va_arg(ap, short *);
		*hcountp = (short)count;
	    } else {
		countp = va_arg(ap, int *);
		*countp = count;
	    }
	    continue;

	default:		/* unknown specifier */
	    continue;
	}

	/* render the results */
	if (len > avail)
	    len = avail;
	if (!width)
	    width = len;
	if (width > avail)
	    width = avail;
	j = width - len;
	if (j > 0) {
	    avail -= j;
	    count += j;
	}

	if (!ljust)
	    while (j-- > 0)
		*p++ = ' ';

	strncpy(p, x, len);
	avail -= len;
	count += len;
	p += len;

	if (ljust)
	    while (j-- > 0)
		*p++ = ' ';
    }
    *p = 0;
    return count;
}

int
afs_snprintf(char *p, size_t avail, const char *fmt, ...)
{
    va_list ap;
    int result;

    va_start(ap, fmt);
    result = afs_vsnprintf(p, avail, fmt, ap);
    va_end(ap);
    return result;
}

#if defined(AFS_OSF20_ENV) && !defined(AFS_DUX50_ENV) || defined(AFS_AIX32_ENV) || (defined(AFS_SUN55_ENV) && !defined(AFS_SUN56_ENV)) || !defined(HAVE_VSNPRINTF)

#if defined(AFS_AIX51_ENV) || defined(AFS_NT40_ENV)
int
vsnprintf(char *p, size_t avail, const char *fmt, va_list ap)
#else
void
vsnprintf(char *p, unsigned int avail, char *fmt, va_list ap)
#endif
{
    int result;
    result = afs_vsnprintf(p, avail, fmt, ap);
#if defined(AFS_AIX51_ENV) || defined(AFS_NT40_ENV)
    return result;
#endif
}
#endif /* AFS_OSF20_ENV || AFS_AIX32_ENV */

#ifndef AFS_NT40_ENV
#if defined(AFS_OSF20_ENV) && !defined(AFS_DUX50_ENV) || defined(AFS_AIX32_ENV) || (defined(AFS_SUN55_ENV) && !defined(AFS_SUN56_ENV)) || !defined(HAVE_SNPRINTF)

#ifdef AFS_AIX51_ENV
int
snprintf(char *p, size_t avail, const char *fmt, ...)
#else
void
snprintf(char *p, unsigned int avail, char *fmt, ...)
#endif
{
    va_list ap;
    int result;

    va_start(ap, fmt);
    result = afs_vsnprintf(p, avail, fmt, ap);
    va_end(ap);
#ifdef AFS_AIX51_ENV
    return result;
#endif
}
#endif /* AFS_OSF20_ENV || AFS_AIX32_ENV */
#endif /* AFS_NT40_ENV */
