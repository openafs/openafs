/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef __gator_textcb_h
#define	__gator_textcb_h  1

/*------------------------------------------------------------------------
 * gator_textcb.h
 *
 * Definitions and interface for the gator circular buffer package for
 * its scrollable text object.
 *
 *------------------------------------------------------------------------*/

#include <lock.h>		/*AFS locks */

#define GATOR_TEXTCB_MAXINVERSIONS 10	/*Max highlight inversions */

/*
  * Description of the text hanging off each circular buffer entry.
  */
struct gator_textcb_entry {
    int ID;			/*Overall ID (number) */
    int highlight;		/*(Starting) highlight value */
    int inversion[GATOR_TEXTCB_MAXINVERSIONS];	/*Highlighting inversions */
    int numInversions;		/*Num of above inversions */
    int charsUsed;		/*Num chars used */
    char *textp;		/*Ptr to text buffer itself */
};

/*
 * Circular buffer header.  Note: we actually allocate one more char
 * 	per line than we admit, to make sure we can always null-
 *	terminate each line.
 */
struct gator_textcb_hdr {
    struct Lock cbLock;		/*Lock for this circular buffer */
    int maxEntriesStored;	/*Max num. text entries we store */
    int maxCharsPerEntry;	/*Max characters in each entry */
    int currEnt;		/*Entry currently being written */
    int currEntIdx;		/*Index of current entry */
    int oldestEnt;		/*Oldest entry stored */
    int oldestEntIdx;		/*Index of oldest entry */
    struct gator_textcb_entry *entry;	/*Ptr to array of text entries */
    char *blankLine;		/*Ptr to blank line */
};

/*
  * Operations for text circular buffers.
  */
extern int gator_textcb_Init(int);
    /*
     * Summary:
     *    Initialize this package.  MUST BE THE FIRST ROUTINE CALLED!
     *
     * Args:
     *    int a_debug : Should debugging output be turned on?
     *
     * Returns:
     *    Zero if successful,
     *    Error code otherwise.
     */

extern struct gator_textcb_hdr *gator_textcb_Create(int, int);
    /*
     * Summary:
     *    Create a new text circular buffer.
     *
     * Args:
     *    int a_maxEntriesStored : How many entries should it have?
     *    int a_maxCharsPerEntry : Max chars in each entry.
     *
     * Returns:
     *    Ptr to the fully-initialized circular buffer hdr if successful,
     *    Null pointer otherwise.
     */

extern int gator_textcb_Write(struct gator_textcb_hdr *, char *a_textToWrite,
			      int, int, int);
    /*
     * Summary:
     *    Write the given string to the text circular buffer.  Line
     *    breaks are caused either by overflowing the current text
     *    line or via explicit '\n's.
     *
     * Args:
     *    struct gator_textcb_hdr *a_cbhdr : Ptr to circ buff hdr.
     *    char *a_textToWrite              : Ptr to text to insert.
     *    int a_numChars                   : Number of chars to write.
     *    int a_highlight                  : Use highlighting?
     *    int a_skip;                      : Force a skip to the next line?
     *
     * Returns:
     *    Zero if successful,
     *    Error code otherwise.
     */

extern int gator_textcb_BlankLine(struct gator_textcb_hdr *, int);
    /*
     * Summary:
     *    Write out some number of blank lines to the given circular
     *    buffer.
     *
     * Args:
     *    struct gator_textcb_hdr *a_cbhdr : Ptr to circ buff hdr.
     *    int a_numBlanks                  : Num. blank lines to write.
     *
     * Returns:
     *    Zero if successful,
     *    Error code otherwise.
     */

extern int gator_textcb_Delete(struct gator_textcb_hdr *a_cbhdr);
    /*
     * Summary:
     *    Delete the storage used by the given circular buffer, including
     *    the header itself.
     *
     * Args:
     *    struct gator_textcb_hdr *a_cbhdr : Ptr to the header of the
     *                                          circ buffer to reap.
     *
     * Returns:
     *    Zero if successful,
     *    Error code otherwise.
     */

#endif /* __gator_textcb_h */
