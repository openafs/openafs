# This is -*- perl -*-

package OpenAFS::Auth;
use strict;
use warnings;
use OpenAFS::Dirpath;
use OpenAFS::ConfigUtils;

my $path = $OpenAFS::Dirpath::openafsdirpath;

#
# Create an auth type for the specified Kerberos implementation.
#
# parameters:
#   type   -- Kerberos implementation: mit, heimdal, kaserver
#   keytab -- path and name of the keytab file for mit and heimdal
#   cell   -- cell name. if not specified, attempts to find the 
#               cell name in the ThisCell configuration file.
#   realm  -- realm name. if not specified, assume the realm name
#               is the same as the cell name, in uppercase.
#
# example:
#  my $auth = OpenAFS::Auth::create(
#               'type'=>'mit', 
#               'keytab'=>'/path/to/file/krb5.keytab');
#
#  $auth->authorize('admin');
#
sub create {
  my $self = {
     # default values
     'type' => 'MIT',
     'keytab' =>  "$path->{'afsconfdir'}/krb5.keytab",
     'cell' => '',
     'realm' => '', 
     'debug' => '0',
     # user specified values
     @_,
  };

  # check for supported kerberos type.
  my $type = $self->{'type'};
  $self->{'type'} = _check_kerberos_type($type) or 
    die "Unsupported kerberos type: $type\n";

  # create the sub-class for the kerberos type.
  my $class = "OpenAFS::Auth::$self->{'type'}";
  $self = bless($self, $class);

  # attempt get default values.
  unless ($self->{'cell'}) {
    eval {
      $self->{'cell'} = $self->_lookup_cell_name();
    }
  }
  unless ($self->{'realm'}) {
    if ($self->{'cell'}) {
      my $cell = $self->{'cell'};
      ($self->{'realm'} = $cell) =~ tr[a-z][A-Z];
    }
  }
  unless ($self->{'keytab'}) {
    $self->{'keytab'} = "$path->{'afsconfdir'}/krb5.keytab";
  }

  # kerberos type specific sanity checks.
  $self->_sanity_check();

  if ($self->debug) {
    print "debug: Auth::create()\n";
    foreach my $k (sort keys(%$self)) {
      print "debug:  $k => $self->{$k}\n";
    }
  }
  return $self;
}

#
# Check for supported kerberos type, and allow for case insensitivity.
#
sub _check_kerberos_type {
  my $type = shift;
  foreach my $supported ('MIT', 'Heimdal', 'Kaserver') {
     if ($type =~ /^$supported$/i) {
        return $supported;
     }
  }
  return undef;
}

#
# Returns the cell name from the ThisCell configuration file.
#
sub _lookup_cell_name {
  my $self = shift;
  my $cell;
  open(CELL, "$path->{'afsconfdir'}/ThisCell") 
    or die "error: Cannot open $path->{'afsconfdir'}/ThisCell: $!\n";
  $cell = <CELL>;
  chomp $cell;
  close CELL;
  return $cell;
}

#
# Placeholder for make_keyfile. Sub-classes should override.
#
sub make_keyfile {
  my $self = shift;
  return;
}

# 
# Make the krb.conf file if the realm name is different
# than the cell name. The syntax is something like,
#
#   UMICH.EDU
#   UMICH.EDU fear.ifs.umich.edu admin server
#   UMICH.EDU surprise.ifs.umich.edu
#   UMICH.EDU ruthless.ifs.umich.edu
#
sub make_krb_config {
  my $self = shift;
  my $cell = $self->{'cell'};
  my $realm = $self->{'realm'};

  if ($realm && $realm ne $cell) {
    unless ( -d $path->{'afsconfdir'} ) {
      die "error: OpenAFS configuration directory '$path->{'afsconfdir'}' is missing.\n";
    }
    unless ( -w $path->{'afsconfdir'} ) {
      die "error: Write access to the configuration directory '$path->{'afsconfdir'}' is required.\n";
    }
    print "debug: Making $path->{'afsconfdir'}/krb.conf file for realm $realm\n" if $self->{'debug'};
    open(KRB, "> $path->{'afsconfdir'}/krb.conf") or die "error: Failed to open $path->{'afsconfdir'}/krb.conf, $!\n";
    print KRB "$realm\n";
    close KRB;
  } 
}

#
# Enable/disable debug messages.
#
sub debug {
  my $self = shift;
  if (@_) {
    $self->{'debug'} = shift;
  }
  return $self->{'debug'};
}


#------------------------------------------------------------------------------------
# MIT Kerberos authorization commands.
#
package OpenAFS::Auth::MIT;
use strict;
use OpenAFS::Dirpath;
use OpenAFS::ConfigUtils;
our @ISA = ("OpenAFS::Auth");

#
# Sanity checks before we get started.
#
sub _sanity_check {
  my $self = shift;
  unless (defined $path->{'afssrvbindir'}) {
    die "error: \$path->{'afssrvbindir'} is not defined.\n";
  }
  unless (-f "$path->{'afssrvbindir'}/aklog") {
    die "error: $path->{'afssrvbindir'}/aklog not found.\n";
  }
  unless (-x "$path->{'afssrvbindir'}/aklog") {
    die "error: $path->{'afssrvbindir'}/aklog not executable.\n";
  }
  unless ($self->{'realm'}) {
    die "error: Missing realm parameter Auth::create().\n";
  }
  unless ($self->{'keytab'}) {
    die "error: Missing keytab parameter Auth::create().\n";
  }
  unless ( -f $self->{'keytab'} ) {
    die "error: Kerberos keytab file not found: $self->{'keytab'}\n";
  }
  unless ( -f $self->{'keytab'} ) {
    die "error: Keytab file not found: $self->{'keytab'}\n";
  }
}

#
# Create the KeyFile from the Kerberos keytab file. The keytab file
# should be created using the Kerberos kadmin command (or with the kadmin.local command
# as root on the KDC). See the OpenAFS asetkey man page for details.
# 
sub make_keyfile {
  my $self = shift;

  # asetkey annoyance. The current asetkey implementation requires the ThisCell and CellServDB files
  # to be present but they really are not needed to create the KeyFile. This check is done here
  # rather than in the _sanity_checks() because the ThisCell/CellServerDB are created later in 
  # the process of creating the new cell.
  unless ( -f "$path->{'afsconfdir'}/ThisCell" ) {
    die "error: OpenAFS configuration file is required, $path->{'afsconfdir'}/ThisCell\n";
  }
  unless ( -f "$path->{'afsconfdir'}/CellServDB" ) {
    die "error: OpenAFS configuration file is required, $path->{'afsconfdir'}/CellServDB\n";
  }

  unless ( -f "$path->{'afssrvbindir'}/asetkey" ) {
    die "error: $path->{'afssrvbindir'}/asetkey is missing.\nWas OpenAFS built with Kerberos support?\n";
  }
  unless ( -x "$path->{'afssrvbindir'}/asetkey" ) {
    die "error: Do not have execute permissions on $path->{'afssrvbindir'}/asetkey\n";
  }
  unless ( -d $path->{'afsconfdir'} ) {
    die "error: OpenAFS configuration directory '$path->{'afsconfdir'}' is missing.\n";
  }
  unless ( -w $path->{'afsconfdir'} ) {
    die "error: Write access to the OpenAFS configuration directory '$path->{'afsconfdir'}' is required.\n";
  }


  # Run klist to get the kvno of the afs key. Search for afs/cellname@REALM
  # then afs@REALM. klist must be in the path.
  my %keys = ();
  my $kvno;
  my $principal;
  my $afs_kvno;
  my $afs_principal;
  if ($self->debug) {
    print "debug: reading $self->{'keytab'} to find afs kvno\n";
  }
  open(KLIST, "klist -k $self->{'keytab'} |") or die "make_keyfile: Failed to run klist.";
  while (<KLIST>) {
    chomp;
    next if /^Keytab/;  # skip headers
    next if /^KVNO/;
    next if /^----/;
    ($kvno, $principal) = split;
    if ($self->debug) {
      print "debug:  kvno=$kvno principal=$principal\n";
    }
    $keys{$principal} = $kvno;
  }
  close KLIST;
  my $cell = $self->{'cell'};
  my $realm = $self->{'realm'};
  foreach my $principal ("afs/$cell\@$realm", "afs\@$realm") {
    if ($self->debug) {
      print "debug: searching for $principal\n";
    }
    if (defined $keys{$principal}) {
      $afs_principal = $principal;
      $afs_kvno = $keys{$afs_principal};
      if ($self->debug) {
         print "debug: found principal=$afs_principal kvno=$afs_kvno\n";
      }
      last;
    }
  }
  unless ($afs_kvno) {
    die "error: Could not find an afs key matching 'afs/$cell\@$realm' or ".
      "'afs/$cell' in keytab $self->{'keytab'}\n";
  }

  # Run asetkey on the keytab to create the KeyFile. asetkey must be in the PATH.
  run("$path->{'afssrvbindir'}/asetkey add $afs_kvno $self->{'keytab'} $afs_principal");
}

#
# Get kerberos ticket and AFS token for the user.
#
sub authorize {
  my $self = shift;
  my $principal = shift || 'admin';
  my $opt_aklog = "";
  $opt_aklog .= " -d" if $self->debug;

  run("kinit -k -t $self->{'keytab'} $principal");
  run("$path->{'afssrvbindir'}/aklog $opt_aklog");
  run("$path->{'afssrvbindir'}/tokens");
}


#------------------------------------------------------------------------------------
package OpenAFS::Auth::Heimdal;
use strict;
use OpenAFS::Dirpath;
use OpenAFS::ConfigUtils;
our @ISA = ("OpenAFS::Auth");

#
# Various checks during initialization.
#
sub _sanity_check {
  my $self = shift;
  unless ($self->{'realm'}) {
    die "Missing realm parameter Auth::create().\n";
  }
  unless ($self->{'keytab'}) {
    die "Missing keytab parameter Auth::create().\n";
  }
  unless ( -f $self->{'keytab'} ) {
    die "keytab file not found: $self->{'keytab'}\n";
  }
}

#
# Get kerberos ticket and AFS token for the user.
#
sub authorize {
  my $self = shift;
  my $principal = shift || 'admin';  
  run("kinit -k -t $self->{'keytab'} $principal\@$self->{'realm'} && afslog");
}

#------------------------------------------------------------------------------------
package OpenAFS::Auth::Kaserver;
use strict;
use OpenAFS::Dirpath;
use OpenAFS::ConfigUtils;
our @ISA = ("OpenAFS::Auth");

#
# Various checks during initialization.
#
sub _sanity_check {
  my $self = shift;
  unless ($self->{'realm'}) {
    die "Missing realm parameter Auth::create().\n";
  }
}

#
# Get kerberos ticket and AFS token for the user.
#
sub authorize {
  my $self = shift;
  my $principal = shift || 'admin';
  run("echo \"Proceeding w/o authentication\"|klog -pipe ${principal}\@$self->{'realm'}");
}

1;
