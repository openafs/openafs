#!/bin/sh
/bin/rm -rf /usr/afs/db/prdb.DB0 /usr/afs/db/prdb.DBSYS1 /usr/afs/db/vldb.DB0 /usr/afs/db/vldb.DBSYS1 /usr/afs/local/BosConfig /usr/afs/etc/UserList /usr/afs/etc/ThisCell /usr/afs/etc/CellServDB 
/bin/rm -rf /vicepa/AFSIDat 
/bin/rm -rf /vicepb/AFSIDat
/bin/rm -rf /vicepa/V*.vol
/bin/rm -rf /vicepb/V*.vol
exit 0
