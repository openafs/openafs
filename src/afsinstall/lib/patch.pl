# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

package Patch;
@Functions = ("Substitution", "Insert", "Replace", "EOF");

# 
# PatchError(Msg)
# 
# This subroutine will print a message and exit.
# 
# Msg is the message to print
# 
sub PatchError {
  my ($msg) = @_;
  die $msg;
}

# 
# FileOpen(File)
#
# This subroutine will open the given File for reading and
# will open a second file with the name File.new for writing.
# The subroutine will save these newly-created subroutines in
# an associative array for Patch to use.
#
# This subroutine will cause the whole program to exit if it
# cannot open either file.
#

sub FileOpen {
    my($File) = @_;
    my $FH = "${File}_FH";
    my $FHOUT = "${FH}OUT";
    my $Error = (defined(&main::ErrorMsg)) ? \&main::ErrorMsg : \&PatchError;
    open($FH, "$File") || &$Error("Cannot open $File: $!");
    open($FHOUT, ">$File.new") || &$Error("Cannot open $File.new: $!");
    $FileHandles{$File} = [$FH, $FHOUT];
}

# 
# Verbose()
# 
# This subroutine will cause the subroutine Patch to be in verbose
# mode if called.
#

sub Verbose {
  $Verbose = 1;
}

#
# Debug()
#
# This subroutine will cause the subroutine Patch to be in verbose
# mode if called.
#

sub Debug {
  $Debug = 1;
}

#
# sub Patch(File, ActionArrayReference)
#     File = string 
#     ActionArrayReference = reference to an array of Actions
#     Actions = (RE, Function, SearchLines, NewLines)
#
# This subroutine will try to patch a given File by following
# the given Actions provided in the Actions array passed to it
# as a reference 
#
# The Action array reference passed to it contains references to
# the actual Actions to be taken.
#
# The Actions are implemented as an array as described above.
# RE is a flag to use Regular Expressions or not (1 or 0)
# Function is one of Substitution, Insert, Replace or EOF
# SearchLines is a newline-separated string of lines to search for.
# NewLines is a newline-separated string of lines which will be new
#  to the given file.
#
# ASUMPTIONS
#
# EOF:    The function will do nothing and return Success if the New lines are
#         present anywhere in the file.
# Returns 1 on Success, 0 otherwise
#

sub Patch {
  my ($File, $ActionList) = @_;	# the filename and list reference
  my ($status, $ActionRef, $FH, $tmp, $RE);
  local($NEWFH);
  my ($SearchPatternArrayRef, $NewPatternArrayRef, $ArrayRef);
  my $ReplacementMatchRef;
  local ($SearchBufferRef, $NewBufferRef);
  my ($SearchLines, $NewLines, @SearchArray, @NewArray, $Function);
  my (%NewIndex, %NewBuffer, %SearchIndex, %SearchBuffer);
  my (%NewMatch, %NewLinesPrinted, %ReplacementMatch);
  local ($NewIndexRef, $SearchIndexRef);
  local $CurrentLine = 0;
  my $Index = 0;
  my (@Done);
  my $Errors = 0;
  my $NextLoop;
  my $Redo = 0;

  # Check to see if the Action List is empty
  if ($#$ActionList == -1) {
    print "No Actions specified for $File\n";
    return 0;
  }

  # Check to see if the given file has already been opened with FileOpen
  if (!exists($FileHandles{$File})) {
    print "$File has not been previously opened with Patch::FileOpen\n";
    return 0;
  }
  
  print "\nAttempting to patch $File...\n" if ($Verbose);
  
  $ArrayRef = $FileHandles{$File};
  ($FH, $NEWFH) = @$ArrayRef;

  # Initialize the assoc. arrays which will be used to keep track of indices
  # and the previously found matches

  foreach $ActionRef (@$ActionList) {
    my (@SearchPatternArray, @NewPatternArray);
    $Done{$Index} = 0;

    $SearchIndex{$Index} = 0;
    $SearchBuffer{$Index} = [];
    $NewIndex{$Index} = 0; 
    $NewBuffer{$Index} = [];
    $NewLinesPrinted{$Index} = 0;
    &DebugPrint("SearchBuffer[$Index]= $SearchBuffer{$Index}");
    &DebugPrint("NewBuffer[$Index]= $NewBuffer{$Index}");

    # recreate the new Action Array as follows:
    # 0) Index 
    # 1) Function
    # 2) Reference to SearchPattern array
    # 3) Reference to NewPattern array
    # 4) NewLines string

    # remove the RE flag from the list
    $RE = shift @$ActionRef;

    # prepend the Index number to the front of the list
    unshift(@$ActionRef, $Index);

    if (!grep(/$$ActionRef[1]/, @Functions)) {
      Abort("Unknown function: $$ActionRef[1]");
      return 0;
    }
    if (!$RE && $$ActionRef[1] eq "Substitution") {
      Abort("The Substitution function requires a Regular Expression");
      return 0;
    }

    # Since we will be using split with a limit, we need to get rid of the 
    # extra new-line
    chomp $$ActionRef[2];
    
    @SearchArray = split(/\n/, $$ActionRef[2], length($$ActionRef[2]));
    if ($$ActionRef[1] ne "EOF" && $#SearchArray == -1) {
      Abort("Cannot have an empty Search parameter");
      return 0;
    }
    # The Search parameter for Substitution is limited to a single line
    if ($$ActionRef[1] eq "Substitution" && 
	$#SearchArray) {
      Abort("Cannot have a multi-line Search parameter with Substitution:");
      &PrintArray(\@SearchArray);
      return 0;
    }
    # delimit each character if we are not expecting regular expressions
    foreach $tmp (@SearchArray) {
      $tmp = quotemeta $tmp if (!$RE);
      push(@SearchPatternArray, $tmp);
    }
    $$ActionRef[2] = \@SearchPatternArray;
    
    chomp $$ActionRef[3];
    @NewArray = split(/\n/, $$ActionRef[3],length($$ActionRef[3]));
    if ($#NewArray == -1) {
      Abort("Cannot have an empty New parameter");
      return 0;
    }
    # The Replace parameter for Substitution is limited to a single line
    if ($$ActionRef[1] eq "Substitution" && 
	$#NewArray) {
      Abort("Cannot have a multi-line Replace parameter with Substitution:");
      &PrintArray(\@NewArray);
      return 0;
    }
    # delimit each character if we are not expecting regular expressions
    foreach $tmp (@NewArray) {
      $tmp = quotemeta $tmp if (!$RE);
      push(@NewPatternArray, $tmp);
    }
    splice(@$ActionRef, 3, 0, \@NewPatternArray);

    # Now we have to create a new string out of the Search and Replace(New)
    # parameters if the function is Substitution. This new string is used
    # to determine whether or not we have a file which has successfully
    # been substituted.
    if ($$ActionRef[1] eq "Substitution") {
      my $search = $$ActionRef[2][0];
      my $replace = $$ActionRef[3][0];
      my $new, $tmp, $end;
      my @matches;
      my $index = 0;
      # split the search string on the opening '(' if there are any
      my (@search) = split(/\(/, $search);
      foreach $tmp (@search) {
	# Check to see if the '(' was was preceeded by a '\' which indicates
	#  that a match was not being performed.
	# If so prepend the current string plus the '(', lost during the split
	#  to the next string.
	if (substr($tmp, -1, 1) eq "\\") {
	  $search[$index+1] = "$search[$index]($search[$index+1]";
	  next;
	}
	next if (!$index++);	# the first array value is before a match
	# Now we need to find the occurrence of the final ')' trying not
	#  to match a '\)'. Once this is found we can push the entire string
	#  that was orignally between the '()' into our matches buffer.
	my $end = $[;
	while (($end = index($tmp, ")", $end)) >= $[) {
	  if (substr($tmp, $end-1, 1) eq "\\") {
	  } else {
	    push(@matches, substr($tmp, 0, $end));
	  }
	  $end++;
	}
      }

      # if the sequence "#" is found then the intent was to place a # 
      # character in the replace portion as a comment in the string to
      # pacify the perl preprocessor. The quotes need to be stripped in the
      # ReplacementMatch string.
      $replace =~ s/"#"/#/;

      # Split the Replace line on the input '.' characters which are used to
      #  show how to combine the replace piece of the s/// function
      my (@new) = split(/\./, $replace);
      if (!$#new) {
	# We are not expecting to have any $# replacements since there were 
	#  no '.' characters in the string. In this case just use the actual
	#  string when trying to look for that new string.
	$ReplacementMatch{$Index} = shift @new;
      } else {
	# Go through the '.'-separated components of the string and replace
	# any occurrence of $# with the original search pattern.
	# Otherwise just save the original string.
	$index = 0;
	while ($tmp = shift @new) {
	  &DebugPrint("\t\t$tmp");
	  if ($tmp =~ /^\$\d+$/) {
	    &DebugPrint("\tFound a match");
	    $ReplacementMatch{$Index} .= $matches[$index];
	    $index++;
	  } else {
	    $ReplacementMatch{$Index} .= $tmp;
	  }
	}
	&DebugPrint("index=$index, matches = $#matches");
	if ($index != $#matches+1) {
	  &Abort("Substitution does not have matching matches");
	  return 0;
	}
      }
    }

    if ($Debug) {
      print "Index    = $$ActionRef[0]\n";
      print "Function = $$ActionRef[1]\n";
      print "Search   = ";
      &PrintArray($$ActionRef[2]);
      print "New      = ";
      &PrintArray($$ActionRef[3]);
      print "NewLines = $$ActionRef[4]\n";
      print "ReplacementMatch = $ReplacementMatch{$Index}\n";
      print "----------------------------\n";
    }
    $Index++;
  }	
  
 MAINLOOP: while (<$FH>) {
   if (!$Redo) {
     $CurrentLine++;
     $LineReferences{$CurrentLine} = 0;
   }
   $Redo = 0;
   &DebugPrint("$CurrentLine=>$_");
   $NextLoop = 0;
   chomp;			# get rid of newline character
   # go through each action on this line of the file
   foreach $ActionRef (@$ActionList) {
     ($Index, $Function, $SearchPatternArrayRef, $NewPatternArrayRef, 
      $NewLines) = @$ActionRef;
     # define references to asociative array values for easier use
     $NewIndexRef = \$NewIndex{$Index};
     $NewBufferRef = $NewBuffer{$Index};
     $SearchIndexRef = \$SearchIndex{$Index};
     $SearchBufferRef = $SearchBuffer{$Index};
     $NewMatchRef = \$NewMatch{$Index};
     $$NewMatchRef = 0;
     $NewLinesPrintedRef = \$NewLinesPrinted{$Index};
     $ReplacementMatchRef = \$ReplacementMatch{$Index};

     # if the function is a substitution and the current line matches the
     # Search pattern then try to perform the substitution and flag that
     # the action as done if successful
     if ($Function eq "Substitution" && 
	 /$$SearchPatternArrayRef[$$SearchIndexRef]/) {
       &DebugPrint("\tSubstitution");
       &VerbosePrint("Substituting line in $File...");
       $Done{$Index} |= s/$$SearchPatternArrayRef[$$SearchIndexRef]/"$$NewPatternArrayRef[$$NewIndexRef]"/ee;
     }

     # This is a look-ahead check to see if the "New" lines will be matched
     $$NewMatchRef = 1 if (($$NewIndexRef <= $#$NewPatternArrayRef) &&
			   /$$NewPatternArrayRef[$$NewIndexRef]/i);

     # "EOF" function has no use for the Search paramaters
     # see if the "Search" lines can be found
     if ($Function ne "EOF" && 
	 ($$SearchIndexRef <= $#$SearchPatternArrayRef) && 
	 /$$SearchPatternArrayRef[$$SearchIndexRef]/) {
       &DebugPrint("Search");
       # print the "New" buffer and clear the indices if we were previsouly 
       # tracking the "New" lines and we no longer are
       &ResetNew(1) if $$NewIndexRef && !$$NewMatchRef;
       # see if all of the "Search" lines been found
       if ($$SearchIndexRef == $#$SearchPatternArrayRef) {
	 if ($Function eq "Insert") {
	   # reset the search indices and print the buffer and current line
	   &ResetSearch(1);
	   print $NEWFH "$_\n";
	   &DebugPrint("\tSearch Printed: $_");
	 } else {
	   # just reset the search indices
	   &ResetSearch(0);
	 }
	 # now that we are caught up with everything else, print the newlines
	 print $NEWFH "$NewLines\n";
	 &DebugPrint("\tSearch Printed: $NewLines");
	 # mark that the new lines have already been printed
	 $$NewLinesPrintedRef = 1 if ($$NewMatchRef && $Function ne "Insert");
	 $$NewLinesPrintedRef = 1 if ($Function eq "Insert");
	 &VerbosePrint ("Replacing line[s] in $File...");
	 $Done{$Index} = 1;	#  flag that this Action is done
       } else {
	 # add this line to the buffer and increment the index to the next
	 # "Search" line
	 &AddToBuffer($SearchBufferRef, $_);
	 $$SearchIndexRef++;
       }
       # set flag so that we don't check for inconsistencies
       $NextLoop = 1;
     }
     # check to see if we already determined a "New" match
     if ($$NewMatchRef) {
       &DebugPrint("New");
       # clean the "Search" buffer if it is partial and the $NextLine was not
       # set before. If it was set, then search is still active
       &ResetSearch(1) if ($$SearchIndexRef && !$NextLoop);
       &DebugPrint("$NextLoop and $$NewLinesPrintedRef");
       # see if all of the "New" lines have been found
       if ($$NewIndexRef == $#$NewPatternArrayRef) {
	 # reset the Search indices but do not print the buffer
	 &ResetSearch(0);
	 # print the buffered lines aw sell as the current line unless they
	 # were already printed by "Search"; reset the indices
	 if (!$NextLoop && !$$NewLinesPrintedRef) {
	   &ResetNew(1);
	   print $NEWFH "$_\n";
	   &DebugPrint("\tNew printed $_");
	 } else {
	   # the lines were previously printed; just reset everything
	   &ResetNew(0);
	 }
	 $Done{$Index} = 1;	# flag that this Action is done
       } else {
	 # add the current line to the New buffer unless this line has
	 # already been printed
	 &AddToBuffer($NewBufferRef, $_) if (!$$NewLinesPrintedRef);
	 $$NewIndexRef++;
       }
       # set flag so that we don't check for inconsistencies
       $NextLoop = 1;
     }

     # If the substitution action hasn't already been successful during
     # this run and the current line matches the replacement line
     # then mark the action as done
     if (!$Done{$Index} && $Function eq "Substitution" && 
	 /$$ReplacementMatchRef/) {
       &DebugPrint("\tFound the ReplacementMatch");
       $Done{$Index} = 1;	# flag that this Action is done
     }       

     # if this point is reached and the NewIndex is not at zero, then some but
     # not all of the New lines were found.
     # skip this block if Search or New already succeeded
     if (!$NextLoop && $$NewIndexRef) {
       &DebugPrint("New check");
       &ResetNew(1);
       $Redo = 1;
       redo MAINLOOP;
     }

     # if this point is reached and the SearchIndex is not at zero, then some 
     # but not all of the Search lines were found 
     # skip this block if Search or New already succeeded
     if (!$NextLoop && $$SearchIndexRef) {
       &DebugPrint("Search check");
       &ResetSearch(!$Done{$Index});
       $Redo = 1;
       redo MAINLOOP;
     }

     # print the new lines at the end of the file if the function is "EOF", the
     # next read on the file will be EOF and the new lines are not already
     # present at the end of the file
     if (!$NextLoop && $Function eq "EOF" && eof($FH) && !$Done{$Index}) {
       &DebugPrint("EOF");
       print $NEWFH "$_\n";
       print $NEWFH "$NewLines\n";
       &DebugPrint("\tEOF Printed: $_\n$NewLines");
       &VerbosePrint("Appending new line[s] to end of $File...");
       $Done{$Index} = 1;
       next MAINLOOP;
     }
   }
   if (!$NextLoop) {
     print $NEWFH "$_\n";
     &DebugPrint("\tPrinted: $_");
   }
 }
  close $FH;
  close $NEWFH;

  $Errors = grep(/0/, values %Done);
  if (!$Errors ) {
    $status = system("diff $File $File.new > /dev/null 2>&1");
    if ($status) {
      if (!rename($File, "$File.old")) {
	print "Could not rename $File to $File.old: $!";
      }
      if (!rename("$File.new", $File)) {
	print "Could not rename $File.new to $File: $!";
      } else {
	&CopyStat("$File.old", $File);
      }
    }
    else {
      &VerbosePrint ("No difference. Leaving the old $File intact");
      unlink("$File.new");
    }
    return 1;
  }
  else {
    &VerbosePrint("$Errors Action[s] did not succeed.");
    unlink("$File.new");
    return 0;
  }
}

# 
# AddToBuffer(BufferRef, Line)
# 
# This subroutine will add a given line to a buffer
# 
# BufferRef is a reference to a buffer
# Line is the line to be added
# 

sub AddToBuffer {
  my($BufferRef, $line) = @_;
  push(@$BufferRef, [$CurrentLine, $line]);
  $LineReferences{$CurrentLine}++;
  &DebugPrint("Adding to buffer[$BufferRef]=>$CurrentLine, $line");
  &DebugPrint("References for $CurrentLine is $LineReferences{$CurrentLine}");
}

# 
# ResetSearch(Print)
# 
# This subroutine will reset the Search variables
# 
# Print indicates whether or not to print the contents of the SearchBuffer
# 

sub ResetSearch {
  my($print) = @_;
  my($BufferedLine);
  my($ref, $lineno);
  &DebugPrint("ResetSearch");
  if ($print) {
    # print the lines that were matched up to this point since they were not
    # printed before
    foreach $ref (@$SearchBufferRef) {
      ($lineno, $BufferedLine) = @$ref;
      &DebugPrint("References for $lineno is $LineReferences{$lineno}");
      if ($LineReferences{$lineno} == 1) {
	print $NEWFH "$BufferedLine\n"; 
	&DebugPrint("\tResetSearch Printed: $BufferedLine [$SearchBufferRef,$lineno]");
      } else {
	&DebugPrint("not printing [$SearchBufferRef]=>$lineno,$BufferedLine\n");
      }
      $LineReferences{$lineno}--;
    }
  }
  # reset the Search indices and buffer
  $$SearchIndexRef = 0;
  @$SearchBufferRef = ();
}

# 
# ResetNew()
# 
# This subroutine will reset the New variables
# 
# Print indicates whether or not to print the contents of the NewBuffer
#

sub ResetNew {
  my ($print) = @_;
  my($BufferedLine);
  my($ref, $lineno);
  &DebugPrint("ResetNew");
  if ($print) {
    # print the lines that were matched up to this point since they were not
    # printed before
    foreach $ref (@$NewBufferRef) {
      ($lineno, $BufferedLine) = @$ref;
      &DebugPrint("References for $lineno is $LineReferences{$lineno}");
      if ($LineReferences{$lineno} == 1) {
	print $NEWFH "$BufferedLine\n"; 
	&DebugPrint("\tResetNew Printed: $BufferedLine [$NewBufferRef,$lineno]");
      } else {
	&DebugPrint("not printing [$NewBufferRef]=>$lineno,$BufferedLine\n");
      }
      $LineReferences{$lineno}--;
    }
  }
  # reset the New indices and buffer
  $$NewIndexRef = 0;
  @$NewBufferRef = ();
}

# 
# Abort()
# 
# This subroutine will print an abort message
# 

sub Abort {
  my ($mesg) = @_;
  print "Aborting, $mesg\n";
}

# 
# VerbosePrint(Msg)
# 
# This subroutine will print a message if the program is running in verbose
# mode set earlier by Verbose()
# 
# Msg is the message to print
# 

sub VerbosePrint {
  my ($mesg) = @_;
  print "\t$mesg\n" if ($Verbose);
}

# 
# DebugPrint(Msg)
# 
# This subroutine will print a message if the program is running in Debug
# mode set earlier by Debug()
# 
# Msg is the message to print
# 

sub DebugPrint {
  my ($mesg) = @_;
  print "$mesg\n" if ($Debug);
}

# 
# PrintArray(Array)
# 
# This subroutine will print out all of the contents of an array.
# Primarily used in debugging.
# 
# Array is the array to print
# 
sub PrintArray {
  my ($arrayref) = @_;
  my ($element);
  foreach $element (@$arrayref) {
    print "$element\n";
  }
}

#
# CopyStat(SrcFile, DstFile)
# 
# This subroutine copies the mode, uid and gid from SrcFile to DstFile
#
sub CopyStat {
  my ($user, $group, $srcfile, $destfile, $mode, $rc, @statinfo);
  $srcfile = shift @_;
  $destfile = shift @_;
  @statinfo = stat($srcfile);
  $mode = $statinfo[2];
  $user = $statinfo[4];
  $group = $statinfo[5];
  &VerbosePrint("Copying owner,group,mode of \"$srcfile\" to \"$destfile\"");
  $rc = chown $user, $group, $destfile;
  &VerbosePrint("Could not change mode bits of", $destfile) if (!$rc);
  $rc = chmod $mode, $destfile;
  &VerbosePrint("Could not change mode bits of", $destfile) if (!$rc); 
}

1;
