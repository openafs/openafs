#!/usr/bin/perl -w

use strict;

use Parallel::MPI::Simple;

use AFS::Load::Config;

my @steps = ();
my %nodeconf = (
	'logfile' => '/dev/null',
	'afsconfig' => '-cachedir cache.afsload.$RANK',
);

MPI_Init();

if ($#ARGV < 0) {
	print STDERR "Usage: $0 <testconfig.conf>\n";
	exit(1);
}
my $conf_file = $ARGV[0];

my $rank = MPI_Comm_rank(MPI_COMM_WORLD);
my $size = MPI_Comm_size(MPI_COMM_WORLD);

if ($size < 2) {
	die("We only have $size processes; we must have at least 2 for a\n".
	    "director and at least one client node\n");
}

# $rank-1, because the 'director' has rank 0, and node rank 1 is specified as
# "node 0" in the configuration file.
AFS::Load::Config::load_conf($rank-1, $conf_file, \@steps, \%nodeconf)
	or die("Error parsing configuration file\n");

if (scalar @steps < 1) {
	die("No steps defined in the test config; nothing to run?\n");
}

if ($rank == 0) {
	require Test::More;
	Test::More->import();

	Test::More::plan('tests', scalar @steps);

} else {

	open STDOUT, '>>', $nodeconf{'logfile'}
		or die("Error opening logfile ".$nodeconf{'logfile'}." for stdout\n");
	open STDERR, '>>', $nodeconf{'logfile'}
		or die("Error opening logfile ".$nodeconf{'logfile'}." for stderr\n");
	
	print "======= Starting node ".($rank-1)." at ".scalar(localtime())."\n\n";

	require AFS::ukernel;

	AFS::ukernel::uafs_Setup("/afs") and die("uafs_Setup: $!\n");
	AFS::ukernel::uafs_ParseArgs($nodeconf{'afsconfig'}) and die("uafs_ParseArgs: $!\n");
	AFS::ukernel::uafs_Run() and die("uafs_Run: $!\n");
}

# one-index the steps, since Test::More test numbers start at 1
my $nStep = 1;
my @allres;
for my $step (@steps) {
	my @acts = @$step;
	my $nAct = 1;
	my @res = ();

	my $name = shift @acts;
	if ($name) {
		$name = "Step $nStep: $name";
	} else {
		$name = "Step $nStep";
	}

	if ($rank > 0) {
		# rank 0 is the director; for all other nodes, run the actual
		# actions we're supposed to do
		for my $actref (@acts) {
			my $act = $$actref;
			my @stat;
			my $actstr = "unknown";

			eval { $actstr = $act->str(); };
			if (not $@) {
				eval { @stat = $act->do(); };
			}

			if ($@) {
				push(@res, [-1, $nAct, $actstr, -1, "Internal error: $@"]);
			} elsif ($stat[0]) {
				push(@res, [int($!), $nAct, $actstr, @stat]);
			}
			$nAct++;
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	# collect results from all nodes for this step
	@allres = MPI_Gather(\@res, 0, MPI_COMM_WORLD);

	if ($rank == 0) {
		my $tested = undef;
		my $i = -1;

		# first array element will be for rank 0, which is the director, which
		# will never have useful information
		shift @allres;

		foreach my $resref (@allres) {
			my @res = @$resref;
			$i++;
			if (scalar @res == 0) {
				next;
			}

			if (not $tested) {
				fail("$name");
				$tested = 1;
			}

			diag("node $i failed: ");

			foreach my $failref (@res) {
				my @fail = @$failref;
				diag("\tOn action $fail[1]: $fail[2]");
				diag("\t\terrno: $fail[0]");
				diag("\t\terror code: $fail[3]");
				if (length $fail[4] > 0) {
					diag("\t\terror string: $fail[4]");
				}
			}
		}

		if (not $tested) {
			pass("$name");
		}

		@allres = undef;
	}
	MPI_Barrier(MPI_COMM_WORLD);
	$nStep++;
}

if ($rank > 0) {
	AFS::ukernel::uafs_Shutdown();
}

MPI_Finalize();
