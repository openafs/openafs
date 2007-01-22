/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>

const char * osi_ProgramType_names[osi_ProgramType_Max_Id+1] =
    {
	"Library",
	"Bosserver",
	"Cache Manager",
	"File Server",
	"Volume Server",
	"Salvager",
	"Salvage Server",
	"Salvage Server Child Process",
	"Protection Database Server",
	"Volume Location Database Server",
	"KA Database Server",
	"Backup Database Server",
	"Utility",
	"Ephemeral Utility",
	"Trace Consumer Process",
	"Test Application",
	"Trace Kernel",
	"Backup Coordinator Server",
	"Backup Tape Coordinator",
	"Unknown"
    };

const char *
osi_ProgramTypeToString(osi_ProgramType_t ptype)
{
    if ((ptype < osi_ProgramType_Min_Id) ||
	(ptype > osi_ProgramType_Max_Id)) {
	ptype = osi_ProgramType_Max_Id;
    }

    return osi_ProgramType_names[ptype];
}
