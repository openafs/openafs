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

#include <sys/time.h>

main() {
	struct timeval blat[10], notmuch;
	unsigned int approx[3];
	int i;
	approx[0] = FT_ApproxTime();
	notmuch.tv_sec = 0;
	notmuch.tv_usec = 300000;
	for (i=0; i<5; i++) {
	    FT_GetTimeOfDay(&blat[i], 0);
	    select(0,0,0,0,&notmuch);
	}
	sleep(5);
	approx[1] = FT_ApproxTime();
	for (i=5; i<10; i++) FT_GetTimeOfDay(&blat[i], 0);
	sleep(5);
	approx[2] = FT_ApproxTime();
	for (i = 0; i<10; i++)
	    printf("%u.%u\n", blat[i].tv_sec, blat[i].tv_usec);
	printf("\n%u %u %u\n", approx[0], approx[1], approx[2]);
}
