# This is -*- perl -*-

package OpenAFS::OS;

use strict;
use vars qw( @ISA @EXPORT $openafsinitcmd);
@ISA = qw(Exporter);
require Exporter;
@EXPORT = qw($openafsinitcmd);

# OS-specific configuration
$openafsinitcmd = {
        'client-start'      => '/etc/init.d/openafs-client start',
        'client-stop'       => '/etc/init.d/openafs-client stop',
	'client-forcestart' => '/etc/init.d/openafs-client force-start',
        'client-restart'    => '/etc/init.d/openafs-client restart',
	'filesrv-start'     => '/etc/init.d/openafs-fileserver start',
	'filesrv-stop'      => '/etc/init.d/openafs-fileserver stop',
	'filesrv-forcestart'=> '/etc/init.d/openafs-fileserver force-start',
	'filesrv-restart'   => '/etc/init.d/openafs-fileserver restart',
};

1;
