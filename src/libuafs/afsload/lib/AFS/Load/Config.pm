package AFS::Load::Config;

=head1 NAME

AFS::Load::Config - afsload configuration file format

=head1 SYNOPSIS

  nodeconfig
    node * afsconfig "-fakestat -cachedir /tmp/afsload/cache.$RANK"
    node 0-15 logfile "/tmp/afsload/log.$RANK"
    node 16-* logfile /dev/null
  step
    node * chdir /afs/.localcell/afsload
  step
    node 0 creat foo "foo contents"
  step name "read newly created file"
    node * read foo "foo contents"
  step
    node 0 unlink foo

=head1 DESCRIPTION

The afsload scripts run certain operations on various OpenAFS userspace
client nodes, according to a test configuration. The general syntax of
this configuration is described here, but the documentation for
individual test actions are documented in AFS::Load::Action.

In general, keywords are composed of any characters besides whitespace
and quotes. Keywords are separated by whitespace, except when quoted,
and any duplicate whitespace is ignored. No interpolation or
preprocessing is done when reading the configuration file itself, though
individual actions or directives may perform some kind of interpolation
on the given arguments to the directive.

=head1 RANGES

Everything in the configuration can be specified to apply to all nodes,
some subset of the nodes, or a specific node. This is specified by
giving a range of node ranks that the configuration directive applies
to. This range can take one of the following forms:

=over 4

=item B<number>

A single number by itself only applies to a node with that rank.

=item B<number-number>

Two numbers separated by a hyphen applies to any node that has a rank
equal to either of those two numbers, or is between those two numbers.

=item B<*>

An asterisk applies to all nodes.

=item B<number-*>

A number followed by a hyphen and an asterisk applies to any node whose
rank is equal to the specified number or higher. You can think of this
as the same as the B<number-number> case, where the asterisk is treated
as an infinite number.

=item <range,range>

Any combination of the above range specifications can be specified,
separated by commas, and it will apply to any node to which any of the
supplied ranges apply.

=back

For example, a range of 0,4-7,10-* will apply to all nodes that have a
rank of 0, 4, 5, 6, 7, 10, and any rank higher than 10.

=head1 NODECONFIG

The first directive that should be specified is the 'nodeconfig'
directive, which defines the configuration for the various nodes. To
specify some configuration for some nodes, specify the 'node' directive,
followed by a range of node ranks, followed by the configuration
directive and any arguments:

  node <range> <directive> <arguments>

Right now only two directives can be given:

=over 4

=item B<afsconfig>

This specifies the arguments to give to the userspace client equivalent
of afsd. Specify this as a single string; so if you want to use multiple
arguments, you must quote the string and separate arguments by spaces.

The literal string $RANK is replaced with the numeric rank of the node,
anywhere the string $RANK appears in the config.

For example:

  node * afsconfig "-fakestat -cachedir /tmp/afsload/cache.$RANK"

will make all nodes turn on fakestat, and will use a cache directory in
/tmp/afsload/cache.$RANK. Note that the afsload scripts do not interpret
the given afsd-like parameters; they are just passed to libuafs. In
particular this means that you must create all of the given cache
directorie before running afsload, as libuafs/afsd does not create it
for you.

=item B<logfile>

This specifies where to direct output for this node. The userspace
client as well as perl itself may print some information or warnings,
etc to stdout or stderr. Since having all nodes print to the same stdout
can be unreadable, this allows you to specify a file for each node that
you can look at later if necessary.

The literal string $RANK is replaced with the numeric rank of the node,
anywhere the string $RANK appears in the given log file name. If this is
unspecified, it defaults to /dev/null, so all output from the node will
be lost.

=back

=head1 STEP

After the nodeconfig directives are specified, the rest of the
configuration consists of 'step' directives. Each step directive marks a
synchronization point between all running nodes; all nodes must complete
all previous actions before any node will proceed beyond a step
directive.

Each step is specified by just the directive "step" in the configuration
file. Each step may be given a name to make it easier to identify in the
test run output. To do this, just specify "step name myname" instead of
just "step".

In each step, you must specify a series of action directives that
dictate what each node does during each step. If you don't specify that
a node should do anything, that node just waits for the other nodes to
complete their actions.

Each action is specified like so

  node <range> <action> <action arguments>

Where the action and action arguments are documented in
AFS::Load::Action, for all defined actions.

All actions on different nodes between step directives are performed in
parallel, with no guarantee on the ordering in which they occur. If you
specify multiple actions for the same node between step directives,
those actions occur sequentially in the order they were specified. For
example:

  step
    node 0 creat file1 foo
    node 0 read file1 foo
    node 1-* read file2 bar

In this step, node 0 will create file1 and then read it. While that is
ocurring, all other nodes will read file2, which may occur before,
after, or during one of the other actions node 0 is performing.

=head1 AUTHORS

Andrew Deason E<lt>adeason@sinenomine.netE<gt>, Sine Nomine Associates.

=head1 COPYRIGHT

Copyright 2010-2011 Sine Nomine Associates.

=cut

use strict;
use Text::ParseWords qw(parse_line);

use AFS::Load::Action;

my @saw_nodes = ();
my $in_nodeconfig = 0;

sub _range_check($$) {
	my ($max, $word) = @_;
	if ($max == 0) {
		return;
	}
	foreach (split /,/, $word) {
		if (m/^(\d+)-(\d+|[*])$/) {
			# X-Y range
			my ($lo, $hi);
			$lo = int($1);

			if ($2 eq "*") {
				$hi = $max;
			} else {
				$hi = int($2);
			}

			if ($lo < 0 || $lo > $max || $hi < $lo || $hi > $max) {
				die("Invalid range $lo-$hi; you can only specify from 0 to $max, ".
				    "and the second range element must be greater than the first");
			}

			if (not $in_nodeconfig) {
				for (my $i = $lo; $i <= $hi; $i++) {
					$saw_nodes[$i] = 1;
				}
			}
		} elsif (m/^(\d+)$/) {
			# plain number
			my $n = int($1);
			if ($n < 0 || $n > $max) {
				die("Invalid node id $n; you can only specify from 0 to $max\n");
			}
			if (not $in_nodeconfig) {
				$saw_nodes[$n] = 1;
			}
		} elsif ($_ eq "*") {
			if (not $in_nodeconfig) {
				for (my $i = 0; $i <= $max; $i++) {
					$saw_nodes[$i] = 1;
				}
			}
		} else {
			die("unparseable range element $_");
		}
	}
}

sub _range_match($$) {
	my ($rank, $word) = @_;

	if ($rank < 0) {
		$rank *= -1;
		$rank--;
		_range_check($rank, $word);
		return 1;
	}

	foreach (split /,/, $word) {
		if (m/^(\d+)-(\d+|[*])$/) {
			# X-Y range
			my ($lo, $hi);
			$lo = int($1);
			if ($rank < $lo) {
				next;
			}
			if ($2 eq "*") {
				return 1;
			}
			$hi = int($2);
			if ($rank <= $hi) {
				return 1;
			}
		} elsif (m/^(\d+)$/) {
			# plain number
			if (int($1) == $rank) {
				return 1;
			}
		} elsif ($_ eq "*") {
			return 1;
		} else {
			die("unparseable range element $_");
		}
	}
	return 0;
}

sub _nextword($$) {
	my ($wordref, $iref) = @_;
	my $ret = undef;
	while (!defined($ret) and $$iref < scalar(@$wordref)) {
		$ret = $$wordref[$$iref];
		$$iref++;
	}
	return $ret;
}

sub check_conf($$) {
	my ($np, $conf_file) = @_;
	my $max;
	my $rank;
	my @steps;
	my %nodeconf;
	my $counter = 0;

	# subtract 2 from the number of processes, since node ids are 0-indexed,
	# and we need one process for the 'director' node
	$max = $np - 2;

	$rank = -1 * $max;
	$rank--;

	load_conf($rank, $conf_file, \@steps, \%nodeconf)
		or die("Error parsing configuration file\n");
	
	for (my $i = 0; $i <= $max; $i++) {
		if (not defined($saw_nodes[$i]) or !$saw_nodes[$i]) {
			$counter++;
			if ($counter > 5) {
				next;
			}
			print STDERR "# WARNING: node $i does not appear to have any\n";
			print STDERR "#          actions associated with it\n";
		}
	}
	if ($counter > 5) {
		print STDERR "# ... along with ".($counter-5)." other nodes\n";
	}
}

sub load_conf($$$$) {
	my ($rank, $conf_file, $stepsref, $nodeconfref) = @_;
	my $conf_h;
	my $conf;

	open($conf_h, "<$conf_file") or die("Cannot open $conf_file: $!\n");
	{
		local $/;
		$conf = <$conf_h>;
	}
	close($conf_h);

	my @words = parse_line(qr/\s+/, 0, $conf);
	push(@words, "step");
	my @actwords = ();
	my @acts = ();
	my $didRange = 0;
	my $ignore = 0;

	my $i = 0;

	while ($i < scalar @words) {
		my $word;
		$word = _nextword(\@words, \$i);
		if (not defined($word)) {
			next;
		}
		if ($word eq "nodeconfig") {
			$in_nodeconfig = 1;

			# keep going until we see a "step"
			while ($i < scalar @words && $words[$i] ne "step") {
				my ($key, $val);

				$word = _nextword(\@words, \$i);
				if ($word ne "node") {
					die("Expected nodeconfig/node, got nodeconfig/$word");
				}

				$word = _nextword(\@words, \$i);
				if (!_range_match($rank, $word)) {
					# skip this 'node' directive
					while ($i < scalar @words) {
						# skip until we see the next 'node'
						$word = _nextword(\@words, \$i);
						if ($word eq "node" || $word eq "step") {
							$i--;
							last;
						}
					}
					next;
				}

				$key = _nextword(\@words, \$i);
				$val = _nextword(\@words, \$i);

				$$nodeconfref{$key} = $val;
			}

			$in_nodeconfig = 0;

		} elsif ($word eq "step") {
			my @acts = ();
			my $nAct = 0;
			my $name = undef;

			if (!($i < scalar @words)) {
				last;
			}

			if (defined($words[$i]) && $words[$i] eq "name") {
				$word = _nextword(\@words, \$i);
				$name = _nextword(\@words, \$i);
			}

			# keep going until we see the next "step"
			while ($i < scalar @words && $words[$i] ne "step") {
				$word = _nextword(\@words, \$i);

				if ($word ne "node") {
					die("Expected step/node, got step/$word");
				}

				$word = _nextword(\@words, \$i);
				if (!_range_match($rank, $word)) {
					$nAct++;
					while ($i < scalar @words) {
						# skip until we see the next 'node'
						$word = _nextword(\@words, \$i);
						if ($word eq "node" || $word eq "step") {
							$i--;
							last;
						}
					}
					next;
				}

				my @actwords = ();

				while ($i < scalar @words) {
					$word = _nextword(\@words, \$i);
					if ($word eq "node" || $word eq "step") {
						$i--;
						last;
					}
					push(@actwords, $word);
				}

				my $act = AFS::Load::Action->parse($nAct, @actwords);
				push(@acts, \$act);
				$nAct++;
			}
			push(@$stepsref, [$name, @acts]);
		} else {
			die("Unknown top-level config directive '$word'\n");
		}
	}

	foreach my $key (keys %$nodeconfref) {
		$$nodeconfref{$key} =~ s/\$RANK/$rank/g;
	}

	return 1;
}

1;
