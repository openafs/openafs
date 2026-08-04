/*
 * Copyright (c) 2026 Sine Nomine Associates. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/acl.h>
#include <afs/prs_fs.h>

#include <tests/tap/basic.h>
#include "common.h"

/* shorthand 'read' -> rl */
#define READ	(PRSFS_READ | \
		 PRSFS_LOOKUP)

/* shorthand 'write' -> rlidwk */
#define WRITE	(PRSFS_READ   | \
		 PRSFS_LOOKUP | \
		 PRSFS_INSERT | \
		 PRSFS_DELETE | \
		 PRSFS_WRITE  | \
		 PRSFS_LOCK)

/* shorthand 'mail' -> ikl */
#define MAIL	(PRSFS_INSERT | \
		 PRSFS_LOCK   | \
		 PRSFS_LOOKUP)

/* shorthand 'all' -> rlidwka */
#define ALL	(PRSFS_READ   | \
		 PRSFS_LOOKUP | \
		 PRSFS_INSERT | \
		 PRSFS_DELETE | \
		 PRSFS_WRITE  | \
		 PRSFS_LOCK   | \
		 PRSFS_ADMINISTER)

#define R_rlidwkaH	(ALL |\
			 PRSFS_USR7)

#define R_rlA		(READ |\
			 PRSFS_USR0)

#define R_ABC		(PRSFS_USR0 |\
			 PRSFS_USR1 |\
			 PRSFS_USR2)

#define R_rlidwkAEH	(WRITE |\
			 PRSFS_USR0 |\
			 PRSFS_USR4 |\
			 PRSFS_USR7)

#define R_iklBCD	(PRSFS_INSERT |\
			 PRSFS_LOCK   |\
			 PRSFS_LOOKUP |\
			 PRSFS_USR1   |\
			 PRSFS_USR2   |\
			 PRSFS_USR3)

static void
test_ParseRights(void)
{
    int tc_i;
    struct {
	const char *rights_str;
	int code;
	afs_uint32 mask;
	enum aclu_rights_type rtype;
	char bad_char;
	int null_badchar;

    } *tc, test_cases[] = {
	/* rights_str	code	mask	rtype */
	{ "rl",		0,	READ,	ACLU_RTYPE_SET },
	{ "rrl",	0,	READ,	ACLU_RTYPE_SET },
	{ "rlidwk",	0,	WRITE,	ACLU_RTYPE_SET },
	{ "ikl",	0,	MAIL,	ACLU_RTYPE_SET },
	{ "ikkkl",	0,	MAIL,	ACLU_RTYPE_SET },
	{ "rlidwka",	0,	ALL,	ACLU_RTYPE_SET },
	{ "rlidwka=",	0,	ALL,	ACLU_RTYPE_SET },
	{ "rlidwk+",	0,	WRITE,	ACLU_RTYPE_RELADD },
	{ "ikl-",	0,	MAIL,	ACLU_RTYPE_RELDEL },

	{ "rlidwkaH",	0,	R_rlidwkaH,	ACLU_RTYPE_SET },
	{ "Arl",	0,	R_rlA,		ACLU_RTYPE_SET },
	{ "ABC",	0,	R_ABC,		ACLU_RTYPE_SET },
	{ "rlidwkAEH",	0,	R_rlidwkAEH,	ACLU_RTYPE_SET },
	{ "iBCDkl",	0,	R_iklBCD,	ACLU_RTYPE_SET },
	{ "rlidwkaH+",	0,	R_rlidwkaH,	ACLU_RTYPE_RELADD },
	{ "iBCDkl-",	0,	R_iklBCD,	ACLU_RTYPE_RELDEL },

	{ "read",	0,	READ,	ACLU_RTYPE_SET },
	{ "write",	0,	WRITE,	ACLU_RTYPE_SET },
	{ "mail",	0,	MAIL,	ACLU_RTYPE_SET },
	{ "all",	0,	ALL,	ACLU_RTYPE_SET },
	{ "write+",	0,	WRITE,	ACLU_RTYPE_RELADD },
	{ "mail-",	0,	MAIL,	ACLU_RTYPE_RELDEL },
	{ "all=",	0,	ALL,	ACLU_RTYPE_SET },
	{ "",		0,	0,	ACLU_RTYPE_SET },

	/* rights_str	code	mask	rtype	bad_char */
	{ "foobar",	EINVAL,	0,	0,	'f'  },
	{ "rlbogus",	EINVAL,	0,	0,	'b'  },
	{ "rlbogus+",	EINVAL,	0,	0,	'b'  },
	{ "rlidwka*",	EINVAL,	0,	0,	'*'  },
	{ "iBC&Dkl",	EINVAL,	0,	0,	'&'  },

	/* rights_str	code	mask	rtype		bad_char	null_badchar */
	{ "rlidwk",	0,	WRITE,	ACLU_RTYPE_SET,	0,		1 },
	{ "rlbogus",	EINVAL,	0,	0,		0,		1 },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	afs_uint32 mask = 0xffff;
	enum aclu_rights_type rtype = 0;
	char bad_char = '#';
	char *bad_char_p = &bad_char;

	if (tc->null_badchar) {
	    bad_char_p = NULL;
	}

	is_int(aclu_ParseRights(tc->rights_str, &mask, &rtype, bad_char_p),
	       tc->code,
	       "aclu_ParseRights(%s) == %d",
	       tc->rights_str,
	       tc->code);

	if (tc->code == 0) {
	    is_hex(mask, tc->mask, "... mask matches");
	    is_hex(rtype, tc->rtype, "... rtype matches");
	}
	if (!tc->null_badchar) {
	    is_hex(bad_char, tc->bad_char, "... bad_char matches");
	}
    }
}

int
main(void)
{
    plan(110);

    test_ParseRights();

    return 0;
}
