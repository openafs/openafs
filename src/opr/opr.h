#ifndef OPENAFS_OPR_OPR_H
#define OPENAFS_OPR_OPR_H 1

/* macros */

#define opr_containerof(ptr, structure, member) \
   ((structure *)((char *)(ptr)-(char *)(&((structure *)NULL)->member)))

/* assert.c */

#ifdef AFS_NT40_ENV
# define opr_abort() opr_NTAbort()
extern void opr_NTAbort(void);
#else
# define opr_abort() abort()
#endif

extern void opr_AssertionFailed(char *, int);
extern void opr_AssertFailU(const char *, const char *, int);

#define opr_Assert(ex) \
    do {if (!(ex)) opr_AssertionFailed(__FILE__, __LINE__);} while(0)

/* casestrcpy.c */
#define lcstring opr_lcstring
#define ucstring opr_ucstring
#define stolower opr_stolower
#define stoupper opr_stoupper
#define strcompose opr_strcompose

extern char *opr_lcstring(char *d, char *s, int n);
extern char *opr_ucstring(char *d, char *s, int n);
extern void opr_stolower(char *s);
extern void opr_stoupper(char *s);
extern char *opr_strcompose(char *buf, size_t len, ...);

#endif
