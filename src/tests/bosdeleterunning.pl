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

eval { &AFS_bos_delete(localhost,sleeper,); };

if (! $@) {
    exit (1);
}
exit(0);



