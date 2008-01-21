# CMUCS AFStools
# Copyright (c) 1996, Carnegie Mellon University
# All rights reserved.
#
# See CMU_copyright.pm for use and distribution information

package OpenAFS::config;

=head1 NAME

OpenAFS::config - AFStools configuration

=head1 SYNOPSIS

  use OpenAFS::config;

=head1 DESCRIPTION

This module contains various AFStools configuration variables which are used
by the other AFStools modules.  These describe how AFStools should act in a
particular installation, and are mostly pretty mundane.  All of the defaults
here are pretty reasonable, so you shouldn't have to change anything unless
your site is particularly exotic.

Note that this file only describes how a particular B<installation> of AFStools
should act, not how it should act upon a particular B<cell>.  Since the cell
AFStools is running in is not necessarily the same as the cell on which it
is acting, most configuration that is really per-cell should be located in a
cell-specific module.

This module should only be used by other parts of AFStools.  As such, the
variables described here are not normally visible to user programs, and this
file is mostly of interest to administrators who are installing AFStools.

=over 4

=cut

use OpenAFS::CMU_copyright;
use OpenAFS::Dirpath;
use Exporter;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw($def_ConfDir
                @CmdList
                @CmdPath
                $err_table_dir
               );

# The following configuration variables are defined here.  Mention them
# all an extra time, to suppress annoying warnings.  Doesn't perl have
# a way of doing this???
@x = ();
@x = ($def_ConfDir, @CmdList, @CmdPath);

=item $def_ConfDir - Default configuration directory

This is the default AFS configuration directory, where files like ThisCell,
CellServDB, and so on are found.  If the AFStools parameter I<confdir> is
set, it will generally be searched before this directory.  Normally, this
should be set to F</usr/vice/etc> and not changed, as that path is hardwired
into AFS.  However, it might be necessary to modify this if your site uses
an exotic locally-compiled version of AFS.

=cut

$def_ConfDir = $OpenAFS::Dirpath::openafsdirpath->{'viceetcdir'};
#$def_ConfDir = "/usr/vice/etc";


=item @CmdList - List of AFS commands

This is a list of AFS commands that the AFStools package might want to invoke
using B<OpenAFS::wrapper::wrapper>.  Don't remove anything from this list if you
know what's good for you.  It's OK to add things, though, if you think you
might use the wrapper features for something.

=cut

@CmdList = ('fs', 'pts', 'vos', 'bos', 'kas', 'krbkas', 'sys');


=item @CmdPath - Path to search for AFS commands

This is the list of directories where B<OpenAFS::wrapper::wrapper> will look for
AFS commands.  For AFStools to work properly, every command listed in
I<@OpenAFS::config::CmdList> must appear in one of these directories.  The default
should be sufficient for most sites; we deal with Transarc's reccommendations
as well as common practice.  Note that on machines for which /usr/afs/bin
exists (typically, AFS fileservers), that directory is first.  This is probably
what you want...

=cut

@CmdPath = (split(/:/, $ENV{PATH}),
            $OpenAFS::Dirpath::openafsdirpath->{'afssrvbindir'},        # For servers
	    '/usr/local/bin',      # Many sites put AFS in /usr/local
	    '/usr/local/etc',
	    '/usr/afsws/bin',      # For people who use Transarc's
	    '/usr/afsws/etc');     # silly reccommendations

=item $err_table_dir - Error table directory

This is the location of error tables used by the errcode and errstr
routines in OpenAFS::errtrans.  Each file in this directory should be a
com_err error table (in source form), and should be named the same
as the com_err error package contained within.

=cut

$err_table_dir = '/usr/local/lib/errtbl';

1;

=back

=head1 COPYRIGHT

The CMUCS AFStools, including this module are
Copyright (c) 1996, Carnegie Mellon University.  All rights reserved.
For use and redistribution information, see CMUCS/CMU_copyright.pm

=cut
