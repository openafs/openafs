#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($host, $ret, @parts, $count);
$host = `hostname`;
&AFS_Init();

@parts = &AFS_vos_listpart("localhost",);
$ret = shift(@parts);
if ($ret ne "a") {
    exit (1);
}
$ret = shift(@parts);
if ($ret ne "b") {
    exit (1);
}
exit(0);



