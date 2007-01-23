/* rc6 (TM)
 * Unoptimized sample implementation of Ron Rivest's submission to the
 * AES bakeoff.
 *
 * Salvo Salasio, 19 June 1998
 *
 * Intellectual property notes:  The name of the algorithm (RC6) is
 * trademarked; any property rights to the algorithm or the trademark
 * should be discussed with discussed with the authors of the defining
 * paper "The RC6(TM) Block Cipher": Ronald L. Rivest (MIT),
 * M.J.B. Robshaw (RSA Labs), R. Sidney (RSA Labs), and Y.L. Yin (RSA Labs),
 * distributed 18 June 1998 and available from the lead author's web site.
 *
 * This sample implementation is placed in the public domain by the author,
 * Salvo Salasio.  The ROTL and ROTR definitions were cribbed from RSA Labs'
 * RC5 reference implementation.
 */

#include "k5s_rc6.h"

/* RC6 is parameterized for w-bit words, b bytes of key, and
 * r rounds.  The AES version of RC6 specifies b=16, 24, or 32;
 * w=32; and r=20.
 */
  
#define w 32	/* word size in bits */
#define r 20	/* based on security estimates */

#define P32 0xB7E15163	/* Magic constants for key setup */
#define Q32 0x9E3779B9

/* derived constants */
#define bytes   (w / 8)				/* bytes per word */
/* #define c       ((b + bytes - 1) / bytes)	*//* key in words, rounded up */
#define R24     (2 * r + 4)
#define lgw     5                       	/* log2(w) -- wussed out */

#if 0	/* defined(__GNUC__) && defined(_IBMR2) --these don't help gnu c */
#define GETWORD(p,w)	asm("lbrx %0,0,%1" : "=r"(w) : "r"(p))
#define PUTWORD(p,w)	asm("stbrx %0,0,%1" : : "r"(w), "r"(p))
#endif
#if defined(__GNUC__) && defined(i386)
#define GETWORD(p,w)	(w) = *((long*)(p))
#define PUTWORD(p,w)	*((long*)(p)) = (w)
#endif

/* Rotations */
#ifndef ROTL
#define ROTL(x,y) (((x)<<(y&(w-1))) | ((x)>>(w-(y&(w-1)))))
#endif
#ifndef ROTR
#define ROTR(x,y) (((x)>>(y&(w-1))) | ((x)<<(w-(y&(w-1)))))
#endif
#ifndef GETWORD
#define GETWORD(p,w)	((w) = (0[p] + (((RC6_INT32)1[p])<<8) + (((RC6_INT32)2[p])<<16) + (((RC6_INT32)3[p])<<24)))
#endif
#ifndef PUTWORD
#define PUTWORD(p,w)	((0[p] = w), (1[p] = (w >> 8)), (2[p] = (w>>16)), (3[p] = (w>>24)))
#endif

int RC6_set_key(struct rc6_ks_struct *ks, unsigned b, const unsigned char *K)
{
	int i, j, s, v, c;
	RC6_INT32 L[(32 + bytes - 1) / bytes]; /* Big enough for max b */
	RC6_INT32 A, B;

	if (b < RC6_MIN_KEY_LENGTH || b > RC6_MAX_KEY_LENGTH)
		return -1;
	c = ((b + bytes - 1) / bytes);

	j = 0;
	for (i = b; i >= 4; i -= 4)
	{
		GETWORD(K, L[j++]);
		K += 4;
	}
	if (i)
	{
		L[j] = 0;
		switch(i)
		{
		case 3: L[j] += (((RC6_INT32)2[K]) << 16);
		case 2: L[j] += (((RC6_INT32)1[K]) << 8);
		case 1:	L[j] += 0[K];
		}
	}

	ks->S[0] = P32;
	for (i = 1; i <= 2 * r + 3; i++)
		ks->S[i] = ks->S[i - 1] + Q32;

	A = B = i = j = 0;
	v = R24;
	if (c > v) v = c;
	v *= 3;

	s = 0;
	for (;;)
	{
		A = ks->S[i] + A + B;
		A = ks->S[i] = ROTL(A, 3);
		if (++s >= v) break;
		B = A + B;
		B = L[j] = ROTL(L[j] + B, B);
		++i; if (i >= R24) i = 0;
		++j; if (j >= c) j = 0;
	}
#if 1
{
volatile RC6_INT32 *lp;
lp = L;
	lp[0] = 0; lp[1] = 0; lp[2] = 0; lp[3] = 0;
	lp[4] = 0; lp[5] = 0; lp[6] = 0; lp[7] = 0;
}
#else
	memset(L, 0, sizeof L);
#endif
	return 0;
}

void RC6_encrypt(const struct rc6_ks_struct *ks, const unsigned char *pt, unsigned char *ct)
{
	register RC6_INT32 A, B, C, D, t, u;
	int i;

	GETWORD(pt+0, A);
	GETWORD(pt+4, B);
	GETWORD(pt+8, C);
	GETWORD(pt+12, D);
	B += ks->S[0];
	D += ks->S[1];
	for (i = 2; i <= 2 * r; i += 2)
	{
		t = B * (2 * B + 1); t = ROTL(t, lgw);
		u = D * (2 * D + 1); u = ROTL(u, lgw);
		A = ROTL(A ^ t, u) + ks->S[i];
		C = ROTL(C ^ u, t) + ks->S[i + 1];
		t = A;
		A = B;
		B = C;
		C = D;
		D = t;
	}
	A += ks->S[2 * r + 2];
	C += ks->S[2 * r + 3];
	PUTWORD(ct, A);
	PUTWORD(ct+4, B);
	PUTWORD(ct+8, C);
	PUTWORD(ct+12, D);
}

void RC6_decrypt(const struct rc6_ks_struct *ks, const unsigned char *ct, unsigned char *pt)
{
	register RC6_INT32 A, B, C, D, t, u;
	int i;

	GETWORD(ct+0, A);
	GETWORD(ct+4, B);
	GETWORD(ct+8, C);
	GETWORD(ct+12, D);
	C -= ks->S[2 * r + 3];
	A -= ks->S[2 * r + 2];
	for (i = 2 * r; i >= 2; i -= 2)
	{
		t = D;
		D = C;
		C = B;
		B = A;
		A = t;
		u = D * (2 * D + 1); u = ROTL(u, lgw);
		t = B * (2 * B + 1); t = ROTL(t, lgw);
		C = ROTR(C - ks->S[i + 1], t) ^ u;
		A = ROTR(A - ks->S[i], u) ^ t;
	}
	D -= ks->S[1];
	B -= ks->S[0];
	PUTWORD(pt, A);
	PUTWORD(pt+4, B);
	PUTWORD(pt+8, C);
	PUTWORD(pt+12, D);
}
