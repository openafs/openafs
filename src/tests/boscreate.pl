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

open(FOO, ">sleeper.sh"); 
print FOO "#!/bin/sh\n";
print FOO "while true; do sleep 60; done\n";
print FOO "exit 0\n";
close FOO;
chmod 0755, "sleeper.sh";

&AFS_bos_install(localhost,["sleeper.sh"],,);
&AFS_bos_create(localhost,sleeper,simple,["/usr/afs/bin/sleeper.sh"],);

exit(0);



