=item B<-cell> <I<cell name>>

Names the cell in which to run the command. Do not combine this argument
with the B<-localauth> flag. For more details, see L<vos(1)>.

=item B<-noauth>

Assigns the unprivileged identity C<anonymous> to the issuer. Do not
combine this flag with the B<-localauth> flag. For more details, see
L<vos(1)>.

=item B<-localauth>

Constructs a server ticket using a key from the local
F</usr/afs/etc/KeyFile> file. The B<vos> command interpreter presents it
to the Volume Server and Volume Location Server during mutual
authentication. Do not combine this flag with the B<-cell> argument or
B<-noauth> flag. For more details, see L<vos(1)>.

=item B<-verbose>

Produces on the standard output stream a detailed trace of the command's
execution. If this argument is omitted, only warnings and error messages
appear.

=item B<-encrypt>

Encrypts the command so that the operation's results are not transmitted
across the network in clear text. This option is available in OpenAFS
versions 1.4.11 or later and 1.5.60 or later.

=item B<-noresolve>

Shows all servers as IP addresses instead of the DNS name. This is very
useful when the server address is registered as 127.0.0.1 or when dealing
with multi-homed servers. This option is available in OpenAFS
versions 1.4.8 or later and 1.5.35 or later.

=item B<-config> <I<configuration directory>>

Set the location of the configuration directory to be used. This defaults to
F</usr/vice/etc>, except if B<-localauth> is specified, in which case the
default is F</usr/afs/etc>. This option allows the use of alternative
configuration locations for testing purposes.

=item B<-help>

Prints the online help for this command. All other valid options are
ignored.
