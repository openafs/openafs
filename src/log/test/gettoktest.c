/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include <itc.h>
#include <stdio.h>
#include <sys/file.h>
#include <r/xdr.h>
#include <afs/comauth.h>
#include <errno.h>

extern int errno;

main(argc, argv)
     int argc;
     char **argv;
{				/*main program */

    int i;			/*Cell entry number */
    int useCellEntry;		/*Do a cellular call? */
    ClearToken cToken;		/*Clear token returned */
    SecretToken sToken;		/*Secret token returned */
    char cellID[64];		/*Cell ID returned */
    int isPrimary;		/*Is cell ID primary? */
    int rc;			/*Return value from U_CellGetLocalTokens */

    printf("\nTesting the cellular Venus token interface.\n\n");
    printf("Getting tokens, non-cellular request:\n");
    useCellEntry = 0;
    rc = U_CellGetLocalTokens(useCellEntry, i, &cToken, &sToken, cellID,
			      &isPrimary);
    if (rc)
	printf("        **Error getting local tokens: %d, errno is %d\n", rc,
	       errno);
    else
	printf("        Got local tokens: Vice ID is %d\n", cToken.ViceId);

    printf("\nGetting tokens, cellular request.\n");
    useCellEntry = 1;
    rc = 0;
    for (i = 0; (i <= 1000) && !(rc && errno == EDOM); i++) {
	printf("  Cell entry %2d: ", i);
	rc = U_CellGetLocalTokens(useCellEntry, i, &cToken, &sToken, cellID,
				  &isPrimary);
	if (rc) {
	    /*Something didn't go well.  Print out errno, unless we got an EDOM */
	    if (errno == EDOM)
		printf("--End of list--\n");
	    else
		printf("** Error in call, errno is %d\n", errno);
	} else
	    printf("Vice ID %4d in cell '%s' [isPrimary=%d]\n", cToken.ViceId,
		   cellID, isPrimary);
    }

    printf("\nEnd of cellular Venus token interface test.\n\n");

}				/*main program */
