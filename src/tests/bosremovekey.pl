#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($host, @hosts, $this, $cell, $cnt, %ret);
$host = `hostname`;
chomp $host;
&AFS_Init();
$cell = &AFS_fs_wscell();

&AFS_bos_removekey(localhost,250,);
foreach $this (keys %ret) {
    if ($this == 250) {
	exit(1);
    }
}
exit(0);



