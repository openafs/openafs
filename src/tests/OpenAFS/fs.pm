# CMUCS AFStools
# Copyright (c) 1996, 2001 Carnegie Mellon University
# All rights reserved.
#
# See CMU_copyright.ph for use and distribution information
#
#: * fs.pm - Wrappers around the FS commands (fileserver/cache manager)
#: * This module provides wrappers around the various FS commands, which
#: * perform fileserver and cache manager control operations.  Right now,
#: * these are nothing more than wrappers around 'fs'; someday, we might
#: * talk to the cache manager directly, but not anytime soon.
#:

package OpenAFS::fs;
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT :afs_internal);
use OpenAFS::wrapper;
use Exporter;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&AFS_fs_getacl          &AFS_fs_setacl
                &AFS_fs_cleanacl        &AFS_fs_getquota
                &AFS_fs_setquota        &AFS_fs_whereis
		&AFS_fs_examine         &AFS_fs_setvol
                &AFS_fs_getmount        &AFS_fs_mkmount
                &AFS_fs_rmmount         &AFS_fs_checkvolumes
                &AFS_fs_flush           &AFS_fs_flushmount
                &AFS_fs_flushvolume     &AFS_fs_messages
                &AFS_fs_newcell         &AFS_fs_rxstatpeer
                &AFS_fs_rxstatproc      &AFS_fs_setcachesize
                &AFS_fs_setcell         &AFS_fs_setcrypt
                &AFS_fs_setclientaddrs  &AFS_fs_copyacl
                &AFS_fs_storebehind     &AFS_fs_setserverprefs
                &AFS_fs_checkservers    &AFS_fs_checkservers_interval
                &AFS_fs_exportafs       &AFS_fs_getcacheparms
                &AFS_fs_getcellstatus   &AFS_fs_getclientaddrs
                &AFS_fs_getcrypt        &AFS_fs_getserverprefs
                &AFS_fs_listcells       &AFS_fs_setmonitor
                &AFS_fs_getmonitor      &AFS_fs_getsysname
                &AFS_fs_setsysname      &AFS_fs_whichcell
                &AFS_fs_wscell);

#: ACL-management functions:
#: AFS access control lists are represented as a Perl list (or usually, a
#: reference to such a list).  Each element in such a list corresponds to
#: a single access control entry, and is a reference to a 2-element list
#: consisting of a PTS entity (name or ID), and a set of rights.  The
#: rights are expressed in the usual publically-visible AFS notation, as
#: a string of characters drawn from the class [rlidwkaABCDEFGH].  No
#: rights are denoted by the empty string; such an ACE will never returned
#: by this library, but may be used as an argument to remove a particular
#: ACE from a directory's ACL.
#:
#: One might be inclined to ask why we chose this representation, instead of
#: using an associative array, as might seem obvious.  The answer is that
#: doing so would have implied a nonambiguity that isn't there.  Suppose you
#: have an ACL %x, and want to know if there is an entry for user $U on that
#: list.  You might think you could do this by looking at $x{$U}.  The
#: problem here is that two values for $U (one numeric and one not) refer to
#: the same PTS entity, even though they would reference different elements
#: in such an ACL.  So, we instead chose a representation that wasn't a hash,
#: so people wouldn't try to do hash-like things to it.  If you really want
#: to be able to do hash-like operations, you should turn the list-form ACL
#: into a hash table, and be sure to do name-to-number translation on all the
#: keys as you go.
#:
#: AFS_fs_getacl($path)
#: Get the ACL on a specified path.
#: On success, return a list of two references to ACLs; the first is the
#: positive ACL for the specified path, and the second is the negative ACL.
#:
$AFS_Help{fs_getacl} = '$path => (\@posacl, \@negacl)';
sub AFS_fs_getacl {
  my($path) = @_;
  my(@args, @posacl, @negacl, $neg);

  @args = ('listacl', '-path', $path);
  &wrapper('fs', \@args,
	   [
	    [ '^(Normal|Negative) rights\:', sub {
	      $neg = ($_[0] eq 'Negative');
	    }],
	    [ '^  (.*) (\S+)$', sub { #',{
	      if ($neg) {
		push(@negacl, [@_]);
	      } else {
		push(@posacl, [@_]);
	      }
	    }]]);
  (\@posacl, \@negacl);
}

#: AFS_fs_setacl(\@paths, \@posacl, \@negacl, [$clear])
#: Set the ACL on a specified path.  Like the 'fs setacl' command, this
#: function normally only changes ACEs that are mentioned in one of the two
#: argument lists.  If a given ACE already exists, it is changed; if not, it
#: is added.  To delete a single ACE, specify the word 'none' or the empty
#: string in the rights field.  ACEs that already exist but are not mentioned
#: are left untouched, unless $clear is specified.  In that case, all
#: existing ACE's (both positive and negative) are deleted.
$AFS_Help{fs_setacl} = '\@paths, \@posacl, \@negacl, [$clear] => Success?';
sub AFS_fs_setacl {
  my($paths, $posacl, $negacl, $clear) = @_;
  my($ace, $U, $access);

  if (@$posacl) {
    @args = ('setacl', '-dir', @$paths);
    push(@args, '-clear') if ($clear);
    push(@args, '-acl');
    foreach $e (@$posacl) {
      ($U, $access) = @$e;
      $access = 'none' if ($access eq '');
      push(@args, $U, $access);
    }
    &wrapper('fs', \@args);
  }
  if (@$negacl) {
    @args = ('setacl', '-dir', @$paths, '-negative');
    push(@args, '-clear') if ($clear && !@$posacl);
    push(@args, '-acl');
    foreach $e (@$negacl) {
      ($U, $access) = @$e;
      $access = 'none' if ($access eq '');
      push(@args, $U, $access);
    }
    &wrapper('fs', \@args);
  }
  if ($clear && !@$posacl && !@$negacl) {
    @args = ('setacl', '-dir', @$paths,
	     '-acl', 'system:anyuser', 'none', '-clear');
    &wrapper('fs', \@args);
  }
  1;
}

#: AFS_fs_cleanacl(\@paths)
#: Clean the ACL on the specified path, removing any ACEs which refer to PTS
#: entities that no longer exist.  All the work is done by 'fs'.
#:
$AFS_Help{'fs_cleanacl'} = '\@paths => Success?';
sub AFS_fs_cleanacl {
  my($paths) = @_;
  my(@args);

  @args = ('cleanacl', '-path', @$paths);
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_getquota($path) [listquota]
#: Get the quota on the specified path.
#: On success, returns the quota.
#:
$AFS_Help{'fs_getquota'} = '$path => $quota';
sub AFS_fs_getquota {
  my($path) = @_;
  my(@args, $quota);

  @args = ('listquota', '-path', $path);
  &wrapper('fs', \@args,
	   [[ '^\S+\s+(\d+)\s+\d+\s+\d+\%', \$quota ]]);
  $quota;
}

#: AFS_fs_setquota($path, $quota) [setquota]
#: Set the quota on the specified path to $quota.  If $quota is
#: given as 0, there will be no limit to the volume's size.
#: On success, return 1
#:
$AFS_Help{'fs_setquota'} = '$path, $quota => Success?';
sub AFS_fs_setquota {
  my($path, $quota) = @_;
  my(@args);

  @args = ('setquota', '-path', $path, '-max', $quota);
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_whereis($path)  [whereis, whichcell]
#: Locate the fileserver housing the specified path, and the cell in which it
#: is located.
#: On success, returns a list of 2 or more elements.  The first element is the
#: name of the cell in which the volume is located.  The remaining elements
#: the names of servers housing the volume; for a replicated volume, there may
#: (should) be more than one such server.
#:
$AFS_Help{'fs_whereis'} = '$path => ($cell, @servers)';
sub AFS_fs_whereis {
  my($path) = @_;
  my(@args, $cell, @servers);

  @args = ('whichcell', '-path', $path);
  &wrapper('fs', \@args,
	   [[ "lives in cell \'(.*)\'", \$cell ]]);

  @args = ('whereis', '-path', $path);
  &wrapper('fs', \@args,
	   [[ 'is on host(s?)\s*(.*)', sub {
	     @servers = split(' ', $_[1]);
	   }]]);
  ($cell, @servers);
}

#: AFS_fs_examine($path)
#: Get information about the volume containing the specified path.
#: On success, return an associative array containing some or all
#: of the following elements:
#: - vol_name
#: - vol_id
#: - quota_max
#: - quota_used
#: - quota_pctused
#: - part_size
#: - part_avail
#: - part_used
#: - part_pctused
#:
$AFS_Help{'fs_examine'} = '$path => %info';
sub AFS_fs_examine {
  my($path) = @_;
  my(@args, %info);

  @args = ('examine', '-path', $path);
  %info = &wrapper('fs', \@args,
		   [[ 'vid = (\d+) named (\S+)',       'vol_id', 'vol_name' ],
		    [ 'disk quota is (\d+|unlimited)', 'quota_max' ],
		    [ 'blocks used are (\d+)',         'quota_used' ],
		    [ '(\d+) blocks available out of (\d+)',
		     'part_avail', 'part_size']]);
  if ($info{'quota_max'} eq 'unlimited') {
    $info{'quota_max'} = 0;
    $info{'quota_pctused'} = 0;
  } else {
    $info{'quota_pctused'} = ($info{'quota_used'} / $info{'quota_max'}) * 100;
    $info{'quota_pctused'} =~ s/\..*//;
  }
  $info{'part_used'} = $info{'part_size'} - $info{'part_avail'};
  $info{'part_pctused'} = ($info{'part_used'} / $info{'part_size'}) * 100;
  $info{'part_pctused'} =~ s/\..*//;
  %info;
}

#: AFS_fs_setvol($path, [$maxquota], [$motd])
#: Set information about the volume containing the specified path.
#: On success, return 1.
$AFS_Help{'fs_setvol'} = '$path, [$maxquota], [$motd] => Success?';
sub AFS_fs_setvol {
  my($path, $maxquota, $motd) = @_;
  my(@args);

  @args = ('setvol', '-path', $path);
  push(@args, '-max', $maxquota) if ($maxquota || $maxquota eq '0');
  push(@args, '-motd', $motd) if ($motd);
  &wrapper('fs', \@args);
  1;
}


#: AFS_fs_getmount($path)
#: Get the contents of the specified AFS mount point.
#: On success, return the contents of the specified mount point.
#: If the specified path is not a mount point, return the empty string.
$AFS_Help{'fs_getmount'} = '$path => $vol';
sub AFS_fs_getmount {
  my($path) = @_;
  my(@args, $vol);

  @args = ('lsmount', '-dir', $path);
  &wrapper('fs', \@args,
	   [[ "mount point for volume '(.+)'", \$vol ]]);
  $vol;
}


#: AFS_fs_mkmount($path, $vol, [$cell], [$rwmount], [$fast])
#: Create an AFS mount point at $path, leading to the volume $vol.
#: If $cell is specified, create a cellular mount point to that cell.
#: If $rwmount is specified and nonzero, create a read-write mount point.
#: If $fast is specified and nonzero, don't check to see if the volume exists.
#: On success, return 1.
$AFS_Help{'fs_mkmount'} = '$path, $vol, [$cell], [$rwmount], [$fast] => Success?';
sub AFS_fs_mkmount {
  my($path, $vol, $cell, $rwmount, $fast) = @_;
  my(@args);

  @args = ('mkmount', '-dir', $path, '-vol', $vol);
  push(@args, '-cell', $cell) if ($cell);
  push(@args, '-rw') if ($rwmount);
  push(@args, '-fast') if ($fast);
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_rmmount($path) [rmmount]
#: Remove an AFS mount point at $path
#: On success, return 1
$AFS_Help{'fs_rmmount'} = '$path => Success?';
sub AFS_fs_rmmount {
  my($path) = @_;
  my(@args);

  @args = ('rmmount', '-dir', $path);
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_checkvolumes()
#: Check/update volume ID cache
#: On success, return 1
$AFS_Help{'fs_checkvolumes'} = '=> Success?';
sub AFS_fs_checkvolumes {
  my(@args);

  @args = ('checkvolumes');
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_flush(\@paths)
#: Flush files named by @paths from the cache
#: On success, return 1
$AFS_Help{'fs_flush'} = '\@paths => Success?';
sub AFS_fs_flush {
  my($paths) = @_;
  my(@args);

  @args = ('flush');
  push(@args, '-path', @$paths) if $paths;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_flushmount(\@paths)
#: Flush mount points named by @paths from the cache
#: On success, return 1
$AFS_Help{'fs_flushmount'} = '\@paths => Success?';
sub AFS_fs_flushmount {
  my($paths) = @_;
  my(@args);

  @args = ('flushmount');
  push(@args, '-path', @$paths) if $paths;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_flushvolume(\@paths)
#: Flush volumes containing @paths from the cache
#: On success, return 1
$AFS_Help{'fs_flushvolume'} = '\@paths => Success?';
sub AFS_fs_flushvolume {
  my($paths) = @_;
  my(@args);

  @args = ('flushvolume');
  push(@args, '-path', @$paths) if $paths;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_messages($mode)
#: Set cache manager message mode
#: Valid modes are 'user', 'console', 'all', 'none'
#: On success, return 1
$AFS_Help{'fs_messages'} = '$mode => Success?';
sub AFS_fs_messages {
  my($mode) = @_;
  my(@args);

  @args = ('messages', '-show', $mode);
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_newcell($cell, \@dbservers, [$linkedcell])
#: Add a new cell to the cache manager's list, or updating an existing cell
#: On success, return 1
$AFS_Help{'fs_newcell'} = '$cell, \@dbservers, [$linkedcell] => Success?';
sub AFS_fs_newcell {
  my($cell, $dbservers, $linkedcell) = @_;
  my(@args);

  @args = ('newcell', '-name', $cell, '-servers', @$dbservers);
  push(@args, '-linkedcell', $linkedcell) if $linkedcell;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_rxstatpeer($enable, [$clear])
#: Control per-peer Rx statistics:
#: - if $enable is 1, enable stats
#: - if $enable is 0, disable stats
#: - if $clear  is 1, clear stats
#: On success, return 1
$AFS_Help{'fs_rxstatpeer'} = '$enable, [$clear] => Success?';
sub AFS_fs_rxstatpeer {
  my($enable, $clear) = @_;
  my(@args);

  @args = ('rxstatpeer');
  push(@args, '-enable')  if $enable;
  push(@args, '-disable') if defined($enable) && !$enable;
  push(@args, '-clear')   if $clear;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_rxstatproc($enable, [$clear])
#: Control per-process Rx statistics:
#: - if $enable is 1, enable stats
#: - if $enable is 0, disable stats
#: - if $clear  is 1, clear stats
#: On success, return 1
$AFS_Help{'fs_rxstatproc'} = '$enable, [$clear] => Success?';
sub AFS_fs_rxstatproc {
  my($enable, $clear) = @_;
  my(@args);

  @args = ('rxstatproc');
  push(@args, '-enable')  if $enable;
  push(@args, '-disable') if defined($enable) && !$enable;
  push(@args, '-clear')   if $clear;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_setcachesize($size)
#: Set the cache size to $size K
#: On success, return 1
$AFS_Help{'fs_setcachesize'} = '$size => Success?';
sub AFS_fs_setcachesize {
  my($size) = @_;
  my(@args);

  @args = ('setcachesize', '-blocks', $size);
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_setcell(\@cells, $suid)
#: Set cell control bits for @cells
#: - if $suid is 1, enable suid programs
#: - if $suid is 0, disable suid programs
#: On success, return 1
$AFS_Help{'fs_setcell'} = '\@cells, [$suid] => Success?';
sub AFS_fs_setcell {
  my($cells, $suid) = @_;
  my(@args);

  @args = ('setcell', '-cell', @$cells);
  push(@args, '-suid')   if $suid;
  push(@args, '-nosuid') if defined($suid) && !$suid;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_setcrypt($enable)
#: Control cache manager encryption
#: - if $enable is 1, enable encrypted connections
#: - if $enable is 0, disable encrypted connections
#: On success, return 1
$AFS_Help{'fs_setcrypt'} = '$enable => Success?';
sub AFS_fs_setcrypt {
  my($enable) = @_;
  my(@args);

  @args = ('setcrypt', '-crypt', $enable ? 'on' : 'off');
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_setclientaddrs(\@addrs)
#: Set client network interface addresses
#: On success, return 1
$AFS_Help{'fs_setclientaddrs'} = '\@addrs => Success?';
sub AFS_fs_setclientaddrs {
  my($addrs) = @_;
  my(@args);

  @args = ('setclientaddrs');
  push(@args, '-address', @$addrs) if $addrs;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_copyacl($from, \@to, [$clear])
#: Copy the access control list on $from to each directory named in @to.
#: If $clear is specified and nonzero, the target ACL's are cleared first
#: On success, return 1
$AFS_Help{'fs_copyacl'} = '$from, \@to, [$clear] => Success?';
sub AFS_fs_copyacl {
  my($from, $to, $clear) = @_;
  my(@args);

  @args = ('copyacl', '-fromdir', $from, '-todir', @$to);
  push(@args, '-clear') if $clear;
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_storebehind(\@paths, [$size], [$def])
#: Set amount of date to store after file close
#: If $size is specified, the size for each file in @paths is set to $size.
#: If $default is specified, the default size is set to $default.
#: Returns the new or current default value, and a hash mapping filenames
#: to their storebehind sizes.  A hash entry whose value is undef indicates
#: that the corresponding file will use the default size.
$AFS_Help{'fs_storebehind'} = '\@paths, [$size], [$def] => ($def, \%sizes)';
sub AFS_fs_storebehind {
  my($paths, $size, $def) = @_;
  my(@args, %sizes, $ndef);

  @args = ('storebehind', '-verbose');
  push(@args, '-kbytes', $size) if defined($size);
  push(@args, '-files', @$paths) if $paths && @$paths;
  push(@args, '-allfiles', $def) if defined($def);
  &wrapper('fs', \@args, [
    ['^Will store up to (\d+) kbytes of (.*) asynchronously',
     sub { $sizes{$_[1]} = $_[0] }],
    ['^Will store (.*) according to default',
     sub { $sizes{$_[0]} = undef }],
    ['^Default store asynchrony is (\d+) kbytes', \$ndef],
  ]);
  ($ndef, \%sizes);
}

#: AFS_fs_setserverprefs(\%fsprefs, \%vlprefs)
#: Set fileserver and/or VLDB server preference ranks
#: Each of %fsprefs and %vlprefs maps server names to the rank to be
#: assigned to the specified servers.
#: On success, return 1.
$AFS_Help{'fs_setserverprefs'} = '\%fsprefs, \%vlprefs => Success?';
sub AFS_fs_setserverprefs {
  my($fsprefs, $vlprefs) = @_;
  my(@args, $srv);

  @args = ('setserverprefs');
  if ($fsprefs && %$fsprefs) {
    push(@args, '-servers');
    foreach $srv (keys %$fsprefs) {
      push(@args, $srv, $$fsprefs{$srv});
    }
  }
  if ($vlprefs && %$vlprefs) {
    push(@args, '-vlservers');
    foreach $srv (keys %$vlprefs) {
      push(@args, $srv, $$vlprefs{$srv});
    }
  }
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_checkservers([$fast], [$allcells], [$cell])
#: Check to see what fileservers are down
#: If $cell is specified, fileservers in the specified cell are checked
#: If $allcells is specified and nonzero, fileservers in all cells are checked
#: If $fast is specified and nonzero, don't probe servers
$AFS_Help{'fs_checkservers'} = '[$fast], [$allcells], [$cell] => @down';
sub AFS_fs_checkservers {
  my($fast, $allcells, $cell) = @_;
  my(@args, @down);

  @args = ('checkservers');
  push(@args, '-all')         if $allcells;
  push(@args, '-fast')        if $fast;
  push(@args, '-cell', $cell) if $cell;
  &wrapper('fs', \@args, [
    ['^These servers unavailable due to network or server problems: (.*)\.',
     sub { push(@down, split(' ', $_[0])) }],
  ]);
  @down;
}

#: AFS_fs_checkservers_interval([$interval])
#: Get and/or set the down server check interval
#: If $interval is specified and nonzero, it is set as the new interval
#: On success, returns the old interval in seconds
$AFS_Help{'fs_checkservers_interval'} = '$interval => $oldinterval';
sub AFS_fs_checkservers_interval {
  my($interval) = @_;
  my(@args, $oldinterval);

  @args = ('checkservers', '-interval', $interval);
  &wrapper('fs', \@args, [
    ['^The new down server probe interval \((\d+) secs\)',    \$oldinterval],
    ['^The current down server probe interval is (\d+) secs', \$oldinterval],
  ]);
  $oldinterval;
}

#: AFS_fs_exportafs($type, \%options);
#: Get and/or modify protocol translator settings
#: $type is the translator type, which must be 'nfs'
#: %options specifies the options to be set.  Each key is the name of an
#: option, which is enabled if the value is 1, and disabled if the value
#: is 0.  The following options are supported:
#:   start       Enable exporting of AFS
#:   convert     Copy AFS owner mode bits to UNIX group/other mode bits
#:   uidcheck    Strict UID checking
#:   submounts   Permit mounts of /afs subdirectories
#: On success, returns an associative array %modes, which is of the same
#: form, indicating which options are enabled.
$AFS_Help{'fs_exportafs'} = '$type, \%options => %modes';
sub AFS_fs_exportafs {
  my($type, $options) = @_;
  my(@args, %modes);

  @args = ('exportafs', '-type', $type);
  foreach (qw(start convert uidcheck submounts)) {
    push(@args, "-$_", $$options{$_} ? 'on' : 'off') if exists($$options{$_});
  }

  &wrapper('fs', \@args, [
    ['translator is disabled',  sub { $modes{'start'}     = 0 }],
    ['translator is enabled',   sub { $modes{'start'}     = 1 }],
    ['strict unix',             sub { $modes{'convert'}   = 0 }],
    ['convert owner',           sub { $modes{'convert'}   = 1 }],
    [q/no 'passwd sync'/,       sub { $modes{'uidcheck'}  = 0 }],
    [q/strict 'passwd sync'/,   sub { $modes{'uidcheck'}  = 1 }],
    ['Only mounts',             sub { $modes{'submounts'} = 0 }],
    ['Allow mounts',            sub { $modes{'submounts'} = 1 }],
  ]);
  %modes;
}


#: AFS_fs_getcacheparms()
#: Returns the size of the cache, and the amount of cache space used.
#: Sizes are returned in 1K blocks.
$AFS_Help{'fs_getcacheparms'} = 'void => ($size, $used)';
sub AFS_fs_getcacheparms {
  my(@args, $size, $used);

  @args = ('getcacheparms');
  &wrapper('fs', \@args, [
    [q/AFS using (\d+) of the cache's available (\d+) 1K byte blocks/,
     \$used, \$size],
  ]);
  ($size, $used);
}

#: AFS_fs_getcellstatus(\@cells)
#: Get cell control bits for cells listed in @cells.
#: On success, returns a hash mapping cells to their status; keys are
#: cell names, and values are 1 if SUID programs are permitted for that
#: cell, and 0 if not.
$AFS_Help{'fs_getcellstatus'} = '\@cells => %status';
sub AFS_fs_getcellstatus {
  my($cells) = @_;
  my(@args, %status);

  @args = ('getcellstatus', '-cell', @$cells);
  &wrapper('fs', \@args, [
    ['Cell (.*) status: setuid allowed',    sub { $status{$_[0]} = 1 }],
    ['Cell (.*) status: no setuid allowed', sub { $status{$_[0]} = 0 }],
  ]);
  %status;
}

#: AFS_fs_getclientaddrs
#: Returns a list of the client interface addresses
$AFS_Help{'fs_getclientaddrs'} = 'void => @addrs';
sub AFS_fs_getclientaddrs {
  my(@args, @addrs);

  @args = ('getclientaddrs');
  &wrapper('fs', \@args, [
    ['^(\d+\.\d+\.\d+\.\d+)', \@addrs ]
  ]);
  @addrs;
}

#: AFS_fs_getcrypt
#: Returns the cache manager encryption flag
$AFS_Help{'fs_getcrypt'} = 'void => $crypt';
sub AFS_fs_getcrypt {
  my(@args, $crypt);

  @args = ('getcrypt');
  &wrapper('fs', \@args, [
    ['^Security level is currently clear', sub { $crypt = 0 }],
    ['^Security level is currently crypt', sub { $crypt = 1 }],
  ]);
  $crypt;
}

#: AFS_fs_getserverprefs([$vlservers], [$numeric])
#: Get fileserver or vlserver preference ranks
#: If $vlservers is specified and nonzero, VLDB server ranks
#: are retrieved; otherwise fileserver ranks are retrieved.
#: If $numeric is specified and nonzero, servers are identified
#: by IP address instead of by hostname.
#: Returns a hash whose keys are server names or IP addresses, and
#: whose values are the ranks of those servers.
$AFS_Help{'fs_getserverprefs'} = '[$vlservers], [$numeric] => %prefs';
sub AFS_fs_getserverprefs {
  my($vlservers, $numeric) = @_;
  my(@args, %prefs);

  @args = ('getserverprefs');
  push(@args, '-numeric')   if $numeric;
  push(@args, '-vlservers') if $vlservers;
  &wrapper('fs', \@args, [
    ['^(\S+)\s*(\d+)', \%prefs],
  ]);
  %prefs;
}

#: AFS_fs_listcells([$numeric')
#: Get a list of cells known to the cache manager, and the VLDB
#: servers for each cell.
#: If $numeric is specified and nonzero, VLDB servers are identified
#: by IP address instead of by hostname.
#: Returns a hash where each key is a cell name, and each value is
#: a list of VLDB servers for the corresponding cell.
$AFS_Help{'fs_listcells'} = '[$numeric] => %cells';
sub AFS_fs_listcells {
  my($numeric) = @_;
  my(@args, %cells);

  @args = ('listcells');
  push(@args, '-numeric') if $numeric;
  &wrapper('fs', \@args, [
    ['^Cell (\S+) on hosts (.*)\.',
      sub { $cells{$_[0]} = [ split(' ', $_[1]) ] }],
  ]);
  %cells;
}

#: AFS_fs_setmonitor($server)
#: Set the cache manager monitor host to $server.
#: If $server is 'off' or undefined, monitoring is disabled.
#: On success, return 1.
$AFS_Help{'fs_setmonitor'} = '$server => Success?';
sub AFS_fs_setmonitor {
  my($server) = @_;
  my(@args);

  @args = ('monitor', '-server', defined($server) ? $server : 'off');
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_getmonitor
#: Return the cache manager monitor host, or undef if monitoring is disabled.
$AFS_Help{'fs_getmonitor'} = 'void => $server';
sub AFS_fs_getmonitor {
  my(@args, $server);

  @args = ('monitor');
  &wrapper('fs', \@args, [
    ['Using host (.*) for monitor services\.', \$server],
  ]);
  $server;
}

#: AFS_fs_getsysname
#: Returns the current list of system type names
$AFS_Help{'fs_getsysname'} = 'void => @sys';
sub AFS_fs_getsysname {
  my(@args, @sys);

  @args = ('sysname');
  &wrapper('fs', \@args, [
    [q/Current sysname is '(.*)'/, \@sys],
    [q/Current sysname list is '(.*)'/,
      sub { push(@sys, split(q/' '/, $_[0])) }],
  ]);
  @sys;
}

#: AFS_fs_setsysname(\@sys)
#: Sets the system type list to @sys
#: On success, return 1.
$AFS_Help{'fs_setsysname'} = '$server => Success?';
sub AFS_fs_setsysname {
  my($sys) = @_;
  my(@args);

  @args = ('sysname', '-newsys', @$sys);
  &wrapper('fs', \@args);
  1;
}

#: AFS_fs_whichcell(\@paths)
#: Get the cells containing the specified paths
#: Returns a hash in which each key is a pathname, and each value
#: is the name of the cell which contains the corresponding file.
$AFS_Help{'fs_whichcell'} = '\@paths => %where';
sub AFS_fs_whichcell {
  my($paths) = @_;
  my(@args, %where);

  @args = ('whichcell', '-path', @$paths);
  &wrapper('fs', \@args, [
    [q/^File (.*) lives in cell '(.*)'/, \%where],
  ]);
  %where;
}

#: AFS_fs_wscell
#: Returns the name of the workstation's home cell
$AFS_Help{'fs_wscell'} = 'void => $cell';
sub AFS_fs_wscell {
  my(@args, $cell);

  @args = ('wscell');
  &wrapper('fs', \@args, [
    [q/^This workstation belongs to cell '(.*)'/, \$cell],
  ]);
  $cell;
}

