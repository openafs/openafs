/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/* This include file defines the error code base for each of the packages in
   AFS.  This is an informal system so people should feel free to add their own
   packages if they don't appear here. */

/* The convention is that each package is assigned a small integer.  This
   package number is shifted into the high 16 bits to for the error number base
   for that package.  As an added convention we add 1000 to this base to allow
   the space of the kernel errno numbers to "show" through.  These code will be
   the same in all packages and most packages will probably not use them. */

/* Each package is assumed to have a short two or three (or four or five)
   letter mnemonic which will be used to name external variables, procedures
   and constants.  This prefix will be used to name the constant that defines
   the minimum error code for each package.  Each package should define its own
   constant defining the maximum error code using the same conventions (eg
   KAMAXERROR). */

#ifndef ERROROFFSET

#define ERROROFFSET 1000

#define KAMINERROR    ((  1 <<16)+ERROROFFSET) /* kerberos authentication server */
#define PRMINERROR    ((  2 <<16)+ERROROFFSET) /* AFS protection server */
#define VLMINERROR    ((  3 <<16)+ERROROFFSET) /* Volume Location database */
#define VSMINERROR    ((  4 <<16)+ERROROFFSET) /* Volume Services */
#define RXMINERROR    ((  5 <<16)+ERROROFFSET) /* Rx RPC facility */
#define RXGENMINERROR ((  6 <<16)+ERROROFFSET) /* RxGen */
#define LWPMINERROR   ((  7 <<16)+ERROROFFSET) /* Light Weight Process package */
#define RXKADMINERROR ((  8 <<16)+ERROROFFSET) /* Kerberos security module for Rx */
#define CMDMINERROR   ((  9 <<16)+ERROROFFSET) /* Command line parser utilities */
#define UMINERROR     (( 10 <<16)+ERROROFFSET) /* Ubik replicated database */
#define VICEMINERROR  (( 11 <<16)+ERROROFFSET) /* Vast Integrated Computing Environment */
#define BOSMINERROR   (( 12 <<16)+ERROROFFSET) /* Bozo Operations System */
#define ESMINERROR    (( 13 <<16)+ERROROFFSET) /* Error and Statistics logger */

#endif
