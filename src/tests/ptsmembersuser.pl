#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my (@membership, $group);
&AFS_Init();

@membership = &AFS_pts_members(testuser1,);
while ($group = pop(@membership)) {
    if ($group ne "testgroup1") {
	exit(1);
    }
}
&AFS_pts_add([testuser1],["system:administrators"],);
@membership = &AFS_pts_members(testuser1,);
while ($group = pop(@membership)) {
    if ($group eq "testgroup1") {
    } else {
	if ($group eq "system:administrators") {
	} else {
	    exit(1);
	}
    }
}
&AFS_pts_remove([testuser1],["system:administrators"],);

exit(0);



