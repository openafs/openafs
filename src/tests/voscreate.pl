#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($host, $ret);
$host = `hostname`;
&AFS_Init();

$ret = &AFS_vos_create("testvol","localhost","a",,);
$ret = &AFS_vos_create("testvol2","localhost","a",,);

exit(0);



