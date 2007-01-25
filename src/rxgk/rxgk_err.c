/* Generated from rxgk_err.et */

#include <stddef.h>
#include <com_err.h>
#include "rxgk_err.h"

static const char *RXGK_error_strings[] = {
	/* 000 */ "Security module structure inconsistent",
	/* 001 */ "Packet too short for security challenge",
	/* 002 */ "Security level negotiation failed",
	/* 003 */ "Ticket length too long or too short",
	/* 004 */ "packet had bad sequence number",
	/* 005 */ "caller not authorized",
	/* 006 */ "illegal key: bad parity or weak",
	/* 007 */ "security object was passed a bad ticket",
	/* 008 */ "ticket contained unknown key version number",
	/* 009 */ "authentication expired",
	/* 010 */ "sealed data inconsistent",
	/* 011 */ "user data too long",
	/* 012 */ "caller not authorized to use encrypted connections",
	/* 013 */ "incompatible version of rxgk",
	/* 014 */ "no security data for setup",
	/* 015 */ "invalid checksum type",
	/* 016 */ "Checksum length too long or too short",
	NULL
};

#define num_errors 17

void initialize_RXGK_error_table_r(struct et_list **list)
{
    initialize_error_table_r(list, RXGK_error_strings, num_errors, ERROR_TABLE_BASE_RXGK);
}

void initialize_RXGK_error_table(void)
{
    init_error_table(RXGK_error_strings, ERROR_TABLE_BASE_RXGK, num_errors);
}
