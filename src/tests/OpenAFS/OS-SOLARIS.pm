# This is -*- perl -*-

package OpenAFS::OS;

use strict;
use vars qw( @ISA @EXPORT $openafsinitcmd);
@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw($openafsinitcmd);

# OS-specific configuration
$openafsinitcmd = {
        'client-start'      => 'modload /usr/vice/etc/modload/libafs.nonfs.o; /usr/vice/etc/afsd -nosettime',
        'client-stop'       => 'echo Solaris client cannot be stopped',
	'client-forcestart' => 'modload /usr/vice/etc/modload/libafs.nonfs.o; /usr/vice/etc/afsd -nosettime',
        'client-restart'    => 'echo Solaris client cannot be restarted',
	'filesrv-start'     => '/usr/afs/bin/bosserver',
	'filesrv-stop'      => '/usr/afs/bin/bos shutdown localhost -local -wait; pkill /usr/afs/bin/bosserver',
	'filesrv-forcestart'=> '/usr/afs/bin/bosserver',
	'filesrv-restart'   => '/usr/afs/bin/bos shutdown localhost -local -wait; pkill /usr/afs/bin/bosserver; sleep 1; /usr/afs/bin/bosserver',
};

1;
