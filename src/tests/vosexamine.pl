#!/usr/bin/env perl
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT %AFS_Help);
use OpenAFS::afsconf;
use OpenAFS::fs;
use OpenAFS::pts;
use OpenAFS::vos;
use OpenAFS::bos;

my ($host, %info, %info2, @rosites, $tmp, @rosite);
$host = `hostname`;
chomp $host;
&AFS_Init();

%info = &AFS_vos_examine("rep",);
if ($info{'rwpart'} ne "a") {
    exit 1;
}

$ret = $info{'rosites'};
@rosites = @$ret;
while ($ret = pop(@rosites)) {
    @rosite = @$ret;
    if ($rosite[1] ne "a") {
	exit 1;
    }
}

exit(0);



