#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my (@owned, $group);
&AFS_Init();

@owned = &AFS_pts_listown(testuser1,);
while ($group = pop(@owned)) {
    if ($group ne "testgroup1") {
	exit(1);
    }
}

exit(0);



