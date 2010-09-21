Sending process signals to the File Server Process can change its
behavior in the following ways:

  Process          Signal       OS     Result
  ---------------------------------------------------------------------

  File Server      XCPU        Unix    Prints a list of client IP
                                       Addresses.

  File Server      USR2      Windows   Prints a list of client IP
                                       Addresses.

  File Server      POLL        HPUX    Prints a list of client IP
                                       Addresses.

  Any server       TSTP        Any     Increases Debug level by a power
                                       of 5 -- 1,5,25,125, etc.
                                       This has the same effect as the
                                       -d XXX command-line option.

  Any Server       HUP         Any     Resets Debug level to 0

  File Server      TERM        Any     Run minor instrumentation over
                                       the list of descriptors.

  Other Servers    TERM        Any     Causes the process to quit.

  File Server      QUIT        Any     Causes the File Server to Quit.
                                       Bos Server knows this.

The basic metric of whether an AFS file server is doing well is the number
of connections waiting for a thread,
which can be found by running the following command:

   % rxdebug <server> | grep waiting_for | wc -l

Each line returned by C<rxdebug> that contains the text "waiting_for"
represents a connection that's waiting for a file server thread.

If the blocked connection count is ever above 0, the server is having
problems replying to clients in a timely fashion.  If it gets above 10,
roughly, there will be noticeable slowness by the user.  The total number of
connections is a mostly irrelevant number that goes essentially
monotonically for as long as the server has been running and then goes back
down to zero when it's restarted.

The most common cause of blocked connections rising on a server is some
process somewhere performing an abnormal number of accesses to that server
and its volumes.  If multiple servers have a blocked connection count, the
most likely explanation is that there is a volume replicated between those
servers that is absorbing an abnormally high access rate.

To get an access count on all the volumes on a server, run:

   % vos listvol <server> -long

and save the output in a file.  The results will look like a bunch of B<vos
examine> output for each volume on the server.  Look for lines like:

   40065 accesses in the past day (i.e., vnode references)

and look for volumes with an abnormally high number of accesses.  Anything
over 10,000 is fairly high, but some volumes like root.cell and other
volumes close to the root of the cell will have that many hits routinely.
Anything over 100,000 is generally abnormally high.  The count resets about
once a day.

Another approach that can be used to narrow the possibilities for a
replicated volume, when multiple servers are having trouble, is to find all
replicated volumes for that server.  Run:

   % vos listvldb -server <server>

where <server> is one of the servers having problems to refresh the VLDB
cache, and then run:

   % vos listvldb -server <server> -part <partition>

to get a list of all volumes on that server and partition, including every
other server with replicas.

Once the volume causing the problem has been identified, the best way to
deal with the problem is to move that volume to another server with a low
load or to stop any runaway programs that are accessing that volume
unnecessarily.  Often the volume will be enough information to tell what's
going on.

If you still need additional information about who's hitting that server,
sometimes you can guess at that information from the failed callbacks in the
F<FileLog> log in F</var/log/afs> on the server, or from the output of:

   % /usr/afsws/etc/rxdebug <server> -rxstats

but the best way is to turn on debugging output from the file server.
(Warning: This generates a lot of output into FileLog on the AFS server.)
To do this, log on to the AFS server, find the PID of the fileserver
process, and do:

    kill -TSTP <pid>

where <pid> is the PID of the file server process.  This will raise the
debugging level so that you'll start seeing what people are actually doing
on the server.  You can do this up to three more times to get even more
output if needed.  To reset the debugging level back to normal, use (The
following command will NOT terminate the file server):

    kill -HUP <pid>

The debugging setting on the File Server should be reset back to normal when
debugging is no longer needed.  Otherwise, the AFS server may well fill its
disks with debugging output.

The lines of the debugging output that are most useful for debugging load
problems are:

    SAFS_FetchStatus,  Fid = 2003828163.77154.82248, Host 171.64.15.76
    SRXAFS_FetchData, Fid = 2003828163.77154.82248

(The example above is partly truncated to highlight the interesting
information).  The Fid identifies the volume and inode within the volume;
the volume is the first long number.  So, for example, this was:

   % vos examine 2003828163
   pubsw.matlab61                   2003828163 RW    1040060 K  On-line
       afssvr5.Stanford.EDU /vicepa 
       RWrite 2003828163 ROnly 2003828164 Backup 2003828165 
       MaxQuota    3000000 K 
       Creation    Mon Aug  6 16:40:55 2001
       Last Update Tue Jul 30 19:00:25 2002
       86181 accesses in the past day (i.e., vnode references)

       RWrite: 2003828163    ROnly: 2003828164    Backup: 2003828165
       number of sites -> 3
          server afssvr5.Stanford.EDU partition /vicepa RW Site 
          server afssvr11.Stanford.EDU partition /vicepd RO Site 
          server afssvr5.Stanford.EDU partition /vicepa RO Site 

and from the Host information one can tell what system is accessing that
volume.

Note that the output of L<vos_examine(1)> also includes the access count, so
once the problem has been identified, vos examine can be used to see if the
access count is still increasing.  Also remember that you can run vos
examine on the read-only replica (e.g., pubsw.matlab61.readonly) to see the
access counts on the read-only replica on all of the servers that it's
located on.
