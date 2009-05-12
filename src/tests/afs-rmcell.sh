#!/bin/sh

source OpenAFS/Dirpath.sh

# Remove database files
/bin/rm -rf ${AFSDBDIR}/prdb.DB0 ${AFSDBDIR}/prdb.DBSYS1 \
	${AFSDBDIR}/vldb.DB0 ${AFSDBDIR}/vldb.DBSYS1 \
	${AFSDBDIR}/kaserver.DB0 ${AFSDBDIR}/kaserver.DBSYS1 

# Remove cell configuration (server-side)
/bin/rm -rf ${AFSCONFDIR}/ThisCell ${AFSCONFDIR}/CellServDB \
	${AFSCONFDIR}/KeyFile ${AFSCONFDIR}/krb.conf ${AFSCONFDIR}/UserList

# Remove remaining server configuration, logs, and local
/bin/rm -rf ${AFSBOSCONFIGDIR}/BosConfig ${AFSLOGSDIR}/logs/* \
	${AFSLOCALDIR}/* 

# Remove data
/bin/rm -rf /vicepa/AFSIDat 
/bin/rm -rf /vicepb/AFSIDat
/bin/rm -rf /vicepa/V*.vol
/bin/rm -rf /vicepb/V*.vol
exit 0
