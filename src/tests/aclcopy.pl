#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($wscell, $path, @pos1, @pos2, @neg1, @neg2, $ret, @tmp, @tmp2, @tmp3, $found, $path2);
&AFS_Init();
$wscell = &AFS_fs_wscell();
$found = 0;

eval { &AFS_pts_createuser(user1,,); };
$path = "/afs/${wscell}/service/acltest";
$path2 = "/afs/${wscell}/service/acltest2";
mkdir $path, 0777;
mkdir $path2, 0777;
($pos1, $neg1) = &AFS_fs_getacl($path);
$ret = &AFS_fs_copyacl($path,[$path2],1);
($pos2, $neg2) = &AFS_fs_getacl($path2);
if (@pos1 != @pos2) {
    exit(1);
}
if (@neg1 != @neg2) {
    exit(1);
}
exit(0);



