#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($wscell, $path, @pos1, @pos2, @neg1, @neg2, $ret, @tmp, @tmp2, @tmp3, $found, $listref, $first);
&AFS_Init();
$wscell = &AFS_fs_wscell();
$found = 0;
$first = 0;

$path = "/afs/${wscell}/service/acltest";
mkdir $path, 0777;
($pos1, $neg1) = &AFS_fs_getacl($path);
$listref = "[ ";
while ($ret = pop(@$pos1)) {
    $first++;
    @tmp2=@$ret;
    if ($tmp2[0] eq "group1") { 
	$found++; 
    } else { 
	unshift @tmp, @$ret;
	if ($first == 0 ) {
	    $listref = $listref . "[ @tmp2 ]";
	} else {
	    $listref = $listref . ", [ @tmp2 ]";
	}
    }
}
$listref = $listref . " ]";

if ($found != 1) {
    print "WARNING: Can't remove user not on ACL. This shouldn't happen.\n";
    exit(1);
}

$ret = &AFS_fs_setacl([$path],$listref,,);
($pos2, $neg2) = &AFS_fs_getacl($path);

@tmp2=();
while ($ret = pop(@$pos1)) {
    unshift @tmp2, @$ret;
}

exit(0);



