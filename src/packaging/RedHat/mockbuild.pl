#!/usr/bin/perl
# mockbuild - mass RPM build driver for use with mock and many kernel-devel
#             RPMs
# from Simon Wilkinson for OpenAFS
# for "modern" yum, use "showdupesfromrepos=1" in all cfg files in /etc/mock

use strict;
use warnings;

use Getopt::Long;
use File::Path;
use IO::Dir;
use File::Temp qw/ tempfile tempdir /;

my $suser="nsu";
my $rootbase="/var/lib/mock/";
my $resultbase="/tmp/result/";
my $stashbase="/disk/scratch/repository/";
my $mockcommand = "/usr/bin/mock";
my $resultfile;
my $buildall = 0;
my $ignorerelease = 1;
my @newrpms;

sub findKernels {
  my ($root, $platform, $uname, $osv, @modules) = @_;
  $osv =~s/[^\d]//g;

  my ($fh, $tmpconf) = tempfile( "yum.confXXXX", DIR => "/tmp");
  open(OLDCONF, "$root/etc/yum.conf");
  while(<OLDCONF>) {
      $_ =~ s#/var/cache/yum#/var/cache/mock/${platform}/yum_cache#;
      print $fh $_;
  }
  close(OLDCONF);
  my $archv = "";
  open(PLATCONF, "/etc/mock/${platform}.cfg");
  while (<PLATCONF>) {
      if ($_ =~ "legal_host_arches") {
	  $_ =~ s/ //g;
	  $_ =~ /\(([\S]*)\)/;
	  $_ = $1;
	  if ($platform =~ "i386") {
	      $_ =~ s/\'x86_64\'//;
	      $_ =~ s/\,\,/\,/;
	      $_ =~ s/\,\$//;
	  }
	  if ($_ ne "") {
	      $archv="--archlist=$_";
	  }
      }
  }
  close(PLATCONF);
  my $modlist = join(" ", @modules);
  my @kernels;
  if ($uname) {
    @kernels = `repoquery $archv --releasever=$osv --whatprovides kernel-devel-uname-r --qf "%{name}.%{arch} %{version}-%{release}" -c $tmpconf`;
  } else {
    @kernels = `strace -o /tmp/out repoquery $archv --releasever=$osv --show-duplicates --whatprovides $modlist --qf "%{name}.%{arch} %{version}-%{release}" -c $tmpconf`;
  }
  unlink $tmpconf;

  return @kernels;
}


my %platconf = ( "centos-4-i386" => { osver => "el4",
				     kmod => '1',
				     basearch => 'i386',
				     updaterepo => 'update',
				     results => 'rhel4/i386' },
		 "centos-4-x86_64" => { osver => "el4",
				       kmod => '1',
				       basearch => 'x86_64',
				       updaterepo => 'update',
				       results => "rhel4/x86_64" },
		 "centos-5-i386" => { osver => "el5", 
				      kmod => '1', 
				      basearch => 'i386',
				      updaterepo => 'update',
				      results => "rhel5/i386" },
		 "centos-5-x86_64" => { osver => "el5",
					kmod => '1',
				   	basearch => 'x86_64',
				        updaterepo => 'update',
					results => "rhel5/x86_64" },
		 "centos-6-i386" => { osver => "el6", 
				      kmod => '1', 
				      basearch => 'i686',
				      updaterepo => 'update',
				      results => "rhel6/i686" },
		 "centos-6-x86_64" => { osver => "el6",
					kmod => '1',
				   	basearch => 'x86_64',
				        updaterepo => 'update',
					results => "rhel6/x86_64" },
		 "epel-4-i386" => { osver => "el4",
				     kmod => '1',
				     basearch => 'i386',
				     updaterepo => 'update',
				     results => 'rhel4/i386' },
		 "epel-4-x86_64" => { osver => "el4",
				       kmod => '1',
				       basearch => 'x86_64',
				       updaterepo => 'update',
				       results => "rhel4/x86_64" },
		 "epel-5-i386" => { osver => "el5", 
				      kmod => '1', 
				      basearch => 'i386',
				      updaterepo => 'update',
				      results => "rhel5/i386" },
		 "epel-5-x86_64" => { osver => "el5",
					kmod => '1',
				   	basearch => 'x86_64',
				        updaterepo => 'update',
					results => "rhel5/x86_64" },
		 "epel-6-i386" => { osver => "el6", 
				      kmod => '1', 
				      basearch => 'i686',
				      updaterepo => 'update',
				      results => "rhel6/i686" },
		 "epel-6-x86_64" => { osver => "el6",
					kmod => '1',
				   	basearch => 'x86_64',
				        updaterepo => 'update',
					results => "rhel6/x86_64" },
                 "fedora-14-i386" => { osver => "fc14",
                                        kmod => "1",
                                        basearch => "i686",
                                        results => "fedora-14/i686" },
                 "fedora-14-x86_64" => { osver => "fc14",
                                        kmod => "1",
                                        basearch => "x86_64",
                                        results => "fedora-14/x86_64" },
                 "fedora-15-i386" => { osver => "fc15",
                                        kmod => "1",
                                        basearch => "i686",
                                        results => "fedora-15/i686" },
                 "fedora-15-x86_64" => { osver => "fc15",
                                        kmod => "1",
                                        basearch => "x86_64",
                                        results => "fedora-15/x86_64" },
                 "fedora-16-i386" => { osver => "fc16",
                                        kmod => "1",
                                        basearch => "i686",
                                        results => "fedora-16/i686" },
                 "fedora-16-x86_64" => { osver => "fc16",
                                        kmod => "1",
                                        basearch => "x86_64",
                                        results => "fedora-16/x86_64" },
                 "fedora-17-i386" => { osver => "fc17",
                                        kmod => "1",
                                        basearch => "i686",
                                        results => "fedora-16/i686" },
                 "fedora-17-x86_64" => { osver => "fc17",
                                        kmod => "1",
                                        basearch => "x86_64",
                                        results => "fedora-16/x86_64" },
		 "fedora-development-i386" => { osver => "fcd",
					  kmod => '1',
					  basearch => 'i386',
					  results => 'fedora-devel/i386'},
		 "fedora-development-x86_64" => { osver => "fcd",
					    kmod => '1',
					    basearch => 'x86_64',
					    results => 'fedora-devel/x86_64'} 
);

# The following are kernels that we can't successfully build modules against
# due to issues in the packaged kernel-devel RPM.

my %badkernels = (
	"2.6.21-2950.fc8" => { "xen" => 1}, # Missing build ID
);

my $help;
my $ok = GetOptions("resultdir=s" => \$resultbase,
		    "resultlist=s" => \$resultfile,
		    "help" => \$help);

my @platforms = @ARGV;
my $srpm = pop @platforms;

if (!$ok || $help || !$srpm || $#platforms==-1) {
  print "Usage: $0 [options] <platform> [<platform> [...]]  <srpm>\n";
  print "    Options are : \n";
  print "         --resultdir <dir>    Location to place output RPMS\n";
  print "\n";
  print "    Platform may be:\n";
  foreach ("all", sort(keys(%platconf))) { print "        ".$_."\n"; };
  exit(1);
}

my $oafsversion = `rpm -q --queryformat=%{VERSION} -p $srpm` or die $!;
chomp $oafsversion;
$oafsversion=~/([0-9]+)\.([0-9]+)\.([0-9]+)/;
my $major = $1;
my $minor = $2;
my $pathlevel = $3;

# OpenAFS SRPMs newer than 1.6.0 use the dist, rather than osvers variable

my $usedist = ($minor >= 6);

my $oafsrelease = `rpm -q --queryformat=%{RELEASE} -p $srpm` or die $!;
chomp $oafsrelease;

# Before we used the dist tag, the release variable of the srpm was 1.<release>
if (!$usedist) {
	$oafsrelease=~s/^[^\.]*\.(.*)$/$1/;
}
print "Release is $oafsrelease\n";

if ($platforms[0] eq "all" and $#platforms == 0) {
  @platforms = keys(%platconf);
}

foreach my $platform (@platforms) {
  print "*******************************************************************\n";
  print "Building for $platform\n";
  die "Unknown platform $platform" if !$platconf{$platform};
  my $osver = $platconf{$platform}{'osver'};
  my $root = $rootbase.$platform."/root";
  my $mockresults = $rootbase.$platform."/result";
  my $resultdir = $resultbase."/".$platconf{$platform}{'results'};
  my $basearch = $platconf{$platform}{'basearch'};
  my $rpmstashdir = $stashbase.$platconf{$platform}{'results'}."/";

  my $yumcachedir;
  if (exists($platconf{$platform}{'updaterepo'})) {
    $yumcachedir = $rootbase."cache/".$platform."/yum_cache/".
		   $platconf{$platform}{'updaterepo'}."/packages/";
  }

  File::Path::mkpath($resultdir, 0, 0755);

  if (system($mockcommand, "-r", $platform, "init") != 0) {
    die "Initialising the root failed: $?\n";
  }

  my %modulelist;

  print "-------------------------------------------------------------------\n";
  print "Finding available kernel modules\n";

  my $arbitraryversion = "";

  my @kernels;
  if ($platform=~/fedora-development/) {
    @kernels = findKernels($root, $platform, 0, $osver, "kernel-devel");
  } elsif ($platform=~/centos-4/) {
    @kernels = findKernels($root, $platform, 0, $osver, "kernel-devel", "kernel-smp-devel", 
				 "kernel-hugemem-devel", "kernel-xenU-devel");
  } else {
    @kernels = findKernels($root, $platform, 0, $osver, 'kernel-devel');
  }

  foreach my $kernel (@kernels) {
      chomp $kernel;
      my ($package, $version)=split(/\s+/, $kernel);
      my ($arch) = ($package=~/\.(.*)$/);
      my ($variant) = ($package=~/kernel-(.*)-devel/);
      $variant = "" if !defined($variant);
      next if ($package !~ /^kernel/);
      next if ($arch eq "noarch");
      next 
	  if (exists($badkernels{$version}) && ($badkernels{$version}{$variant}));
      next if ($variant =~/debug$/); # Fedora debug kernels are bad
      next if ($kernel !~ /$osver/ ); # fc15 kernel in fc14 repo?

      print "$arch : $variant : $version\n";
      $modulelist{$arch} ={} if !$modulelist{$arch};
      $modulelist{$arch}{$version} = {} if !$modulelist{$arch}{$version};
      $modulelist{$arch}{$version}{$variant} = 1;
      $arbitraryversion = $version;
  }

  if (!$arbitraryversion) {
    die "Unable to find a valid kernel-devel package for $platform\n";
  }

  print "-------------------------------------------------------------------\n";
  print "Building the userland RPMs\n";
  my @rpms = ('openafs', 'openafs-authlibs', 'openafs-authlibs-devel', 
	      'openafs-client', 'openafs-compat', 'openafs-debuginfo', 
	      'openafs-devel', 'openafs-docs', 'openafs-kernel-source', 
	      'openafs-kpasswd', 'openafs-krb5', 'openafs-server',
	      'dkms-openafs');
  my @missingrpms=();

  my $missing = 0;
  foreach my $rpm (@rpms) {
    my $rpmname;
    if ($usedist) {
	$rpmname = $rpm."-".$oafsversion."-".$oafsrelease.".".$osver.".".
		   $basearch.".rpm";
    } else {
	$rpmname = $rpm."-".$oafsversion."-".$osver.".".$oafsrelease.".".
		   $basearch.".rpm";
    }
    if (! -f $resultdir."/".$rpmname) {
      $missing++;
      print "$resultdir/$rpmname is missing!\n";
      push @missingrpms, $rpmname;
    }
  }
  if ($missing) {
    system($mockcommand." -r ".$platform." rebuild ".
		        ' --define "fedorakmod 1" '.
		        ' --define "kernvers '.$arbitraryversion.'" '.
		        ' --define "osvers '.$osver.'" '.
			' --define "dist .'.$osver.'" '.
		        ' --define "build_modules 0" '.
		        ' --define "build_userspace 1" '.
		        ' --define "build_authlibs 1" '.
		        $srpm) == 0
      or die "build failed with : $!\n";
    foreach my $rpmname (@missingrpms) {
      system("cp ".$mockresults."/".$rpmname." ".$resultdir) == 0
          or die "Copy failed with : $!\n";
      push @newrpms, $resultdir."/".$rpmname;
    }
  } else {
    print "All userland RPMs present for $platform. Skipping build\n";
  }

  print "-------------------------------------------------------------------\n";
  print "Building kernel modules\n";

  foreach my $arch (keys(%modulelist)) {
    foreach my $version (keys(%{$modulelist{$arch}})) {
      my $kversion = $version;
      $kversion=~s/-/_/g;
      my @tobuild;

      if ($buildall) {
	@tobuild = keys(%{$modulelist{$arch}{$version}});
      } else {
        foreach my $variant (keys(%{$modulelist{$arch}{$version}})) {
          my $dvariant=$variant;
          $dvariant.="-" if ($dvariant);
          if (!-f $resultdir."/kmod-openafs-".$dvariant.
		  $oafsversion."-".$oafsrelease.".".$kversion.".".
		  $arch.".rpm") {
	    my @done = glob ($resultdir."/kmod-openafs-".$dvariant.
			     $oafsversion."-*.".$kversion.".".$arch.".rpm");

	    if ($ignorerelease && $#done>=0) {
	      print "Kernel module for $kversion already exists for an".
	            "older release. Skipping building it this time.\n";
	    } else {
	      push @tobuild, $variant;
	      print $resultdir."/kmod-openafs-".$dvariant.
                    $oafsversion."-".$oafsrelease.".".$kversion.".".
                    $arch.".rpm is missing\n";
	    }
          }
        }
      }

      if ($#tobuild>=0) {
        my $variants = join(" ", map {$_ or '\\\\\"\\\\\"'} @tobuild);
        print "Building $arch module for kernel $version with variants $variants\n";
        system ("setarch $arch $mockcommand -r $platform rebuild ".
			     " --arch ".$arch.
			     ' --define "fedorakmod 1" '.
			     ' --define "osvers '.$osver.'" '.
			     ' --define "dist .'.$osver.'" '.
			     ' --define "kernvers '.$version.'" '.
			     ' --define "kvariants '.$variants.'" '.
			     ' --define "build_modules 1" '.
			     ' --define "build_userspace 0" '.
			     $srpm) == 0
          or die "Build failed with : $!\n";
        foreach my $variant (@tobuild) {
          if ($variant) {
            $variant.="-";
          }
          system("cp ".$mockresults."/kmod-openafs-".$variant.$oafsversion."-".$oafsrelease.".".$kversion.".".$arch.".rpm $resultdir") == 0
            or die "Copy failed with : $!\n";
	  push @newrpms, $resultdir."/kmod-openafs-".$variant.$oafsversion."-".$oafsrelease.".".$kversion.".".$arch.".rpm";
        }
      } else {
         print "All kernel modules already built for $version on $arch\n";
      }
    }
  }
  print "-------------------------------------------------------------------\n";
  print "Creating repository data\n";
  system ("cd $resultdir; createrepo .") == 0
    or die "Unable to build repository data\n";

  if ($yumcachedir) {
    print "-------------------------------------------------------------------\n";
    print "Stashing kernel-devel RPMs\n";
  
    my $changed;
    my $dirh = IO::Dir->new($yumcachedir);
    if (defined($dirh)) {
      my $file;
      while (defined($file = $dirh->read)) {
        if ( $file=~/^kernel.*devel/ &&
              -f $yumcachedir.$file && ! -f $rpmstashdir.$file) {
          print "Stashing $file for later use\n";
          system("cp ".$yumcachedir.$file." ".$rpmstashdir.$file) == 0
            or die "Copy failed with : $!\n";
          $changed++;
        }
      }
    }
 
    if ($changed) {
      print "Updating stash repodata\n";
      system ("cd $rpmstashdir; createrepo .") == 0
        or die "Unable to update RPM stash repository data\n";
    }
  }
}

print "=====================================================================\n";
print "All builds complete\nBuilt:\n";
print join("\n",@newrpms);
if (defined($resultfile)) {
  my $resultfh=new IO::File $resultfile, 'w';
  print $resultfh join("\n",@newrpms);
}

