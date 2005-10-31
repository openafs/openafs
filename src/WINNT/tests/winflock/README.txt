
To run winflock.exe:

C:\> winflock.exe -d (dir) > verbose.log

By default, there a lot of logging generated to stdout while the actual test 
results are reported to stderr.  Redirecting stdout to a log file cleans out
the output.

The directory specified by (dir) must exist.  This is where the test files
will be created and tests run against.

Eg:

   winflock.exe -d \\afs\athena.mit.edu\user\a\s\asanka\test > verbose.log


