# CMUCS AFStools
# Copyright (c) 1996, Carnegie Mellon University
# All rights reserved.
#
# See CMU_copyright.ph for use and distribution information
#
#: * bos.pm - Wrappers around BOS commands (basic overseer server)
#: * This module provides wrappers around the various bosserver 
#: * commands, giving them a nice perl-based interface.  Someday, they might
#: * talk to the servers directly instead of using 'bos', but not anytime
#: * soon.
#:

package OpenAFS::bos;
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT :afs_internal);
use OpenAFS::wrapper;
use Exporter;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&AFS_bos_create        &AFS_bos_addhost
		&AFS_bos_addkey        &AFS_bos_adduser
		&AFS_bos_delete        &AFS_bos_exec
		&AFS_bos_getdate       &AFS_bos_getlog
		&AFS_bos_getrestart    &AFS_bos_install
		&AFS_bos_listhosts     &AFS_bos_listkeys
		&AFS_bos_listusers     &AFS_bos_prune
		&AFS_bos_removehost    &AFS_bos_removekey
		&AFS_bos_removeuser    &AFS_bos_restart
		&AFS_bos_salvage       &AFS_bos_setauth
		&AFS_bos_setcellname   &AFS_bos_setrestart
		&AFS_bos_shutdown      &AFS_bos_start
		&AFS_bos_startup       &AFS_bos_status
		&AFS_bos_stop          &AFS_bos_uninstall);

#: AFS_bos_addhost($server, $host, [$clone], [$cell])
#: Add a new database server host named $host to the database
#: on $server.
#: If $clone is specified, create an entry for a clone server.
#: On success, return 1.
#:
$AFS_Help{bos_addhost} = '$server, $host, [$clone], [$cell] => Success?';
sub AFS_bos_addhost {
  my($server, $host, $clone, $cell) = @_;
  my(@args);

  @args = ('addhost', '-server', $server, '-host', $host);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-clone') if ($clone);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_addkey($server, $key, $kvno, [$cell])
#: Add a key $key with key version number $kvno on server $server
#: On success, return 1.
#:
$AFS_Help{bos_addkey} = '$server, $key, $kvno, [$cell] => Success?';
sub AFS_bos_addkey {
  my($server, $key, $kvno, $cell) = @_;
  my(@args);

  @args = ('addkey', '-server', $server, '-key', $key, '-kvno', $kvno);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_adduser($server, \@user, [$cell])
#: Add users specified in @users to bosserver superuser list on $server.
#: On success, return 1.
#:
$AFS_Help{bos_adduser} = '$server, \@user, [$cell] => Success?';
sub AFS_bos_adduser {
  my($server, $user, $cell) = @_;
  my(@args);

  @args = ('adduser', '-server', $server, '-user', @$user);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_create($server, $instance, $type, \@cmd, [$cell])
#: Create a bnode with name $instance
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_create} = '$server, $instance, $type, \@cmd, [$cell] => Success?';
sub AFS_bos_create {
  my($server, $instance, $type, $cmd, $cell) = @_;
  my(@args);

  @args = ('create', '-server', $server, '-instance', $instance, '-type', 
	   $type, '-cmd', @$cmd);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_delete($server, $instance, [$cell])
#: Delete a bnode with name $instance
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_delete} = '$server, $instance, [$cell] => Success?';
sub AFS_bos_delete {
  my($server, $instance, $cell) = @_;
  my(@args);

  @args = ('delete', '-server', $server, '-instance', $instance);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_exec($server, $cmd, [$cell])
#: Exec a process on server $server
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_exec} = '$server, $cmd, [$cell] => Success?';
sub AFS_bos_exec {
  my($server, $cmd, $cell) = @_;
  my(@args);

  @args = ('exec', '-server', $server, '-cmd', $cmd);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_getdate($server, $file, [$cell])
#: Get the date for file $file from server $server 
#: On success, return ($exedate, $bakdate, $olddate).
#:
$AFS_Help{bos_getdate} = '$server, $file, [$cell] => ($exedate, $bakdate, $olddate)';
sub AFS_bos_getdate {
  my($server, $file, $cell) = @_;
  my(@args, $exedate, $bakdate, $olddate);

  @args = ('getdate', '-server', $server, '-file', $file);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args,
	   [[ 'dated (.*), (no )?\.BAK', \$exedate],
	    [ '\.BAK file dated (.*), (no )?\.OLD', \$bakdate],
	    [ '\.OLD file dated (.*)\.', \$olddate]]);
  ($exedate, $bakdate, $olddate);
}

#: AFS_bos_getlog($server, $file, [$cell])
#: Get log named $file from server $server 
#: On success, return 1.
#:
$AFS_Help{bos_getlog} = '$server, $file, [$cell] => Success?';
sub AFS_bos_getlog {
  my($server, $file, $cell) = @_;
  my(@args);

  @args = ('getlog', '-server', $server, '-file', $file);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args, 
	   [[ '^Fetching log file .*', '.']], { pass_stdout });
  1;
}

#: AFS_bos_getrestart($server, [$cell])
#: Get the restart time for server $server 
#: On success, return ($genrestart, $binrestart).
#:
$AFS_Help{bos_getrestart} = '$server, [$cell] => ($genrestart, $binrestart)';
sub AFS_bos_getrestart {
  my($server, $cell) = @_;
  my(@args, $genrestart, $binrestart);

  @args = ('getrestart', '-server', $server);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args,
	   [[ '^Server .* restarts at\s*(.*\S+)', \$genrestart],
	    [ '^Server .* restarts for new binaries at\s*(.*\S+)', \$binrestart]]);
  ($genrestart, $binrestart);
}

#: AFS_bos_install($server, \@files, [$dir], [$cell])
#: Install files in \@files on server $server in directory $dir
#: or the default directory.
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_install} = '$server, \@files, [$dir], [$cell] => Success?';
sub AFS_bos_install {
  my($server, $files, $dir, $cell) = @_;
  my(@args, $file);

  @args = ('install', '-server', $server, '-file', @$files);
  push(@args, '-dir', $dir)        if ($dir);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args, [[ 'bos: installed file .*', '.' ]],
	   { 'errors_last' => 1 });
  1;
}

#: AFS_bos_listhosts($server, [$cell])
#: Get host list on server $server.
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, an array of hosts with the first entry being the cellname.
#:
$AFS_Help{bos_listhosts} = '$server, [$cell] => @ret';
sub AFS_bos_listhosts {
  my($server, $cell) = @_;
  my(@args, @ret);

  @args = ('listhosts', '-server', $server);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args, 
	   [[ '^Cell name is (.*)', sub { 
	       push(@ret, $_[0]);
	   } ],
	    [ 'Host \S+ is (\S+)', sub {
		push(@ret, $_[0]);
	    } ]
	    ]);
  @ret;
}

#: AFS_bos_listkeys($server, [$showkey], [$cell])
#: Get key list on server $server.
#: The server name ($server) may be a hostname or IP address
#: If specified, $showkey indicates keys and not checksums should be shown.
#: If specified, work in $cell instead of the default cell.
#: On success, an array of hosts with the first entry being the cellname.
#:
$AFS_Help{bos_listkeys} = '$server, [$showkey], [$cell] => %ret';
sub AFS_bos_listkeys {
  my($server, $showkey, $cell) = @_;
  my(@args, %ret);

  @args = ('listkeys', '-server', $server);
  push(@args, '-showkey')          if ($showkey);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  %ret = &wrapper('bos', \@args, 
		  [[ '^key (\d+) has cksum (\d+)', sub {
		      my(%ret) = %OpenAFS::wrapper::result;
		      $ret{$_[0]} = $_[1];
		      %OpenAFS::wrapper::result = %ret;
		      } ],
		   [ '^key (\d+) is \'(\S+)\'', sub {
		      my(%ret) = %OpenAFS::wrapper::result;
                      $ret{$_[0]} = $_[1];
		      %OpenAFS::wrapper::result = %ret;
                      } ],
		   [ '^Keys last changed on\s*(.*\S+)', sub {
		      my(%ret) = %OpenAFS::wrapper::result;
		       $ret{'date'} = $_[0];
		      %OpenAFS::wrapper::result = %ret;
		      } ],
		   [ 'All done.', '.']]);
  %ret;
}

#: AFS_bos_listusers($server, [$cell])
#: Get superuser list on server $server.
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, an array of users.
#:
$AFS_Help{bos_listusers} = '$server, [$cell] => @ret';
sub AFS_bos_listusers {
  my($server, $cell) = @_;
  my(@args, @ret);

  @args = ('listusers', '-server', $server);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args, [[ '^SUsers are: (\S+)', sub { 
      push(@ret, split(' ',$_[0]));
  } ]]);
  @ret;
}

#: AFS_bos_prune($server, [$bak], [$old], [$core], [$all], [$cell])
#: Prune files on server $server
#: If $bak is specified, remove .BAK files
#: If $old is specified, remove .OLD files
#: If $core is specified, remove core files
#: If $all is specified, remove all junk files
#: On success, return 1.
#:
$AFS_Help{bos_prune} = '$server, [$bak], [$old], [$core], [$all], [$cell] => Success?';
sub AFS_bos_prune {
  my($server, $bak, $old, $core, $all, $cell) = @_;
  my(@args);

  @args = ('prune', '-server', $server, '-bak', $bak, '-old', $old, '-core', $core, '-all', $all);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-bak') if ($bak);
  push(@args, '-old') if ($old);
  push(@args, '-core') if ($core);
  push(@args, '-all') if ($all);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_removehost($server, $host, [$cell])
#: Remove a new database server host named $host from the database
#: on $server.
#: On success, return 1.
#:
$AFS_Help{bos_removehost} = '$server, $host, [$cell] => Success?';
sub AFS_bos_removehost {
  my($server, $host, $cell) = @_;
  my(@args);

  @args = ('removehost', '-server', $server, '-host', $host);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_removekey($server, $kvno, [$cell])
#: Remove a key with key version number $kvno on server $server
#: On success, return 1.
#:
$AFS_Help{bos_removekey} = '$server, $kvno, [$cell] => Success?';
sub AFS_bos_removekey {
  my($server, $kvno, $cell) = @_;
  my(@args);

  @args = ('removekey', '-server', $server, '-kvno', $kvno);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_removeuser($server, \@user, [$cell])
#: Remove users specified in @users to bosserver superuser list on $server.
#: On success, return 1.
#:
$AFS_Help{bos_removeuser} = '$server, \@user, [$cell] => Success?';
sub AFS_bos_removeuser {
  my($server, $user, $cell) = @_;
  my(@args);

  @args = ('removeuser', '-server', $server, '-user', @$user);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_restart($server, [\@inst], [$bosserver], [$all], [$cell])
#: Restart bosserver instances specified in \@inst, or if $all is
#: specified, all instances.
#: If $bosserver is specified, restart the bosserver.
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_restart} = '$server, [\@inst], [$bosserver], [$all], [$cell] => Success?';
sub AFS_bos_restart {
  my($server, $inst, $bosserver, $all, $cell) = @_;
  my(@args);

  @args = ('restart', '-server', $server);
  push(@args, '-instance', @$inst) if ($inst);
  push(@args, '-bosserver')        if ($bosserver);
  push(@args, '-all')              if ($all);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_salvage($server, [$partition], [$volume], [$file], [$all], [$showlog], [$parallel], [$tmpdir], [$orphans], [$cell])
#: Invoke the salvager, providing a partition $partition if specified, and 
#: further a volume id $volume if specified. 
#: If specified, $file is a file to write the salvager output into.
#: If specified, $all indicates all partitions should be salvaged.
#: If specified, $showlog indicates the log should be displayed on completion.
#: If specified, $parallel indicates the number salvagers that should be run
#: in parallel.
#: If specified, $tmpdir indicates a directory in which to store temporary 
#: files.
#: If specified, $orphans indicates how to handle orphans in a volume
#: (valid options are ignore, remove and attach).
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_salvage} = '$server, [$partition], [$volume], [$file], [$all], [$showlog], [$parallel], [$tmpdir], [$orphans], [$cell] => Success?';
sub AFS_bos_salvage {
  my($server, $partition, $volume, $file, $all, $showlog, $parallel, $tmpdir, $orphans, $cell) = @_;
  my(@args);

  @args = ('salvage', '-server', $server);
  push(@args, '-partition', $partition)if ($partition);
  push(@args, '-volume', $volume)      if ($volume);
  push(@args, '-file', $file)      if ($file);
  push(@args, '-all')              if ($all);
  push(@args, '-showlog')          if ($showlog);
  push(@args, '-parallel', $parallel)  if ($parallel);
  push(@args, '-tmpdir', $tmpdir)  if ($tmpdir);
  push(@args, '-orphans', $orphans)if ($orphans);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args, [['bos: shutting down fs.', '.'],
			   ['Starting salvage.', '.'],
			   ['bos: waiting for salvage to complete.', '.'],
			   ['bos: salvage completed', '.'],
			   ['bos: restarting fs.', '.']],
	   { 'errors_last' => 1 });
  1;
}

#: AFS_bos_setauth($server, $authrequired, [$cell])
#: Set the authentication required flag for server $server to 
#: $authrequired.
#: On success, return 1.
#:
$AFS_Help{bos_setauth} = '$server, $authrequired, [$cell] => Success?';
sub AFS_bos_setauth {
  my($server, $authrequired, $cell) = @_;
  my(@args);

  @args = ('setauth', '-server', $server, '-authrequired', $authrequired);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_setcellname($server, $name, [$cell])
#: Set the cellname for server $server to $name
#: On success, return 1.
#:
$AFS_Help{bos_setcellname} = '$server, $name, [$cell] => Success?';
sub AFS_bos_setcellname {
  my($server, $name, $cell) = @_;
  my(@args);

  @args = ('setcellname', '-server', $server, '-name', $name);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_setrestart($server, $time, [$general], [$newbinary], [$cell])
#: Set the restart time for server $server to $time
#: If specified, $general indicates only the general restart time should be 
#: set.
#: If specified, $newbinary indicates only the binary restart time should be 
#: set.
#: On success, return 1.
#:
$AFS_Help{bos_setrestart} = '$server, $time, [$general], [$newbinary], [$cell] => Success?';
sub AFS_bos_setrestart {
  my($server, $time, $general, $newbinary, $cell) = @_;
  my(@args);

  @args = ('setrestart', '-server', $server, '-time', $time);
  push(@args, '-general')          if ($general);
  push(@args, '-newbinary')        if ($newbinary);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_shutdown($server, [\@inst], [$wait], [$cell])
#: Stop all bosserver instances or if \@inst is specified,
#: only those in \@inst on server $server 
#: waiting for them to stop if $wait is specified.
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_shutdown} = '$server, [\@inst], [$wait], [$cell] => Success?';
sub AFS_bos_shutdown {
  my($server, $inst, $wait, $cell) = @_;
  my(@args);

  @args = ('shutdown', '-server', $server);
  push(@args, '-instance', @$inst) if ($inst);
  push(@args, '-wait')             if ($wait);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_start($server, \@inst, [$cell])
#: Start bosserver instances in \@inst on server $server .
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_start} = '$server, \@inst, [$cell] => Success?';
sub AFS_bos_start {
  my($server, $inst, $cell) = @_;
  my(@args);

  @args = ('start', '-server', $server, '-instance', @$inst);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_startup($server, [\@inst], [$cell])
#: Start all bosserver instances or if \@inst is specified, only
#: those in \@inst on server $server .
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_startup} = '$server, [\@inst], [$cell] => Success?';
sub AFS_bos_startup {
  my($server, $inst, $cell) = @_;
  my(@args);

  @args = ('startup', '-server', $server);
  push(@args, '-instance', @$inst) if ($inst);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_status($server, [\@bnodes], [$cell])
#: Get status for the specified bnodes on $server, or for all bnodes
#: if none are given.
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return an associative array whose keys are the names
#: of bnodes on the specified server, and each of whose values is
#: an associative array describing the status of the corresponding
#: bnode, containing some or all of the following elements:
#: - name         Name of this bnode (same as key)
#: - type         Type of bnode (simple, cron, fs)
#: - status       Basic status
#: - aux_status   Auxillary status string, for bnode types that provide it
#: - num_starts   Number of process starts
#: - last_start   Time of last process start
#: - last_exit    Time of last exit
#: - last_error   Time of last error exit
#: - error_code   Exit code from last error exit
#: - error_signal Signal from last error exit
#: - commands     Ref to list of commands
#:
$AFS_Help{bos_status} = '$server, [\@bnodes], [$cell] => %bnodes';
sub AFS_bos_status {
  my($server, $bnodes, $cell) = @_;
  my(@args, %finres, %blist, @cmds);

  @args = ('status', '-server', $server, '-long');
  push(@args, '-instance', @$bnodes) if ($bnodes);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  %finres = &wrapper('bos', \@args,
           [['^(Instance)', sub {
              my(%binfo) = %OpenAFS::wrapper::result;

              if ($binfo{name}) {
                $binfo{commands} = [@cmds] if (@cmds);
                $blist{$binfo{name}} = \%binfo;

                @cmds = ();
                %OpenAFS::wrapper::result = ();
              }
            }],
            ['^Instance (.*), \(type is (\S+)\)\s*(.*)',            'name', 'type', 'status'   ],
            ['Auxilliary status is: (.*)\.',                        'aux_status'               ],
            ['Process last started at (.*) \((\d+) proc starts\)',  'last_start', 'num_starts' ],
            ['Last exit at (.*\S+)',                                'last_exit'                ],
            ['Last error exit at (.*),',                            'last_error'               ],
            ['by exiting with code (\d+)',                          'error_code'               ],
            ['due to signal (\d+)',                                 'error_signal'             ],
            [q/Command \d+ is '(.*)'/, sub { push(@cmds, $_[0]) }],
           ]);
  if ($finres{name}) {
    $finres{commands} = [@cmds] if (@cmds);
    $blist{$finres{name}} = \%finres;
  }
  %blist;
}

#: AFS_bos_stop($server, \@inst, [$wait], [$cell])
#: Stop bosserver instances in \@inst on server $server 
#: waiting for them to stop if $wait is specified.
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_stop} = '$server, \@inst, [$wait], [$cell] => Success?';
sub AFS_bos_stop {
  my($server, $inst, $wait, $cell) = @_;
  my(@args);

  @args = ('stop', '-server', $server, '-instance', @$inst);
  push(@args, '-wait')             if ($wait);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args);
  1;
}

#: AFS_bos_uninstall($server, \@files, [$dir], [$cell])
#: Uninstall files in \@files on server $server in directory $dir
#: or the default directory.
#: The server name ($server) may be a hostname or IP address
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{bos_uninstall} = '$server, \@files, [$dir], [$cell] => Success?';
sub AFS_bos_uninstall {
  my($server, $files, $dir, $cell) = @_;
  my(@args);

  @args = ('uninstall', '-server', $server, '-file', @$files);
  push(@args, '-dir', $dir) if ($dir);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-localauth')        if ($AFS_Parms{'authlvl'} == 2);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('bos', \@args, [[ '^bos: uninstalled file .*', '.' ]],
	   { 'errors_last' => 1 });
  1;
}

1;
