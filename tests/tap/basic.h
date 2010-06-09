/*
 * Basic utility routines for the TAP protocol.
 *
 * Copyright 2009, 2010 Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2008
 *     Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#ifndef TAP_BASIC_H
#define TAP_BASIC_H 1

#include <stdarg.h>             /* va_list */
#include <sys/types.h>          /* pid_t */

/*
 * __attribute__ is available in gcc 2.5 and later, but only with gcc 2.7
 * could you use the __format__ form of the attributes, which is what we use
 * (to avoid confusion with other macros).
 */
#ifndef __attribute__
# if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
#  define __attribute__(spec)   /* empty */
# endif
#endif

/*
 * BEGIN_DECLS is used at the beginning of declarations so that C++
 * compilers don't mangle their names.  END_DECLS is used at the end.
 */
#undef BEGIN_DECLS
#undef END_DECLS
#ifdef __cplusplus
# define BEGIN_DECLS    extern "C" {
# define END_DECLS      }
#else
# define BEGIN_DECLS    /* empty */
# define END_DECLS      /* empty */
#endif

/*
 * Used for iterating through arrays.  ARRAY_SIZE returns the number of
 * elements in the array (useful for a < upper bound in a for loop) and
 * ARRAY_END returns a pointer to the element past the end (ISO C99 makes it
 * legal to refer to such a pointer as long as it's never dereferenced).
 */
#define ARRAY_SIZE(array)       (sizeof(array) / sizeof((array)[0]))
#define ARRAY_END(array)        (&(array)[ARRAY_SIZE(array)])

BEGIN_DECLS

/*
 * The test count.  Always contains the number that will be used for the next
 * test status.
 */
extern unsigned long testnum;

/* Print out the number of tests and set standard output to line buffered. */
void plan(unsigned long count);

/*
 * Prepare for lazy planning, in which the plan will be  printed automatically
 * at the end of the test program.
 */
void plan_lazy(void);

/* Skip the entire test suite.  Call instead of plan. */
void skip_all(const char *format, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));

/*
 * Basic reporting functions.  The okv() function is the same as ok() but
 * takes the test description as a va_list to make it easier to reuse the
 * reporting infrastructure when writing new tests.
 */
void ok(int success, const char *format, ...)
    __attribute__((__format__(printf, 2, 3)));
void okv(int success, const char *format, va_list args);
void skip(const char *reason, ...)
    __attribute__((__format__(printf, 1, 2)));

/* Report the same status on, or skip, the next count tests. */
void ok_block(unsigned long count, int success, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));
void skip_block(unsigned long count, const char *reason, ...)
    __attribute__((__format__(printf, 2, 3)));

/* Check an expected value against a seen value. */
void is_int(long wanted, long seen, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));
void is_double(double wanted, double seen, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));
void is_string(const char *wanted, const char *seen, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));
void is_hex(unsigned long wanted, unsigned long seen, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));

/* Bail out with an error.  sysbail appends strerror(errno). */
void bail(const char *format, ...)
    __attribute__((__noreturn__, __nonnull__, __format__(printf, 1, 2)));
void sysbail(const char *format, ...)
    __attribute__((__noreturn__, __nonnull__, __format__(printf, 1, 2)));

/* Report a diagnostic to stderr prefixed with #. */
void diag(const char *format, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void sysdiag(const char *format, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));

END_DECLS

#endif /* TAP_BASIC_H */
