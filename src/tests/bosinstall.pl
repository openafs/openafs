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

open(FOO, ">foo.sh"); 
print FOO "#!/bin/sh\n";
print FOO "touch /tmp/garbage\n";
print FOO "exit 0\n";
close FOO;
chmod 0755, "foo.sh";

&AFS_bos_install(localhost,["foo.sh"],,);

exit(0);



