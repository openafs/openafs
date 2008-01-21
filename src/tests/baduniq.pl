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

&AFS_vos_restore("badvol","localhost","a","/tmp/t.uniq-bad","100","full",);
&AFS_bos_salvage("localhost","a","badvol",,,,,,);
&AFS_fs_mkmount("badvol", "badvol",,,);
if ( -f "badvol/test" ) {
 &AFS_fs_rmmount("badvol");
 exit(0);
}
&AFS_fs_rmmount("badvol");
exit(1);



