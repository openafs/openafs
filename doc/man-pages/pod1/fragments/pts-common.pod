=item B<-auth>

Use the calling user's tokens to communicate with the Protection Server. For
more details, see L<pts(1)>.

=item B<-cell> <I<cell name>>

Names the cell in which to run the command. For more details, see
L<pts(1)>.

=item B<-config> <I<config directory>>

Use an alternate config directory. For more details, see L<pts(1)>.

=item B<-encrypt>

Encrypts any communication with the Protection Server. For more details, see
L<pts(1)>.

=item B<-force>

Enables the command to continue executing as far as possible when errors
or other problems occur, rather than halting execution at the first error.

=item B<-help>

Prints the online help for this command. All other valid options are
ignored.

=item B<-localauth>

Constructs a server ticket using a key from the local
F</usr/afs/etc/KeyFile> file. Do not combine this flag with the B<-cell> 
or B<-noauth> options. For more details, see L<pts(1)>.

=item B<-noauth>

Assigns the unprivileged identity anonymous to the issuer. For more
details, see L<pts(1)>.
