/* 
 * $Id$
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology
 * For distribution and copying rights, see the file "mit-copyright.h"
 */

#if !defined(lint) && !defined(SABER)
static char *rcsid = "$Id$";
#endif /* lint || SABER */

#include <afs/stds.h>
#include "aklog.h"

int
main(int argc, char *argv[])
{
    aklog(argc, argv);
    exit(0);
}

