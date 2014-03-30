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
#include <tests/tap/basic.h>
#include <opr/fmt.h>

int
testfmtr(opr_fmt_ctx *ctx, char c, va_list va) {
    const char *str = va_arg(va, const char *);
    while(*str) { ctx->put(ctx, *str++); }
    return 0;
}

int
testfmtr2(opr_fmt_ctx *ctx, char c, va_list va) {
    char *str = ctx->user;
    while (*str) { ctx->put(ctx, *str++); }
    return 0;
}

int main()
{
  static opr_fmtr fmtrs[256] = { ['s'] = testfmtr, ['t'] = testfmtr2 };
  static char buf[100];

  plan(1);

  opr_fmt(fmtrs, "EHLO", buf, 100, "FOO %s %t", "abc");
  ok(0 == strcmp(buf,"FOO abc EHLO"), "Basic substitution test");

  return 0;
}
