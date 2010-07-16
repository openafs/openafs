The Volume Server records a trace of its activity in the
F</usr/afs/logs/VolserLog> file. Use the B<bos getlog> command to display
the contents of the file.

The Volume Server processes the B<vos> commands that administrators use to
create, delete, move, and replicate volumes, as well as prepare them for
archiving to tape or other media.

By default, the VL Server runs nine lightweight processes (LWPs). To
change the number, use the B<-p> argument.

This command does not use the syntax conventions of the AFS command
suites. Provide the command name and all option names in full.
