/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include <WINNT/afsreg.h>
#include <WINNT/afsevent.h>

int main(int argc, char **argv)
{
    HKEY applogKey, afslogKey;
    long status;

    /* Determine if registry is (likely to be) configured */

    status = RegOpenKeyAlt(AFSREG_NULL_KEY, AFSREG_APPLOG_KEY,
			   KEY_READ, 0, &applogKey, NULL);

    if (status == ERROR_SUCCESS) {
	status = RegOpenKeyAlt(applogKey, AFSREG_SVR_APPLOG_SUBKEY,
			       KEY_READ, 0, &afslogKey, NULL);

	if (status == ERROR_SUCCESS) {
	    (void) RegCloseKey(afslogKey);
	}

	(void) RegCloseKey(applogKey);
    }

    if (status != ERROR_SUCCESS) {
	printf("\n%s: expected event source %s not found in registry; "
	       "test not run.\n", argv[0], AFSREG_SVR_APPLOG_SUBKEY);
	exit(0);
    }

    /* Log test message w/o any insertion strings */

    printf("Logging server INFORMATION event (0 insert strings)\n");
    if (ReportInformationEventAlt(AFSEVT_SVR_TEST_MSG_NOARGS, 0)) {
	printf("\n%s: logging failed\n", argv[0]);
	exit(1);
    }

    printf("Logging server WARNING event (0 insert strings, status 0)\n");
    if (ReportWarningEventAlt(AFSEVT_SVR_TEST_MSG_NOARGS, 0, 0)) {
	printf("\n%s: logging failed\n", argv[0]);
	exit(1);
    }

    printf("Logging server ERROR event (0 insert strings, status 15)\n");
    if (ReportErrorEventAlt(AFSEVT_SVR_TEST_MSG_NOARGS, 15, 0)) {
	printf("\n%s: logging failed\n", argv[0]);
	exit(1);
    }

    /* Log test message with two insertion strings */

    printf("Logging server INFORMATION event (2 insert strings)\n");
    if (ReportInformationEventAlt(AFSEVT_SVR_TEST_MSG_TWOARGS,
				  "Insert string 1", "Insert String 2", 0)) {
	printf("\n%s: logging failed\n", argv[0]);
	exit(1);
    }

    printf("Logging server WARNING event (2 insert strings, status 0)\n");
    if (ReportWarningEventAlt(AFSEVT_SVR_TEST_MSG_TWOARGS, 0,
			      "Insert string 1", "Insert String 2", 0)) {
	printf("\n%s: logging failed\n", argv[0]);
	exit(1);
    }

    printf("Logging server ERROR event (2 insert strings, status 15)\n");
    if (ReportErrorEventAlt(AFSEVT_SVR_TEST_MSG_TWOARGS, 15,
			    "Insert string 1", "Insert String 2", 0)) {
	printf("\n%s: logging failed\n", argv[0]);
	exit(1);
    }

    return 0;
}
