/* 
 * $Id: aklog.c,v 1.1.2.5 2005/07/15 16:11:24 rra Exp $
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#if !defined(lint) && !defined(SABER)
static char *rcsid = "$Id: aklog.c,v 1.1.2.5 2005/07/15 16:11:24 rra Exp $";
#endif /* lint || SABER */

#include <afs/stds.h>
#include "aklog.h"

#ifndef WINDOWS

int
main(int argc, char *argv[])
{
    aklog(argc, argv);
    exit(0);
}

#else /* WINDOWS */

#include <windows.h>
#include <windowsx.h>

static void parse_cmdline();


int PASCAL
WinMain(HINSTANCE hinst, HINSTANCE hprevinstance, LPSTR cmdline, int noshow)
{
	int argc = 0;
	char **argv;

    aklog_params params;

	parse_cmdline(cmdline, &argv, &argc);

    aklog(argc, argv, &params);

	return 0;
}

/*
 * Generate agrv/argc here from command line.
 * Note that windows doesn't pass us the executible name, so
 * we need to fill that in manually.
 */

static void
parse_cmdline(char *cmdline, char ***pargv, int *pargc)
{
	char **argv;
	int argc = 0;
	char *arg, *sep = " \t";
	int argv_size = 10;		/* to start with */


	argv = malloc(argv_size * sizeof(char *));

	if (!argv) {
		MessageBox(NULL, "Fatal Error: Out of memory", AKLOG_DIALOG_NAME, 
				   MB_OK | MB_ICONSTOP);
		exit(1);
	}

	argv[argc++] = "aklog";

	arg = strtok(cmdline, sep);

	while(arg) {
		argv[argc] = strdup(arg);

		if (!argv[argc]) {
			MessageBox(NULL, "Fatal Error: Out of memory", AKLOG_DIALOG_NAME, 
					   MB_OK | MB_ICONSTOP);
			exit(1);
		}

		argc++;

		if (argc == argv_size) {
			argv_size += 10;
			argv = realloc(argv, argv_size * sizeof(char *));

			if (!argv) {
				MessageBox(NULL, "Fatal Error: Out of memory",
						   AKLOG_DIALOG_NAME, 
						   MB_OK | MB_ICONSTOP);
				exit(1);
			}
		}

		arg = strtok(NULL, sep);
	}

	argv[argc] = NULL;

	*pargv = argv;
	*pargc = argc;
}

#endif /* WINDOWS */

