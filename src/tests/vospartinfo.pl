#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($host, $ret, %info);
$host = `hostname`;
&AFS_Init();

%info = &AFS_vos_partinfo("localhost",,);

exit(0);



