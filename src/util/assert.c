
#ifndef lint
#endif

/*
############################################################################
#        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        #
#                                                                          #
# Permission to use, copy, modify, and distribute this software and its    #
# documentation for any purpose and without fee is hereby granted,         #
# provided that the above copyright notice appear in all copies and        #
# that both that copyright notice and this permission notice appear in     #
# supporting documentation, and that the name of IBM not be used in        #
# advertising or publicity pertaining to distribution of the software      #
# without specific, written prior permission.                              #
#                                                                          #
# IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL #
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM #
# BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      #
# DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  #
# IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   #
# OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    #
############################################################################
*/
/* ReallyAbort:  called from assert. May/85 */
#include <afs/param.h>
#include <stdio.h>

#ifdef AFS_NT40_ENV
void afs_NTAbort(void)
{
    _asm int 3h; /* always trap. */
}
#endif


void AssertionFailed(char *file, int line)
{
    fprintf(stderr, "Assertion failed! file %s, line %d.\n", file, line);
    fflush(stderr);
#ifdef AFS_NT40_ENV
    afs_NTAbort();
#else
    abort();
#endif
}

