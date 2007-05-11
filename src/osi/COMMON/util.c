/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi_impl.h>
#include <osi/osi_string.h>

const char * osi_ProgramType_names[osi_ProgramType_Max_Id+1] =
    {
	"Library",
	"Basic Overseer Server (bosserver)",
	"Cache Manager (libafs)",
	"File Server (fileserver)",
	"Volume Server (volserver)",
	"Salvager (salvager)",
	"Salvage Server (salvageserver)",
	"Salvage Server Child Process",
	"Protection Database Server (ptserver)",
	"Volume Location Database Server (vlserver)",
	"KA Database Server (kaserver)",
	"Backup Database Server (buserver)",
	"Utility",
	"Ephemeral Utility",
	"Trace Consumer Process",
	"Test Application",
	"Trace Kernel (libktrace)",
	"Backup Coordinator Server",
	"Backup Tape Coordinator (butc)",
	"Update Server (upserver)",
	"Update Client",
	"Basic Overseer Client (bos)",
	"vos",
	"AFS Cache Manager Daemon (afsd)",
	"Remote System Call Daemon (rmtsysd)",
	"Unknown"
    };

const char * osi_ProgramType_shortNames[osi_ProgramType_Max_Id+1] =
    {
	"[lib]",
	"bosserver",
	"libafs",
	"fileserver",
	"volserver",
	"salvager",
	"salvageserver",
	"salvageserver[child]",
	"ptserver",
	"vlserver",
	"kaserver",
	"buserver",
	"[util]",
	"[short util]",
	"[trace consumer]",
	"[test suite]",
	"libktrace",
	"bucoord",
	"butc",
	"upserver",
	"up",
	"bos",
	"vos",
	"afsd",
	"rmtsysd",
	"[unknown]"
    };

/*
 * try to map a program type onto a string
 *
 * [IN] ptype  -- program type enumeration value
 *
 * returns:
 *   a string containing the human readable name for this program type
 */
const char *
osi_ProgramTypeToString(osi_ProgramType_t ptype)
{
    if ((ptype < osi_ProgramType_Min_Id) ||
	(ptype > osi_ProgramType_Max_Id)) {
	ptype = osi_ProgramType_Max_Id;
    }

    return osi_ProgramType_names[ptype];
}

/*
 * try to map a program type onto its abbreviated name
 *
 * [IN] ptype  -- program type enumeration value
 *
 * returns:
 *   a string containing the human readable short name for this program type
 */
const char *
osi_ProgramTypeToShortName(osi_ProgramType_t ptype)
{
    if ((ptype < osi_ProgramType_Min_Id) ||
	(ptype > osi_ProgramType_Max_Id)) {
	ptype = osi_ProgramType_Max_Id;
    }

    return osi_ProgramType_shortNames[ptype];
}

/*
 * try to map a string onto a program type
 *
 * [IN] ptstring  -- string containing program type name
 * [OUT] ptype    -- address in which to store ptype enumeration
 *
 * returns:
 *   OSI_OK on success
 *   OSI_FAIL on unknown string
 */
osi_result
osi_ProgramTypeFromShortName(const char * ptstring,
			     osi_ProgramType_t * ptype)
{
    osi_result res = OSI_FAIL;
    unsigned int i;

    for (i = 0; i < osi_ProgramType_Max_Id; i++) {
	if (!osi_string_cmp(ptstring, osi_ProgramType_shortNames[i])) {
	    *ptype = (osi_ProgramType_t) i;
	    res = OSI_OK;
	    break;
	}
    }

    return res;
}
