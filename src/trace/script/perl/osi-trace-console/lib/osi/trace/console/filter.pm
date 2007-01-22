package osi::trace::console::filter;

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
				   filter_set,
				   filter_get
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('osi::trace::console::filter', $VERSION);


# Preloaded methods go here.

1;
__END__

=head1 NAME

osi::trace::console::filter - Perl extension for probe name filters

=head1 SYNOPSIS

  use osi::trace::console;

  $handle = new osi::trace::console::filter;

=head1 DESCRIPTION

  osi trace console library probe name filter perl API

=head2 EXPORT

  new
  DESTROY
  filter_set
  filter_get

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

sub filter_set {
  my ($self, $filter) = @_;

  $self->{__filter} = $filter;
}

sub filter_get {
  my ($self) = @_;

  return $self->{__filter};
}

