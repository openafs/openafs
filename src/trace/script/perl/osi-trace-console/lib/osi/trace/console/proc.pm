package osi::trace::console::proc;

use 5.006;
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
				   proc_set,
				   proc_get,
				   pid,
				   program_type,
				   trace_interface_version,
				   capabilities,
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);


# these fields are given values by the XS boot logic
our $ProgramType_Library               = undef;
our $ProgramType_BosServer             = undef;
our $ProgramType_CacheManager          = undef;
our $ProgramType_FileServer            = undef;
our $ProgramType_VolServer             = undef;
our $ProgramType_Salvager              = undef;
our $ProgramType_SalvageServer         = undef;
our $ProgramType_SalvageServerWorker   = undef;
our $ProgramType_PtServer              = undef;
our $ProgramType_VLServer              = undef;
our $ProgramType_KAServer              = undef;
our $ProgramType_BuServer              = undef;
our $ProgramType_Utility               = undef;
our $ProgramType_EphemeralUtility      = undef;
our $ProgramType_TraceCollector        = undef;
our $ProgramType_TestSuite             = undef;
our $ProgramType_TraceKernel           = undef;
our $ProgramType_Backup                = undef;
our $ProgramType_BuTC                  = undef;
our $ProgramType_UpServer              = undef;
our $ProgramType_UpClient              = undef;
our $ProgramType_Bos                   = undef;
our $ProgramType_Vos                   = undef;
our $ProgramType_AFSD                  = undef;
our $ProgramType_RMTSYSD               = undef;


our $VERSION = '0.01';

require XSLoader;
XSLoader::load('osi::trace::console::proc', $VERSION);


# Preloaded methods go here.

1;
__END__

=head1 NAME

osi::trace::console::proc - Perl extension for proc address handles

=head1 SYNOPSIS

  use osi::trace::console;

  $handle = new osi::trace::console::proc;

=head1 DESCRIPTION

  osi trace console library proc address handle perl API

=head2 EXPORT

  new
  DESTROY
  proc_set
  proc_get

=head1 SEE ALSO

For further information regarding OpenAFS, the osi and osi_trace libraries, or
this perl interface, please see http://www.openafs.org

Discussion related to the use of this perl interface should be directed to 
the following mailing list: E<lt>openafs-info@openafs.orgE<gt>

=head1 AUTHOR

Tom Keiser, E<lt>tkeiser@sinenomine.netE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2007 Sine Nomine Associates and others.
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

sub proc_set {
  my ($self, $proc) = @_;

  $self->{__gen_id} = $proc;
}

sub proc_get {
  my ($self, $proc) = @_;

  return $self->{__gen_id};
}

sub pid {
  my ($self) = @_;

  return $self->{__i_pid};
}

sub program_type {
  my ($self) = @_;

  return $self->{__i_ptype};
}

sub trace_interface_version {
  my ($self) = @_;

  return $self->{__i_version};
}

sub capabilities {
  my ($self) = @_;

  return $self->{__i_caps};
}
