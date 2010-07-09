#!/usr/bin/perl

use strict;
use warnings;

use English;
use Getopt::Long;
use File::Basename;
use File::Temp;
use File::Path;
use IO::File;
use IO::Pipe;
use Pod::Usage;
use Cwd;

# Import an external git repository into the OpenAFS tree, taking the path
# to a local clone of that repository, a file containing a list of mappings
# between that repository and the location in the OpenAFS one, and optionally
# a commit-ish

my $help;
my $man;
my $externalDir;
my $result = GetOptions("help|?" => \$help,
			"man" => \$man,
			"externaldir=s" => \$externalDir);
		
pod2usage(1) if $help;
pod2usage(-existatus => 0, -verbose =>2) if $man;

my $module = shift;
my $clonePath = shift;
my $commitish = shift;

pod2usage(2) if !defined($module) || !defined($clonePath);

if (!$commitish) {
  $commitish = "HEAD";
}

# Use the PROGRAM_NAME to work out where we should be importing to.
if (!$externalDir) {
  $externalDir = dirname(Cwd::abs_path($PROGRAM_NAME));
}

# Read in our mapping file
my %mapping;
my $fh = IO::File->new("$externalDir/$module-files")
  or die "Couldn't open mapping file : $!\n";
while (<$fh>) {
  next if /^\s#/;
  if (/^(.+)\s+(.+)$/) {
    $mapping{$1} = $2;
  } elsif (/\w+/) {
    die "Unrecognised line in mapping file : $_\n";
  }
}
undef $fh;

# Create the external directory, if it doesn't exist.
mkdir "$externalDir/$module" if (! -d "$externalDir/$module");

# Make ourselves a temporary directory
my $tempdir = File::Temp::tempdir(CLEANUP => 1);

# Write a list of all of the files that we're going to want out of the other
# repository in a format we can use with tar.
$fh = IO::File->new($tempdir."/filelist", "w")
  or die "Can't open temporary file list for writing\n";
foreach (sort keys(%mapping)) {
  $fh->print("source/".$_."\n");
}
undef $fh;

# Change directory to the root of the source repository
chdir $clonePath
  or die "Unable to change directory to $clonePath : $!\n";

# Figure out some better names for the commit object we're using
my $commitSha1 = `git rev-parse $commitish`;
my $commitDesc = `git describe $commitish`;
chomp $commitSha1;
chomp $commitDesc;

# Populate our temporary directory with the originals of everything that was
# listed in the mapping file
system("git archive --format=tar --prefix=source/ $commitish".
       "  | tar -x -C $tempdir -T $tempdir/filelist") == 0
 or die "git archive and tar failed : $!\n";

# change our CWD to the module directory - git ls-files seems to require this
chdir "$externalDir/$module"
  or die "Unable to change directory to $externalDir/$module : $!\n";

# Now we're about to start fiddling with local state. Make a note of where we
# were.

# Use git stash to preserve whatever state there may be in the current
# working tree. Sadly git stash returns a 0 exit status if there are no
# local changes, so we need to check for local changes first.

my $stashed;
if (system("git diff-index --quiet --cached HEAD --ignore-submodules") != 0 ||
    system("git diff-files --quiet --ignore-submodules") != 0) {
  if (system("git stash") != 0) {
    die "git stash failed with : $!\n";
  }
  $stashed = 1;
}

eval {
  # Use git-ls-files to get the list of currently committed files for the module
  my $lspipe = IO::Pipe->new();
  $lspipe->reader(qw(git ls-files));

  my %filesInTree;
  while(<$lspipe>) {
    chomp;
    $filesInTree{$_}++;
  }

  foreach my $source (sort keys(%mapping)) {
    if (-f "$tempdir/source/$source") {
      File::Path::make_path(File::Basename::dirname($mapping{$source}));
      system("cp $tempdir/source/$source ".
	     "   $externalDir/$module/".$mapping{$source}) == 0
         or die "Copy failed with $!\n";
      system("git add $externalDir/$module/".$mapping{$source}) == 0
         or die "git add failed with $!\n";
      delete $filesInTree{$mapping{$source}}
    } else {
      die "Couldn't find file $source in original tree\n";
    }
  }

  # Use git rm to delete everything that's committed that we don't have a
  # relacement for.
  foreach my $missing (keys(%filesInTree)) {
    system("git rm $missing") == 0
      or die "Couldn't git rm $missing : $!\n";
  }

  if (system("git status") == 0) {
    my $fh=IO::File->new("$tempdir/commit-msg", "w")
      or die "Unable to write commit message\n";
    $fh->print("Import of code from $module\n");
    $fh->print("\n");
    $fh->print("This commit updates the code imported from the external\n");
    $fh->print("$module git repository to their revision\n$commitSha1\n");
    $fh->print("which is described as $commitDesc\n");
    undef $fh;
    system("git commit -F $tempdir/commit-msg") == 0
      or die "Commit failed : $!\n";
  }
};

my $code = 0;

if ($@) {
  print STDERR "Import failed with $@\n";
  print STDERR "Attempting to reset back to where we were ...\n";
  system("git reset --hard HEAD") == 0
    or die "Unable to reset, sorry. You'll need to pick up the pieces\n";
  $code = 1;
} 

if ($stashed) {
  system("git stash pop") == 0
    or die "git stash pop failed with : $!\n";
}

exit $code;

__END__

=head1 NAME

import-external-git - Import bits of an external git repo to OpenAFS

=head1 SYNOPSIS

import-external-git [options] <module> <repository> [<commitish>]

  Options
    --help		brief help message
    --man		full documentation
    --externalDir	exact path to import into

=head1 DESCRIPTION

import-external-git imports selected files from an external git repository
into the OpenAFS src/external tree. For a given <module> it assumes that
src/external/<module>-files already exists, and contains a space separated
list of source and destination file names. <repository> should point to a
local clone of the external project's git repository, and <commitish> points
to an object within that tree. If <commitish> isn't specified, the current
branch HEAD of that repository is used.

=cut
