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

#ifndef _OPR_FMT_H_
#define _OPR_FMT_H_

typedef struct opr_fmt_ctx_priv_s opr_fmt_ctx_priv;
typedef struct opr_fmt_ctx_s opr_fmt_ctx;

typedef int (*opr_fmtr)(opr_fmt_ctx *, char, va_list);

struct opr_fmt_ctx_s {
    opr_fmtr *fmtrs;         /* Byte-wise dispatch table; 256 entries */
    void *user;              /* User-controlled private data */
    void (*put)(opr_fmt_ctx *, char); /* Write a character to the buffer */

    opr_fmt_ctx_priv *priv;  /* Private for internals of opr_fmt */
};

int opr_fmt(opr_fmtr *fmtrs, void *user, char *out, int n, const char *fstr, ...);

#endif
