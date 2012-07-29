#ifndef OPENAFS_OPR_OPR_H
#define OPENAFS_OPR_OPR_H 1

/* macros */

/* should use offsetof() if available */
#define opr_containerof(ptr, structure, member) \
   ((structure *)((char *)(ptr)-(char *)(&((structure *)NULL)->member)))

/* assert.c */

#ifdef AFS_NT40_ENV
# define opr_abort() opr_NTAbort()
extern void opr_NTAbort(void);
#else
# define opr_abort() abort()
#endif

extern void opr_AssertionFailed(char *, int) AFS_NORETURN;
extern void opr_AssertFailU(const char *, const char *, int) AFS_NORETURN;

#define opr_Assert(ex) \
    do {if (!(ex)) opr_AssertionFailed(__FILE__, __LINE__);} while(0)

/* casestrcpy.c */
#define lcstring opr_lcstring
#define ucstring opr_ucstring
#define stolower opr_stolower
#define stoupper opr_stoupper
/* XXX str* is in the implementation namespace when <string.h> is included */
#define strcompose opr_strcompose

extern char *opr_lcstring(char *d, const char *s, int n) AFS_NONNULL((1,2));
extern char *opr_ucstring(char *d, const char *s, int n) AFS_NONNULL((1,2));
extern void opr_stolower(char *s) AFS_NONNULL(());
extern void opr_stoupper(char *s) AFS_NONNULL(());
extern char *opr_strcompose(char *buf, size_t len, ...) AFS_NONNULL((1));

#endif
