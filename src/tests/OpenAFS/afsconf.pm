# CMUCS AFStools
# Copyright (c) 1996, Carnegie Mellon University
# All rights reserved.
#
# See CMUCS/CMU_copyright.pm for use and distribution information

package OpenAFS::afsconf;

=head1 NAME

OpenAFS::afsconf - Access to AFS config info

=head1 SYNOPSIS

  use OpenAFS::afsconf;

  $cell = AFS_conf_localcell();
  $cell = AFS_conf_canoncell($cellname);
  @servers = AFS_conf_cellservers($cellname);
  @cells = AFS_conf_listcells();
  %info = AFS_conf_cacheinfo();

=head1 DESCRIPTION

This module provides access to information about the local workstation's
AFS configuration.  This includes information like the name of the
local cell, where AFS is mounted, and access to information in the
F<CellServDB>.  All information returned by this module is based on the
configuration files, and does not necessarily reflect changes made
on the afsd command line or using B<fs> commands.

=cut

use OpenAFS::CMU_copyright;
use OpenAFS::config;
use OpenAFS::util qw(:DEFAULT :afs_internal);
use Exporter;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&AFS_conf_localcell
                &AFS_conf_canoncell
		&AFS_conf_listcells
		&AFS_conf_cellservers
		&AFS_conf_cacheinfo);


# _confpath($file) - Return path to a configuration file
sub _confpath {
  my($file) = @_;

  if ($conf_paths{$file}) {
    $conf_paths{$file};
  } elsif ($AFS_Parms{confdir} && -r "$AFS_Parms{confdir}/$file") {
    $conf_paths{$file} = "$AFS_Parms{confdir}/$file";
  } elsif (-r "$def_ConfDir/$file") {
    $conf_paths{$file} = "$def_ConfDir/$file";
  } else {
    die "Unable to locate $file\n";
  }
}

=head2 AFS_conf_localcell()

Return the canonical name of the local cell.  This depends on the contents
of the F<ThisCell> file in the AFS configuration directory.

=cut

$AFS_Help{conf_localcell} = '=> $lclcell';
sub AFS_conf_localcell {
  my($path) = _confpath(ThisCell);
  my($result);

  return '' if (!$path);
  if (open(THISCELL, $path)) {
    chomp($result = <THISCELL>);
    close(THISCELL);
    $result;
  } else {
    die "Unable to open $path: $!\n";
  }
}

=head2 AFS_conf_canoncell($cellname)

Return the canonical name of the specified cell, as found in F<CellServDB>.
I<$cellname> may be any unique prefix of a cell name, as with various AFS
commands that take cell names as arguments.

=head2 AFS_conf_cellservers($cellname)

Return a list of servers in the specified cell.  As with B<AFS_conf_canoncell>,
I<$cellname> may be any unique prefix of a cell name.  The resulting list
contains server hostnames, as found in F<CellServDB>.

=cut

$AFS_Help{conf_canoncell} = '$cellname => $canon';
$AFS_Help{conf_cellservers} = '$cellname => @servers';

sub AFS_conf_canoncell   { &_findcell($_[0], 0); }
sub AFS_conf_cellservers { &_findcell($_[0], 1); }

sub _findcell {
  my($cellname, $doservers) = @_;
  my($path, $found, @servers, $looking);

  return $canon_name{$cellname} if (!$doservers && $canon_name{$cellname});
  $path = _confpath(CellServDB) || die "Unable to locate CellServDB\n";

  if (open(CELLSERVDB, $path)) {
    my($cellpat) = $cellname;
    $cellpat =~ s/(\W)/\\$1/g;
    while (<CELLSERVDB>) {
      $looking = 0 if (/^\>/);
      if (/^\>$cellpat/) {
	if ($found) {
	  close(CELLSERVDB);
	  die "Cell name $cellname is not unique\n";
	} else {
	  chop($found = $_);
	  $found =~ s/^\>(\S+).*/$1/;
	  $looking = 1 if ($doservers);
	}
      } elsif ($looking && (/^[\.\d]+\s*\#\s*(.*\S+)/ || /^([\.\d]+)/)) {
	push(@servers, $1);
      }
    }
    close(CELLSERVDB);
    if ($found) {
      $canon_name{$cellname} = $found;
      $doservers ? @servers : ($found);
    } else {
      die "Cell $cellname not in CellServDB\n";
    }
  } else {
    die "Unable to open $path: $!\n";
  }
}

=head2 AFS_conf_listcells()

Return a list of canonical names (as found in F<CellServDB>) of all
known AFS cells.

=cut

$AFS_Help{conf_listcells} = '=> @cells';
sub AFS_conf_listcells {
  my($path, @cells);

  $path = _confpath(CellServDB) || die "Unable to locate CellServDB!\n";

  if (open(CELLSERVDB, $path)) {
    while (<CELLSERVDB>) {
      if (/^\>(\S+)/) {
	push(@cells, $1);
      }
    }
    close(CELLSERVDB);
    @cells;
  } else {
    die "Unable to open $path: $!\n";
  }
}

=head2 AFS_conf_cacheinfo()

Return a table of information about the local workstation's cache
configuration.  This table may contain any or all of the following elements:

=over 14

=item afsroot

Mount point for the AFS root volume

=item cachedir

Location of the AFS cache directory

=item cachesize

AFS cache size

=item hardcachesize

Hard limit on AFS cache size (if specified; probably Mach-specific)

=item translator

Name of AFS/NFS translator server (if set)

=back

=cut

$AFS_Help{conf_cacheinfo} = '=> %info';
sub AFS_conf_cacheinfo {
  my($path) = _confpath('cacheinfo');
  my(%result, $line, $hcs);

  if ($path) {
    if (open(CACHEINFO, $path)) {
      chop($line = <CACHEINFO>);
      close(CACHEINFO);
      (@result{'afsroot', 'cachedir', 'cachesize'} , $hcs) = split(/:/, $line);
      $result{'hardcachesize'} = $hcs if ($hcs);
    } else {
      die "Unable to open $path: $!\n";
    }
  }
  if ($ENV{'AFSSERVER'}) {
    $result{'translator'} = $ENV{'AFSSERVER'};
  } elsif (open(SRVFILE, "$ENV{HOME}/.AFSSERVER")
	   || open(SRVFILE, "/.AFSSERVER")) {
    $result{'translator'} = <SRVFILE>;
    close(SRVFILE);
  }
  %result;
}


1;

=head1 COPYRIGHT

The CMUCS AFStools, including this module are
Copyright (c) 1996, Carnegie Mellon University.  All rights reserved.
For use and redistribution information, see CMUCS/CMU_copyright.pm

=cut
