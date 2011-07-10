#ifndef OPENAFS_OPR_OPR_H
#define OPENAFS_OPR_OPR_H 1

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
