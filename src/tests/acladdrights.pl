#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($wscell, $path, @pos1, @pos2, @neg1, @neg2, $ret, @tmp, @tmp2, @tmp3, $found);
&AFS_Init();
$wscell = &AFS_fs_wscell();
$found = 0;

eval { &AFS_pts_createuser(user1,,); };
$path = "/afs/${wscell}/service/acltest";
mkdir $path, 0777;
($pos1, $neg1) = &AFS_fs_getacl($path);
$ret = &AFS_fs_setacl([$path],[["user1", "rlidw"]],,);
($pos2, $neg2) = &AFS_fs_getacl($path);
while ($ret = pop(@$pos2)) {
    @tmp2=@$ret;
    if ($tmp2[0] eq "user1") { 
	if ($tmp2[1] ne "rlidw") {
	    exit (1);
	}
	$tmp2[1] = "rl";
	unshift @tmp, @tmp2;
    } else {
	unshift @tmp, @$ret;
    }
}

@tmp2=();
while ($ret = pop(@$pos1)) {
    unshift @tmp2, @$ret;
}

exit(0);



