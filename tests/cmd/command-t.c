/*
 * Copyright (c) 2010 Your File System Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Test the command line parsing library
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/cmd.h>

#include <tap/basic.h>

static int
testproc(struct cmd_syndesc *as, void *rock)
{
    is_string("foo", as->parms[0].items->data,
	      "first option matches");
    is_string("bar", as->parms[1].items->data,
	      "second option matches");
    ok(as->parms[2].items != NULL,
       "flag is set");

    return 0;
}

int
main(int argc, char **argv)
{
    char *tv[100];
    struct cmd_syndesc *opts;
    struct cmd_syndesc *retopts;
    int code;
    int tc;
    int retval;
    char *retstring;

    plan(58);

    initialize_CMD_error_table();

    opts = cmd_CreateSyntax(NULL, testproc, NULL, NULL);
    cmd_AddParm(opts, "-first", CMD_SINGLE, CMD_REQUIRED, "first option");
    cmd_AddParm(opts, "-second", CMD_LIST, CMD_OPTIONAL, "second option");
    cmd_AddParm(opts, "-flag", CMD_FLAG, CMD_OPTIONAL, "a flag");

    /* A simple command line */
    code = cmd_ParseLine("-first foo -second bar -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(0, code, "dispatching simple comamnd line succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "parsing simple command line succeeds");
    is_string("foo", retopts->parms[0].items->data, " ... 1st option matches");
    is_string("bar", retopts->parms[1].items->data, " ... 2nd option matches");
    ok(retopts->parms[2].items != NULL, " ... 3rd option matches");
    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* unknown switch */
    code = cmd_ParseLine("-first foo -second bar -third -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(CMD_UNKNOWNSWITCH, code, "invalid options fail as expected");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(CMD_UNKNOWNSWITCH, code, "and still fail with cmd_Parse");
    cmd_FreeArgv(tv);

    /* missing parameter */
    code = cmd_ParseLine("-first foo -second -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(CMD_TOOFEW, code, "missing parameters fail as expected");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(CMD_TOOFEW, code, "and still fail with cmd_Parse");
    cmd_FreeArgv(tv);

    /* missing option */
    code = cmd_ParseLine("-second bar -third -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(CMD_UNKNOWNSWITCH, code, "missing options fail as expected");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(CMD_UNKNOWNSWITCH, code, "and still fail with cmd_Parse");
    cmd_FreeArgv(tv);

    code = cmd_ParseLine("-first foo baz -second bar -third -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(CMD_NOTLIST, code, "too many parameters fails as expected");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(CMD_NOTLIST, code, "and still fail with cmd_Parse");
    cmd_FreeArgv(tv);

    /* Positional parameters */
    code = cmd_ParseLine("foo bar -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(0, code, "dispatching positional parameters succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "and works with cmd_Parse");
    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* Abbreviations */
    code = cmd_ParseLine("-fi foo -s bar -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(0, code, "dispatching abbreviations succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "and works with cmd_Parse");
    cmd_FreeOptions(&retopts);

    cmd_FreeArgv(tv);

    /* Ambiguous */
    code = cmd_ParseLine("-f foo -s bar -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(CMD_UNKNOWNSWITCH, code, "ambiguous abbreviations correctly fail");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(CMD_UNKNOWNSWITCH, code, "and fail with cmd_Parse too");
    cmd_FreeArgv(tv);

    /* Disable positional commands */
    cmd_DisablePositionalCommands();
    code = cmd_ParseLine("foo bar -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(CMD_NOTLIST, code, "positional parameters can be disabled");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(CMD_NOTLIST, code, "and fail with cmd_Parse too");
    cmd_FreeArgv(tv);

    /* Disable abbreviations */
    cmd_DisableAbbreviations();
    code = cmd_ParseLine("-fi foo -s bar -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(CMD_UNKNOWNSWITCH, code, "dispatching abbreviations succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(CMD_UNKNOWNSWITCH, code, "and fail with cmd_Parse too");

    cmd_FreeArgv(tv);

    /* Try the new cmd_Parse function with something different*/
    code = cmd_ParseLine("-first one -second two -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "Parsing with cmd_Parse works");
    is_string("one", retopts->parms[0].items->data, " ... 1st option matches");
    is_string("two", retopts->parms[1].items->data, " ... 2nd option matches");
    ok(retopts->parms[2].items != NULL, " ... 3rd option matches");

    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);
    /* Try adding a couple of parameters at specific positions */
    cmd_AddParmAtOffset(opts, "-fifth", CMD_SINGLE, CMD_OPTIONAL,
		       "fifth option", 5);
    cmd_AddParmAtOffset(opts, "-fourth", CMD_SINGLE, CMD_OPTIONAL,
		       "fourth option", 4);
    code = cmd_ParseLine("-first a -fourth b -fifth c", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "parsing our new options succeeds");
    is_string("b", retopts->parms[4].items->data, " Fourth option in right place");
    is_string("c", retopts->parms[5].items->data, " Fifth option in right place");
    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* Check Accessors */
    code = cmd_ParseLine("-first 1 -second second -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);

    code = cmd_OptionAsInt(retopts, 0, &retval);
    is_int(0, code, "cmd_OptionsAsInt succeeds");
    is_int(1, retval, " ... and returns correct value");

    code = cmd_OptionAsString(retopts, 1, &retstring);
    is_int(0, code, "cmd_OptionsAsString succeeds");
    is_string("second", retstring, " ... and returns correct value");
    free(retstring);
    retstring = NULL;

    code = cmd_OptionAsFlag(retopts, 2, &retval);
    is_int(0, code, "cmd_OptionsAsFlag succeeds");
    ok(retval, " ... and flag is correct");

    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);


    return 0;
}

