#
# This is probably horrific code to any Perl coder.  I'm sorry,
# I'm not one.  It runs.
#
# Proposed Coding Standard:
#
#     * Subroutines starting with test_ should be TAP tests
#       utilizing ok(), is(), etc... and return the number
#       of tests run if they get that far (could exit early
#       from a BAIL_OUT())
#
use File::Basename;
use Test::More;

sub check_command_binary {
    my $c = shift(@_);
    if (! -e "$c") {
	BAIL_OUT("Cannot find $c");
    }
}

# TAP test: test_command_man_pages
#
# Gather a list of a command's subcommands (like listvol for vos)
# by running a command with the "help" argument.  From that list
# of subcommands spit out, see if a man page exists for that
# command_subcommand
#
# Arguments: two scalars:
#
#                builddir : A path to the OpenAFS build directory,
#                           such as /tmp/1.4.14
#
#         fullpathcommand : The full path to the actual command's
#                           binary, such as /tmp/1.4.14/src/volser/vos
#
# Returns: the number of tests run
#
sub test_command_man_pages {
    my ($builddir, $fullpathcommand) = @_;

    my $command = basename($fullpathcommand);

    # build up our list of available commands from the help output
    open(HELPOUT, "$fullpathcommand help 2>&1 |") or BAIL_OUT("can't fork: $!");
    my @subcommlist;
    my @comm;
    while (<HELPOUT>) {
        # Skip the header thingy
        next if /Commands are/;
        @comm = split();
        push(@subcommlist, $comm[0]);
    }
    close HELPOUT;
    @subcommlist = sort(@subcommlist);

    # The following is because File::Find makes no sense to me
    # for this purpose, and actually seems totally misnamed
    my $found = 0;
    my $subcommand = "";
    my $frex = "";
    # Since we don't know what man section it might be in,
    # search all existing man page files for a filename match
    my @mandirglob = glob("$builddir/doc/man-pages/man[1-8]/*");
    # For every subcommand, see if command_subcommand.[1-8] exists
    # in our man page build dir.
    foreach (@subcommlist) {
        my $subcommand = $_;
        $found = 0;
        my $frex = $command . '_' . $subcommand . '.[1-8]';
        # diag("Looking for $frex");
        foreach my $x (@mandirglob) {
	    # diag("TRYING: $x");
	    $x = basename($x);
	    if ($x =~ /$frex$/) {
		# diag("FOUND");
		$found = 1;
		last;
	    }
	}
	$testcount = $testcount + 1;
	ok($found eq 1, "existence of man page for $command" . "_$subcommand");
    }
    return $testcount;
}
1;
