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
use afstest qw(src_path obj_path);

sub check_command_binary {
    my $c = shift(@_);
    if (! -e "$c") {
	BAIL_OUT("Cannot find $c");
    }
}

#
# Run the command help to determine the list of sub-commands.
#
sub lookup_sub_commands {
    my ($srcdir, $command) = @_;

    my $fullpathcommand = "$srcdir/$command";
    check_command_binary($fullpathcommand);

    # build up our list of available commands from the help output
    open(HELPOUT, "$fullpathcommand help 2>&1 |") or BAIL_OUT("can't fork: $!");
    my @subcommlist;
    my @comm;
    while (<HELPOUT>) {
        # Skip the header thingy
        next if /Commands are/;
        # Skip the version subcommand, it's always present but not interesting
        next if /^version/;
        @comm = split();
        push(@subcommlist, $comm[0]);
    }
    close HELPOUT;
    @subcommlist = sort(@subcommlist);
    return @subcommlist;
}

# TAP test: test_command_man_pages
#
# Test if a man page exists for each command sub-command.
# Runs one test per sub-command.
#
# Arguments:
#
#                  srcdir : A path to the OpenAFS source directory,
#                           such as /tmp/1.4.14
#
#                 command : the name of the command (e.g. vos)
#
#             subcommlist : a list of sub-commands for command
#
sub test_command_man_pages {
    my ($srcdir, $command, @subcommlist) = @_;

    # The following is because File::Find makes no sense to me
    # for this purpose, and actually seems totally misnamed
    my $found = 0;
    my $subcommand = "";
    my $frex = "";
    # Since we don't know what man section it might be in,
    # search all existing man page files for a filename match
    my @mandirglob = glob("$srcdir/doc/man-pages/man[1-8]/*");
    # For every subcommand, see if command_subcommand.[1-8] exists
    # in our man page source dir.
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
	ok($found eq 1, "existence of man page for $command" . "_$subcommand");
    }
}

#
# Setup the test plan and run all of the tests for the given command suite.
#
# Call like so:
# run_manpage_tests("src/ptserver", "pts");
#
sub run_manpage_tests($$) {
    my ($subdir, $command) = @_;

    my $srcdir = src_path();
    my $objdir = obj_path();

    my @sub_commands = lookup_sub_commands("$objdir/$subdir", $command);
    die("No subcommands found in $objdir/$subdir/$command?") unless(@sub_commands);

    plan tests => scalar @sub_commands;

    test_command_man_pages($srcdir, $command, @sub_commands);
}
1;
