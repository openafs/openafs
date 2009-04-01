# CMUCS AFStools
# Copyright (c) 1996, 2001 Carnegie Mellon University
# All rights reserved.
#
# See CMU_copyright.ph for use and distribution information

package OpenAFS::wrapper;

=head1 NAME

OpenAFS::wrapper - AFS command wrapper

=head1 SYNOPSIS

  use OpenAFS::wrapper;
  %result = &wrapper($cmd, \@args, \@pspec, \%options);

=head1 DESCRIPTION

This module provides a generic wrapper for calling an external program and
parsing its output.  It is primarily intended for use by AFStools for calling
AFS commands, but is general enough to be used for running just about any
utility program.  The wrapper is implemented by a single function,
B<OpenAFS::wrapper::wrapper>, which takes several arguments:

=over 4

=item $cmd

The command to run.  This can be a full path, or it can be a simple command
name, in which case B<wrapper()> will find the binary on its internal path.

=item \@args

A reference to the list of arguments to be passed to the command.  Each
element of the list is passed as a single argument, as in B<exec()>.

=item \@pspec

A reference to the list describing how to parse the command's output.
See below for details.

=item \%options

A reference to a table of command execution and parsing options.

=back

On success, B<wrapper()> returns an associative array of data gathered
from the command's output.  The exact contents of this array are
caller-defined, and depend on the parsing instructions given.  On failure,
an exception will be thrown (using B<die>), describing the reason for the
failure.

The I<%options> table may be used to pass any or all of the following
options into B<wrapper()>, describing how the command should be executed
and its output parsed:

=over 4

=item pass_stderr

If specified and nonzero, the command's stderr will be passed directly
to the calling program's, instead of being parsed.  This is useful when
we want to process the command's output, but let the user see any
diagnostic output or error messages.

=item pass_stdout

If specified and nonzero, the command's stdout will be passed directly
to the calling program's, instead of being parsed.  This is useful when
the command being run produces diagnostic or error messages on stderr
that we want to parse, but provides bulk data on stdout that we don't
want to touch (e.g. B<vos dump> when the output file is stdout).

=item path

If specified, the path to be used for the program to execute, instead of
deriving it from the command name.  This is useful when we want the
command's argv[0] (which is always I<$cmd>) to be different from the
path to the program.

=item errors_last

If specified and nonzero, the built-in instructions for catching errors
from the command will be added to the end of the instructions in @pspec
instead of to the beginning.

=back

=head1 PARSING COMMAND OUTPUT

The I<@pspec> list describes how to parse command output.  Each element
of the list acts like an "instruction" describing how to parse the command's
output.  As each line of output is received from the program, the parsing
instructions are run over that line in order.  This process continues for
every line of output until the program terminates, or the process is
aborted early by flow-control operators.

Each parsing instruction is a reference to a list, which consists of a
regular expression and a list of "actions".  As a line of output is
processed, it is compared to each instruction's regexp in turn.  Whenever
a match is found, the actions associated with that instruction are taken,
in order.  Each instruction's regexp may contain one or more parenthesized
subexpressions; generally, each "action" uses up one subexpression, but there
are some exceptions.  Due to the current design of B<wrapper()>, each regexp
must have at least one subexpression, even if it is not used.

The acceptable actions are listed below, each followed by a number in brackets
indicating how many subexpressions are "used" by this action.  It is an error
if there are not enough subexpressions left to satisfy an action.  In the
following descriptions, I<$action> is the action itself (typically a string or
reference), I<$value> is the value of the subexpression that will be used, and
I<%result> is the result table that will be returned by B<wrapper> when the
command completes.

=over 4

=item string  [1]

Sets $result{$action} to $value.  Note that several specific strings have
special meaning, and more may be added in the future.  To ensure compatibility
with future versions of B<wrapper>, use only valid Perl identifiers as
"string" actions.

=item scalar ref  [1]

Sets $$action to $value.

=item list ref  [*]

Pushes the remaining subexpression values onto @$action.  This action uses
all remaining subexpression values.

=item hash ref  [2]

Sets $$action{$value0} to $value1.

=item code ref  [*]

Calls the referenced function, with all remaining subexpression values as
its arguments.  Any values returned by the function will be used to refill
the (now empty) subexpression value list, and thus may be used as arguments
by subsequent actions.  If only a few values are required, use a function
like this:

  sub usetwo {  # uses two values and preserves the rest
    my($val1, $val2, @rest) = @_;

    print STDOUT "Got $val1, $val2\n";
    @rest;
  }

=item '.'  [0]

End processing for this line of output, ignoring any remaining instructions.
Remaining actions in this instruction will be processed.

=item '+n'  [0]

Skip the next I<n> instructions.  This, along with the '.' action, can be
used to build simple flow-control constructs based on the contents of
lines of output.

=item '-x'  [0..1]

Signal an error after this instruction.  Remaining actions in this instruction
will be processed, but no further instructions will be processed for this
line, and no further lines of output will be processed.  If I<x> is given,
it will be used as a regexp to match against the B<previous> line of output,
and the first parenthesized subexpression resulting from that match will be
used as the error string.  Otherwise, one subexpression from the current
line will be used up as the error string.

=item '?'  [1]

Prints $value to STDOUT.

=back

=cut

use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT :afs_internal);
use Exporter;
use Symbol;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&wrapper);
@EXPORT_OK = qw(&wrapper &fast_wrapper);

sub wrapper {
  my($cmd, $args, $instrs, $options) = @_;
  my($prevline, $pid, $exception);
  my(@instrs, $instr, $action, @values, $path);
  local(%result);
  my(@werrinstrs) = ([ '^(wrapper\:.*)',       '-' ]);
  my(@cerrinstrs) = ([ '^(' . $cmd  . '\:.*)', '-' ],
                     [ '^(' . $path . '\:.*)', '-' ]);

  if ($options->{errors_last}) {
    @instrs = (@werrinstrs, @$instrs, @cerrinstrs);
  } else {
    @instrs = (@werrinstrs, @cerrinstrs, @$instrs);
  }

  if ($options->{path}) {
    $path = $options->{path};
  } elsif ($cmd =~ /^\//) {
    $path = $cmd;
  } else {
    $path = $AFScmd{$cmd};
  }

  if ($AFS_Trace{wrapper}) {
    print STDERR "Instructions:\n";
    foreach $instr (@$instrs) {
      print STDERR "  /", $instr->[0], "/\n";
      if ($AFS_Trace{wrapper} > 2) {
	my(@actions) = @$instr;
	shift(@actions);
	print "  => ",
	join(', ', map { ref($_) ? "<" . ref($_) . " reference>"
			   : $_ } @actions),
	  "\n";
      }
    }
  }

  ## Start the child
  if ($options->{pass_stdout}) {
    open(REALSTDOUT, ">&STDOUT");
  }
  $pid = open(AFSCMD, "-|");
  if (!defined($pid)) {
    die "wrapper: Fork failed for $cmd: $!\n";
  }

  ## Run the appropriate program
  if (!$pid) {

    if ($AFS_Trace{wrapper} > 1) {
      print STDERR "Command: $path ", join(' ', @$args), "\n";
    }

    open(STDERR, ">&STDOUT") if (!$options{pass_stderr});
    if ($options{pass_stdout}) {
      open(STDOUT, ">&REALSTDOUT");
      close(REALSTDOUT);
    }

    { exec($path $cmd, @$args); }
    # Need to be careful here - we might be doing "vos dump" to STDOUT
    if ($options{pass_stdout}) {
      print STDERR "wrapper: Exec failed for $cmd: $!\n";
    } else {
      print STDOUT "wrapper: Exec failed for $cmd: $!\n";
    }
    exit(127);
  }
  if ($options{pass_stdout}) {
    close(REALSTDOUT);
  }

  ## Now, parse the output
 line:
  while (<AFSCMD>) {
    my($skip) = 0;

    print STDERR $_ if ($AFS_Trace{wrapper} > 3);
    chop;

  instr:
    foreach $instr (@instrs) {
      my($dot, $action, @actions);

      if ($skip) {
	$skip--;
	next instr;
      }
      $dot = 0;
      if ($instr->[0]) {
	@values = ($_ =~ $instr->[0]);
	next instr if (!@values);
      } else {
	@values = ();
      }
      
    act:
      @actions = @$instr;
      shift(@actions);
      foreach $action (@actions) {
	if      (ref($action) eq 'SCALAR') {
	  if (@values) {
	    $$action = shift(@values);
	  } else {
	    last act;
	  }
	} elsif (ref($action) eq 'ARRAY') {
	  push(@$action, @values);
	  @values = ();
	} elsif (ref($action) eq 'HASH') {
	  if (@values > 1) {
	    $$action{$values[0]} = $values[1];
	    shift(@values); shift(@values);
	  } elsif (@values) {
	    $$action{shift @values} = '';
	    last act;
	  } else {
	    last act;
	  }
	} elsif (ref($action) eq 'CODE') {
	  @values = &$action(@values);
	} elsif (ref($action)) {
	  $exception = "Unknown reference to " . ref($action)
                     . "in parse instructions";
	  last line;
	} else {                  ## Must be a string!
	  if ($action eq '.') {
	    $dot = 1;
	  } elsif ($action =~ /\+(\d+)/) {
	    $skip = $1;
	  } elsif ($action =~ /-(.*)/) {
	    my($pat) = $1;

	    if ($pat && $prevline) {
	      ($exception) = ($prevline =~ $pat);
	    } elsif (@values) {
	      $exception = shift(@values);
	    } else {
	      $exception = $_;
	    }
	  } elsif ($action eq '?') {
	    print STDOUT (@values ? shift(@values) : $_), "\n";
	  } elsif (@values) {
	    $result{$action} = shift(@values);
	  } else {
	    last act;
	  }
	}
      }
      
      last line if ($exception);
      last instr if ($dot);
    }
    $prevline = $_;
  }
  close(AFSCMD);
  $exception .= "\n" if ($exception && $exception !~ /\n$/);
  die $exception if ($exception);
  %result;
}


## Generate code for a fast wrapper (see example below)
sub _fastwrap_gen {
  my($instrs, $refs) = @_;
  my($SRC, $N, $N1, $X, $instr, $pattern, @actions, $action);

  $N = $X = 0;
  $N1 = 1;

  $SRC = <<'#####';
sub {
  my($FD, $refs) = @_;
  my($prevline, @values, $skip, $exception);

  line: while (<$FD>) {
#####

  $SRC .= "    print STDERR \$_;\n" if ($AFS_Trace{'wrapper'} > 3);
  $SRC .= "    chop;\n";

  foreach $instr (@$instrs) {
    ($pattern, @actions) = (@$instr);
    $SRC .= ($pattern ? <<"#####" : <<"#####");

    instr_$N:
    die \$exception if \$exception;
    if (\$skip) { \$skip-- } else {
      \@values = (\$_ =~ /$pattern/);
      if (\@values) {
#####

    instr_$N:
    die \$exception if \$exception;
    if (\$skip) { \$skip-- } else {
      \@values = ();
      if (1) {
#####

    foreach $action (@actions) {
      if      (ref($action) eq 'SCALAR') {
        $refs[++$X] = $action;
        $SRC .= <<"#####";

        if (\@values) { \${\$refs[$X]} = shift (\@values) }
        else { goto instr_$N1 }
#####

      } elsif (ref($action) eq 'ARRAY') {
        $refs[++$X] = $action;
        $SRC .= <<"#####";

        push(\@{\$refs[$X]}, \@values);
        \@values = ();
#####

      } elsif (ref($action) eq 'HASH') {
        $refs[++$X] = $action;
        $SRC .= <<"#####";

        if (\@values > 1) {
          \$refs[$X]{\$values[0]} = shift(\$values[1]);
          shift(\@values); shift(\@values);
        } elsif (\@values) {
          \$refs[$X]{shift(\@values)} = '';
          goto instr_$N1;
        } else {
          goto instr_$N1;
        }
#####

      } elsif (ref($action) eq 'CODE') {
        $refs[++$X] = $action;
        $SRC .= "\n        \@values = \$refs[$X]->(\@values);\n";

      } elsif (ref($action)) {
        die "Unknown reference to " . ref($action) . "in parse instructions\n";

      } elsif ($action eq '.') {
        $SRC .= "\n        next line;\n";

      } elsif ($action eq '?') {
        $SRC .= <<"#####";

        if (\@values) { print STDOUT shift(\@values), "\\n" }
        else         { print STDOUT \$_, "\\n" }
#####

      } elsif ($action =~ /\+(\d+)/) {
        $SRC .= "\n        \$skip = $1;\n";

      } elsif ($action =~ /-(.*)/) {
        $SRC .= $1 ? <<"#####" : <<"#####";

        if (\$prevline)  { (\$exception) = (\$prevline =~ /$1/) }
        elsif (\@values) { \$exception = shift(\@values) }
        else            { \$exception = \$_ }
#####

        if (\@values)    { \$exception = shift(\@values) }
        else            { \$exception = \$_ }
#####

      } else {
        $SRC .= <<"#####";

        if (\@values) { \$result{"\Q$action\E"} = shift(\@values) }
        else { goto instr_$N1 }
#####
      }
    }

    $N++; $N1++;
    $SRC .= <<'#####';
      }
    }
#####
  }

  $SRC .= <<'#####';
  } continue {
    die $exception if $exception;
    $prevline = $_;
  }
}
#####

  $SRC;
}

####################### Example code #######################
# sub {
#   my($FD, $refs) = @_;
#   my($prevline, @values, $skip, $exception);
# 
#   line: while (<$FD>) {
#     print STDERR $_;     ## if ($AFS_Trace{'wrapper'} > 3);
#     chop;
# 
#     ## Following block repeated for each instruction
#     instr_N:
#     die $exception if $exception;
#     if ($skip) { $skip-- } else {
#       @values = ($_ =~ /## pattern ##/);    ## () if no pattern
#       if (@values) {                        ## 1 if no pattern
#         ## For each action, include one of the following blocks:
# 
#         ## SCALAR ref
#         if (@values) { ${$refs[X]} = shift (@values) }
#         else { goto instr_N+1 }
# 
#         ## ARRAY ref
#         push(@{$refs[X]}, @values);
#         @values = ();
# 
#         ## HASH ref
#         if (@values > 1) {
#           $refs[X]{shift(@values)} = shift(@values);
#         } elsif (@values) {
#           $refs[X]{shift(@values)} = '';
#           goto instr_N+1;
#         } else {
#           goto instr_N+1;
#         }
# 
#         ## CODE ref
#         @values = $refs[X]->(@values);
# 
#         ## string '.'
#         next line;
# 
#         ## string '?'
#         if (@values) { print STDOUT shift(@values), "\n" }
#         else         { print STDOUT $_, "\n" }
# 
#         ## string '+DDD'
#         $skip = DDD;
# 
#         ## string '-XXX'
#         if ($prevline)  { ($exception) = ($prefline =~ /XXX/) }
#         elsif (@values) { $exception = shift(@values) }
#         else            { $exception = $_ }
#         
#         ## string '-'
#         if (@values)    { $exception = shift(@values) }
#         else            { $exception = $_ }
# 
#         ## anything else
#         if (@values) { $result{XXX} = shift(@values) }
#         else { goto instr_N+1 }
#       }
#     }
# 
#   } continue {
#     die $exception if $exception;
#     $prevline = $_;
#   }
# }
############################################################


## The following does exactly the same thing as wrapper(),
## but should be considerably faster.  Instead of interpreting
## parsing instructions, it translates them into perl code,
## which is then compiled into the interpreter.  The chief
## benefit to this approach is that we no longer compile
## one RE per instruction per line of input.

sub fast_wrapper {
  my($cmd, $args, $instrs, $options) = @_;
  my(@instrs, $SRC, $CODE, $path, $pid, $refs, $FD, $exception);
  local(%result);
  my(@werrinstrs) = ([ '^(wrapper\:.*)',       '-' ]);
  my(@cerrinstrs) = ([ '^(' . $cmd  . '\:.*)', '-' ],
                     [ '^(' . $path . '\:.*)', '-' ]);

  $FD = gensym;
  $refs = [];
  if ($options->{errors_last}) {
    @instrs = (@werrinstrs, @$instrs, @cerrinstrs);
  } else {
    @instrs = (@werrinstrs, @cerrinstrs, @$instrs);
  }
  $SRC = _fastwrap_gen(\@instrs, $refs);
  $CODE = eval $SRC;

  if ($options->{path}) {
    $path = $options->{path};
  } elsif ($cmd =~ /^\//) {
    $path = $cmd;
  } else {
    $path = $AFScmd{$cmd};
  }

  if ($AFS_Trace{'wrapper'}) {
    print STDERR "Instructions:\n";
    foreach $instr (@$instrs) {
      print STDERR "  /", $instr->[0], "/\n";
      if ($AFS_Trace{'wrapper'} > 2) {
	my(@actions) = @$instr;
	shift(@actions);
	print "  => ",
	join(', ', map { ref($_) ? "<" . ref($_) . " reference>"
			   : $_ } @actions),
	  "\n";
      }
    }
  }

  if ($AFS_Trace{'wrapper'} > 2) { print STDERR "Input parse code:\n$SRC\n" }

  ## Start the child
  if ($options->{pass_stdout}) {
    open(REALSTDOUT, ">&STDOUT");
  }
  $pid = open($FD, "-|");
  if (!defined($pid)) {
    die "wrapper: Fork failed for $cmd: $!\n";
  }

  ## Run the appropriate program
  if (!$pid) {
    if ($AFS_Trace{'wrapper'} > 1) {
      print STDERR "Command: $path ", join(' ', @$args), "\n";
    }

    open(STDERR, ">&STDOUT") if (!$options{pass_stderr});
    if ($options{pass_stdout}) {
      open(STDOUT, ">&REALSTDOUT");
      close(REALSTDOUT);
    }

    { exec($path $cmd, @$args) }
    # Need to be careful here - we might be doing "vos dump" to STDOUT
    if ($options{pass_stdout}) {
      print STDERR "wrapper: Exec failed for $cmd: $!\n";
    } else {
      print STDOUT "wrapper: Exec failed for $cmd: $!\n";
    }
    exit(127);
  }
  if ($options{pass_stdout}) {
    close(REALSTDOUT);
  }

  ## Now, parse the output
  eval { $CODE->($FD, $refs) };
  $exception = $@;

  close($FD);

  $exception .= "\n" if ($exception && $exception !~ /\n$/);
  die $exception if ($exception);
  %result;
}


1;

=head1 EXAMPLES

The following set of instructions is used by B<wrapper> to detect errors
issued by the command, or by the child process spawned to invoke the command.
I<$cmd> is the name of the command to run, and I<$path> is the path to the
binary actually invoked.

  [ '^(wrapper\:.*)',       '-' ]
  [ '^(' . $cmd  . '\:.*)', '-' ]
  [ '^(' . $path . '\:.*)', '-' ]

The following instruction is added by the B<OpenAFS::vos> module to catch errors
generated by B<vos> commands, which often take the form of a generic error
message (Error in vos XXX command), with a description of the specific problem
on the preceeding line:

  [ 'Error in vos (.*) command', '-(.*)' ]

If the AFStools parameter I<vostrace> is nonzero, the following instruction
is added to force all lines of output to be copied to STDOUT.  Note that this
is different from specifying the I<pass_stdout> option, which would pass the
command's STDOUT directly to ours without parsing it.

  [ '', '?' ]

B<OpenAFS::vos::AFS_vos_listvldb> uses the following instructions to parse the
output of "vos listvldb".  This is a fairly complex example, which illustrates
many of the features of B<wrapper>.

  1  ['^(VLDB|Total) entries', '.']
  2  ['^(\S+)', sub {
       my(%vinfo) = %OpenAFS::wrapper::result;
       if ($vinfo{name}) {
         $vinfo{rosites} = [@rosites] if (@rosites);
         $vlist{$vinfo{name}} = \%vinfo;
         @rosites = ();
         %OpenAFS::wrapper::result = ();
       }
     }],
  3  ['^(\S+)',                       'name'   ],
  4  ['RWrite\:\s*(\d+)',             'rwid'   ],
  5  ['ROnly\:\s*(\d+)',              'roid'   ],
  6  ['Backup\:\s*(\d+)',             'bkid'   ],
  7  ['Volume is currently (LOCKED)', 'locked' ],
  8  ['server (\S+) partition /vicep(\S+) RW Site',       'rwserv', 'rwpart'],
  9  ['server (\S+) partition /vicep(\S+) RO Site',       sub {
       push(@rosites, [$_[0], $_[1]]);
     }],

Instruction 1 matchees the header and trailer lines printed out by B<vos>, and
terminates processing of those lines before instructions 2 and 3 have a chance
to match it.  This is a simple example of a conditional - the next two
instructions are used only if this one doesn't match.  If we wanted to consider
additional instructions even on lines that do match this one, we could place
them above this one, or use '+2' instead of '.', which would skip only the next
two instructions and allow remaining ones to be processed.

Instruction 2 matches the first line printed for each volume, stores away any
information that has been collected about the previous volume, and prepares for
the new one.  Besides being a good example of use of a code reference as an
action, this instruction also takes advantage of the fact that B<wrapper>'s
%result array is a dynamically-scoped variable, and so can be modified by code
referenced in parsing instructions.

The remaining instructions are fairly simple.  Instructions 3 through 8 use
simple strings to add information about the volume to %result.  Instruction 9
is a bit more complicated; it uses a function to add a server/partition pair
to the current volume's list of RO sites.

=head1 COPYRIGHT

The CMUCS AFStools, including this module are
Copyright (c) 1996, 2001 Carnegie Mellon University.  All rights reserved.
For use and redistribution information, see CMUCS/CMU_copyright.pm

=cut
