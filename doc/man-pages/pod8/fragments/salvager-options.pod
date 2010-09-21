=over 4

=item [I<initcmd>]

Accommodates the command's use of the AFS command parser, and is optional.

=item B<-partition> <I<name of partition to salvage>>

Specifies the name of the partition to salvage. Specify the full partition
name using the form F</vicepI<x>> or F</vicepI<xx>>. Omit this argument to
salvage every partition on the file server machine.

=item B<-volumeid> <I<volume id to salvage>>

Specifies the volume ID of a specific read/write volume to salvage.  The
B<-partition> argument must be provided along with this one and specify
the volume's actual site.

=item B<-debug>

Allows only one Salvager subprocess to run at a time, regardless of the
setting of the B<-parallel> option. Include it when running the Salvager
in a debugger to make the trace easier to interpret.

=item B<-nowrite>

Brings all undamaged volumes online without attempting to salvage any
damaged volumes.

=item B<-inodes>

Records in the F</usr/afs/logs/SalvageLog> file a list of all AFS inodes
that the Salvager modified.

=item B<-force>

Inspects all volumes for corruption, not just those that are marked as
having been active when a crash occurred.

=item B<-oktozap>

Removes a volume that is so damaged that even issuing the B<vos zap>
command with the B<-force> flag is ineffective. Combine it with the
B<-partition> and B<-volumeid> arguments to identify the volume to remove.
Using this flag will destroy data that cannot be read, so use only with
caution and when you're certain that nothing in that volume is still
needed.

=item B<-rootinodes>

Records in the F</usr/afs/logs/SalvageLog> file a list of all AFS inodes
owned by the local superuser C<root>.

=item B<-salvagedirs>

Salvages entire directory structures, even if they do not appear to be
damaged. By default, the Salvager salvages a directory only if it is
flagged as corrupted.

=item B<-blockreads>

Forces the Salvager to read a partition one disk block (512 bytes) at a
time and to skip any blocks that are too badly damaged to be salvaged.
This allows it to salvage as many volumes as possible. By default, the
Salvager reads large disk blocks, which can cause it to exit prematurely
if it encounters disk errors. Use this flag if the partition to be
salvaged has disk errors.

=item B<-parallel> <I<# of max parallel partition salvaging>>

Specifies the maximum number of Salvager subprocesses to run in parallel.
Provide one of three values:

=over 4

=item *

An integer from the range C<1> to C<32>. A value of C<1> means that a
single Salvager process salvages the partitions sequentially.

=item *

The string C<all> to run up to four Salvager subprocesses in parallel on
partitions formatted as logical volumes that span multiple physical
disks. Use this value only with such logical volumes.

=item *

The string C<all> followed immediately (with no intervening space) by an
integer from the range C<1> to C<32>, to run the specified number of
Salvager subprocesses in parallel on partitions formatted as logical
volumes. Use this value only with such logical volumes.

=back

The BOS Server never starts more Salvager subprocesses than there are
partitions, and always starts only one process to salvage a single
volume. If this argument is omitted, up to four Salvager subprocesses run
in parallel but partitions on the same device are salvaged serially.

=item B<-tmpdir> <I<name of dir to place tmp files>>

Names a local disk directory in which the Salvager places the temporary
files it creates during a salvage operation, instead of writing them to
the partition being salvaged (the default). If the Salvager cannot write
to the specified directory, it attempts to write to the partition being
salvaged.

=item B<-showlog>

Displays on the standard output stream all log data that is being written
to the F</usr/afs/logs/SalvageLog> file.

=item B<-showsuid>

Displays a list of the pathnames for all files that have the setuid or
setgid mode bit set.

=item B<-showmounts>

Records in the F</usr/afs/logs/SalvageLog> file all mount points found in
each volume. The Salvager does not repair corruption in the volumes, if
any exists.

=item B<-orphans> (ignore | remove | attach)

Controls how the Salvager handles orphaned files and directories.  Choose
one of the following three values:

=over 4

=item ignore

Leaves the orphaned objects on the disk, but prints a message to the
F</usr/afs/logs/SalvageLog> file reporting how many orphans were found and
the approximate number of kilobytes they are consuming. This is the
default if the B<-orphans> argument is omitted.

=item remove

Removes the orphaned objects, and prints a message to the
F</usr/afs/logs/SalvageLog> file reporting how many orphans were removed
and the approximate number of kilobytes they were consuming.

=item attach

Attaches the orphaned objects by creating a reference to them in the vnode
of the volume's root directory. Since each object's actual name is now
lost, the Salvager assigns each one a name of the following form:

=over 4

=item C<__ORPHANFILE__.I<index>> for files.

=item C<__ORPHANDIR__.I<index>> for directories.

=back

where I<index> is a two-digit number that uniquely identifies each
object. The orphans are charged against the volume's quota and appear in
the output of the B<ls> command issued against the volume's root
directory.

=back

=item B<-help>

Prints the online help for this command. All other valid options are
ignored.

=back
