package osi::trace::console::agent;

use 5.008008;
use strict;
use warnings;

require Exporter;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use osi::trace::console ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
				   new,
				   DESTROY,
				   addr_set,
				   addr_get,
				   port_set,
				   port_get,
				   transport_set,
				   transport_get
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);


# these fields are given values by the XS boot logic
our $TRANSPORT_RX_UDP   = undef;
our $TRANSPORT_RX_TCP   = undef;
our $TRANSPORT_SNMP_V1  = undef;


our $VERSION = '0.01';

require XSLoader;
XSLoader::load('osi::trace::console::agent', $VERSION);


# Preloaded methods go here.

1;
__END__

=head1 NAME

osi::trace::console::agent - Perl extension for agent address handles

=head1 SYNOPSIS

  use osi::trace::console;

  $handle = new osi::trace::console::agent;

=head1 DESCRIPTION

  osi trace console library agent address handle perl API

=head2 EXPORT

  new
  DESTROY
  addr_set
  addr_get
  port_set
  port_get
  transport_set
  transport_get

=head1 SEE ALSO

For further information regarding OpenAFS, the osi and osi_trace libraries, or
this perl interface, please see http://www.openafs.org

Discussion related to the use of this perl interface should be directed to 
the following mailing list: E<lt>openafs-info@openafs.orgE<gt>

=head1 AUTHOR

Tom Keiser, E<lt>tkeiser@sinenomine.netE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006 Sine Nomine Associates and others.
All Rights Reserved.

This software has been released under the terms of the IBM Public
License.  For details, see the LICENSE file in the top-level source
directory or online at http://www.openafs.org/dl/license10.html

=cut

sub new {
  my ($class) = @_;

  my $self = { };
  bless $self, $class;
  return $self;
}

sub DESTROY {
}

sub addr_set {
  my ($self, $addr) = @_;

  $self->{__addr} = $addr;
}

sub port_set {
  my ($self, $port) = @_;

  $self->{__port} = $port;
}

sub transport_set {
  my ($self, $transport) = @_;

  $self->{__transport} = $transport;
}

sub addr_get {
  my ($self) = @_;

  return $self->{__addr};
}

sub port_get {
  my ($self, $port) = @_;

  return $self->{__port};
}

sub transport_get {
  my ($self, $transport) = @_;

  return $self->{__transport};
}
