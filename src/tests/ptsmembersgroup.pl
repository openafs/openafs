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

@membership = &AFS_pts_members(testgroup1,);
while ($group = pop(@membership)) {
    if ($group ne "testuser1") {
	exit(1);
    }
}
&AFS_pts_add([admin],[testgroup1],);
@membership = &AFS_pts_members(testgroup1,);
while ($group = pop(@membership)) {
    if ($group eq "testuser1") {
    } else {
	if ($group eq "admin") {
	} else {
	    exit(1);
	}
    }
}
&AFS_pts_remove([admin],["testgroup1"],);

exit(0);



