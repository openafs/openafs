#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($host, $ret, %vols, $lvol, %vol);
$host = `hostname`;
&AFS_Init();

%vols = &AFS_vos_listvol("localhost","b",);
$lvol=$vols{'testvol3'};
%vol=%$lvol;
# if it worked it worked...
if ($vol{'part'} ne "b") {
    exit(1);
}
exit(0);



