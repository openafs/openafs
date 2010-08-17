/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* snprintf.c - Formatted, length-limited print to a string */

#include <afsconfig.h>
#include <afs/param.h>


#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#ifndef AFS_NT40_ENV
#include <netinet/in.h>
#include <netdb.h>
#ifndef HAVE_VSYSLOG
#include <syslog.h>
#endif
#else
#include <winsock2.h>
#endif
#if defined(AFS_AIX32_ENV) || defined(AFS_SUN_ENV) || defined(AFS_XBSD_ENV) || defined(AFS_HPUX_ENV) || defined(AFS_SGI65_ENV)
#include <sys/socket.h>
#endif

/* This is an enhanced version of the *printf functionality shipped
 * with Heimdal.  In addition to the standard Unix formatting types
 * this version also supports Microsoft's I32 and I64 type modifiers
 * and the OpenAFS I type which is used to generate output from
 * network byte order IPv4 addresses (either dotted notation or
 * hostname lookups.  Implementation details follow:
 *
 *   - Actually obeys the length limit, which (unfortunately) many
 *     implementations of snprintf do not.
 *
 *   - Supports all the standard format specifiers for integers
 *     (d, i, o, u, x, X), floating-point values (f, e, E, g, G),
 *     and strings and characters (c, s, %), plus a few unusual
 *     but useful ones described below.
 *
 *   - The Microsoft integral size modifiers I32 and I64 are
 *     supported.  I32 is equivalent to 'l'.
 *     I64 is equivalent to 'll'.
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
 *   - The 'p' specifier for printing pointers is implemented using
 *     compile time knowledge.  (AFS_64BITUSERPOINTER_ENV)
 *
 *   - Floating-point specifier (%e, %f, %g) are implemented by
 *     calling the standard sprintf, and thus may be unsafe.
 *
 *   - The '%...$' notation is used primarily when the format string
 *     is specified by the user, who knows but cannot change the order
 *     of the arguments.  Such usage is inherently dangerous and
 *     insecure; thus, it is not supported.
 *
 *   - Passing in a format and an NULL buffer is supported.  This
 *     will compute the size of the buffer required by the format
 *     and the provided input parameters.
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
 *
 * A test program exists in src/util/tests/snprintf_tests.c.
 */

#define MAXPREC 100

enum format_flags {
    minus_flag     =  1,
    plus_flag      =  2,
    space_flag     =  4,
    alternate_flag =  8,
    zero_flag      = 16
};

/*
 * Common state
 */

struct snprintf_state {
    unsigned char *str;
    unsigned char *s;
    unsigned char *theend;
    size_t sz;
    size_t max_sz;
    void (*append_char)(struct snprintf_state *, unsigned char);
    /* XXX - methods */
};

static int
afs_sn_reserve (struct snprintf_state *state, size_t n)
{
    return state->s + n > state->theend;
}

static void
afs_sn_append_char (struct snprintf_state *state, unsigned char c)
{
    if (!afs_sn_reserve (state, 1))
	*state->s++ = c;
}

static int
as_reserve (struct snprintf_state *state, size_t n)
{
    if (state->s + n > state->theend) {
	size_t off = state->s - state->str;
	unsigned char *tmp;

	if (state->max_sz && state->sz >= state->max_sz)
	    return 1;

	state->sz = max(state->sz * 2, state->sz + n);
	if (state->max_sz)
	    state->sz = min(state->sz, state->max_sz);
	tmp = (unsigned char *)realloc (state->str, state->sz);
	if (tmp == NULL)
	    return 1;
	state->str = tmp;
	state->s = state->str + off;
	state->theend = state->str + state->sz - 1;
    }
    return 0;
}

static void
as_append_char (struct snprintf_state *state, unsigned char c)
{
    if(!as_reserve (state, 1))
	*state->s++ = c;
}

/* longest integer types */

#ifdef AFS_64BIT_ENV
typedef afs_uint64 u_longest;
typedef afs_int64 longest;
#else
typedef afs_uint32 u_longest;
typedef afs_int32 longest;
#endif

static int
pad(struct snprintf_state *state, int width, char c)
{
    int len = 0;
    while(width-- > 0){
	(*state->append_char)(state,  c);
	++len;
    }
    return len;
}

/* return true if we should use alternatve hex form */
static int
use_alternative (int flags, u_longest num, unsigned base)
{
    return (flags & alternate_flag) && base == 16 && num != 0;
}

static int
append_number(struct snprintf_state *state,
	      u_longest num, unsigned base, const char *rep,
	      int width, int prec, int flags, int minusp)
{
    int len = 0;
    u_longest n = num;
    char nstr[MAXPREC]; /* enough for <192 bit octal integers */
    int nstart, nlen;
    char signchar;

    /* given precision, ignore zero flag */
    if(prec != -1)
	flags &= ~zero_flag;
    else
	prec = 1;

    /* format number as string */
    nstart = sizeof(nstr);
    nlen = 0;
    nstr[--nstart] = '\0';

    do {
	nstr[--nstart] = rep[n % base];
	++nlen;
	n /= base;
    } while(n);

    /* zero value with zero precision should produce no digits */
    if(prec == 0 && num == 0) {
	nlen--;
	nstart++;
    }

    /* figure out what char to use for sign */
    if(minusp)
	signchar = '-';
    else if((flags & plus_flag))
	signchar = '+';
    else if((flags & space_flag))
	signchar = ' ';
    else
	signchar = '\0';

    if((flags & alternate_flag) && base == 8) {
	/* if necessary, increase the precision to
	   make first digit a zero */

	/* XXX C99 claims (regarding # and %o) that "if the value and
           precision are both 0, a single 0 is printed", but there is
           no such wording for %x. This would mean that %#.o would
           output "0", but %#.x "". This does not make sense, and is
           also not what other printf implementations are doing. */

	if(prec <= nlen && nstr[nstart] != '0' && nstr[nstart] != '\0')
	    prec = nlen + 1;
    }

    /* possible formats:
       pad | sign | alt | zero | digits
       sign | alt | zero | digits | pad   minus_flag
       sign | alt | zero | digits zero_flag */

    /* if not right justifying or padding with zeros, we need to
       compute the length of the rest of the string, and then pad with
       spaces */
    if(!(flags & (minus_flag | zero_flag))) {
	if(prec > nlen)
	    width -= prec;
	else
	    width -= nlen;

	if(use_alternative(flags, num, base))
	    width -= 2;

	if(signchar != '\0')
	    width--;

	/* pad to width */
	len += pad(state, width, ' ');
    }
    if(signchar != '\0') {
	(*state->append_char)(state, signchar);
	++len;
    }
    if(use_alternative(flags, num, base)) {
	(*state->append_char)(state, '0');
	(*state->append_char)(state, rep[10] + 23); /* XXX */
	len += 2;
    }
    if(flags & zero_flag) {
	/* pad to width with zeros */
	if(prec - nlen > width - len - nlen)
	    len += pad(state, prec - nlen, '0');
	else
	    len += pad(state, width - len - nlen, '0');
    } else
	/* pad to prec with zeros */
	len += pad(state, prec - nlen, '0');

    while(nstr[nstart] != '\0') {
	(*state->append_char)(state, nstr[nstart++]);
	++len;
    }

    if(flags & minus_flag)
	len += pad(state, width - len, ' ');

    return len;
}

/*
 * return length
 */

static int
append_string (struct snprintf_state *state,
	       const unsigned char *arg,
	       int width,
	       int prec,
	       int flags)
{
    int len = 0;

    if(arg == NULL)
	arg = (const unsigned char*)"(null)";

    if(prec != -1)
	width -= prec;
    else
	width -= (int)strlen((const char *)arg);
    if(!(flags & minus_flag))
	len += pad(state, width, ' ');

    if (prec != -1) {
	while (*arg && prec--) {
	    (*state->append_char) (state, *arg++);
	    ++len;
	}
    } else {
	while (*arg) {
	    (*state->append_char) (state, *arg++);
	    ++len;
	}
    }
    if(flags & minus_flag)
	len += pad(state, width, ' ');
    return len;
}

static int
append_char(struct snprintf_state *state,
	    unsigned char arg,
	    int width,
	    int flags)
{
    int len = 0;

    while(!(flags & minus_flag) && --width > 0) {
	(*state->append_char) (state, ' ')    ;
	++len;
    }
    (*state->append_char) (state, arg);
    ++len;
    while((flags & minus_flag) && --width > 0) {
	(*state->append_char) (state, ' ');
	++len;
    }
    return 0;
}

#define MAXPREC 100
static int
append_float(struct snprintf_state *state,
             char type,
             double arg,
             int width,
             int prec,
             int flags)
{
    int len = 0;
    char fbuf[20], xbuf[MAXPREC + 21];

    sprintf(fbuf, "%%%s%s.*L%c",
            (flags & plus_flag) ? "+" : ((flags & space_flag) ? " " : ((flags & minus_flag) ? "-" : "")),
            (flags & alternate_flag) ? "#" : "", type);
    if (prec == -1)
        prec = 6;
    if (prec > MAXPREC)
 	prec = MAXPREC;
    sprintf(xbuf, fbuf, prec, arg);

    len = append_string(state, (unsigned char *)xbuf, width, -1, 0);
    return len;
}

static int
append_address(struct snprintf_state *state,
               afs_uint32 arg,
               int width,
               int prec,
               int flags)
{
    int len = 0;
    struct hostent * he;
    struct in_addr ia;
    char * x, *y;

    /* IP address:
     * value is provided as a network-order unsigned long integer
     * precision specifies max hostname length, as for %s
     * if precision is explicitly 0, no hostname lookup is done
     * if 0fill specified, IPaddr fields are 0-filled to 3 digits
     * if spsign specified, IPaddr fields are space-filled to 3 digits
     */
    ia.s_addr = arg;
    if (prec == 0)
        he = 0;
    else
        he = gethostbyaddr((char *)&ia, 4, AF_INET);
    if (he) {
        x = he->h_name;
        len = (int)strlen(x);
        if (prec != -1 && prec < len)
            width = prec;
        else
            width = len;
        if (flags & alternate_flag) {
            for (y = x; *y; y++)
                if (isupper(*y))
                    *y = tolower(*y);
        } else if (flags & plus_flag) {
            for (y = x; *y; y++)
                if (islower(*y))
                    *y = toupper(*y);
        }
        len = append_string(state, (unsigned char *)x, width, prec, 0);
    } else {
        char xbuf[16];
        arg = ntohl(arg);
        if (flags & zero_flag) {
            x = "%03u.%03u.%03u.%03u";
        } else if (flags & space_flag) {
            x = "%3u.%3u.%3u.%3u";
        } else {
            x = "%u.%u.%u.%u";
        }
        /* typecast to whatever '%u' is! */
        sprintf(xbuf, x, (unsigned int)((arg & 0xff000000) >> 24),
                         (unsigned int)((arg & 0x00ff0000) >> 16),
                         (unsigned int)((arg & 0x0000ff00) >> 8),
                         (unsigned int)(arg & 0x000000ff));
        len = append_string(state, (unsigned char *)xbuf, 0, -1, 0);
    }

    return len;
}

/*
 * This can't be made into a function...
 */

#if defined(AFS_64BIT_ENV)
#if defined(AFS_NT40_ENV)

#define PARSE_INT_FORMAT(res, arg, unsig) \
if (long_long_flag) \
     res = (unsig __int64)va_arg(arg, unsig __int64); \
else if (long_flag || addr_flag) \
     res = (unsig long)va_arg(arg, unsig long); \
else if (size_t_flag) \
     res = (size_t)va_arg(arg, size_t); \
else if (short_flag) \
     res = (unsig short)va_arg(arg, unsig int); \
else \
     res = (unsig int)va_arg(arg, unsig int)

#else /* AFS_NT40_ENV */

#define PARSE_INT_FORMAT(res, arg, unsig) \
if (long_long_flag) \
     res = (unsig long long)va_arg(arg, unsig long long); \
else if (long_flag || addr_flag) \
     res = (unsig long)va_arg(arg, unsig long); \
else if (size_t_flag) \
     res = (size_t)va_arg(arg, size_t); \
else if (short_flag) \
     res = (unsig short)va_arg(arg, unsig int); \
else \
     res = (unsig int)va_arg(arg, unsig int)
#endif

#else

#define PARSE_INT_FORMAT(res, arg, unsig) \
if (long_flag || addr_flag) \
     res = (afs_uint32)va_arg(arg, afs_uint32); \
else if (size_t_flag) \
     res = (size_t)va_arg(arg, size_t); \
else if (short_flag) \
     res = (unsig short)va_arg(arg, unsig int); \
else \
     res = (unsig int)va_arg(arg, unsig int)

#endif

/*
 * zyxprintf - return length, as snprintf
 */

static int
xyzprintf (struct snprintf_state *state, const char *char_format, va_list ap)
{
    const unsigned char *format = (const unsigned char *)char_format;
    unsigned char c;
    int len = 0;

    while((c = *format++)) {
	if (c == '%') {
	    int flags          = 0;
	    int width          = 0;
	    int prec           = -1;
	    int size_t_flag    = 0;
	    int long_long_flag = 0;
	    int long_flag      = 0;
	    int short_flag     = 0;
            int addr_flag      = 0;

	    /* flags */
	    while((c = *format++)){
		if(c == '-')
		    flags |= minus_flag;
		else if(c == '+')
		    flags |= plus_flag;
		else if(c == ' ')
		    flags |= space_flag;
		else if(c == '#')
		    flags |= alternate_flag;
		else if(c == '0')
		    flags |= zero_flag;
		else if(c == '\'')
		    ; /* just ignore */
		else
		    break;
	    }

	    if((flags & space_flag) && (flags & plus_flag))
		flags ^= space_flag;

	    if((flags & minus_flag) && (flags & zero_flag))
		flags ^= zero_flag;

	    /* width */
	    if (isdigit(c))
		do {
		    width = width * 10 + c - '0';
		    c = *format++;
		} while(isdigit(c));
	    else if(c == '*') {
		width = va_arg(ap, int);
		c = *format++;
	    }

	    /* precision */
	    if (c == '.') {
		prec = 0;
		c = *format++;
		if (isdigit(c))
		    do {
			prec = prec * 10 + c - '0';
			c = *format++;
		    } while(isdigit(c));
		else if (c == '*') {
		    prec = va_arg(ap, int);
		    c = *format++;
		}
	    }

	    /* size */

	    if (c == 'h') {
		short_flag = 1;
		c = *format++;
	    } else if (c == 'z') {
		size_t_flag = 1;
		c = *format++;
	    } else if (c == 'l') {
		long_flag = 1;
		c = *format++;
		if (c == 'l') {
		    long_long_flag = 1;
		    c = *format++;
		}
	    } else if (c == 'I') {
                /* This could be either Microsoft I{32,64} notation */
                c = *format++;
                if (c == '3') {
                    long_flag = 1;
                    c = *format++;
                    if (c == '2') {
                        c = *format++;
                    }
                } else if (c == '6') {
                    long_flag = 1;
                    c = *format++;
                    if (c == '4') {
                        long_long_flag = 1;
                        c = *format++;
                    }
                } else {
                    /* Or the OpenAFS special %I meaning network address */
                    addr_flag = 1;
                    --format;
                    c = 'I';
                }
            } else if (c == 'p') {
                flags |= zero_flag;
                if (prec == -1)
                    prec = 2 * sizeof(void *);
                if (sizeof(void *) == sizeof(afs_uint64))
                    long_long_flag = 1;
                else if (sizeof(void *) == sizeof(afs_uint32))
                    long_flag = 1;
                else
                    long_flag = 1;
            }

	    if(c != 'd' && c != 'i' && c != 'I')
		flags &= ~(plus_flag | space_flag);

	    switch (c) {
	    case 'c' :
		append_char(state, va_arg(ap, int), width, flags);
		++len;
		break;
	    case 's' :
		len += append_string(state,
				     va_arg(ap, unsigned char*),
				     width,
				     prec,
				     flags);
		break;
	    case 'd' :
	    case 'i' : {
		longest arg;
		u_longest num;
		int minusp = 0;

		PARSE_INT_FORMAT(arg, ap, signed);

		if (arg < 0) {
		    minusp = 1;
		    num = -arg;
		} else
		    num = arg;

		len += append_number (state, num, 10, "0123456789",
				      width, prec, flags, minusp);
		break;
	    }
	    case 'u' : {
		u_longest arg;

		PARSE_INT_FORMAT(arg, ap, unsigned);

		len += append_number (state, arg, 10, "0123456789",
				      width, prec, flags, 0);
		break;
	    }
	    case 'o' : {
		u_longest arg;

		PARSE_INT_FORMAT(arg, ap, unsigned);

		len += append_number (state, arg, 010, "01234567",
				      width, prec, flags, 0);
		break;
	    }
	    case 'x' : {
		u_longest arg;

		PARSE_INT_FORMAT(arg, ap, unsigned);

		len += append_number (state, arg, 0x10, "0123456789abcdef",
				      width, prec, flags, 0);
		break;
	    }
	    case 'X' :{
		u_longest arg;

		PARSE_INT_FORMAT(arg, ap, unsigned);

		len += append_number (state, arg, 0x10, "0123456789ABCDEF",
				      width, prec, flags, 0);
		break;
	    }
	    case 'p' : {
#ifdef AFS_64BITUSERPOINTER_ENV
		u_longest arg = (u_longest)va_arg(ap, void*);
#else
                u_longest arg = (unsigned long)va_arg(ap, void*);
#endif
		len += append_number (state, arg, 0x10, "0123456789ABCDEF",
				      width, prec, flags, 0);
		break;
	    }
	    case 'n' : {
		int *arg = va_arg(ap, int*);
		*arg = (int)(state->s - state->str);
		break;
	    }
            case 'I' : {
                u_longest arg;

                PARSE_INT_FORMAT(arg, ap, unsigned);

                len += append_address (state, (unsigned long)arg, width, prec, flags);
                break;
            }
            case 'e':
            case 'E':
            case 'f':
            case 'g':
            case 'G': {
                double arg = (double)va_arg(ap, double);

                len += append_float (state, c, arg, width, prec, flags);
                break;
            }
	    case '\0' :
		--format;
		/* FALLTHROUGH */
	    case '%' :
		(*state->append_char)(state, c);
		++len;
		break;
	    default :
		(*state->append_char)(state, '%');
		(*state->append_char)(state, c);
		len += 2;
		break;
	    }
	} else {
	    (*state->append_char) (state, c);
	    ++len;
	}
    }
    return len;
}


int
afs_vsnprintf (char *str, size_t sz, const char *format, va_list args)
{
    struct snprintf_state state;
    int ret;
    unsigned char *ustr = (unsigned char *)str;

    state.max_sz = 0;
    state.sz     = sz;
    state.str    = ustr;
    state.s      = ustr;
    state.theend = ustr + sz - (sz > 0);
    state.append_char = afs_sn_append_char;

    ret = xyzprintf (&state, format, args);
    if (state.s != NULL && sz != 0)
	*state.s = '\0';
    return ret;
}

int
afs_snprintf (char *str, size_t sz, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = afs_vsnprintf (str, sz, format, args);
    va_end(args);

#ifdef PARANOIA
    {
	int ret2;
	unsigned char *tmp;

	tmp = (unsigned char *)malloc (sz);
	if (tmp == NULL)
	    abort ();

	va_start(args, format);
	ret2 = afs_vsprintf (tmp, format, args);
	va_end(args);
	if (ret != ret2 || strcmp(str, tmp))
	    abort ();
	free (tmp);
    }
#endif

    return ret;
}

int
afs_vasnprintf (char **ret, size_t max_sz, const char *format, va_list args)
{
    int st;
    struct snprintf_state state;

    state.max_sz = max_sz;
    state.sz     = 1;
    state.str    = (unsigned char *)malloc(state.sz);
    if (state.str == NULL) {
	*ret = NULL;
	return -1;
    }
    state.s = state.str;
    state.theend = state.s + state.sz - 1;
    state.append_char = as_append_char;

    st = xyzprintf (&state, format, args);
    if (st > state.sz) {
	free (state.str);
	*ret = NULL;
	return -1;
    } else {
	char *tmp;

	*state.s = '\0';
	tmp = (char *)realloc (state.str, st+1);
	if (tmp == NULL) {
	    free (state.str);
	    *ret = NULL;
	    return -1;
	}
	*ret = tmp;
	return st;
    }
}

int
afs_vasprintf (char **ret, const char *format, va_list args)
{
    return afs_vasnprintf (ret, 0, format, args);
}

int
afs_asprintf (char **ret, const char *format, ...)
{
    va_list args;
    int val;

    va_start(args, format);
    val = afs_vasprintf (ret, format, args);
    va_end(args);

#ifdef PARANOIA
    {
	int ret2;
	unsigned char *tmp;
	tmp = (unsigned char *)malloc (val + 1);
	if (tmp == NULL)
	    abort ();

	va_start(args, format);
	ret2 = afs_vsprintf (tmp, format, args);
	va_end(args);
	if (val != ret2 || strcmp(*ret, tmp))
	    abort ();
	free (tmp);
    }
#endif

    return val;
}

int
afs_asnprintf (char **ret, size_t max_sz, const char *format, ...)
{
    va_list args;
    int val;

    va_start(args, format);
    val = afs_vasnprintf (ret, max_sz, format, args);

#ifdef PARANOIA
    {
	int ret2;
	unsigned char *tmp;
	tmp = (unsigned char *)malloc (val + 1);
	if (tmp == NULL)
	    abort ();

	ret2 = afs_vsprintf (tmp, format, args);
	if (val != ret2 || strcmp(*ret, tmp))
	    abort ();
	free (tmp);
    }
#endif

    va_end(args);
    return val;
}

#if defined(AFS_OSF20_ENV) && !defined(AFS_DUX50_ENV) || defined(AFS_AIX32_ENV) || (defined(AFS_SUN55_ENV) && !defined(AFS_SUN56_ENV)) || !defined(HAVE_VSNPRINTF) || defined(TEST_SNPRINTF)

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
#ifndef HAVE_VSYSLOG
void
vsyslog(int priority, const char *format, va_list args)
{
  char buf[1024];
  afs_vsnprintf(buf, sizeof(buf), format, args);
  syslog(priority, "%s", buf);
}
#endif

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
