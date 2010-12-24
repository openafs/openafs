The Salvager restores internal consistency to corrupted read/write volumes
on the local file server machine where possible. For read-only or backup
volumes, it inspects only the volume header:

=over 4

=item *

If the volume header is corrupted, the Salvager removes the volume
completely and records the removal in its log file,
F</usr/afs/logs/SalvageLog>. Issue the B<vos release> or B<vos backup>
command to create the read-only or backup volume again.

=item *

If the volume header is intact, the Salvager skips the volume (does not
check for corruption in the contents). However, if the File Server notices
corruption as it initializes, it sometimes refuses to attach the volume or
bring it online. In this case, it is simplest to remove the volume by
issuing the B<vos remove> or B<vos zap> command. Then issue the B<vos
release> or B<vos backup> command to create it again.

=back

Unlike other server process initialization commands, the Salvager
command is designed to be issued at the command shell prompt, as well as
being placed into a file server machine's F</usr/afs/local/BosConfig> file
with the B<bos create> command. It is also possible to invoke the Salvager
remotely by issuing the B<bos salvage> command.

Combine the command's options as indicated to salvage different numbers of
read/write volumes:

=over 4

=item *

To salvage all volumes on the file server machine, provide no arguments.
No volumes on the machine are accessible to Cache Managers during the
salvage, because the BOS Server stops the File Server and Volume Server
processes while the Salvager runs.

=item *

To salvage all of the volumes on one partition, provide the B<-partition>
argument. As for a salvage of all volumes on the machine, no volumes on
the machine are accessible to Cache Managers during the salvage operation.

=item *

To salvage only one volume, combine the B<-partition> and B<-volumeid>
arguments. Only that volume is inaccessible to Cache Managers, because the
BOS Server does not shutdown the File Server and Volume Server processes.

=back

The Salvager normally salvages only those read/write volumes that are
marked as having been active when a crash occurred. To have it salvage all
relevant read/write volumes, add the B<-force> flag.

The Salvager normally creates new inodes as it repairs damage. If the
partition is so full that there is no room for new inodes, use the
B<-nowrite> argument to bringing undamaged volumes online without
attempting to salvage damaged volumes. Then use the B<vos move> command to
move one or more of the undamaged volumes to other partitions, freeing up
the space that the Salvager needs to create new inodes.

By default, multiple Salvager subprocesses run in parallel: one for each
partition up to four, and four subprocesses for four or more
partitions. To increase or decrease the number of subprocesses running in
parallel, provide a positive integer value for the B<-parallel> argument.

If there is more than one server partition on a physical disk, the
Salvager by default salvages them serially to avoid the inefficiency of
constantly moving the disk head from one partition to another. However,
this strategy is often not ideal if the partitions are configured as
logical volumes that span multiple disks. To force the Salvager to salvage
logical volumes in parallel as if they were on separate disks, provide the
string C<all> as the value for the B<-parallel> argument.

To set both parameters at the same time, append the number of Salvager
processes to the string C<all>. For example, C<-parallel all5> treats
each partition as a separate disk and runs five Salvager processes, thus
salvaging five partitions at a time.

The Salvager creates temporary files as it runs, by default writing them
to the partition it is salvaging. The number of files can be quite large,
and if the partition is too full to accommodate them, the Salvager
terminates without completing the salvage operation (it always removes the
temporary files before exiting). Other Salvager subprocesses running at
the same time continue until they finish salvaging all other partitions
where there is enough disk space for temporary files. To complete the
interrupted salvage, reissue the command against the appropriate
partitions, adding the B<-tmpdir> argument to redirect the temporary files
to a local disk directory that has enough space.

The B<-orphans> argument controls how the Salvager handles orphaned files
and directories that it finds on server partitions it is salvaging. An
I<orphaned> element is completely inaccessible because it is not
referenced by the vnode of any directory that can act as its parent (is
higher in the filespace). Orphaned objects occupy space on the server
partition, but do not count against the volume's quota.

To generate a list of all mount points that reside in one or more volumes,
rather than actually salvaging them, include the B<-showmounts> flag.

This command does not use the syntax conventions of the AFS command
suites. Provide the command name and all option names in full.

