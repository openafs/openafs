# This is -*- perl -*-

package OpenAFS::Auth;
use OpenAFS::Dirpath;

use strict;
#use vars qw( @ISA @EXPORT );
#@ISA = qw(Exporter);
#require Exporter;
#@EXPORT = qw($openafs-authadmin $openafs-authuser);

sub getcell {
    my($cell);
    open(CELL, "$openafsdirpath->{'afsconfdir'}/ThisCell") 
	or die "Cannot open $openafsdirpath->{'afsconfdir'}/ThisCell: $!\n";
    $cell = <CELL>;
    chomp $cell;
    close CELL;
    return $cell;
}

sub getrealm {
    my($cell);
    open(CELL, "$openafsdirpath->{'afsconfdir'}/ThisCell") 
	or die "Cannot open $openafsdirpath->{'afsconfdir'}/ThisCell: $!\n";
    $cell = <CELL>;
    chomp $cell;
    close CELL;
    $cell =~ tr/a-z/A-Z/;
    return $cell;
}

sub authadmin {
    my $cell = &getrealm;
    my $cmd = "echo \"Proceeding w/o authentication\"|klog -pipe admin\@${cell}";
    system($cmd);
}
sub authuser {
    my $cell = &getrealm;
    my $cmd = "echo \"Proceeding w/o authentication\"|klog -pipe user\@${cell}";
    system($cmd);
}

1;
