#include <ctype.h>

#define 	CM_AFSNCNAMESIZE 	36 /* multiple of 4 */
#define         NCSIZE 			300
#define         NHSIZE 			256 /* must be power of 2== NHASHENT */


struct nc {
  unsigned int key;
  struct nc *next, *prev;
  cm_scache_t *dirp, *vp;
  unsigned char name[CM_AFSNCNAMESIZE];   
};

typedef struct {
  unsigned int enters, lookups, misses, removes;
  unsigned int purgeds, purgevs, purgevols, purges;
  unsigned int cycles, lookuprace;
} cm_dnlcstats_t;

#define dnlcHash(ts, hval) for (hval=0; *ts; ts++) {    \
                                hval *= 173;            \
                                hval += cm_foldUpper[(unsigned char)(*ts)]; \
                           }
extern void cm_dnlcEnter(cm_scache_t *adp, char *name, cm_scache_t *avc);
extern void cm_dnlcRemove(cm_scache_t *adp, char *name);
extern void cm_dnlcPurgedp(cm_scache_t *adp);
extern void cm_dnlcPurgevp(cm_scache_t *avc);
extern void cm_dnlcPurge(void);
extern void cm_dnlcPurgeVol(struct AFSFid *fidp);
extern void cm_dnlcInit(void);
extern void cm_dnlcShutdown(void);
extern cm_scache_t* cm_dnlcLookup(cm_scache_t *adp, cm_lookupSearch_t* sp);
