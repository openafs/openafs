/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Machine-type definitions: IBM AIX 2.2.1 (RT/PC)
 */

#include <mit-cpyright.h>

/* WARNING: This is currently identical to conf-bsd-ibm032.h and should probably change for AIX!! */
#define BSDUNIX			/* This screwes us up in read_pssword */
#define IBMWS
#define IBMWSASM
#define BITS32
#define BIG
#define MSBFIRST
#define MUSTALIGN
