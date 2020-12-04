=over 4

=item B<-d> <I<debug level>>

Sets the detail level for the debugging trace written to the
F</usr/afs/logs/VolserLog> file. Provide one of the following values, each
of which produces an increasingly detailed trace: C<0>, C<1>, C<5>, C<25>,
and C<125>. 

=item B<-log>

Records in the /usr/afs/logs/VolserLog file the names of all users who
successfully initiate a B<vos> command. The Volume Server also records any
file removals that result from issuing the B<vos release> command with the
B<-f> flag.

=item B<-transarc-logs>

Use Transarc style logging features. Rename the log file
F</usr/afs/logs/VolserLog> to F</usr/afs/logs/VolserLog.old> when the volume server is
restarted.  This option is provided for compatibility with older versions.

=item B<-p> <I<number of processes>>

Sets the number of server lightweight processes (LWPs) to run.  Provide an
integer between C<4> and C<16>. The default is C<9>.

=item B<-auditlog> [<I<interface>>:]<I<path>>[:<I<options>>]

Turns on audit logging, and sets the path for the audit log.  The audit
log records information about RPC calls, including the name of the RPC
call, the host that submitted the call, the authenticated entity (user)
that issued the call, the parameters for the call, and if the call
succeeded or failed.  See L<fileserver(8)> for an explanation of the audit
facility.

=item B<-audit-interface> <I<default interface>>

Sets the default audit interface used by the B<-auditlog> option.  The
initial default is the C<file> interface. See L<fileserver(8)> for
an explanation of each interface.

=item B<-udpsize> <I<size of socket buffer>>

Sets the size of the UDP buffer in bytes, which is 64 KB by
default. Provide a positive integer, preferably larger than the default.

=item B<-jumbo>

Allows the server to send and receive jumbograms. A jumbogram is
a large-size packet composed of 2 to 4 normal Rx data packets that share
the same header. The volserver does not use jumbograms by default, as some
routers are not capable of properly breaking the jumbogram into smaller
packets and reassembling them.

=item B<-nojumbo>

Deprecated; jumbograms are disabled by default.

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

=item B<-allow-dotted-principals>

By default, the RXKAD security layer will disallow access by Kerberos
principals with a dot in the first component of their name. This is to avoid
the confusion where principals user/admin and user.admin are both mapped to the
user.admin PTS entry. Sites whose Kerberos realms don't have these collisions 
between principal names may disable this check by starting the server
with this option.

=item B<-clear-vol-stats>

Clear volume access statistics during volume restore and reclone operations.
This includes clearing the volume access statistics of read-only volumes during
a volume release.  By default, volume access statistics are preserved during
volume restore and reclone operations.

Volume access statistics were cleared by default in versions prior to OpenAFS
1.8.0.  This flag is intended to provide compatible behaviour.

=item B<-sync> <I<sync behavior>>

This is the same as the B<-sync> option in L<fileserver(8)>. See
L<fileserver(8)>.

=item B<-logfile> <I<log file>>

Sets the file to use for server logging.  If logfile is not specified and
no other logging options are supplied, this will be F</usr/afs/logs/VolserLog>.
Note that this option is intended for debugging and testing purposes.
Changing the location of the log file from the command line may result
in undesirable interactions with tools such as B<bos>.

=item B<-config> <I<configuration directory>>

Set the location of the configuration directory used to configure this
service.  In a typical configuration this will be F</usr/afs/etc> - this
option allows the use of alternative configuration locations for testing
purposes.

=item B<-rxmaxmtu> <I<bytes>>

Defines the maximum size of an MTU.  The value must be between the
minimum and maximum packet data sizes for Rx.

=item B<-rxbind>

Bind the Rx socket to the primary interface only. (If not specified, the Rx
socket will listen on all interfaces.)

=item B<-syslog>[=<I<syslog facility>>]

Specifies that logging output should go to syslog instead of the normal
log file. B<-syslog>=I<FACILITY> can be used to specify to which facility
the log message should be sent.

=item B<-sleep> <I<sleep_time>/I<run_time>>

This option is obsolete, and is now only accepted for compatibility with older
releases. All it does now is log a warning message about how the option is
obsolete.

=item B<-restricted_query> (anyuser | admin)

Restrict RPCs that query information about volumes to a specific group
of users. You can use C<admin> to restrict to AFS administrators.  The
C<anyuser> option doesn't restrict the RPCs and leaves it open for all
users including unauthenticated users, this is the default.

=item B<-s2scrypt> (never | always | inherit)

Set the cryptographic disposition of inter-volserver traffic.

=over 4

=item B<never>

All inter-volserver traffic is unencrypted.  This is the default behavior.

=item B<always>

All inter-volserver traffic is encrypted (using rxkad).

=item B<inherit>

Inter-volserver traffic will be encrypted if the client connection triggering
the server-to-server traffic is encrypted.  This has the effect of encrypting
inter-server traffic if the "-encrypt" option is provided to
L<B<vos release>|vos_release(1)>, for example.

=back

=item B<-help>

Prints the online help for this command. All other valid options are
ignored.

=back
