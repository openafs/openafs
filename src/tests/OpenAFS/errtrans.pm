# CMUCS AFStools
# Copyright (c) 1996, Carnegie Mellon University
# All rights reserved.
#
# See CMUCS/CMU_copyright.pm for use and distribution information

package OpenAFS::errtrans;

=head1 NAME

OpenAFS::errtrans - com_err error translation

=head1 SYNOPSIS

  use OpenAFS::errtrans
  $code = errcode($name);
  $code = errcode($pkg, $err);
  $string = errstr($code, [$volerrs]);

=head1 DESCRIPTION

This module translates "common" error codes such as those produced
by MIT's com_err package, and used extensively in AFS.  It also knows
how to translate system error codes, negative error codes used by Rx,
and a few "special" error codes used by AFS's volume package.

In order to work, these routines depend on the existence of error
table files in $err_table_dir, which is usually /usr/local/lib/errtbl.
Each file should be named after a com_err error package, and contain
the definition for that package.

Note that the AFS version of com_err translates package names to uppercase
before generating error codes, so a table which claims to define the 'pt'
package actually defines the 'PT' package when compiled by AFS's compile_et.
Tables that are normally fed to AFS's compile_et should be installed using
the _uppercase_ version of the package name.

The error tables used in AFS are part of copyrighted AFS source code, and
are not included with this package.  However, I have included a utility
(gen_et) which can generate error tables from the .h files normally
produced by compile_et, and Transarc provides many of these header files
with binary AFS distributions (in .../include/afs).  See the gen_et
program for more details.

=cut

use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT :afs_internal);
use OpenAFS::config qw($err_table_dir);
use Symbol;
use Exporter;
use POSIX;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&errcode &errstr);


@NumToChar = ('', 'A'..'Z', 'a'..'z', '0'..'9', '_');
%CharToNum = map(($NumToChar[$_], $_), (1 .. $#NumToChar));

%Vol_Codes = ( VSALVAGE    => 101,
               VNOVNODE    => 102,
               VNOVOL      => 103,
               VVOLEXISTS  => 104,
               VNOSERVICE  => 105,
               VOFFLINE    => 106,
               VONLINE     => 107,
               VDISKFULL   => 108,
               VOVERQUOTA  => 109,
               VBUSY       => 110,
               VMOVED      => 111
             );
%Vol_Desc  = ( 101 => "volume needs to be salvaged",
               102 => "no such entry (vnode)",
               103 => "volume does not exist / did not salvage",
               104 => "volume already exists",
               105 => "volume out of service",
               106 => "volume offline (utility running)",
               107 => "volume already online",
               108 => "unknown volume error 108",
               109 => "unknown volume error 109",
               110 => "volume temporarily busy",
               111 => "volume moved"
             );
%Rx_Codes  = ( RX_CALL_DEAD           => -1,
               RX_INVALID_OPERATION   => -2,
               RX_CALL_TIMEOUT        => -3,
               RX_EOF                 => -4,
               RX_PROTOCOL_ERROR      => -5,
               RX_USER_ABORT          => -6,
               RX_ADDRINUSE           => -7,
               RX_MSGSIZE             => -8,
               RXGEN_CC_MARSHAL       => -450,
               RXGEN_CC_UNMARSHAL     => -451,
               RXGEN_SS_MARSHAL       => -452,
               RXGEN_SS_UNMARSHAL     => -453,
               RXGEN_DECODE           => -454,
               RXGEN_OPCODE           => -455,
               RXGEN_SS_XDRFREE       => -456,
               RXGEN_CC_XDRFREE       => -457
             );
%Rx_Desc   = (   -1 => "server or network not responding",
                 -2 => "invalid RPC (Rx) operation",
                 -3 => "server not responding promptly",
                 -4 => "Rx unexpected EOF",
                 -5 => "Rx protocol error",
                 -6 => "Rx user abort",
                 -7 => "port address already in use",
                 -8 => "Rx message size incorrect",
               -450 => "Rx client: XDR marshall failed",
               -451 => "Rx client: XDR unmarshall failed",
               -452 => "Rx server: XDR marshall failed",
               -453 => "Rx server: XDR unmarshall failed",
               -454 => "Rx: Decode failed",
               -455 => "Rx: Invalid RPC opcode",
               -456 => "Rx server: XDR free failed",
               -457 => "Rx client: XDR free failed",
               map(($_ => "RPC interface mismatch ($_)"), (-499 .. -458)),
               -999 => "Unknown error"
             );


sub _tbl_to_num {
  my(@tbl) = split(//, $_[0]);
  my($n);

  @tbl = @tbl[0..3] if (@tbl > 4);
  foreach (@tbl) { $n = ($n << 6) + $CharToNum{$_} }
  $n << 8;
}


sub _num_to_tbl {
  my($n) = $_[0] >> 8;
  my($tbl);

  while ($n) {
    $tbl = @NumToChar[$n & 0x3f] . $tbl;
    $n >>= 6;
  }
  $tbl;
}


sub _load_system_errors {
  my($file) = @_;
  my($fh) = &gensym();

  return if ($did_include{$file});
# print "Loading $file...\n";
  $did_include{$file} = 'yes';
  if (open($fh, "/usr/include/$file")) {
    while (<$fh>) {
      if (/^\#define\s*(E\w+)\s*(\d+)/) {
        $Codes{$1} = $2;
      } elsif (/^\#include\s*\"([^"]+)\"/
           ||  /^\#include\s*\<([^>]+)\>/) {
        &_load_system_errors($1);
      }
    }
    close($fh);
  }
}


# Load an error table into memory
sub _load_error_table {
  my($pkg) = @_;
  my($fh, @words, $curval, $tval, $nval);
  my($tid, $tfn, $code, $val, $desc);

  return if ($Have_Table{$pkg});
  # Read in the input file, and split it into words
  $fh = &gensym();
  return unless open($fh, "$err_table_dir/$pkg");
# print "Loading $pkg...\n";
  line: while (<$fh>) {
    s/^\s*//;  # Strip leading whitespace
    while ($_) {
      next line if (/^#/);
      if    (/^(error_table|et)\s*/) { push(@words, 'et');  $_ = $' }
      elsif (/^(error_code|ec)\s*/)  { push(@words, 'ec');  $_ = $' }
      elsif (/^end\s*/)              { push(@words, 'end'); $_ = $' }
      elsif (/^(\w+)\s*/)            { push(@words, $1);    $_ = $' }
      elsif (/^\"([^"]*)\"\s*/)      { push(@words, $1);    $_ = $' }
      elsif (/^([,=])\s*/)           { push(@words, $1);    $_ = $' }
      else { close($fh); return }
    }
  }
  close($fh);

  # Parse the table header
  $_ = shift(@words); return unless ($_ eq 'et');
  if ($words[1] eq 'ec')    { $tid = shift(@words) }
  elsif ($words[2] eq 'ec') { ($tfn, $tid) = splice(@words, 0, 2) }
  else { return; }
  if ($tid ne $pkg) {
    $Have_Table{$tid} = 'yes';
    $_ = $tid;
    $_ =~ tr/a-z/A-Z/;
    $tid = $_ if ($_ eq $pkg);
  }
  $tval = &_tbl_to_num($tid);
  $Have_Table{$pkg} = 'yes';
# print "Package $pkg: table-id = $tid, table-fun = $tfn, base = $tval\n";

  while (@words) {
    $_ = shift(@words); return unless ($_ eq 'ec');
    $code = shift(@words);
    $_ = shift(@words);
    if ($_ eq '=') {
      $val = shift(@words);
      $_ = shift(@words);
    } else {
      $val = $curval;
    }
    return unless ($_ eq ',');
    $desc = shift(@words);
    $nval = $tval + $val;
    $curval = $val + 1;
    $Desc{$nval} = $desc;
    $Codes{$code} = $nval;
#   print "  code $code: value = $nval ($tval + $val), desc = \"$desc\"\n";
  }
}

=head2 errcode($name)

Returns the numeric error code corresponding to the specified error
name.  This routine knows about names of system errors, a few special
Rx and volume-package errors, and any errors defined in installed
error tables.  If the specified error code is not found, returns -999.

=head2 errcode($pkg, $code)

Shifts $code into the specified error package, and returns the
resulting com_err code.  This can be used to generate error codes
for _any_ valid com_err package.

=cut

sub errcode {
  if (@_ > 1) {
    my($pkg, $code) = @_;
    &_tbl_to_num($pkg) + $code;
  } else {
    my($name) = @_;
    my($dir, @tbls, $code);

    &_load_system_errors("errno.h");
    if ($Vol_Codes{$name})   { $Vol_Codes{$name} }
    elsif ($Rx_Codes{$name}) { $Rx_Codes{$name} }
    elsif ($Codes{$name})    { $Codes{$name} }
    else {
      if ($name =~ /^E/) {  # Might be a POSIX error constant
        $! = 0;
        $code = &POSIX::constant($name, 0);
        if (!$!) { return $code; }
      }
      $dir = &gensym();
      if (opendir($dir, $err_table_dir)) {
        @tbls = grep(!/^\.?\.$/, readdir($dir));
        close($dir);
        foreach (@tbls) { &_load_error_table($_) }
      }
      $Codes{$name} ? $Codes{$name} : -999;
    }
  }
}


=head2 errstr($code, [$volerrs])

Returns the error string corresponding to a specified com_err, Rx,
or system error code.  If $volerrs is specified and non-zero, then
volume-package errors are considered before system errors with the
same values.

=cut

sub errstr {
  my($code, $volerrs) = @_;
  my($pkg, $sub);

  if ($Rx_Desc{$code}) { return $Rx_Desc{$code} }
  if ($volerrs && $Vol_Desc{$code}) { return $Vol_Desc{$code} }
  $sub = $code & 0xff;
  $pkg = &_num_to_tbl($code);
  if ($pkg eq '') {
    $! = $sub + 0;
    $_ = $! . '';
    if (/^(Error )?\d+$/) { $Vol_Desc{$sub} ? $Vol_Desc{$sub} : "Error $sub" }
    else { $_ }
  } else {
    &_load_error_table($pkg);
    $Desc{$code} ? $Desc{$code} : "Unknown code $pkg $sub ($code)";
  }
}

1;

=head1 COPYRIGHT

The CMUCS AFStools, including this module are
Copyright (c) 1996, Carnegie Mellon University.  All rights reserved.
For use and redistribution information, see CMUCS/CMU_copyright.pm

=cut
