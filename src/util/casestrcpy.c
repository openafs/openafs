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

#include <afs/param.h>
#include <ctype.h>
#include <stddef.h>
#include <stdarg.h>

/* Just like strncpy but shift-case in transit and forces null termination */
char *lcstring (char *d, char *s, int n)
{   char *original_d = d;
    char  c;

    if ((s == 0) || (d == 0)) return 0;	/* just to be safe */
    while (n) {
	c = *s++;
	if (isupper(c)) c = tolower(c);
	*d++ = c;
	if (c == 0) break;		/* quit after transferring null */
	if (--n == 0) *(d-1) = 0;	/* make sure null terminated */
    }
    return original_d;
}

char *ucstring (char *d, char *s, int n)
{   char *original_d = d;
    char  c;

    if ((s == 0) || (d == 0)) return 0;
    while (n) {
	c = *s++;
	if (islower(c)) c = toupper(c);
	*d++ = c;
	if (c == 0) break;
	if (--n == 0) *(d-1) = 0;	/* make sure null terminated */
    }
    return original_d;
}

/* strcompose - concatenate strings passed to it.
 * Input: 
 *   buf: storage for the composed string. Any data in it will be lost.
 *   len: length of the buffer.
 *   ...: variable number of string arguments. The last argument must be
 *        NULL. 
 * Returns buf or NULL if the buffer was not sufficiently large.
 */             
char *strcompose(char *buf, size_t len, ...)
{
  va_list ap;
  size_t spaceleft = len - 1;
  char *str;
  size_t slen; 

  if (buf == NULL || len <= 0)
    return NULL;

  *buf = '\0';
  va_start(ap, len);
  str = va_arg(ap, char *);
  while(str) { 
    slen = strlen(str);
    if (spaceleft < slen) /* not enough space left */
      return NULL;
    
    strcat(buf, str);
    spaceleft -= slen;
    
    str = va_arg(ap, char *);
  }
  va_end(ap);

  return buf;
}

