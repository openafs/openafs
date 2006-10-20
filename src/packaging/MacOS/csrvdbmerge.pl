#!/usr/bin/perl
# CellServDB merging script
# only tested on darwin systems

use File::Copy;
use IO::File;
use Fcntl;
use strict;


sub doit {
  my ($cur,$in,$last,$new);
  my ($line, $oline, $cell, $pos, $which);
  my %cellstat;

  $cur=new IO::File '<CellServDB';
  $last=new IO::File '<CellServDB.master.last';
  
  while (defined($line=<$cur>)) {
    if ($line =~ /^>([-a-zA-Z0-9\._]+)\s/) {
      if ($cell) {
        $oline=<$last>;
        if ($oline && $oline !~ /^>/) { # fewer servers in user's file than master
          $cellstat{$cell} = 1;
        }
      }
      $cell=$1;
      $cellstat{$cell}=2; 
      # start at the beginning of the old file, and find $cell
      $last->seek(0,SEEK_SET);
      while (<$last>) {
        if (/>$cell\s/) { # note that we don't compare the cell comments
          $cellstat{$cell}=0; 
          last;
        }
      }
      next;
    }
    if (! $cell) {
      die "First CellServDB line isn't a cell\n";
    }
    next if ($cellstat{$cell} == 2); # cell only in local CellServDB
    next if ($cellstat{$cell} == 1); # already found a local change
    $oline=<$last>;
    if ($oline =~ /^>/) { # more servers in user's file than master
      $last->setpos($pos);
      $cellstat{$cell} = 1;
      next;
    }
    next if ($line eq $oline);
    $cellstat{$cell} = 1;
  }
  close($last);
  $cur->seek(0,SEEK_SET);
  $cur=new IO::File '<CellServDB' or die "No CellServDB: $!\n";
  $in=new IO::File '<CellServDB.master' or die "No CellServDB.master: $!\n";
  $new=new IO::File '>CellServDB.NEW' or die "Cannot create output CellServDB: $!\n";
  while (defined($line=<$cur>)) {
    if ($line =~ /^>([-a-zA-Z0-9\._]+)\s/) {
      $cell=$1;
      $which=$cellstat{$cell};
      unless ($which) {
        $in->seek(0,SEEK_SET);
        while (<$in>) {
          if (/>$cell\s/) {
            last;
          }
        }
        if (defined($_)) {
          print $new $_;
          while (defined($line=<$in>) && $line !~ /^>/) {
            print $new $line;
          }
        } 
      }
    }
    if (! $cell) {
      die "First CellServDB line isn't a cell\n";
    }
    if ($which) {
      print $new $line;
    }
  }
  close($in);
  close($cur);
  close($new);
  rename('CellServDB.NEW', 'CellServDB');
  copy('CellServDB.master', 'CellServDB.master.last');
}

doit;
