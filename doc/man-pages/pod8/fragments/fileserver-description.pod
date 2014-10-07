The File Server creates the F</usr/afs/logs/FileLog> log file as it
initializes, if the file does not already exist. It does not write a
detailed trace by default, but the B<-d> option may be used to
increase the amount of detail. Use the B<bos getlog> command to
display the contents of the log file.

The command's arguments enable the administrator to control many aspects
of the File Server's performance, as detailed in L</OPTIONS>.  By default
the File Server sets values for many arguments that are suitable
for a medium-sized file server machine. To set values suitable for a small
or large file server machine, use the B<-S> or B<-L> flag
respectively. The following list describes the parameters and
corresponding argument for which the File Server sets default
values, and the table below summarizes the setting for each of the three
machine sizes.

=over 4

=item *

The maximum number of threads
the File Server uses to handle requests for data; corresponds to the 
B<-p> argument. The File Server always uses a minimum of 32 KB of 
memory for these processes.

=item *

The maximum number of directory blocks the File Server caches in memory;
corresponds to the B<-b> argument. Each cached directory block (buffer)
consumes 2,092 bytes of memory.

=item *

The maximum number of large vnodes the File Server caches in memory for
tracking directory elements; corresponds to the B<-l> argument. Each large
vnode consumes 292 bytes of memory.

=item *

The maximum number of small vnodes the File Server caches in memory for
tracking file elements; corresponds to the B<-s> argument.  Each small
vnode consumes 100 bytes of memory.

=item *

The maximum volume cache size, which determines how many volumes the File
Server can cache in memory before having to retrieve data from disk;
corresponds to the B<-vc> argument.

=item *

The maximum number of callback structures the File Server caches in
memory; corresponds to the B<-cb> argument. Each callback structure
consumes 16 bytes of memory.

=item *

The maximum number of Rx packets the File Server uses; corresponds to the
B<-rxpck> argument. Each packet consumes 1544 bytes of memory.

=back

The default values are:

  Parameter (Argument)               Small (-S)     Medium   Large (-L)
  ---------------------------------------------------------------------
  Number of threads (-p)                     6           9          128
  Number of cached dir blocks (-b)          70          90          120
  Number of cached large vnodes (-l)       200         400          600
  Number of cached small vnodes (-s)       200         400          600
  Maximum volume cache size (-vc)          200         400          600
  Number of callbacks (-cb)             20,000      60,000       64,000
  Number of Rx packets (-rxpck)            100         150          200

To override any of the values, provide the indicated argument (which can
be combined with the B<-S> or B<-L> flag).

The amount of memory required for the File Server varies. The approximate
default memory usage is 751 KB when the B<-S> flag is used (small
configuration), 1.1 MB when all defaults are used (medium configuration),
and 1.4 MB when the B<-L> flag is used (large configuration). If
additional memory is available, increasing the value of the B<-cb> and
B<-vc> arguments can improve File Server performance most directly.

By default, the File Server allows a volume to exceed its quota by 1 MB
when an application is writing data to an existing file in a volume that
is full. The File Server still does not allow users to create new files in
a full volume. To change the default, use one of the following arguments:

=over 4

=item *

Set the B<-spare> argument to the number of extra kilobytes that the File
Server allows as overage. A value of C<0> allows no overage.

=item *

Set the B<-pctspare> argument to the percentage of the volume's quota the
File Server allows as overage.

=back

By default, the File Server implicitly grants the C<a> (administer) and
C<l> (lookup) permissions to system:administrators on the access control
list (ACL) of every directory in the volumes stored on its file server
machine. In other words, the group's members can exercise those two
permissions even when an entry for the group does not appear on an ACL. To
change the set of default permissions, use the B<-implicit> argument.

The File Server maintains a I<host current protection subgroup> (I<host
CPS>) for each client machine from which it has received a data access
request. Like the CPS for a user, a host CPS lists all of the Protection
Database groups to which the machine belongs, and the File Server compares
the host CPS to a directory's ACL to determine in what manner users on the
machine are authorized to access the directory's contents. When the B<pts
adduser> or B<pts removeuser> command is used to change the groups to
which a machine belongs, the File Server must recompute the machine's host
CPS in order to notice the change. By default, the File Server contacts
the Protection Server every two hours to recompute host CPSs, implying
that it can take that long for changed group memberships to become
effective. To change this frequency, use the B<-hr> argument.

The File Server stores volumes in partitions. A partition is a
filesystem or directory on the server machine that is named C</vicepX>
or C</vicepXX> where XX is "a" through "z" or "aa" though "iv". Up to
255 partitions are allowed. The File Server expects that the /vicepXX
directories are each on a dedicated filesystem. The File Server will
only use a /vicepXX if it's a mountpoint for another filesystem,
unless the file C</vicepXX/AlwaysAttach> exists.  A partition will not be
mounted if the file C</vicepXX/NeverAttach> exists. If both
C</vicepXX/AlwaysAttach> and C</vicepXX/NeverAttach> are present, then
C</vicepXX/AlwaysAttach> wins.  The data in the partition is a special
format that can only be access using OpenAFS commands or an OpenAFS
client.

The File Server generates the following message when a partition is nearly
full:

   No space left on device

This command does not use the syntax conventions of the AFS command
suites. Provide the command name and all option names in full.
