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

#include <tests/tap/basic.h>

enum cmdOptions {
   copt_flag = 0,
   copt_first,
   copt_second,
   copt_sugar,
   copt_fourth,
   copt_fifth,
   copt_perhaps,
   copt_sanity
};

static int
testproc(struct cmd_syndesc *as, void *rock)
{
    is_string("foo", as->parms[copt_first].items->data,
	      "first option matches");
    is_string("bar", as->parms[copt_second].items->data,
	      "second option matches");
    ok(as->parms[copt_flag].items != NULL,
       "flag is set");

    return 0;
}

static void
checkList(struct cmd_item *list, ...)
{
    va_list ap;
    char *el;

    va_start(ap, list);
    el = va_arg(ap, char *);
    while (el != NULL && list != NULL) {
       is_string(el, list->data, "list element matches");
       list = list->next;
       el = va_arg(ap, char *);
    }

    if (el == NULL && list == NULL) {
	ok(1, "List has correct number of elements");
    } else if (el == NULL) {
	ok(0, "List has too many elements");
    } else if (list == NULL) {
	ok(0, "List has too few elements");
    }
}

int
main(int argc, char **argv)
{
    char *tv[100];
    struct cmd_syndesc *opts;
    struct cmd_syndesc *retopts;
    struct cmd_item *list;
    int code;
    int tc;
    int retval;
    char *path;
    char *retstring = NULL;

    plan(109);

    initialize_CMD_error_table();

    opts = cmd_CreateSyntax(NULL, testproc, NULL, 0, NULL);
    cmd_AddParm(opts, "-flag", CMD_FLAG, CMD_OPTIONAL, "a flag");
    cmd_AddParm(opts, "-first", CMD_SINGLE, CMD_REQUIRED, "first option");
    cmd_AddParm(opts, "-second", CMD_LIST, CMD_OPTIONAL, "second option");

    /* A simple command line */
    code = cmd_ParseLine("-first foo -second bar -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(0, code, "dispatching simple comamnd line succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "parsing simple command line succeeds");
    is_string("foo", retopts->parms[copt_first].items->data,
	      " ... 1st option matches");
    is_string("bar", retopts->parms[copt_second].items->data,
	      " ... 2nd option matches");
    ok(retopts->parms[copt_flag].items != NULL, " ... 3rd option matches");
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

    /* Check that paramaters with abbreviation disabled don't make things
     * ambiguous */
    cmd_AddParmAtOffset(opts, copt_sugar, "-sugar", CMD_SINGLE,
		        CMD_OPTIONAL | CMD_NOABBRV, "sugar with that");
    code = cmd_ParseLine("-fi foo -s bar -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Dispatch(tc, tv);
    is_int(0, code, "disabling specific abbreviations succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "and works with cmd_Parse into the bargain");
    cmd_FreeOptions(&retopts);
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
    is_string("one", retopts->parms[copt_first].items->data,
	      " ... 1st option matches");
    is_string("two", retopts->parms[copt_second].items->data,
	      " ... 2nd option matches");
    ok(retopts->parms[copt_flag].items != NULL,
	      " ... 3rd option matches");

    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);
    /* Try adding a couple of parameters at specific positions */
    cmd_AddParmAtOffset(opts, copt_fifth, "-fifth", CMD_SINGLE, CMD_OPTIONAL,
		       "fifth option");
    cmd_AddParmAtOffset(opts, copt_fourth, "-fourth", CMD_SINGLE, CMD_OPTIONAL,
		       "fourth option" );
    code = cmd_ParseLine("-first a -fourth b -fifth c", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "parsing our new options succeeds");
    is_string("b", retopts->parms[copt_fourth].items->data,
	      " Fourth option in right place");
    is_string("c", retopts->parms[copt_fifth].items->data,
	      " Fifth option in right place");
    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* Check Accessors */
    code = cmd_ParseLine("-first 1 -second second -flag", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);

    code = cmd_OptionAsInt(retopts, copt_first, &retval);
    is_int(0, code, "cmd_OptionsAsInt succeeds");
    is_int(1, retval, " ... and returns correct value");
    ok(cmd_OptionPresent(retopts, copt_first),
       " ... and is marked as present");

    code = cmd_OptionAsString(retopts, copt_second, &retstring);
    is_int(0, code, "cmd_OptionsAsString succeeds");
    is_string("second", retstring, " ... and returns correct value");
    free(retstring);
    retstring = NULL;
    ok(cmd_OptionPresent(retopts, copt_second),
       " ... and is marked as present");

    code = cmd_OptionAsFlag(retopts, copt_flag, &retval);
    is_int(0, code, "cmd_OptionsAsFlag succeeds");
    ok(retval, " ... and flag is correct");
    ok(cmd_OptionPresent(retopts, copt_flag),
       " ... and is marked as present");

    ok(!cmd_OptionPresent(retopts, copt_sugar),
       "Missing option is marked as such");

    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* Add an alias */
    code = cmd_AddParmAlias(opts, copt_second, "-twa");
    is_int(0, code, "cmd_AddParmAlias succeeds");

    code = cmd_ParseLine("-first 1 -twa tup", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "cmd_Parse succeeds for alias");
    cmd_OptionAsString(retopts, copt_second, &retstring);
    is_string("tup", retstring, " ... and we have the correct value");
    free(retstring);
    retstring = NULL;

    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* Add something that can be a flag or a value, and put something after
     * it so we can check for parse problems*/
    cmd_AddParm(opts, "-perhaps", CMD_SINGLE_OR_FLAG, CMD_OPTIONAL,
	        "what am I");
    cmd_AddParm(opts, "-sanity", CMD_SINGLE, CMD_OPTIONAL, "sanity check");

    /* Try using as an option */

    code = cmd_ParseLine("-first 1 -perhaps 2 -sanity 3", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "cmd_Parse succeeds for option-as-flag as opt");
    code = cmd_OptionAsInt(retopts, copt_perhaps, &retval);
    is_int(0, code, "cmd_OptionAsInt succeeds");
    is_int(2, retval, " ... and we have the correct value");
    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* And now, as a flag */

    code = cmd_ParseLine("-first 1 -perhaps -sanity 3", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "cmd_Parse succeeds for option-as-flag as flag");
    code = cmd_OptionAsInt(retopts, copt_perhaps, &retval);
    is_int(CMD_MISSING, code, " ... pulling out a value fails as expected");
    cmd_OptionAsFlag(retopts, copt_perhaps, &retval);
    ok(retval, " ... but parsing as a flag works");
    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* Check that we can produce help output */
    code = cmd_ParseLine("-help", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(CMD_HELP, code, "cmd_Parse returns help indicator with help output");
    ok(retopts == NULL, " ... and options is empty");

    /* Check splitting with '=' */

    code = cmd_ParseLine("-first 1 -perhaps=6 -sanity=3", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "cmd_Parse succeeds for items split with '='");
    code = cmd_OptionAsInt(retopts, copt_perhaps, &retval);
    is_int(0, code, "cmd_OptionAsInt succeeds");
    is_int(6, retval, " ... and we have the correct value once");
    code = cmd_OptionAsInt(retopts, copt_sanity, &retval);
    is_int(0, code, "cmd_OptionAsInt succeeds");
    is_int(3, retval, " ... and we have the correct value twice");
    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* Check list behaviour */
    code = cmd_ParseLine("-first 1 -second one two three", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "cmd_Parse succeeds for a list");
    code = cmd_OptionAsList(retopts, copt_second, &list);
    is_int(0, code, "cmd_OptionAsList succeeds");
    checkList(list, "one", "two", "three", NULL);
    cmd_FreeOptions(&retopts);
    cmd_FreeArgv(tv);

    /* Now, try adding a configuration file into the mix */
    if (getenv("C_TAP_SOURCE") == NULL)
	path = strdup("test1.conf");
    else {
	if (asprintf(&path, "%s/cmd/test1.conf", getenv("C_TAP_SOURCE")) < 0)
	    path = NULL;
    }
    if (path != NULL) {
	cmd_SetCommandName("test");
	code = cmd_OpenConfigFile(path);
	is_int(0, code, "cmd_OpenConfigFile succeeds");
    } else {
	skip("no memory to build config file path");
    }

    code = cmd_ParseLine("-first 1", tv, &tc, 100);
    is_int(0, code, "cmd_ParseLine succeeds");
    code = cmd_Parse(tc, tv, &retopts);
    is_int(0, code, "cmd_Parse succeeds when we have a config file");
    code = cmd_OptionAsInt(retopts, copt_perhaps, &retval);
    is_int(0, code, "cmd_OptionsAsInt succeeds");
    is_int(10, retval, " ... and we have the correct value for perhaps");
    code = cmd_OptionAsString(retopts, copt_sanity, &retstring);
    is_int(0, code, "cmd_OptionAsString succeeds");
    is_string("testing", retstring,
	      " ... and we have the correct value for sanity");

    /* Check breaking up a list of options */
    code = cmd_OptionAsList(retopts, copt_second, &list);
    is_int(0, code, "cmd_OptionAsList succeeds");
    checkList(list, "one", "two", "three", "four", NULL);

    return 0;
}

