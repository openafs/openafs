#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($host);
$host = `hostname`;
&AFS_Init();

&AFS_bos_addhost(localhost,"128.2.1.2",,);

exit(0);



