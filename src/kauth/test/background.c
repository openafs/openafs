main (argc, argv)
  int   argc;
  char *argv[];
{   int pid;

    if (argc == 1) exit (-1);
    if (pid = fork ()) {		/* parent */
	printf ("%d", pid);
	exit (0);
    }
    execve (argv[1], argv+1, 0);
    perror ("execve returned");
}
