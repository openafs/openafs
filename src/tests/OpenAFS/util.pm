# CMUCS AFStools
# Copyright (c) 1996, Carnegie Mellon University
# All rights reserved.
#
# See CMUCS/CMU_copyright.ph for use and distribution information

package OpenAFS::util;

=head1 NAME

OpenAFS::util - General AFS utilities

=head1 SYNOPSIS

  use OpenAFS::util;

  AFS_Init();
  AFS_Trace($subject, $level);
  AFS_SetParm($parm, $value);

  use OpenAFS::util qw(GetOpts_AFS);
  %options = GetOpts_AFS(\@argv, \@optlist);

=head1 DESCRIPTION

This module defines a variety of AFS-related utility functions.  Virtually
every application that uses AFStools will need to use some of the utilities
defined in this module.  In addition, a variety of global variables are
defined here for use by all the AFStools modules.  Most of these are
private, but a few are semi-public.

=cut

use OpenAFS::CMU_copyright;
use OpenAFS::config;
require OpenAFS::afsconf;   ## Avoid circular 'use' dependencies
use Exporter;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&AFS_Init
		&AFS_Trace
		&AFS_SetParm);
@EXPORT_OK = qw(%AFS_Parms
                %AFS_Trace
		%AFS_Help
                %AFScmd
		&GetOpts_AFS
		&GetOpts_AFS_Help);
%EXPORT_TAGS = (afs_internal => [qw(%AFS_Parms %AFS_Trace %AFScmd %AFS_Help)],
                afs_getopts  => [qw(&GetOpts_AFS &GetOpts_AFS_Help)] );


=head2 AFS_Init()

This function does basic initialization of AFStools.  It must be called before
any other AFStools function.

=cut

sub AFS_Init
{
  my(@dirs, $c, $i, $x);

  $AFS_Parms{'authlvl'}  = 1;
  $AFS_Parms{'confdir'}  = $def_ConfDir;
  $AFS_Parms{'cell'}     = OpenAFS::afsconf::AFS_conf_localcell();

  # Search for AFS commands
  @dirs = @CmdPath;
  foreach $c (@CmdList)
    {
      $AFScmd{$c} = '';
      foreach $i ($[ .. $#dirs)
	{
          $x = $dirs[$i];
	  if (-x "$x/$c" && ! -d "$x/$c")
	    {
	      $AFScmd{$c} = "$x/$c";
              splice(@dirs, $i, 1);   # Move this item to the start of the array
	      unshift(@dirs, $x);
	      last;
	    }
	}
      return "Unable to locate $c!" if (!$AFScmd{$c});
    }
  0;
}


=head2 AFS_Trace($subject, $level)

Sets the tracing level for a particular "subject" to the specified level.
All tracing levels start at 0, and can be set to higher values to get debugging
information from different parts of AFStools.  This function is generally
only of use to people debugging or extending AFStools.

=cut

$AFS_Help{Trace} = '$subject, $level => void';
sub AFS_Trace {
  my($subject, $level) = @_;

  $AFS_Trace{$subject} = $level;
}


=head2 AFS_SetParm($parm, $value)

Sets the AFStools parameter I<$parm> to I<$value>.  AFStools parameters are
used to alter the behaviour of various parts of the system.  The following
parameters are currently defined:

=over 10

=item authlvl

The authentication level to use for commands that talk directly to AFS
servers (bos, vos, pts, etc.).  Set to 0 for unauthenticated access (-noauth),
1 to use the user's existing tokens, or 2 to use the AFS service key
(-localauth).

=item cell

The default AFS cell in which to work.  This is initially the workstation's
local cell.

=item confdir

The AFS configuration directory to use.  If none is specified, the default
(as defined in OpenAFS::config) will be used.

=item vostrace

Set the tracing level used by various B<vos> utilities.  The default is 0,
which disables any tracing of activity of B<vos> commands.  A setting of 1
copies output from all commands except those which are invoked solely to
get information; a setting of 2 additionally uses the "-verbose" command
on any command whose output is copied.  If a setting of 3 is used, all
B<vos> commands will be invoked with "-verbose", and have their output
copied to stdout.

=back

=cut

$AFS_Help{SetParm} = '$parm, $value => void';
sub AFS_SetParm {
  my($parm, $value) = @_;

  $AFS_Parms{$parm} = $value;
}


#: GetOpts_AFS(\@argv, \@optlist)
#: Parse AFS-style options.
#: \@argv is a hard reference to the list of arguments to be parsed.
#: \@optlist is a hard reference to the list of option specifications for valid
#: options; in their default order.  Each option specification, in turn, is a
#: hard reference to an associative array containing some of the following
#: elements:
#:     name       => The name of the argument
#:     numargs    => Number of arguments (0, 1, or -1 for multiple)
#:     required   => If nonzero, this argument is required
#:     default    => Value to give this option if not specified
#:     noauto     => Don't use this option for unadorned arguments
#:
#: Results are returned in the form of an associative array of options and
#: their values:
#: - Boolean (0-argument) options have a value of 1 if specified.  This type
#:   of option may not be marked 'required'.
#: - Simple (1-argument) options have a value which is the string given by the
#:   user.
#: - Multiple-argument options have a value which is a hard reference to an
#:   array of values given by the user.
#:
#: Argument parsing is done in a similar manner to the argument parser used by
#: various AFS utilities.  Options have multi-character names, and may not be
#: combined with their arguments or other options.  Those options which take
#: arguments use up at least the next argument, regardless of whether it begins
#: with a dash.  Options which can take multiple arguments will eat at least
#: one argument, as well as any following argument up to the next option (i.e.,
#: the next argument beginning with a dash).  An "unadorned" argument will be
#: used by the next argument-taking option.  If there are multiple unadorned
#: arguments, they will be used up by successive arguments much in the same
#: way Perl handles list assignment - each one-argument (scalar) option will
#: use one argument; the first multi-argument (list) option will use up any
#: remaining unadorned arguments.
#:
#: On completion, @argv will be left with any unparsed arguments (this can
#: happen if the last option specified is _not_ a multi-argument option, and
#: there are no "defaulted" options).  This is considered to be an error
#: condition.
#:
sub GetOpts_AFS_Help {
  my($cmd, $optlist) = @_;
  my($option, $optname, $desc);

  foreach $option (@$optlist) {
    $optname = '-' . $$option{name};
    if ($$option{numargs}) {
      $desc = $$option{desc} ? $$option{desc} : $$option{name};
      $desc = " <$desc>";
      $desc .= '+' if ($$option{numargs} < 0);
      $optname .= $desc;
    }
    $optname = "[$optname]" if (!$$option{required});
    $cmd .= " $optname";
  }
  $cmd;
}

sub _which_opt {
  my($optname, @options) = @_;
  my($o, $which, $n);

  foreach $o (@options) {
    next unless ($o =~ /^$optname/);
    $n++;
    $which = $o;
  }
  ($n == 1) ? $which : $optname;
}

sub GetOpts_AFS {
  my($argv, $optlist) = @_;
  my(@autolist, %opttbl, %result);
  my($stop, $key, $value, $diemsg);

  # Initialization:
  @autolist = map {
    if ($_->{numargs} && !$_->{noauto} && !$stop) {
      $stop = 1 if ($_->{numargs} < 0);
      ($_->{name});
    } else {
      ();
    }
  } (@$optlist, { name=>'-help', numargs=>0, required=>0 } );
  %opttbl = map { $_->{name} => $_ } @$optlist;

  while (@$argv) {
    my($optname, $optkind);

    # Parse the next argument.  It can either be an option, or an
    # unadorned argument.  If the former, shift it off and process it.
    # Otherwise, grab the next "automatic" option.  If there are no
    # more automatic options, we have extra arguments and should return.
    if ($argv->[0] =~ /^-(.+)/) {  # Got an option!
      $optname = $1;
      shift(@$argv);
    } else {                       # An unadorned argument
      if (@autolist) {
	$optname = shift(@autolist);
      } else {
	$diemsg = join(' ', "Extra arguments:", @$argv) unless ($diemsg);
        shift @$argv;
        next;
      }
    }
    $optname = &_which_opt($optname, keys %opttbl);

    # Find out how many arguments this thing wants, then remove it from
    # the option table and automatic option list.
    $optkind = $opttbl{$optname}->{numargs};
    delete $opttbl{$optname};
    @autolist = grep($_ ne $optname, @autolist);

    # Parse arguments (if any), and set the result value
    if (!$optkind) {               # Boolean!
      $result{$optname} = 1;
    } elsif ($optkind == 1) {      # Single argument
      # Shift off a single argument, or signal an error
      if (!@$argv) {
        $diemsg = "No argument for -$optname" unless ($diemsg);
        next;
      }
      $result{$optname} = shift(@$argv);
    } elsif ($optkind < 0) {       # Multiple arguments
      # Shift off at least one argument, and any additional
      # ones that are present.  EXCEPT, if there are no more
      # explicitly-specified options but there ARE automatic
      # options left in our list, then only eat up one.
      my($val, @val);
      if (!@$argv) {
	$diemsg = "No argument for -$optname" unless ($diemsg);
        next;
      }
      $val = shift(@$argv);
      push(@val, shift @$argv) while (@$argv && $argv->[0] !~ /^-/);
      if (@autolist && !@$argv) {
	unshift(@$argv, @val);
	@val = ($val);
      } else {
	unshift(@val, $val);
      }
      $result{$optname} = [@val];
    } else {
      die "Invalid argument spec for -$optname ($optkind)\n";
    }
  }

  # Now for a little clean-up
  # Set default values for any unspecified option that has them.
  # Set an error condition if there are any required options that
  # were not specified.
  while (($key, $value) = each %opttbl) {
    if ($value->{required}) {
      $diemsg = "Required option -$key not specified" unless($diemsg);
    }
    $result{$key} = $value->{default};
  }
  if ($diemsg && !$result{help}) { die $diemsg . "\n" }
  %result;
}


1;

=head1 VARIABLES

The following global variables are defined by B<OpenAFS::util>.  None of these
are exported by default.  Those marked "Private" should not be used outside
AFStools; their names, meaning, and even existence may change at any time.

=over 12

=item %AFS_Help - Help info

This array contains argument lists for all publicly-exported AFStools
functions with names of the form AFS_*.  It is intended for programs like
B<testbed>, which provide a direct interactive interface to AFStools.

=item %AFS_Parms - Parameter settings  [Private]

This array contains the settings of AFStools parameters set with
B<OpenAFS::util::AFS_SetParm>.

=item %AFS_Trace - Tracing levels  [Private]

This array contains the tracing levels set with B<OpenAFS::util::AFS_Trace>.

=item %AFScmd - AFS command locations  [Private]

This array contains paths to the various AFS command binaries, for use
by B<OpenAFS::wrapper::wrapper> and possibly other AFStools functions.

=back

=head1 COPYRIGHT

The CMUCS AFStools, including this module are
Copyright (c) 1996, Carnegie Mellon University.  All rights reserved.
For use and redistribution information, see CMUCS/CMU_copyright.pm

=cut
