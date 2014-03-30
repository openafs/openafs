/*
 * Copyright 2014, Nathaniel Wesley Filardo.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <afsconfig.h>
#include <afs/param.h>
#include <roken.h>
#include <afs/opr.h>
#include "fmt.h"

struct opr_fmt_ctx_priv_s {
    const char *inp;
    char *outp;
    int outleft;
    int written;
};

static void
opr_fmt_cb(opr_fmt_ctx *x, char c) {
    x->priv->written++;

    if(x->priv->outleft > 0) {
	*(x->priv->outp++) = c;
	x->priv->outleft--;
    }
}

static int
opr_fmt_internal(opr_fmt_ctx *ctx, va_list va)
{
    opr_fmt_ctx_priv *priv = ctx->priv;
    char c;
    char sawpct = 0;
    while((c = *(priv->inp++))) {
	if(sawpct == 0) {
	    if(c != '%') {
		opr_fmt_cb(ctx, c);
	    } else {
		sawpct = 1;
	    }
	} else {
	    opr_fmtr fmtr;
	    if((fmtr = ctx->fmtrs[(unsigned int) c]) != NULL) {
		switch(fmtr(ctx, c, va)) {
		case  1: break;
		case  0: sawpct = 0; break;
		case -1: return -1;
		default: opr_Assert(0);
		}
	    } else {
		opr_fmt_cb(ctx, c);
		sawpct = 0;
	    }
	}
    }
    opr_fmt_cb(ctx, '\0'); priv->written--;

    return priv->written;
}

/**
 * Carry out a %-escaped variable interpolation according to a given set of
 * callbacks, user data, and format string.  Output is placed into the given
 * buffer, and no more than the indicated number of bytes will be written.
 *
 * Each formatter callback receives
 *   - the formatting context
 *   - the escape character being processed
 *   - the suffix of the varargs passed to opr_fmt
 * To write to the output buffer, it should invoke ctx->put with ctx and the
 * desired emission character.  It should return one of 0 to indicate
 * success and exit from escape state; 1 to indicate success and a desire
 * to continue in escape state (e.g. while scanning for modifiers as in
 * %0x); or -1 to indicate failure, which will abort the whole opr_fmt call.
 *
 * @param fmtrs Is a 256-entry array of callbacks, one for each possible
 *              unsigned char value. (char is cast to an unsigned int,
 *              internally.)
 * @param user Passed through to the callbacks for callee's use.
 * @param out Output buffer.  Will always be properly nul-terminated on
 *            successful (i.e. non-negative) return.
 * @param n Maximum number of bytes to write to the output buffer.
 * @param fstr The format string.  Additional arguments are passed to the
 *             callbacks for their use ala printf.
 *
 * @return The number of bytes that were (if >= 0 and < n) or would have been
 *         (if >= n) written to the buffer, excluding the terminating nul
 *         byte; in the latter case, the buffer is full of the first n-1
 *         bytes of the string (and is properly nul-terminated).  (This
 *         follows C99's printf conventions and may allow user code to
 *         repeat the call with a big enough buffer the next time, rather
 *         than needing to probe for a large enough buffer.)  If the return
 *         value is < 0, an error occurred during one of the callbacks'
 *         operation.
 */
int
opr_fmt(opr_fmtr *fmtrs, void *user, char *out, int n, const char *fstr, ...)
{
    opr_fmt_ctx_priv priv;
    opr_fmt_ctx ctx;
    int ret;
    va_list va;

    priv.inp = fstr;
    priv.outp = out;
    priv.outleft = n;
    priv.written = 0;

    ctx.fmtrs = fmtrs;
    ctx.user  = user ;
    ctx.put   = opr_fmt_cb;
    ctx.priv  = &priv;

    va_start(va, fstr);
    ret = opr_fmt_internal(&ctx, va);
    va_end(va);

    if (ret > 0 && ret >= n) {
      out[n-1] = '\0';
    }

    return ret;
}
