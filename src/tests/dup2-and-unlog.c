#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>

#include <afs/stds.h>
#include <afs/auth.h>

int
main(int argc, char **argv)
{
    int fd;


    fd = open("foo", O_RDWR | O_CREAT, 0666);
    if (fd < 0)
	err(1, "open");

    dup2(fd + 1, fd);

    if (write(fd, "foo\n", 4) != 4)
	errx(1, "write");

    ktc_ForgetAllTokens();

    close(fd);
    close(fd + 1);

    exit(0);
}
