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

#define NINETY_NINE_XS	"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" \
			"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

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

static void
test_StringifyRights(void)
{
    int tc_i;
    struct {
	afs_uint32 rights;
	const char *str;

    } *tc, test_cases[] = {
	{ READ, "rl" },
	{ WRITE, "rlidwk"},
	{ MAIL, "lik"},
	{ ALL, "rlidwka"},

	{ 0xffffffff, "rlidwkaABCDEFGH" },
	{ R_rlidwkaH, "rlidwkaH" },
	{ R_rlA, "rlA" },
	{ R_ABC, "ABC" },
	{ R_rlidwkAEH, "rlidwkAEH" },
	{ R_iklBCD, "likBCD" },
	{ 0, "" },
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct aclu_rightsbuf buf;

	is_string(aclu_StringifyRights(tc->rights, &buf), tc->str,
		  "aclu_StringifyRights(0x%x) == %s",
		  tc->rights, tc->str);
    }
}

struct test_aclu_AclEntry {
    char *name;
    afs_uint32 rights;
};
struct test_aclu_Acl {
    int nplus;
    int nminus;
    struct test_aclu_AclEntry pluslist[20];
    struct test_aclu_AclEntry minuslist[20];

    int dfs;
    char *cell;
};

static int
check_AclEntry(const char *list, int idx, struct aclu_AclEntry *got,
	       struct test_aclu_AclEntry *exp)
{
    if (strcmp(got->name, exp->name) != 0) {
	diag(" left AclEntry %s[%d] name: %s", list, idx, got->name);
	diag("right AclEntry %s[%d] name: %s", list, idx, exp->name);
	return 0;
    }

    if (got->rights != exp->rights) {
	diag(" left AclEntry %s[%d] rights: 0x%x", list, idx, got->rights);
	diag("right AclEntry %s[%d] rights: 0x%x", list, idx, exp->rights);
	return 0;
    }

    return 1;
}

static int
is_acl_v(struct aclu_Acl *got, struct test_aclu_Acl *exp, const char *fmt,
	 va_list ap)
{
    int success, entry_i;
    const char *exp_cell;
    struct aclu_AclEntry *got_entry;

    opr_Assert(exp != NULL);

    if (got == NULL) {
	diag(" left: NULL");
	diag("right: not NULL");
	goto fail;
    }

    if (got->nplus != exp->nplus) {
	diag(" left nplus: %d", got->nplus);
	diag("right nplus: %d", exp->nplus);
	goto fail;
    }

    if (got->nminus != exp->nminus) {
	diag(" left nminus: %d", got->nminus);
	diag("right nminus: %d", exp->nminus);
	goto fail;
    }

    got_entry = got->pluslist;
    for (entry_i = 0; entry_i < got->nplus; entry_i++) {
	if (got_entry == NULL) {
	    goto fail;
	}
	success = check_AclEntry("pluslist", entry_i,
				 got_entry, &exp->pluslist[entry_i]);
	if (!success) {
	    goto fail;
	}
	got_entry = got_entry->next;
    }

    got_entry = got->minuslist;
    for (entry_i = 0; entry_i < got->nminus; entry_i++) {
	if (got_entry == NULL) {
	    goto fail;
	}
	success = check_AclEntry("minuslist", entry_i,
				 got_entry, &exp->minuslist[entry_i]);
	if (!success) {
	    goto fail;
	}
	got_entry = got_entry->next;
    }

    if (got->dfs != exp->dfs) {
	diag(" left dfs: %d", got->dfs);
	diag("right dfs: %d", exp->dfs);
	goto fail;
    }

    exp_cell = exp->cell;
    if (exp_cell == NULL) {
	exp_cell = "";
    }
    if (strcmp(got->cell, exp_cell) != 0) {
	diag(" left cell: %s", got->cell);
	diag("right cell: %s", exp->cell);
	goto fail;
    }

    success = 1;

 done:
    okv(success, fmt, ap);

    return success;

 fail:
    success = 0;
    goto done;
}

static int
is_acl(struct aclu_Acl *got, struct test_aclu_Acl *exp, const char *fmt, ...)
{
    int success;
    va_list args;

    va_start(args, fmt);
    success = is_acl_v(got, exp, fmt, args);
    va_end(args);

    return success;
}

static void
test_ParseAcl(void)
{
    int tc_i;
    struct {
	const char *str;
	struct test_aclu_Acl acl;

    } *tc, test_cases[] = {
	{ "2\n0\nadmins 127\nreaders 9\n", /* str */
	    { 2, 0, /* nplus, nminus */
		{   { "admins", 127 }, /* pluslist entry 1 */
		    { "readers", 9 },  /* pluslist entry 2 */
		},
	    },
	},

	{ "1\n1\nadmins 127\nbadusers 9\n",
	    { 1, 1,
		{{ "admins", 127 }},
		{{ "badusers", 9 }},
	    },
	},

	{ "0\n2\nbaduser 0\nworseuser 8\n",
	    { 0, 2,
		{{ 0 }},
		{ { "baduser", 0 }, { "worseuser", 8 } },
	    },
	},

	{ "3\n2\na 1\nb 2\nc 4\nd 8\ne 16\n",
	    { 3, 2,
		{ { "a", 1 }, { "b", 2 }, { "c", 4 } },
		{ { "d", 8 }, { "e", 16 } },
	    },
	},

	{ "0\n0\n",
	    { 0, 0 },
	},

	{ "",
	    { 0, 0 },
	},

	{ "0",
	    { 0, 0 },
	},

	{ "foo",
	    { 0, 0 },
	},

	{ "0\n",
	    { 0, 0 },
	},

	{ "1\n0\nadmin\t127\n",
	    { 1, 0,
		{{ "admin", 127 }},
	    },
	},

	{ "0\n1\nadmin\t127\n",
	    { 0, 1,
		{{ 0 }},
		{{ "admin", 127 }},
	    },
	},

	{ "1\n0\nadmin\t0\n",
	    { 1, 0,
		{{ "admin", 0 }},
	    },
	},

	{ "1\n0\nadmin\t2147483647\n",
	    { 1, 0,
		{{ "admin", 2147483647 }},
	    },
	},

	{ "1\n0\nadmin\t2147483648\n",
	    { 1, 0,
		{{ "admin", 2147483648 }},
	    },
	},

	{ "1\n0\nadmin\t-1\n",
	    { 1, 0,
		{{ "admin", -1 }},
	    },
	},

	{ "1\n0\nadmin\t127",
	    { 1, 0,
		{{ "admin", 127 }},
	    },
	},

	{ "0\n1\nadmin\t127",
	    { 0, 1,
		{{ 0 }},
		{{ "admin", 127 }},
	    },
	},

	{ "1\n0\nadmin\t127\njunk",
	    { 1, 0,
		{{ "admin", 127 }},
	    },
	},

	{ "1\n0\nadm",
	    { 1, 0,
		{{ "adm", 0 }},
	    },
	},

	{ "0\n1\nadm",
	    { 0, 1,
		{{ 0 }},
		{{ "adm", 0 }},
	    },
	},

	{ "-1\n0\nadmin\t127\n",
	    { -1, 0 },
	},

	{ "0\n-1\nadmin\t127\n",
	    { 0, -1 },
	},

	{ "-1\n-1\nadmin\t127\nadmin\t127\n",
	    { -1, -1 },
	},

	{ "2\n0\nadmin\t127\n",
	    { 2, 0,
		{ { "admin", 127 }, { "admin", 127 } },
	    },
	},

	{ "0\n2\nadmin\t127\n",
	    { 0, 2,
		{{ 0 }},
		{ { "admin", 127 }, { "admin", 127 } },
	    },
	},

	{ "1\n1\nadmin\t127\n",
	    { 1, 1,
		{{ "admin", 127 }},
		{{ "admin", 127 }},
	    },
	},

	{ "1\n0\n",
	    { 1, 0,
		{{ "", 0 }},
	    },
	},

	{ "0\n1\n",
	    { 0, 1,
		{{ 0 }},
		{{ "", 0 }},
	    },
	},

	{ "0\n1\n",
	    { 0, 1,
		{{ 0 }},
		{{ "", 0 }},
	    },
	},

	{ "1\n0\n" NINETY_NINE_XS " 127\n",
	    { 1, 0,
		{{ NINETY_NINE_XS, 127 }},
	    },
	},
    };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct aclu_Acl *acl = NULL;

	is_int(aclu_ParseAcl(tc->str, &acl), 0,
	       "[%d] aclu_ParseAcl() == 0",
		tc_i);
	is_acl(acl, &tc->acl, "... acl matches");
	aclu_FreeAcl(&acl);
    }
}

static void
test_ParseEmptyAcl(void)
{
    int tc_i;
    struct {
	const char *str;

    } *tc, test_cases[] = {
	{ "2\n0\nadmins 127\nreaders 9\n" },
	{ "1\n1\nadmins 127\nbadusers 9\n" },
	{ "garbage" },
	{ "" },
    };

    struct test_aclu_Acl blank = { 0, 0 };

    for (afstest_Scan(test_cases, tc, tc_i)) {
	struct aclu_Acl *acl = NULL;

	is_int(aclu_ParseEmptyAcl(tc->str, &acl), 0,
	       "[%d] aclu_ParseEmptyAcl() == 0",
	       tc_i);
	is_acl(acl, &blank, "... acl matches");
	aclu_FreeAcl(&acl);
    }
}

int
main(void)
{
    plan(189);

    test_ParseRights();
    test_StringifyRights();
    test_ParseAcl();
    test_ParseEmptyAcl();

    return 0;
}
