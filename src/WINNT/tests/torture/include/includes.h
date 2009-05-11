#ifndef _INCLUDES_H
#define _INCLUDES_H
/* 
   Unix SMB/CIFS implementation.
   Machine customisation and include handling
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) 2002 by Martin Pool <mbp@samba.org>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

//#ifndef NO_CONFIG_H /* for some tests */
//#include "config.h"
//#endif

//#include "local.h"

#ifndef _WIN32_WINNT	
#define _WIN32_WINNT 0x0500 
#endif 
#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <process.h>
#include <stdlib.h>

#include <time.h>

#undef HAVE_KRB5

#define uint16 int
#define uint32 DWORD

#include "proto.h"

#endif /* _INCLUDES_H */

