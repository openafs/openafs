#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;
use OpenAFS::ConfigUtils;
use OpenAFS::Dirpath;
use OpenAFS::OS;

my ($host);
$host = `hostname`;
&AFS_Init();

&AFS_bos_exec(localhost,"$openafsdirpath->{'afssrvbindir'}/foo.sh",);
if (-f "/tmp/garbage") {
} else {
    exit(1);
}
unlink "/tmp/garbage";
exit(0);



