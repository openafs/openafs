/* Copyright (c) 2004 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __NETIDMGR_VERSION_H
#define __NETIDMGR_VERSION_H

#include<windows.h>

/* Version number macros */
#define KH_VERSION_MAJOR 	1
#define KH_VERSION_MINOR 	1
#define KH_VERSION_PATCH 	0
#define KH_VERSION_AUX 		1

#define KH_VERSION_API          5
#define KH_VERSION_API_MINCOMPAT 5

#define KH_VERSION_LIST 	1,1,0,1
#define KH_VERSION_STRING 	"1.1.0.1"
#define KH_VERSION_STRINGW 	L"1.1.0.1"
#define KH_VERSION_STRINGC 	"1,1,0,1"
#define KH_VERSION_STRINGCW 	L"1,1,0,1"
#define KH_VERSION_STRINGAPI    "5"

/* Version definition macros */
#define KH_VER_FILEFLAGMASK     0x17L
#define KH_VER_FILEFLAGS 	0
#define KH_VER_FILEOS 		VOS_NT_WINDOWS32
#define KH_VER_FILETYPEDLL 	VFT_DLL
#define KH_VER_FILETYPEAPP 	VFT_APP

/* Special macros for NetIDMgr special string resources */
#define NIMV_MODULE             "NIDM_Module"
#define NIMV_PLUGINS            "NIDM_Plugins"
#define NIMV_APIVER             "NIDM_APIVers"
#define NIMV_SUPPORT            "NIDM_Support"

#endif
