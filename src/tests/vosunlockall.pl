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

&AFS_vos_lock("testvol",);
&AFS_vos_lock("service",);
&AFS_vos_unlockvldb("localhost",,);

exit(0);



