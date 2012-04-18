#include <unistd.h>
int
main(int argc, char *argv[], char *envp[])
{
    int euid;
    euid = geteuid();
    if (setuid(euid) != 0)
	return -1;
    return execve(argv[1], &argv[1], envp);
}

