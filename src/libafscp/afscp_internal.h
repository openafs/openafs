#ifndef AFS_SRC_LIBAFSCP_AFSCP_INTERNAL_H
#define AFS_SRC_LIBAFSCP_AFSCP_INTERNAL_H

#include <afs/param.h>
#include <afs/afsint.h>
#include <afs/cellconfig.h>

/* afsint.h conflicts with afscbint.h; provide this here */
extern int RXAFSCB_ExecuteRequest(struct rx_call *);

/* AUTORIGHTS */
extern int _GetSecurityObject(struct afscp_cell *);
extern int _GetVLservers(struct afscp_cell *);
extern int _StatInvalidate(const struct afscp_venusfid *);
extern int _StatStuff(const struct afscp_venusfid *,
		      const struct AFSFetchStatus *);

#ifndef AFSCP_DEBUG
#define afs_dprintf(x)
#else
#define afs_dprintf(x) printf x
#endif

#endif /* AFS_SRC_LIBAFSCP_AFSCP_INTERNAL_H */
