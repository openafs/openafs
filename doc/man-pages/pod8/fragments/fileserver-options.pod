=over 4

=item B<-auditlog> [<I<interface>>:]<I<path>>[:<I<options>>]

Turns on audit logging, and sets the path for the audit log.  The audit
log records information about RPC calls, including the name of the RPC
call, the host that submitted the call, the authenticated entity (user)
that issued the call, the parameters for the call, and if the call
succeeded or failed.

The parameter to B<-auditlog> contains three parts separated by a colon
(see examples below).

The first part is the optional interface name. The default audit
interface is C<file> and can be changed by the B<-audit-interface> option.

The second part is the path to the log file and is required.  Note the path
to the file cannot itself contain a colon.

The third part are parameters that will be passed to the audit interface.
The parameters are optional and the value and format is specific to the
audit interface.

The audit interfaces are:

=over 4

=item B<file>

The C<file> interface writes audit messages to the specified file.
There are no optional parameters to the file interface. This is the default
interface unless changed by the B<-audit-interface> option.

=item B<sysvmq>

The C<sysvmq> interface writes audit messages to a SYSV message (see L<msgget(2)>
and L<msgrcv(2)>). The C<sysvmq> interface writes to the key C<ftok(msgqpath, 1)>,
where C<msqpath> is specified by the I<path to log file> parameter. There are no
optional parameters to the sysvmq interface.

=back

Multiple audit logs can be set up with different interfaces or different
I<path to log file>.

Examples:

    -auditlog /path/to/file
    -auditlog file:/path/to/file
    -auditlog sysvmq:/path/to/sysvmq
    -auditlog /path/to/file -auditlog /path/to/file2

=item B<-audit-interface> <I<default interface>>

Sets the default audit interface used by the B<-auditlog> option.  The
initial default is the C<file> interface.

See B<-auditlog> for information on the different audit interfaces.

=item B<-d> <I<debug level>>

Sets the detail level for the debugging trace written to the
F</usr/afs/logs/FileLog> file. Provide one of the following values, each
of which produces an increasingly detailed trace: C<0>, C<1>, C<5>, C<25>,
and C<125>. The default value of C<0> produces only a few messages.

=item B<-p> <I<number of processes>>

Sets the number of threads to run. Provide a positive integer.
The File Server creates and uses five threads for special purposes, 
in addition to the number specified (but if this argument specifies 
the maximum possible number, the File Server automatically uses five 
of the threads for its own purposes).

The maximum number of threads can differ in each release of OpenAFS.
Consult the I<OpenAFS Release Notes> for the current release.

=item B<-spare> <I<number of spare blocks>>

Specifies the number of additional kilobytes an application can store in a
volume after the quota is exceeded. Provide a positive integer; a value of
C<0> prevents the volume from ever exceeding its quota. Do not combine
this argument with the B<-pctspare> argument.

=item B<-pctspare> <I<percentage spare>>

Specifies the amount by which the File Server allows a volume to exceed
its quota, as a percentage of the quota. Provide an integer between C<0>
and C<99>. A value of C<0> prevents the volume from ever exceeding its
quota. Do not combine this argument with the B<-spare> argument.

=item B<-b> <I<buffers>>

Sets the number of directory buffers. Provide a positive integer.

=item B<-l> <I<large vnodes>>

Sets the number of large vnodes available in memory for caching directory
elements. Provide a positive integer.

=item B<-s> <I<small nodes>>

Sets the number of small vnodes available in memory for caching file
elements. Provide a positive integer.

=item B<-vc> <I<volume cachesize>>

Sets the number of volumes the File Server can cache in memory.  Provide a
positive integer.

=item B<-w> <I<call back wait interval>>

Sets the interval at which the daemon spawned by the File Server performs
its maintenance tasks. Do not use this argument; changing the default
value can cause unpredictable behavior.

=item B<-cb> <I<number of callbacks>>

Sets the number of callbacks the File Server can track. Provide a positive
integer.

=item B<-banner>

Prints the following banner to F</dev/console> about every 10 minutes.

   File Server is running at I<time>.

=item B<-novbc>

Prevents the File Server from breaking the callbacks that Cache Managers
hold on a volume that the File Server is reattaching after the volume was
offline (as a result of the B<vos restore> command, for example). Use of
this flag is strongly discouraged.

=item B<-nobusy>

This option slightly changes the error codes reported to clients when an
unattached volume is accessed by a client during fileserver startup.

Normally, non-DAFS fileservers start accepting requests immediately on startup,
but attachment of volumes can take a while. So if a client tries to access a
volume that is not attached simply because the fileserver hasn't attached it
yet, that client will get an error. With the B<-nobusy> option present, the
fileserver will immediately respond with an error code that indicates the
server is starting up. However, some older clients (before OpenAFS 1.0) don't
understand this error code, and may not function optimally. So the default
behavior, without the B<-nobusy> option, is to at first respond with a
different error code that is understood by more clients, but is
indistinguishable from other scenarios where the volume is busy and not
attached for other reasons.

There is usually no reason to use this option under normal operation.

=item B<-implicit> <I<admin mode bits>>

Defines the set of permissions granted by default to the
system:administrators group on the ACL of every directory in a volume
stored on the file server machine. Provide one or more of the standard
permission letters (C<rlidwka>) and auxiliary permission letters
(C<ABCDEFGH>), or one of the shorthand notations for groups of permissions
(C<all>, C<none>, C<read>, and C<write>). To review the meaning of the
permissions, see the B<fs setacl> reference page.

=item B<-readonly>

Don't allow writes to this fileserver.

=item B<-admin-write>

Allows write requests for members of system:administrators on the read-only
fileserver in question. The C<-admin-write> option has no effect when
C<-readonly> is not specified.

=item B<-hr> <I<number of hours between refreshing the host cps>>

Specifies how often the File Server refreshes its knowledge of the
machines that belong to protection groups (refreshes the host CPSs for
machines). The File Server must update this information to enable users
from machines recently added to protection groups to access data for which
those machines now have the necessary ACL permissions.

=item B<-busyat> <I<< redirect clients when queue > n >>>

Defines the number of incoming RPCs that can be waiting for a response
from the File Server before the File Server returns the error code
C<VBUSY> to the Cache Manager that sent the latest RPC. In response, the
Cache Manager retransmits the RPC after a delay. This argument prevents
the accumulation of so many waiting RPCs that the File Server can never
process them all. Provide a positive integer.  The default value is
C<600>.

=item B<-rxpck> <I<number of rx extra packets>>

Controls the number of Rx packets the File Server uses to store data for
incoming RPCs that it is currently handling, that are waiting for a
response, and for replies that are not yet complete. Provide a positive
integer.

=item B<-rxdbg>

Writes a trace of the File Server's operations on Rx packets to the file
F</usr/afs/logs/rx_dbg>.

=item B<-rxdbge>

Writes a trace of the File Server's operations on Rx events (such as
retransmissions) to the file F</usr/afs/logs/rx_dbg>.

=item B<-rxmaxmtu> <I<bytes>>

Defines the maximum size of an MTU.  The value must be between the
minimum and maximum packet data sizes for Rx.

=item B<-jumbo>

Allows the server to send and receive jumbograms. A jumbogram is
a large-size packet composed of 2 to 4 normal Rx data packets that share
the same header. The fileserver does not use jumbograms by default, as some
routers are not capable of properly breaking the jumbogram into smaller
packets and reassembling them.

=item B<-nojumbo>

Deprecated; jumbograms are disabled by default.

=item B<-rxbind>

Force the fileserver to only bind to one IP address.

=item B<-allow-dotted-principals>

By default, the RXKAD security layer will disallow access by Kerberos
principals with a dot in the first component of their name. This is to avoid
the confusion where principals user/admin and user.admin are both mapped to the
user.admin PTS entry. Sites whose Kerberos realms don't have these collisions 
between principal names may disable this check by starting the server
with this option.

=item B<-L>

Sets values for many arguments in a manner suitable for a large file
server machine. Combine this flag with any option except the B<-S> flag;
omit both flags to set values suitable for a medium-sized file server
machine.

=item B<-S>

Sets values for many arguments in a manner suitable for a small file
server machine. Combine this flag with any option except the B<-L> flag;
omit both flags to set values suitable for a medium-sized file server
machine.

=item B<-realm> <I<Kerberos realm name>>

Defines the Kerberos realm name for the File Server to use. If this
argument is not provided, it uses the realm name corresponding to the cell
listed in the local F</usr/afs/etc/ThisCell> file.

=item B<-udpsize> <I<size of socket buffer in bytes>>

Sets the size of the UDP buffer, which is 64 KB by default. Provide a
positive integer, preferably larger than the default.

=item B<-sendsize> <I<size of send buffer in bytes>>

Sets the size of the send buffer, which is 16384 bytes by default.

=item B<-abortthreshold> <I<abort threshold>>

Sets the abort threshold, which is triggered when an AFS client sends
a number of FetchStatus requests in a row and all of them fail due to
access control or some other error. When the abort threshold is
reached, the file server starts to slow down the responses to the
problem client in order to reduce the load on the file server.

The throttling behaviour can cause issues especially for some versions
of the Windows OpenAFS client. When using Windows Explorer to navigate
the AFS directory tree, directories with only "look" access for the
current user may load more slowly because of the throttling. This is
because the Windows OpenAFS client sends FetchStatus calls one at a
time instead of in bulk like the Unix Open AFS client.

Setting the threshold to 0 disables the throttling behavior. This
option is available in OpenAFS versions 1.4.1 and later.

=item B<-enable_peer_stats>

Activates the collection of Rx statistics and allocates memory for their
storage. For each connection with a specific UDP port on another machine,
a separate record is kept for each type of RPC (FetchFile, GetStatus, and
so on) sent or received. To display or otherwise access the records, use
the Rx Monitoring API.

=item B<-enable_process_stats>

Activates the collection of Rx statistics and allocates memory for their
storage. A separate record is kept for each type of RPC (FetchFile,
GetStatus, and so on) sent or received, aggregated over all connections to
other machines. To display or otherwise access the records, use the Rx
Monitoring API.

=item B<-syslog> [<loglevel>]

Use syslog instead of the normal logging location for the fileserver
process.  If provided, log messages are at <loglevel> instead of the
default LOG_USER.

=item B<-mrafslogs>

Use MR-AFS (Multi-Resident) style logging.  This option is deprecated.

=item B<-transarc-logs>

Use Transarc style logging features. Rename the existing log file
F</usr/afs/logs/FileLog> to F</usr/afs/logs/FileLog.old> when the fileserver is
restarted.  This option is provided for compatibility with older versions.

=item B<-saneacls>

Offer the SANEACLS capability for the fileserver.  This option is
currently unimplemented.

-item B<-cve-2018-7168-enforce>

To address L<CVE-2018-7168|https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2018-7168>
disable fileserver opcode RXAFS_StoreACL (134).  This opcode was replaced
with RXAFS_StoreACL (164).  The opcode 164 is always available; this option serves
only to disable the old opcode 134, for which client intent is unreliable.

=item B<-help>

Prints the online help for this command. All other valid options are
ignored.

=item B<-vhandle-setaside> <I<fds reserved for non-cache io>>

Number of file handles set aside for I/O not in the cache. Defaults to 128.

=item B<-vhandle-max-cachesize> <I<max open files>>

Maximum number of available file handles.

=item B<-vhandle-initial-cachesize> <I<initial open file cache>>

Number of file handles set aside for I/O in the cache. Defaults to 128.

=item B<-vattachpar> <I<number of volume attach threads>>

The number of threads assigned to attach and detach volumes.  The default
is 1.  Warning: many of the I/O parallelism features of Demand-Attach
Fileserver are turned off when the number of volume attach threads is only
1.

This option is only meaningful for a file server built with pthreads
support.

=item B<-m> <I<min percentage spare in partition>>

Specifies the percentage of each AFS server partition that the AIX version
of the File Server creates as a reserve. Specify an integer value between
C<0> and C<30>; the default is 8%. A value of C<0> means that the
partition can become completely full, which can have serious negative
consequences.  This option is not supported on platforms other than AIX.

=item B<-lock>

Prevents any portion of the fileserver binary from being paged (swapped)
out of memory on a file server machine running the IRIX operating system.
This option is not supported on platforms other than IRIX.

=item B<-offline-timeout> <I<timeout in seconds>>

Setting this option to I<N> means that if any clients are reading from a
volume when we want to offline that volume (for example, as part of
releasing a volume), we will wait I<N> seconds for the clients' request
to finish. If the clients' requests have not finished, we will then
interrupt the client requests and send an error to those clients,
allowing the volume to go offline.

If a client is interrupted, from the client's point of view, it will
appear as if they had accessed the volume after it had gone offline. For
RO volumes, this mean the client should fail-over to other valid RO
sites for that volume. This option may speed up volume releases if
volumes are being accessed by clients that have slow or unreliable
network connections.

Setting this option to C<0> means to interrupt clients immediately if a
volume is waiting to go offline. Setting this option to C<-1> means to
wait forever for client requests to finish. The default value is C<-1>.

=item B<-offline-shutdown-timeout> <I<timeout in seconds>>

This option behaves similarly to B<-offline-timeout> but applies to
volumes that are going offline as part of the fileserver shutdown
process. If the value specified is I<N>, we will interrupt any clients
reading from volumes after I<N> seconds have passed since we first
needed to wait for a volume to offline during the shutdown process.

Setting this option to C<0> means to interrupt all clients reading from
volumes immediately during the shutdown process. Setting this option to
C<-1> means to wait forever for client requests to finish during the
shutdown process.

If B<-offline-timeout> is specified, the default value of
B<-offline-shutdown-timeout> is the value specified for
B<-offline-timeout>. Otherwise, the default value is C<-1>.

=item B<-sync> <always | onclose | none>

This option changes how hard the fileserver tries to ensure that data written
to volumes actually hits the physical disk.

Normally, when the fileserver writes to disk, the underlying filesystem or
Operating System may delay writes from actually going to disk, and reorder
which writes hit the disk first. So, during an unclean shutdown of the machine
(if the power goes out, or the machine crashes, etc), file data may become lost
that the server previously told clients was already successfully written.

To try to mitigate this, the fileserver will try to "sync" file data to the
physical disk at numerous points during various I/O. However, this can result
in significantly reduced performance. Depending on the usage patterns, this may
or may not be acceptable. This option dictates specifically what the fileserver
does when it wants to perform a "sync".

There are several options; pass one of these as the argument to -sync. The
default is C<onclose>.

=over 4

=item always

This causes a sync operation to always sync immediately and synchronously.
This is the slowest option that provides the greatest protection against data
loss in the event of a crash.

Note that this is still not a 100% guarantee that data will not be lost or
corrupted during a crash. The underlying filesystem itself may cause data to
be lost or corrupt in such a situation. And OpenAFS itself does not (yet) even
guarantee that all data is consistent at any point in time; so even if the
filesystem and OS do not buffer or reorder any writes, you are not guaranteed
that all data will be okay after a crash.

This was the only behavior allowed in OpenAFS releases prior to 1.4.5.

=item onclose

This causes a sync to do nothing immediately, but causes the relevant file to
be flagged as potentially needing a sync. When a volume is detached, volume
metadata files flaged for synced are synced, as well as data files that have
been accessed recently. Events that cause a volume to detach include:
performing volume operations (dump, restore, clone, etc), a clean shutdown
of the fileserver, or during DAFS "soft detachment".

Effectively this option is the same as C<never> while a volume is attached and
actively being used, but if a volume is detached, there is an additional
guarantee for the data's consistency.

After the removal of the C<delayed> option after the OpenAFS 1.6 series, this
option became the default.

=item never

This causes all syncs to never do anything. This is the fastest option, with
the weakest guarantees for data consistency.

Depending on the underlying filesystem and Operating System, there may be
guarantees that any data written to disk will hit the physical media after a
certain amount of time. For example, Linux's pdflush process usually makes this
guarantee, and ext3 can make certain various consistency guarantees according
to the options given. ZFS on Solaris can also provide similar guarantees, as
can various other platforms and filesystems. Consult the documentation for
your platform if you are unsure.

=item delayed

This option used to exist in OpenAFS 1.6, but was later removed due to issues
encountered with data corruption during normal operation. Outside of the
OpenAFS 1.6 series, it is not a valid option, and the fileserver will fail to
start if you specify this (or any other unknown option). It caused syncs to
occur in a background thread, executing every 10 seconds.

This was the only behavior allowed in OpenAFS releases starting from 1.4.5 up
to and including 1.6.2. It was also the default for the 1.6 series starting in
OpenAFS 1.6.3.

=back

Which option you choose is not an easy decision to make. Various developers
and experts sometimes disagree on which option is the most reasonable, and it
may depend on the specific scenario and workload involved. Some argue that
the C<always> option does not provide significantly greater guarantees over
any other option, whereas others argue that choosing anything besides the
C<always> option allows for an unacceptable risk of data loss. This may
depend on your usage patterns, your platform and filesystem, and who you talk
to about this topic.

=item B<-logfile> <I<log file>>

Sets the file to use for server logging.  If logfile is not specified and
no other logging options are supplied, this will be F</usr/afs/logs/FileLog>.
Note that this option is intended for debugging and testing purposes.
Changing the location of the log file from the command line may result
in undesirable interactions with tools such as B<bos>.

=item B<-vhashsize <I<size>>

The log(2) of the number of volume hash buckets.  Default is 8 (i.e., by
default, there are 2^8 = 256 volume hash buckets). The minimum that can
be specified is 6 (64 hash buckets). In OpenAFS 1.5.77 and earlier, the
maximum that can be specified is 14 (16384 buckets). After 1.5.77, the
maximum that can be specified is 28 (268435456 buckets).

=item B<-config> <I<configuration directory>>

Set the location of the configuration directory used to configure this
service.  In a typical configuration this will be F</usr/afs/etc> - this
option allows the use of alternative configuration locations for testing
purposes.
