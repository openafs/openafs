#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my (%info);
&AFS_Init();

&AFS_pts_setf(testuser1,"S-M--",30,);
%info = &AFS_pts_examine(testuser1,);
if ($info{'creator'} ne "admin") {
    exit(1);
}
if ($info{'flags'} ne "S-M--") {
    exit(1);
}
if ($info{'group_quota'} != 30) {
    exit(1);
}

exit(0);



