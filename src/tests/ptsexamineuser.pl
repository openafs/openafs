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

%info = &AFS_pts_examine(testuser1,);
if ($info{'creator'} ne "admin") {
    exit(1);
}
if ($info{'mem_count'} != 1) {
    exit(1);
}

exit(0);



