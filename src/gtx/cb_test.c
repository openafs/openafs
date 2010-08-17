/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * cb_test: A test of the gator text circular buffer package.
 *------------------------------------------------------------------------*/

#include <afsconfig.h>
#include <afs/param.h>


#include "gtxtextcb.h"		/*Module interface */


#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;

{				/*main */

    afs_int32 code;	/*Return code */
    struct gator_textcb_hdr *newCB;	/*Ptr to new CB hdr */
    char buf[1024];		/*Text buffer */
    int do_debugging;		/*Print out debugging info? */
    int i, j, k;		/*Loop variables */
    struct gator_textcb_entry *curr_ent;	/*Ptr to text entry */
    char *curr_char;		/*Current char to print */

    printf("\nTesting the gator text object circular buffer package\n\n");
    printf("Enter 1 to turn debugging on, 0 otherwise: ");
    scanf("%d", &do_debugging);
    printf("Initializing CB package: debugging %s\n",
	   (do_debugging ? "YES" : "NO"));
    code = gator_textcb_Init(do_debugging);
    if (code) {
	printf("Initialization failed\n");
	exit(-1);
    }
    printf("Creating a CB with up to 100 entries of 80 chars each\n");
    newCB = gator_textcb_Create(100, 80);
    if (newCB == (struct gator_textcb_hdr *)0) {
	printf("Can't create new circular buffer.");
	exit(-1);
    }

    /*
     * Start writing stuff to this circ buff.
     */
    printf("First write\n");
    sprintf(buf, "%s", "0123456789");
    code = gator_textcb_Write(newCB, buf, strlen(buf), 0, 0);
    if (code)
	printf("First write failed, code is %d\n", code);

    printf("Second write, highlighted\n");
    code = gator_textcb_Write(newCB, buf, 10, 1, 0);
    if (code)
	printf("Second write failed, code is %d\n", code);

    printf("Third write, bulk\n");
    sprintf(buf, "%s",
	    "Now is the time for all good men to come to the aid of their country");
    code = gator_textcb_Write(newCB, buf, strlen(buf), 0, 1);
    if (code)
	printf("Third write failed, code is %d\n", code);

    printf("Writing out 3 blank lines\n");
    code = gator_textcb_BlankLine(newCB, 3);
    if (code)
	printf("Blank lines failed with error %d\n", code);

    /*
     * Print out the CB status.
     */
    printf("\nStatus of the circular buffer after the writes:\n");
    printf("\tmaxEntriesStored: %d\n", newCB->maxEntriesStored);
    printf("\tmaxCharsPerEntry: %d\n", newCB->maxCharsPerEntry);
    printf("\tcurrEnt         : %d\n", newCB->currEnt);
    printf("\tcurrEntIdx      : %d\n", newCB->currEntIdx);
    printf("\toldestEnt       : %d\n", newCB->oldestEnt);
    printf("\toldestEntIdx    : %d\n", newCB->oldestEntIdx);

    printf("\nType in any number to continue: ");
    scanf("%d", &i);

    curr_ent = newCB->entry + newCB->oldestEntIdx;
    for (j = 0, i = newCB->oldestEntIdx; j < 5; j++) {
	printf("\nEntry %d, idx %d\n", curr_ent->ID, i);
	printf("\thighlight     : %d\n", curr_ent->highlight);
	printf("\tnumInversions : %d\n", curr_ent->numInversions);
	printf("\tinversions    : ");
	for (k = 0; k < GATOR_TEXTCB_MAXINVERSIONS; k++)
	    printf("%d ", curr_ent->inversion[k]);
	printf("\n");
	printf("\tcharsUsed     : %d\n", curr_ent->charsUsed);
	printf("\ttextp         : '");
	curr_char = curr_ent->textp;
	for (k = 0; k < curr_ent->charsUsed; k++)
	    printf("%c", *curr_char++);
	printf("'\n");
	if (i == newCB->maxEntriesStored - 1) {
	    i = 0;
	    curr_ent = newCB->entry;
	} else {
	    i++;
	    curr_ent++;
	}
    }				/*for loop */

    printf("\nMaking small writes ('a') to force the buffer to circulate\n");
    printf("Enter any number to continue: ");
    scanf("%d", &i);
    sprintf(buf, "%s", "a");
    for (i = 0; i < 100; i++) {
	printf("#");
	code = gator_textcb_Write(newCB, buf, 1, 0, 1 /*skip */ );
	if (code)
	    printf("Small write %d failed, code is %d\n", i, code);
    }
    printf("\n");

    /*
     * Print out the CB status.
     */
    printf("\nStatus of the circular buffer after the writes:\n");
    printf("\tmaxEntriesStored: %d\n", newCB->maxEntriesStored);
    printf("\tmaxCharsPerEntry: %d\n", newCB->maxCharsPerEntry);
    printf("\tcurrEnt         : %d\n", newCB->currEnt);
    printf("\tcurrEntIdx      : %d\n", newCB->currEntIdx);
    printf("\toldestEnt       : %d\n", newCB->oldestEnt);
    printf("\toldestEntIdx    : %d\n", newCB->oldestEntIdx);

    printf("\nType in any number to continue: ");
    scanf("%d", &i);

    curr_ent = newCB->entry + newCB->oldestEntIdx;
    for (j = 0, i = newCB->oldestEntIdx; j < 100; j++) {
	printf("\nEntry %d, idx %d\n", curr_ent->ID, i);
	printf("\thighlight     : %d\n", curr_ent->highlight);
	printf("\tnumInversions : %d\n", curr_ent->numInversions);
	printf("\tinversions    : ");
	for (k = 0; k < GATOR_TEXTCB_MAXINVERSIONS; k++)
	    printf("%d ", curr_ent->inversion[k]);
	printf("\n");
	printf("\tcharsUsed     : %d\n", curr_ent->charsUsed);
	printf("\ttextp         : '");
	curr_char = curr_ent->textp;
	for (k = 0; k < curr_ent->charsUsed; k++)
	    printf("%c", *curr_char++);
	printf("'\n");
	if (i == newCB->maxEntriesStored - 1) {
	    i = 0;
	    curr_ent = newCB->entry;
	} else {
	    i++;
	    curr_ent++;
	}
    }				/*for loop */

    /*
     * Clean-up time!
     */
    code = gator_textcb_Delete(newCB);
    if (code) {
	printf("CB deletion failed, code is %d\n", code);
	exit(-1);
    }

    /*
     * It worked, mon!  Goombay!!
     */
    exit(0);

}				/*main */
