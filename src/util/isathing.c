/* Copyright (C) 1992 Transarc Corporation - All rights reserved */
#include <ctype.h>

/* checks a string to determine whether it's a non-negative decimal integer or not */
int
isint (str)
char *str;
{
char *i;

for (i=str; *i && !isspace(*i); i++) {
    if (!isdigit(*i))
      return 0;
  }

return 1;
}
