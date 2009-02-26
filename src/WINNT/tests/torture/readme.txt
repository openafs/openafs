The windows file system torture test is derived from the
torture test developed by Andrew Tridgell for Samba in 1997.

The torture test engine can be executed either as a single
instance of wintorture.exe or multiple instances managed
by stress.exe and stopstress.exe.

The command line options are:

  -a <sec>  Seconds delay between starting detached processes [stress only]
  -b        Create a chronological log
  -c <txt>  Specifies the script text file to use
  -d <num>  Number of detached processes to run [stress only]
  -e        End thread processing on an error.
  -f <name> Target directory name
  -i <num>  Number of iterations of the stress test to run.
              This option will override the -m option.
  -m <num>  The number of minutes to run
  -n <num>  The number of threads to run within each wintorture process
  -p <path> UNC path to second directory
  -s        Output stats
  -t        Perform AFS trace logging
  -u <UNC>  UNC path to target directory
  -v        Turn on verbose mode

wintorture.exe is a script driven testing engine.  Each thread simultaneously
executes its own copy of the script in a dedicated directory created under 
the target directory.  The scripted commands can describe operations that
are expected to succeed or to fail.  A command that is intended to fail and
does is a success.  As of this writing the wintorture engine understands
the following commands:

"BM_SETUP"
sets benchmark state to setup.

"BM_WARMUP"
sets the benchmark state to warmup.

"BM_MEASURE"
sets the benchmark state to measure.

"RECONNET"
does nothing

"SYNC" <seconds>
does nothing

"NTCreateX" <filename> <create_options> <create_disposition> <file_id>
executes the CreateFile api and associates the resulting file handle with 
the provided <file_id> which can be passed to subsequent commands.  
The <create_options> and <create_disposition> are specified as numeric 
values using the constants as detailed by the CreateFile API.  If the 
command is expected to fail, specify a <file_id> of -1.

"SetLocker" <locker>
If <locker> is specified, switch the UNC target directory to the specified
<locker>.  If <locker> is not specified, restore the original target
directory.

"Xrmdir" <directory> <type>
<directory> specifies the name of the directory to be removed.
<type> is optional and can be the value "all".  If <type> = "all"
is specified a recursive remove directory operation is executed.

"Mkdir" <directory>
<directory> specifies the directory to be created.

"Attach" <locker> <drive>
Perform the MIT "attach" commad on the specified <locker> and optional
<drive>

"Detach" <locker/drive> <type>
Perform the MIT "detach" command on the specified <locker/drive>.  <type>
is required and must be one of "drive" or "locker".

"CreateFile" <path> <size>
Creates the file name specified by <path> of size <size> or 512 bytes
if <size> is smaller than 512 bytes.  The last 512 bytes of the file
will be filled with 'A'.

"CopyFiles" <source> <destination>
Performs a binary verified copy of <source> to <destination>.  If 
<destination> exists it will be overwritten.  <source> may include
wildcards.

"DeleteFiles" <path>
Deletes the file(s) specified by <path>.  <path> may include wildcards.

"Move" <source> <destination>

"Xcopy" <source> <destination>

"Close" <file_id>

"Rename" <source> <destination>

"Unlink" <path>

"Deltree" <path>

"Rmdir" <directory>

"QUERY_PATH_INFORMATION" <path> <flag>

"QUERY_FILE_INFORMATION" <file_id>

"QUERY_FS_INFORMATION" <level>

"FIND_FIRST" <mask>

"WriteX" <file_id> <offset> <size> <ret_size>

"ReadX" <file_id> <offset> <size> <ret_size>

"Flush" <file_id>

"LockingX" <file_id> <offset> <size> <timeout> <locktype> <ntstatus>






