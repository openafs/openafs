/* Include this header if you want the standard 'assert()' macro to have the
 * same behaviour as the new opr_Assert macro
 */
#include <afs/opr.h>

#define assert(ex) opr_Assert(ex)
