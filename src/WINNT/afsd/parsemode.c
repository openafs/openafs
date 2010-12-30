/*
 * Copyright 1991 by Vicent Archer. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "parsemode.h"
#include "fs.h"

afs_uint32
parsemode(char *symbolic, afs_uint32 oldmode)
{
    afs_uint32 who, mask, u_mask = 022, newmode, tmpmask;
    char action;

    newmode = oldmode & ALL_MODES;
    while (*symbolic) {
        who = 0;
        for (; *symbolic; symbolic++) {
                if (*symbolic == 'a') {
                        who |= ALL_MODES;
                        continue;
                }
                if (*symbolic == 'u') {
                        who |= USR_MODES;
                        continue;
                }
                if (*symbolic == 'g') {
                        who |= GRP_MODES;
                        continue;
                }
                if (*symbolic == 'o') {
                        who |= S_IRWXO;
                        continue;
                }
                break;
        }
        if (!*symbolic || *symbolic == ',') {
            Die(EINVAL, "invalid mode");
            exit(1);
        }
        while (*symbolic) {
            if (*symbolic == ',')
                break;
            switch (*symbolic) {
            default:
                Die(EINVAL, "invalid mode");
                exit(1);
            case '+':
            case '-':
            case '=':
                action = *symbolic++;
            }
            mask = 0;
            for (; *symbolic; symbolic++) {
                if (*symbolic == 'u') {
                    tmpmask = newmode & S_IRWXU;
                    mask |= tmpmask | (tmpmask << 3) | (tmpmask << 6);
                    symbolic++;
                    break;
                }
                if (*symbolic == 'g') {
                    tmpmask = newmode & S_IRWXG;
                    mask |= tmpmask | (tmpmask >> 3) | (tmpmask << 3);
                    symbolic++;
                    break;
                }
                if (*symbolic == 'o') {
                    tmpmask = newmode & S_IRWXO;
                    mask |= tmpmask | (tmpmask >> 3) | (tmpmask >> 6);
                    symbolic++;
                    break;
                }
                if (*symbolic == 'r') {
                    mask |= S_IRUSR | S_IRGRP | S_IROTH;
                    continue;
                }
                if (*symbolic == 'w') {
                    mask |= S_IWUSR | S_IWGRP | S_IWOTH;
                    continue;
                }
                if (*symbolic == 'x') {
                    mask |= EXE_MODES;
                    continue;
                }
                if (*symbolic == 's') {
                    mask |= S_ISUID | S_ISGID;
                    continue;
                }
                if (*symbolic == 'X') {
                    if (S_ISDIR(oldmode) || (oldmode & EXE_MODES))
                        mask |= EXE_MODES;
                    continue;
                }
                if (*symbolic == 't') {
                    mask |= S_ISVTX;
                    who |= S_ISVTX;
                    continue;
                }
                break;
            }
            switch (action) {
            case '=':
                if (who)
                    newmode &= ~who;
                else
                    newmode = 0;
            case '+':
                if (who)
                    newmode |= who & mask;
                else
                    newmode |= mask & (~u_mask);
                break;
            case '-':
                if (who)
                    newmode &= ~(who & mask);
                else
                    newmode &= ~mask | u_mask;
            }
        }
        if (*symbolic)
            symbolic++;
  }
  return(newmode);
}

