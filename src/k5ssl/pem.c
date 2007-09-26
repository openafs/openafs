/*
 * MIME base 64 encoding.
 * ascii armour.
 * digit character set: [A-Za-z0-9+/=]
 * RFC 2045.
 */

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "pem.h"

struct pemstate {
    int pemfrag, pems, pemw, pemcount;
    int (*pemf)();
    char *pemarg;
    int pempos, pemlen;;
    char pembuf[8];
};

struct pemstate *
pemopen(int (*f)(), void *arg)
{
    struct pemstate *result;

    if (result = (struct pemstate *) malloc(sizeof *result)) {
	memset(result, 0, sizeof *result);
	result->pemf = f;
	result->pemarg = arg;
    }
    return result;
}

int
pemwrite(struct pemstate *state, void *buf, int s)
{
    int c, i, frag, x = s;
    unsigned char *bp = buf;

    if (!state->pemw) state->pemw = 1;
    while (x) {
	--x; c = *bp++;
	for (;;) {
	    switch(state->pems) {
	    case 5:
		return -1;
	    case 3:
		state->pems -= 4;
		++state->pemcount;
	    default:
		++state->pems;
		switch(state->pems) {
		case 1:
		    if (state->pemcount >= 19) {
			state->pemcount = 0;
			if (state->pempos < sizeof state->pembuf)
			    ;
			else if ((*state->pemf)(state->pemarg,
				state->pembuf, state->pempos) != state->pempos) {
			    goto Fail;
			} else state->pempos = 0;
			state->pembuf[state->pempos++] = '\n';
		    }
		    state->pemfrag = (c & 3) << 4;
		    c >>= 2;
		    break;
		case 2:
		    frag = state->pemfrag;
		    state->pemfrag = (c & 15) << 2;
		    c >>= 4;
		    c += frag;
		    break;
		case 3:
		    frag = state->pemfrag;
		    state->pemfrag = (c & 63);
		    c >>= 6;
		    c += frag;
		    break;
		case 0:
		    c = state->pemfrag;
		    break;
		}
		if (c < 26)
			c += 'A';
		else if (c < 52)
			c += ('a'-26);
		else if (c < 62)
			c += ('0'-52);
		else c = "+/"[c-62];
		if (state->pempos < sizeof state->pembuf)
		    ;
		else if ((*state->pemf)(state->pemarg,
			state->pembuf, state->pempos) != state->pempos) {
		Fail:
		    state->pems = 5;
		    return -1;
		}
		else state->pempos = 0;
		state->pembuf[state->pempos++] = c;
		if (state->pems == 3 && state->pemw != 2) continue;
	    }
	    break;
	}
    }
    return s;
}

int
pemread(struct pemstate *state, void *buf, int s)
{
    int result, nextc, temp, r = 0;
    unsigned char *bp = buf;

    state->pemw = 0;
    while (r < s) {
	for (;;) {
	    switch(state->pems) {
	    case 6:
		r = -1;
	    case 5:
		goto Done;
	    case 3:
		state->pems -= 4;
	    }
	    ++state->pems;
	    temp = 1;
	    for (;;) {
		if (state->pempos >= state->pemlen) {
		    state->pempos = 0;
		    state->pemlen = (*state->pemf)(state->pemarg,
			state->pembuf,
			sizeof state->pembuf);
		    if (state->pemlen <= 0) {
		    Eof:
			state->pems = 5;
			goto Done;
		    }
		}
		nextc = state->pembuf[state->pempos++];
		switch(nextc) {
		case '=':
		    goto Eof;
		Fail:
		    state->pems = 5;
		    goto Done;
		case '+': nextc = 62; break;
		case '/': nextc = 63; break;
		default:
		    if (nextc >= 'A' && nextc <= 'Z')
			nextc -= 'A';
		    else if (nextc >= 'a' && nextc <= 'z')
			nextc -= ('a'-26);
		    else if (nextc >= '0' && nextc <= '9')
			nextc -= ('0'-52);
		    else continue;
		    break;
		}
		break;
	    }
	    switch(state->pems) {
	    case 1:
		result = nextc << 2;
		continue;
	    case 2:
		state->pemfrag = (nextc&15)<<4;
		result += (nextc>>4);
		break;
	    case 3:
		result = state->pemfrag + ((nextc>>2)&15);
		state->pemfrag = (nextc&3)<<6;
		break;
	    case 0:
		result = state->pemfrag + nextc;
		break;
	    }
	    break;
	}
	*bp++ = result;
	++r;
    }
Done:
    if (!r && state->pems == 5) state->pems = 6;
    return r;
}

int
pemclose(struct pemstate *state)
{
    int r;

    if (!state) return -1;
    if (state->pemw && state->pems) {
	char *cp = "==" + (state->pems-1);
	state->pemw = 2;
	pemwrite(state, "", 1);
	while (*cp) {
	    if (state->pempos < sizeof state->pembuf)
		;
	    else if ((*state->pemf)(state->pemarg,
		    state->pembuf, state->pempos) != state->pempos) {
		break;
	    }
	    else state->pempos = 0;
	    state->pembuf[state->pempos++] = *cp++;
	}
    }
    if (state->pempos)
    (void) (*state->pemf)(state->pemarg,
	    state->pembuf, state->pempos);
    r = -(state->pems < 5);
    free((char*)state);
    return r;
}
