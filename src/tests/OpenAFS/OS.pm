# This is -*- perl -*-

package OpenAFS::OS;
use warnings;
use strict;
use OpenAFS::Dirpath;
use OpenAFS::ConfigUtils;

my $path = $OpenAFS::Dirpath::openafsdirpath;

#
# Create the named system object for OS specific init scripts 
# and commands.
#
sub create {
  my $self = {
    'debug'=>0,
    'ostype'=>$path->{'ostype'},
    @_,
  };

  my $class = _get_class($self->{'ostype'});
  $self = bless($self, $class);
  $self->{'commands'} = $self->get_commands();

  # Put the paths to the cache and afsd into the path
  # table. Assume legacy paths if the the viceetcdir is set to
  # the Transarc path.
  if ($path->{'viceetcdir'} eq '/usr/vice/etc') {
    # set in the makefile dest targets
    $path->{'cachedir'} = "/usr/vice"     unless $path->{'cachedir'};  
    $path->{'afsddir'}  = "/usr/vice/etc" unless $path->{'afsddir'};
  }
  else {
    # set in the makefile install targets
    $path->{'cachedir'} = "$path->{'localstatedir'}/openafs" unless $path->{'cachedir'};
    $path->{'afsddir'}  = "$path->{'afssrvsbindir'}"         unless $path->{'afsddir'};
  }  

  return $self;
}

# 
# _get_class(name) - Return the package name for the ostype
#
sub _get_class {
  my $type = shift;
  if ($type=~/linux/i) {
    return "OpenAFS::OS::Linux";
  }
  die "error: Unknow system type. Valid types are: linux\n";
}

#
# command(name [,params...]) - Return the command string or code reference.
#
sub command {
  my $self = shift;
  my $name = shift;
  my $cmd = $self->{'commands'}->{$name};
  unless (defined $cmd) {
    die "error: Unsupported command name $name for OS type $self->{'ostype'}\n";
  }
  # add parameters if present.
  if (scalar @_) {
    if (ref($cmd) eq 'CODE') {
      $cmd = sub { &$cmd(@_) };
    }
    else {
      $cmd = join(' ', ($cmd, @_));
    }
  }
  return $cmd;
}

#--------------------------------------------------------------
# Common unix style os commands.
package OpenAFS::OS::Unix;
use warnings;
use strict;
use OpenAFS::ConfigUtils;
use OpenAFS::Dirpath;
our @ISA = qw(OpenAFS::OS);

#
# remove(target) - recursive remove
#
sub remove {
  my $self = shift;
  my $target = shift;
  run("rm -rf $target");
}

#
# Start the server.
#
sub fileserver_start {
  my $self = shift;
  run("$path->{'afssrvsbindir'}/bosserver");
}

#
# Stop the server.
#
sub fileserver_stop {
  my $self = shift;
  my @bosserver_pids = $self->find_pids("bosserver");
  if (scalar @bosserver_pids) {
    # bosserver is running, try to shutdown with bos.
    eval {
      run("$path->{'afssrvbindir'}/bos shutdown localhost -localauth");
    };
    if ($@) {
      warn "WARNING: Shutdown command failed.\n";
    }
    # Now shutdown bosserver process itself. Kill all of them
    # in case there are remants.
    foreach my $pid (@bosserver_pids) {
      eval { run("kill $pid") };
    }
  }
}

#
# Restart the server.
#
sub fileserver_restart {
  my $self = shift;
  fileserver_stop();
  fileserver_start();
}

#
# Return a list of pids. 
# 
sub find_pids {
  my $self = shift;
  my $process = shift;
  my @pids = ();
  my $ps = "ps -e -o pid,cmd";
  if ($self->{'debug'}) {
    print("debug: searching for process $process\n");
  }
  open(PS, "$ps |") or die "Cannot run command: $ps: $!";
  while (<PS>) {
    chomp;
    my ($pid,$cmd) = split;
    if ($cmd=~/$process/) {
      if ($self->{'debug'}) {
        print("debug:  found $pid $cmd\n");
      }
      push(@pids, $pid);
    }
  }
  close PS;
  return @pids;
}

#--------------------------------------------------------------
package OpenAFS::OS::Linux;
use warnings;
use strict;
use OpenAFS::ConfigUtils;
use OpenAFS::Dirpath;
our @ISA = qw(OpenAFS::OS::Unix);

#
# OS-specific commands. Defer to init scripts where possible.
#
sub get_commands {
  my $self = shift;
  my $syscnf = "$path->{'initdir'}/testclient.conf";

  my $commands = {
    'client-start'         => "SYSCNF=$syscnf $path->{'initdir'}/afs.rc start",
    'client-stop'          => "SYSCNF=$syscnf $path->{'initdir'}/afs.rc stop",
    'client-restart'       => "SYSCNF=$syscnf $path->{'initdir'}/afs.rc restart",
    'client-forcestop'     => sub { $self->client_forcestop() },
    'fileserver-start'     => sub { $self->fileserver_start() },
    'fileserver-stop'      => sub { $self->fileserver_stop() },
    'fileserver-restart'   => sub { $self->fileserver_restart() },
    'remove'               => 'rm -rf',
  };
  return $commands;
} 

#
# Setup the init script configuration, including the install paths.
# Create the required directories for the client, /afs and the 
# cache directory.
#
# N.B.The cacheinfo file is created by the init script.
#
sub configure_client {
  my $self = shift;
  my $config = {
    # defaults
    'cachesize' => '50000',
    # custom
    @_,
  };
  
  my $debug = $self->{'debug'};
  my $syscnf = "$path->{'initdir'}/testclient.conf";

  open (SYSCNF, "> $syscnf") or
    die "error: Cannot open afs.rc configuration file $syscnf, $!\n";

  print "debug: creating afs.rc configuration file $syscnf\n" if $debug; 
  print SYSCNF <<"_SYSCNF_";
AFS_CLIENT=on
AFS_SERVER=off
ENABLE_AFSDB=off
ENABLE_DYNROOT=off
CACHESIZE=$config->{'cachesize'}
OPTIONS="-confdir $path->{'viceetcdir'}"
WAIT_FOR_SALVAGE=no
AFSDIR=/afs
CACHEDIR=$path->{'cachedir'}/cache
CACHEINFO=$path->{'viceetcdir'}/cacheinfo
VERBOSE=
AFS_POST_INIT=
AFSD=$path->{'afsddir'}/afsd
BOSSERVER=$path->{'afssrvsbindir'}/bosserver
BOS=$path->{'afssrvbindir'}/bos
KILLAFS=$path->{'viceetcdir'}/killafs
MODLOADDIR=$path->{'afskerneldir'} 
_SYSCNF_
  close SYSCNF;
  if ($debug) {
    if (open(SYSCNF, "< $syscnf")) {
      while (<SYSCNF>) {
        chomp; print "debug:  $_\n";
      }
      close SYSCNF;
    }
  }

  # Create a cache directory if none.
  unless ( -d "$path->{'cachedir'}/cache" ) {
    print "debug: making cache directory: $path->{'cachedir'}/cache\n" if $debug;
    system("mkdir -p $path->{'cachedir'}/cache");
    system("chmod 0700 $path->{'cachedir'}/cache"); 
  }

  # Create the local /afs directory on which the afs filespace will be mounted. 
  if ( ! -d "/afs" ) {
    print "debug: making the local /afs directory.\n" if $debug;
    system("mkdir /afs");
    system("chmod 0777 /afs");
  }
}

#
# Force the client to stop. The sequence is:
#    umount /afs
#    /usr/vice/etc/afsd -shutdown
#    rmmod openafs (or rmmod libafs)
#
sub client_forcestop {
  my $self = shift;
  print "debug: client forcestop\n" if $self->{'debug'};

  eval { 
    run("umount /afs");
    sleep 1;
  };
  eval { 
    run("$path->{'afsddir'}/afsd -shutdown");
  sleep 1;
  };

  # Remove openafs modules still loaded.
  my $mods = $self->list_modules();
  if ($mods->{'openafs'}) {
     eval { run("/sbin/rmmod openafs") };
  }
  if ($mods->{'libafs'}) {
     eval { run("/sbin/rmmod libafs") };
  }

  # Check.
  $mods = $self->list_modules();
  if ($mods->{'openafs'}) {
     print "warning: kernel module still loaded: openafs\n";
  }
  if ($mods->{'libafs'}) {
     print "warning: kernel module still loaded: libafs\n";
  }

  # remove stale lock set by init script.
  run("rm -f /var/lock/subsys/afs");  
}

#
# list_modules() - Returns a table of loaded module names.
#
sub list_modules {
  my $self = shift;
  my $mods = {};
  if (open(LSMOD, "/sbin/lsmod |")) {
    while(<LSMOD>) {
    chomp;
    my ($mod) = split;
    $mods->{$mod} = 1;
  }
    close LSMOD;
  }
  return $mods;
}

1;
