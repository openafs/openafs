#!/usr/bin/perl
$| = 1;
#
# Build the libktrace_tree by reading component list files in the src dir, and copying the
# listed files from the src and obj dirs into the libktrace_tree directory. 
#
# Dependent components are listed in file named "libktracedep" in each dir with dependencies.
#
use File::Find;
use File::Path;

my $quiet = 0;
my $showonly = 0;

while ( $_ = shift @ARGV )
{
	if (m/^-t/) { $treedir = testArg( shift @ARGV ); next; }
	if (m/^-p/) { $projdir = testArg( shift @ARGV ); next; }
	if (m/^-sn/) { $sysname = testArg( shift @ARGV ); next; }

	if (m/^-os/) { $ostype = testArg( shift @ARGV ); next; }
	elsif (m/^-o/) { $objdir  = testArg( shift @ARGV ); next; }

	if (m/^-q/) { $quiet = 1; next; }
	if (m/^-n/) { $showonly = 1; next; }
	usage;
}
if ( !$treedir || !$projdir || !$ostype || !$sysname)
{
	&usage;
	exit 1;
}
if ( !$objdir )
{
	$objdir = $projdir;
}

$quiet || print "libktrace_tree_dir = $treedir\n";
$quiet || print "top_proj_dir = $projdir\n";
$quiet || print "top_obj_dir = $objdir\n";
$quiet || print "\n";
my $qprojdir = quotemeta($projdir);

#
# Start with an empty dir
#
if ( -e $treedir && ! -d $treedir )
{
	die "Ick. Destination is not a directory and already exists!\n";
}
elsif ( -e $treedir )
{
	#$quiet || print "Cleaning up previous tree build:\n";
	#rmtree([$treedir],!$quiet,0);
	#$quiet || print "Done.\n\n";
}

#
# Find all the dep list files in the src dir
#
finddepth(\&find_libktracedep, $projdir);

#
# Manual actions
#
&copyit("$projdir/configure-libktrace", "$treedir/configure");
&copyit("$projdir/Makefile-libktrace.in", "$treedir/Makefile.in");

system("$objdir/src/config/config", 
	"$projdir/src/libktrace/MakefileProto.$ostype.in", 
	"$treedir/src/libktrace/Makefile.in",
	$sysname);

#
# Subs
#

sub find_libktracedep
{
	my ($fname) = $_;

	if ( $fname eq "libktracedep" )
	{
		&process_libktracedep($File::Find::dir, $fname);
	}
}

sub process_libktracedep
{
	my ($dir, $depname) = @_;
	my ($file);

	my $subdir = $dir;
	$subdir =~ s|^$qprojdir||o;
	$subdir =~ s|^/||gio;
	$subdir =~ s|/$||gio;

	print "# $dir/$depname\n";
	open(COMPS, "$depname");
	while ( defined($file = <COMPS>) )
	{
		my ($destdir, $proj_src,$obj_src);

		chomp($file);
		$file =~ s|#.*||gio;

		#
		# do some simple substitution in dep file
		#
		$file =~ s/MKAFS_OSTYPE/$ostype/ge;
		$file =~ s/AFS_SYSNAME/$sysname/ge;

		next if ( $file eq "" );

		$proj_src = "$projdir/$subdir/$file";
		$obj_src = "$objdir/$subdir/$file";
		$proj_src =~ s|//+|/|gio;
		$obj_src =~ s|//+|/|gio;

		if ( $file !~ /\*/ && $file !~ /\[/  ) # no globs in filename
		{
			my $fname = $file;
			$fname =~ s|.*/||gio;

			if ( -f $proj_src )
			{
				$destdir = mkfullpath($projdir, $proj_src, $treedir);		
				copyit($proj_src, "$destdir/$fname");
			}
	
			if ( $obj_src ne $proj_src && -f $obj_src)
			{
				$destdir = mkfullpath($objdir, $obj_src, $treedir);
				copyit($obj_src, "$destdir/$fname");
			}
		}
		else
		{
			#print "Globbing ($proj_src)\n";
			foreach my $src ( glob($proj_src) )
			{
				my $fname = $src;
				$fname =~ s|.*/||gio;

				$destdir = mkfullpath($projdir, $src, $treedir);		
				copyit($src, "$destdir/$fname");
			}
			#print "Globbing ($obj_src)\n";
			foreach my $src ( glob($obj_src) )
			{
				my $fname = $src;
				$fname =~ s|.*/||gio;

				$destdir = mkfullpath($objdir, $src, $treedir);
				copyit($src, "$destdir/$fname");
			}
		}
	}
	close(COMPS);
		
	$quiet || print "\n";
}

sub usage
{
	print "usage: $0 -p top_proj_dir -os mkafs_ostype -sn sysname\n";
	print "\t\t[-o top_obj_dir] [-t libktrace_tree_dir] [-h] [-q] [-n]\n";
	print "\ttop_proj_dir = /path/to/openafs - top level of openafs checkout/tarball\n";
	print "\ttop_obj_dir = /path/to/objdir - top level of build directory, defaults to top_proj_dir\n";
	print "\tlibktrace_tree_dir = /path/to/libktrace_tree - tree dir to create, defaults to top_obj_dir/libktrace_tree\n";
	print "\t-os = ostype value (i.e. LINUX)\n";
	print "\t-sn = afs sysname (i.e. i386_linux24)\n";
	print "\t-q = quiet build\n";
	print "\t-n = just show what will be done\n";
	print "\t-h = show this message\n";
	
	exit 1;
}

sub testArg
{
	my ($arg) = @_;
	return $arg if ( $arg && $arg ne "" );
	usage;
}

sub mkfullpath
{
	my ($frombase, $fromfile, $destbase) = @_;
	my $qfrombase = quotemeta $frombase;

	my $subdir = $fromfile;
	$subdir =~ s|^$qfrombase||;
	$subdir =~ s|^(.*)/(.*?)$|$1|o;
	$subdir =~ s|^/||gio;
	$subdir =~ s|/$||gio;
	$subdir =~ s|//+|/|gio;

	my $destdir = "$destbase/$subdir";
	$destdir =~ s|/$||gio;
	$destdir =~ s|//+|/|gio;

	$showonly || mkpath([$destdir], !$quiet, 0755);

	return $destdir;
}

sub copyit
{
	my ( $from, $to ) = @_;
	my (@from, @new);

	@from = stat($from);
	@to = stat($to);

	if ( ($#to < 0) || ($from[7] != $to[7]) || ($from[9] != $to[9]) )
	{
		$quiet || print "+ cp -p $from $to\n";
		$showonly || system( "cp", "-p", $from, $to );
	}
}


exit 0;
