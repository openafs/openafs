# CMUCS AFStools
# Copyright (c) 1996, Carnegie Mellon University
# All rights reserved.
#
# See CMU_copyright.ph for use and distribution information
#
#: * vos.pm - Wrappers around VOS commands (volume maintenance)
#: * This module provides wrappers around the various volserver and VLDB
#: * commands, giving them a nice perl-based interface.  Someday, they might
#: * talk to the servers directly instead of using 'vos', but not anytime
#: * soon.
#:

package OpenAFS::vos;
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT :afs_internal);
use OpenAFS::wrapper;
use Exporter;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&AFS_vos_create        &AFS_vos_listvldb
                &AFS_vos_remove        &AFS_vos_delentry
                &AFS_vos_rename        &AFS_vos_syncserv
                &AFS_vos_move          &AFS_vos_syncvldb
                &AFS_vos_examine       &AFS_vos_lock
                &AFS_vos_addsite       &AFS_vos_unlock
                &AFS_vos_remsite       &AFS_vos_unlockvldb
                &AFS_vos_release       &AFS_vos_changeaddr
                &AFS_vos_backup        &AFS_vos_listpart
                &AFS_vos_backupsys     &AFS_vos_partinfo
                &AFS_vos_dump          &AFS_vos_listvol
                &AFS_vos_restore       &AFS_vos_zap
                &AFS_vos_status);

$vos_err_parse = [ 'Error in vos (.*) command', '-(.*)' ];


#: AFS_vos_create($vol, $server, $part, [$quota], [$cell])
#: Create a volume with name $vol
#: The server name ($server) may be a hostname or IP address
#: The partition may be a partition name (/vicepx), letter (x), or number (24)
#: If specified, use $quota for the initial quota instead of 5000 blocks.
#: If specified, work in $cell instead of the default cell.
#: On success, return the volume ID.
#:
$AFS_Help{vos_create} = '$vol, $server, $part, [$quota], [$cell] => $volid';
sub AFS_vos_create {
  my($vol, $server, $part, $quota, $cell) = @_;
  my(@args, $id);

  @args = ('create', '-name', $vol, '-server', $server, '-part', $part);
  push(@args, '-maxquota', $quota) if ($quota ne '');
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args, 
	   [$vos_err_parse,
	    ['^Volume (\d+) created on partition \/vicep\S+ of \S+', \$id ],
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  $id;
}


#: AFS_vos_remove($vol, $server, $part, [$cell])
#: Remove the volume $vol from the server and partition specified by $server and
#: $part.  If appropriate, also remove the corresponding VLDB entry.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_remove} = '$vol, $server, $part, [$cell] => Success?';
sub AFS_vos_remove {
  my($vol, $server, $part, $cell) = @_;
  my(@args);

  @args = ('remove', '-id', $vol, '-server', $server, '-part', $part);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_rename($old, $new, [$cell])
#: Rename the volume $old to have the name $new.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_rename} = '$old, $new, [$cell] => Success?';
sub AFS_vos_rename {
  my($old, $new, $cell) = @_;
  my(@args);

  @args = ('rename', '-oldname', $old, '-newname', $new);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_move($vol, $fromsrv, $frompart, $tosrv, $topart, [$cell])
#: Move the volume specified by $vol.
#: The source location is specified by $fromsrv and $frompart.
#: The destination location is specified by $tosrv and $topart.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.

#:
$AFS_Help{vos_move} = '$vol, $fromsrv, $frompart, $tosrv, $topart, [$cell] => Success?';
sub AFS_vos_move {
  my($vol, $fromsrv, $frompart, $tosrv, $topart, $cell) = @_;
  my(@args);

  @args = ('move', '-id', $vol,
	   '-fromserver', $fromsrv, '-frompartition', $frompart,
	   '-toserver', $tosrv, '-topartition', $topart);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_examine($vol, [$cell])
#: Examine the volume $vol, and return information about it.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return an associative array with some or all of the following:
#: - name         Name of this volume
#: - id           ID of this volume
#: - kind         Kind of volume (RW, RO, or BK)
#: - inuse        Disk space in use
#: - state        On-line or Off-line
#: - maxquota     Maximum disk usage quota
#: - minquota     Minimum disk usage quota (optional)
#: - stamp_create Time when volume was originally created
#: - stamp_update Time volume was last modified
#: - stamp_backup Time backup volume was cloned, or 'Never'
#: - stamp_copy   Time this copy of volume was made
#: - backup_flag  State of automatic backups: empty or 'disabled'
#: - dayuse       Number of accesses in the past day
#: - rwid         ID of read-write volume (even if this is RO or BK)
#: - roid         ID of read-only volume (even if this is RW or BK)
#: - bkid         ID of backup volume (even if this is RW or RO)
#: - rwserv       Name of server where read/write volume is
#: - rwpart       Name of partition where read/write volume is
#: - rosites      Reference to a list of read-only sites.  Each site, in turn,
#:                is a reference to a two-element list (server, part).
#:
$AFS_Help{vos_examine} = '$vol, [$cell] => %info';
sub AFS_vos_examine {
  my($vol, $cell) = @_;
  my(%result, @args, @rosites);

  @args = ('examine', '-id', $vol);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  %result = &wrapper('vos', \@args,
		     [$vos_err_parse,
		      ['^(\S+)\s*(\d+)\s*(RW|RO|BK)\s*(\d+)\s*K\s*([-\w]+)',          'name', 'id', 'kind', 'inuse', 'state'],
		      ['MaxQuota\s*(\d+)\s*K',                             'maxquota'     ],
		      ['MinQuota\s*(\d+)\s*K',                             'minquota'     ],
		      ['Creation\s*(.*\S+)',                               'stamp_create' ],
		      ['Last Update\s*(.*\S+)',                            'stamp_update' ],
		      ['Backup\s+([^\d\s].*\S+)',                          'stamp_backup' ],
		      ['Copy\s*(.*\S+)',                                   'stamp_copy'   ],
		      ['Automatic backups are (disabled) for this volume', 'backup_flag'  ],
		      ['(\d+) accesses in the past day',                   'dayuse'       ],
		      ['RWrite\:\s*(\d+)',                                 'rwid'         ],
		      ['ROnly\:\s*(\d+)',                                  'roid'         ],
		      ['Backup\:\s*(\d+)',                                 'bkid'         ],
		      ['server (\S+) partition /vicep(\S+) RW Site',       'rwserv', 'rwpart'],
		      ['server (\S+) partition /vicep(\S+) RO Site',       sub {
			push(@rosites, [$_[0], $_[1]]);
		      }],
		      ($AFS_Parms{'vostrace'} > 2) ? ([ '', '?']) : () ]);

  $result{'rosites'} = \@rosites if (@rosites);
  %result;
}



#: AFS_vos_addsite($vol, $server, $part, [$cell])
#: Add a replication site for volume $vol
#: The server name ($server) may be a hostname or IP address
#: The partition may be a partition name (/vicepx), letter (x), or number (24)
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_addsite} = '$vol, $server, $part, [$cell] => Success?';
sub AFS_vos_addsite {
  my($vol, $server, $part, $cell) = @_;
  my(@args);

  @args = ('addsite', '-id', $vol, '-server', $server, '-part', $part);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_remsite($vol, $server, $part, [$cell])
#: Remove a replication site for volume $vol
#: The server name ($server) may be a hostname or IP address
#: The partition may be a partition name (/vicepx), letter (x), or number (24)
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_remsite} = '$vol, $server, $part, [$cell] => Success?';
sub AFS_vos_remsite {
  my($vol, $server, $part, $cell) = @_;
  my(@args);

  @args = ('remsite', '-id', $vol, '-server', $server, '-part', $part);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_release($vol, [$cell], [$force])
#: Release the volume $vol.
#: If $force is specified and non-zero, use the "-f" switch.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_release} = '$vol, [$cell], [$force] => Success?';
sub AFS_vos_release {
  my($vol, $cell, $force) = @_;
  my(@args);

  @args = ('release', '-id', $vol);
  push(@args, '-f')                if ($force);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_backup($vol, [$cell])
#: Make a backup of the volume $vol.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_backup} = '$vol, [$cell] => Success?';
sub AFS_vos_backup {
  my($vol, $cell) = @_;
  my(@args);

  @args = ('backup', '-id', $vol);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_backupsys([$prefix], [$server, [$part]], [$exclude], [$cell])
#: Do en masse backups of AFS volumes.
#: If specified, match only volumes whose names begin with $prefix
#: If specified, limit work to the $server and, if given, $part.
#: If $exclude is specified and non-zero, backup only volumes NOT matched.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_backupsys} = '[$prefix], [$server, [$part]], [$exclude], [$cell] => Success?';
sub AFS_vos_backupsys {
  my($prefix, $server, $part, $exclude, $cell) = @_;
  my(@args);

  @args = ('backupsys');
  push(@args, '-prefix', $prefix)  if ($prefix);
  push(@args, '-server', $server)  if ($server);
  push(@args, '-partition', $part) if ($server && $part);
  push(@args, '-exclude')          if ($exclude);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_dump($vol, [$time], [$file], [$cell])
#: Dump the volume $vol
#: If specified, do an incremental dump since $time instead of a full dump.
#: If specified, dump to $file instead of STDOUT
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_dump} = '$vol, [$time], [$file], [$cell] => Success?';
sub AFS_vos_dump {
  my($vol, $time, $file, $cell) = @_;
  my(@args);

  @args = ('dump', '-id', $vol);
  push(@args, '-time', ($time ? $time : 0));
  push(@args, '-file', $file)      if ($file);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ],
	   { pass_stdout => !$file });
  1;
}


#: AFS_vos_restore($vol, $server, $part, [$file], [$id], [$owmode], [$cell])
#: Restore the volume $vol to partition $part on server $server.
#: If specified, restore from $file instead of STDIN
#: If specified, use the volume ID $id
#: If specified, $owmode must be 'abort', 'full', or 'incremental', and
#: indicates what to do if the volume exists.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_restore} = '$vol, $server, $part, [$file], [$id], [$owmode], [$cell] => Success?';
sub AFS_vos_restore {
  my($vol, $server, $part, $file, $id, $owmode, $cell) = @_;
  my(@args);

  @args = ('restore', '-name', $vol, '-server', $server, '-partition', $part);
  push(@args, '-file', $file)      if ($file);
  push(@args, '-id', $id)          if ($id);
  push(@args, '-overwrite', $owmode) if ($owmode);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_listvldb([$vol], [$server, [$part]], [$locked], [$cell])
#: Get a list of volumes in the VLDB.
#: If specified, list only the volume $vol
#: If specified, list only volumes on the server $server.
#: If specified with $server, list only volumes on the partition $part.
#: If $locked is specified and nonzero, list only locked VLDB entries
#: If specified, work in $cell instead of the default cell.
#: On success, return an associative array whose keys are names of volumes
#: on the specified server, and each of whose values is an associative
#: array describing the corresponding volume, containing some or all of
#: these elements:
#: - name         Name of this volume (same as key)
#: - rwid         ID of read-write volume (even if this is RO or BK)
#: - roid         ID of read-only volume (even if this is RW or BK)
#: - bkid         ID of backup volume (even if this is RW or RO)
#: - locked       Empty or LOCKED to indicate VLDB entry is locked
#: - rwserv       Name of server where read/write volume is
#: - rwpart       Name of partition where read/write volume is
#: - rosites      Reference to a list of read-only sites.  Each site, in turn,
#:                is a reference to a two-element list (server, part).
#:
$AFS_Help{vos_listvldb} = '[$vol], [$server, [$part]], [$locked], [$cell] => %vols';
sub AFS_vos_listvldb {
  my($vol, $server, $part, $locked, $cell) = @_;
  my(%finres, %vlist, @rosites);

  @args = ('listvldb');
  push(@args, '-name', $vol)       if ($vol);
  push(@args, '-server', $server)  if ($server);
  push(@args, '-partition', $part) if ($part && $server);
  push(@args, '-locked')           if ($locked);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  %finres = &wrapper('vos', \@args,
		     [$vos_err_parse,
		      ['^(VLDB|Total) entries', '.'],
		      ['^(\S+)', sub {
			my(%vinfo) = %OpenAFS::wrapper::result;

			if ($vinfo{name}) {
			  $vinfo{rosites} = [@rosites] if (@rosites);
			  $vlist{$vinfo{name}} = \%vinfo;

			  @rosites = ();
			  %OpenAFS::wrapper::result = ();
			}
		      }],
		      ['^(\S+)',                                           'name'         ],
		      ['RWrite\:\s*(\d+)',                                 'rwid'         ],
		      ['ROnly\:\s*(\d+)',                                  'roid'         ],
		      ['Backup\:\s*(\d+)',                                 'bkid'         ],
		      ['Volume is currently (LOCKED)',                     'locked'       ],
		      ['server (\S+) partition /vicep(\S+) RW Site',       'rwserv', 'rwpart'],
		      ['server (\S+) partition /vicep(\S+) RO Site',       sub {
			push(@rosites, [$_[0], $_[1]]);
		      }],
		      ($AFS_Parms{'vostrace'} > 2) ? ([ '', '?']) : () ]);

  if ($finres{name}) {
    $finres{rosites} = [@rosites] if (@rosites);
    $vlist{$finres{name}} = \%finres;
  }
  %vlist;
}



#: AFS_vos_delentry($vol, [$cell])
#: Delete the VLDB entry for the volume $vol
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_delentry} = '$vol, [$cell] => Success?';
sub AFS_vos_delentry {
  my($vol, $cell) = @_;
  my(@args);

  @args = ('delentry', '-id', $vol);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_syncserv($server, [$part], [$cell], [$force])
#: Synchronize the server $server with the VLDB
#: If specified, synchronize only partition $part
#: If specified, work in $cell instead of the default cell
#: If $force is specified, force updates to occur
#: On success, return 1.
#:
$AFS_Help{vos_syncserv} = '$server, [$part], [$cell], [$force] => Success?';
sub AFS_vos_syncserv {
  my($server, $part, $cell, $force) = @_;
  my(@args);

  @args = ('syncserv', '-server', $server);
  push(@args, '-partition', $part) if ($part);
  push(@args, '-force')            if ($force);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_syncvldb($server, [$part], [$cell], [$force])
#: Synchronize the VLDB with server $server
#: If specified, synchronize only partition $part
#: If specified, work in $cell instead of the default cell
#: If $force is specified, force updates to occur
#: On success, return 1.
#:
$AFS_Help{vos_syncvldb} = '$server, [$part], [$cell], [$force] => Success?';
sub AFS_vos_syncvldb {
  my($server, $part, $cell, $force) = @_;
  my(@args);

  @args = ('syncvldb', '-server', $server);
  push(@args, '-partition', $part) if ($part);
  push(@args, '-force')            if ($force);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_lock($vol, [$cell])
#: Lock the VLDB entry for volume $vol.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_lock} = '$vol, [$cell] => Success?';
sub AFS_vos_lock {
  my($vol, $cell) = @_;
  my(@args);

  @args = ('lock', '-id', $vol);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_unlock($vol, [$cell])
#: Unlock the VLDB entry for volume $vol.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_unlock} = '$vol, [$cell] => Success?';
sub AFS_vos_unlock {
  my($vol, $cell) = @_;
  my(@args);

  @args = ('unlock', '-id', $vol);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_unlockvldb([$server, [$part]], [$cell])
#: Unlock some or all VLDB entries
#: If specified, unlock only entries for volumes on server $server
#: If specified with $server, unlock only entries for volumes on
#: partition $part, instead of entries for volumes on all partitions
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_unlockvldb} = '[$server, [$part]], [$cell] => Success?';
sub AFS_vos_unlockvldb {
  my($server, $part, $cell) = @_;
  my(@args);

  @args = ('unlockvldb');
  push(@args, '-server', $server)  if ($server);
  push(@args, '-partition', $part) if ($server && $part);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_changeaddr($old, $new, [$cell])
#: Change the IP address of server $old to $new.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{vos_changeaddr} = '$old, $new, [$cell] => Success?';
sub AFS_vos_changeaddr {
  my($old, $new, $cell) = @_;
  my(@args);

  @args = ('changeaddr', '-oldaddr', $old, '-newaddr', $new);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_listpart($server, [$cell])
#: Retrieve a list of partitions on server $server
#: If specified, work in $cell instead of the default cell.
#: On success, return a list of partition letters
#:
$AFS_Help{vos_listpart} = '$server, [$cell] => @parts';
sub AFS_vos_listpart {
  my($server, $cell) = @_;
  my(@args, @parts);

  @args = ('listpart', '-server', $server);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    [ '^(.*\/vicep.*)$', #',
	     sub {
	       push(@parts, map {
		 my($x) = $_;
		 $x =~ s/^\/vicep//;
		 $x;
	       } split(' ', $_[0]));
	     }],
	    ($AFS_Parms{'vostrace'} > 2) ? ([ '', '?']) : () ]);
  @parts;
}


#: AFS_vos_partinfo($server, [$part], [$cell])
#: Get information about partitions on server $server.
#: If specified, only get info about partition $part.
#: If specified, work in $cell instead of the default cell.
#: On success, return an associative array whose keys are partition letters,
#: and each of whose values is a reference to a 2-element list, consisting
#: of the total size of the partition and the amount of space used.
#:
$AFS_Help{vos_partinfo} = '$server, [$part], [$cell] => %info';
sub AFS_vos_partinfo {
  my($server, $part, $cell) = @_;
  my(@args, %parts);

  @args = ('partinfo', '-server', $server);
  push(@args, '-partition', $part) if ($part);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    [ '^Free space on partition /vicep(.+)\: (\d+) K blocks out of total (\d+)',
	     sub {
	       $parts{$_[0]} = [ $_[1], $_[2] ];
	     }],
	    ($AFS_Parms{'vostrace'} > 2) ? ([ '', '?']) : () ]);
  %parts;
}


#: AFS_vos_listvol($server, [$part], [$cell])
#: Get a list of volumes on the server $server.
#: If specified, list only volumes on the partition $part.
#: If specified, work in $cell instead of the default cell.
#: On success, return an associative array whose keys are names of volumes
#: on the specified server, and each of whose values is an associative
#: array describing the corresponding volume, containing some or all of
#: these elements:
#: - name         Name of this volume (same as key)
#: - id           ID of this volume
#: - kind         Kind of volume (RW, RO, or BK)
#: - inuse        Disk space in use
#: - maxquota     Maximum disk usage quota
#: - minquota     Minimum disk usage quota (optional)
#: - stamp_create Time when volume was originally created
#: - stamp_update Time volume was last modified
#: - stamp_backup Time backup volume was cloned, or 'Never'
#: - stamp_copy   Time this copy of volume was made
#: - backup_flag  State of automatic backups: empty or 'disabled'
#: - dayuse       Number of accesses in the past day
#: - serv         Server where this volume is located
#: - part         Partition where this volume is located
#:
$AFS_Help{vos_listvol} = '$server, [$part], [$cell] => %vols';
sub AFS_vos_listvol {
  my($server, $part, $cell) = @_;
  my(%finres, %vlist);

  @args = ('listvol', '-server', $server, '-long');
  push(@args, '-partition', $part) if ($part);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  %finres = &wrapper('vos', \@args,
		     [$vos_err_parse,
		      ['^\S+\s*\d+\s*(RW|RO|BK)', sub {
			my(%vinfo) = %OpenAFS::wrapper::result;

			if ($vinfo{name}) {
			  $vlist{$vinfo{name}} = \%vinfo;
			  %OpenAFS::wrapper::result = ();
			}
		      }],
		      ['^(\S+)\s*(\d+)\s*(RW|RO|BK)\s*(\d+)\s*K',          'name', 'id', 'kind', 'inuse'],
		      ['(\S+)\s*\/vicep(\S+)\:',                           'serv', 'part' ],
		      ['MaxQuota\s*(\d+)\s*K',                             'maxquota'     ],
		      ['MinQuota\s*(\d+)\s*K',                             'minquota'     ],
		      ['Creation\s*(.*\S+)',                               'stamp_create' ],
		      ['Last Update\s*(.*\S+)',                            'stamp_update' ],
		      ['Backup\s+([^\d\s].*\S+)',                          'stamp_backup' ],
		      ['Copy\s*(.*\S+)',                                   'stamp_copy'   ],
		      ['Automatic backups are (disabled) for this volume', 'backup_flag'  ],
		      ['(\d+) accesses in the past day',                   'dayuse'       ],
		      ($AFS_Parms{'vostrace'} > 2) ? ([ '', '?']) : () ]);

  if ($finres{name}) {
    $vlist{$finres{name}} = \%finres;
  }
  %vlist;
}

#: AFS_vos_zap($vol, $server, $part, [$cell], [$force])
#: Remove the volume $vol from the server and partition specified by $server and
#: $part.  Don't bother messing with the VLDB.
#: If specified, work in $cell instead of the default cell.
#: If $force is specified, force the zap to happen
#: On success, return 1.
#:
$AFS_Help{vos_zap} = '$vol, $server, $part, [$cell], [$force] => Success?';
sub AFS_vos_zap {
  my($vol, $server, $part, $cell, $force) = @_;
  my(@args);

  @args = ('zap', '-id', $vol, '-server', $server, '-part', $part);
  push(@args, '-force')            if ($force);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 1);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    $AFS_Parms{'vostrace'} ? ([ '', '?']) : () ]);
  1;
}


#: AFS_vos_status($server, [$cell])
#: Get information about outstanding transactions on $server
#: If specified, work in $cell instead of the default cell
#: On success, return a list of transactions, each of which is a reference
#: to an associative array containing some or all of these elements:
#: - transid      Transaction ID
#: - stamp_create Time the transaction was created
#: - volid        Volume ID
#: - part         Partition letter
#: - action       Action or procedure
#: - flags        Volume attach flags
#: If there are no transactions, the list will be empty.
#:
$AFS_Help{vos_status} = '$server, [$cell] => @trans';
sub AFS_vos_status {
  my($server, $cell) = @_;
  my(@trlist);

  @args = ('status', '-server', $server);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-verbose')          if ($AFS_Parms{'vostrace'} > 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('vos', \@args,
	   [$vos_err_parse,
	    ['^(\-)', sub {
	      my(%trinfo) = %OpenAFS::wrapper::result;
	      
	      if ($trinfo{transid}) {
		push(@trlist, \%trinfo);
		%OpenAFS::wrapper::result = ();
	      }
	    }],
	    ['^transaction\:\s*(\d+)\s*created: (.*\S+)',        'transid', 'stamp_create'],
	    ['^attachFlags:\s*(.*\S+)',                          'flags'],
	    ['^volume:\s*(\d+)\s*partition\: \/vicep(\S+)\s*procedure\:\s*(\S+)',
	     'volid', 'part', 'action'],
	    ($AFS_Parms{'vostrace'} > 2) ? ([ '', '?']) : () ]);

  @trlist;
}

1;
