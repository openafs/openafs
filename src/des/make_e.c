/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Part of the MIT Project Athena Kerberos encryption system,
 * originally written 8/85 by Steve Miller.
 */

#include <afsconfig.h>
#include <afs/param.h>


#include "mit-cpyright.h"
#include <stdio.h>
#include "tables.h"

#include "AFS_component_version_number.c"

main()
{
    int i;

    /* clear the output */
    fprintf(stdout, "\n\tL2 = 0; R2 = 0;");

    /* only take bits from R1, put into either L2 or R2 */
    /* first setup E */
    fprintf(stdout, "\n/* E operations */\n/* right to left */\n");
    /* first list mapping from left to left */

    for (i = 0; i <= 31; i++)
	if (E[i] < 32)
	    fprintf(stdout, "\n\tif (R1 & (1<<%d)) L2 |= 1<<%d;", E[i], i);

    fprintf(stdout, "\n\n/* now from right to right */\n");
    /*  list mapping from left to right */
    for (i = 32; i <= 47; i++)
	if (E[i] < 32)
	    fprintf(stdout, "\n\tif (R1 & (1<<%d)) R2 |= 1<<%d;", E[i],
		    i - 32);

    fprintf(stdout, "\n");
}
