#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($host, %info, %info2, $linfo);
$host = `hostname`;
&AFS_Init();

&AFS_bos_stop(localhost,[sleeper],1,);
%info = &AFS_bos_status("localhost",[sleeper],);
$linfo=$info{'sleeper'};
%info2 = %$linfo;
if ($info2{'num_starts'} != 1) {
    exit 1;
}
if ($info2{'status'} ne "disabled, currently shutdown.") {
    exit 1;
}
exit(0);



