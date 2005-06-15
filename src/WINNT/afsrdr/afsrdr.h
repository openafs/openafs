/* copyright (c) 2005
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any purpose,
 * so long as the name of the university of michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.  if
 * the above copyright notice or any other identification of the
 * university of michigan is included in any copy of any portion of
 * this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the
 * university of michigan as to its fitness for any purpose, and without
 * warranty by the university of michigan of any kind, either express 
 * or implied, including without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  the regents
 * of the university of michigan shall not be liable for any damages,   
 * including special, indirect, incidental, or consequential damages, 
 * with respect to any claim arising out or in connection with the use
 * of the software, even if it has been or is hereafter advised of the
 * possibility of such damages.
 */

/* versioning history
 * 
 * 03-jun 2005 (eric williams) entered into versioning
 */

#include <ntifs.h>

#define rpt0(args) 
#define rpt1(args) 
#define rpt2(args) 
#define rpt3(args) 
#define rpt4(args) 
#define rpt5(args) 

struct AfsRdrExtension
{
struct ComExtension *com;
KMUTEX protectMutex;
PNOTIFY_SYNC notifyList;
LIST_ENTRY listHead;
NPAGED_LOOKASIDE_LIST fcbMemList;
FAST_MUTEX fcbLock;
NPAGED_LOOKASIDE_LIST ccbMemList;
RTL_GENERIC_TABLE fcbTable;
CACHE_MANAGER_CALLBACKS callbacks;
};

struct ComExtension
{
struct AfsRdrExtension *rdr;
LIST_ENTRY outReqList;
KSPIN_LOCK outLock;
RTL_GENERIC_TABLE inTable;
FAST_MUTEX inLock;
KEVENT outEvent, cancelEvent;
};

extern struct AfsRdrExtension *rdrExt;
extern struct ComExtension *comExt;


void ifs_lock_rpcs();
void ifs_unlock_rpcs();
