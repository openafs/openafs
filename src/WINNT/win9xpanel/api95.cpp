/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/
/* api functions for access to AFS pioctl functions from windows gui */

#include "stdafx.h"
#ifdef _MFC_VER
extern "C" {
#endif
#include <afs/param.h>
#include <afs/stds.h>
#ifdef _MFC_VER
	}
#endif
/*#include <afs/pioctl_nt.h>*/
#ifndef _MFC_VER

#include <osi.h>
#include <afsint.h>

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#endif

typedef short int16;
typedef unsigned short u_int16;

#ifdef _MFC_VER
extern "C" {
#endif
#include <afs\fs_utils.h>
#ifdef _MFC_VER
	}
#endif

int ShutdownAfs()
{
  struct ViceIoctl blob;
  int code;
  
  blob.in_size = 0;
  blob.out_size = 0;
  code = pioctl(0, VIOC_SHUTDOWN, &blob, 0);

  return code;
}

