#include <ctype.h>
#include <stdio.h>
#include "pem.h"

int
readbuf(FILE *f, void *buf, int s)
{
    return fread(buf, 1, s, f);
}

int
writebuf(FILE *f, void *buf, int s)
{
    return fwrite(buf, 1, s, f);
}

int
encode(void)
{
    struct pemstate *state;
    int c;
    char buf[6];
    state = pemopen(writebuf, stdout);
    while ((c = fread(buf, 1, sizeof buf, stdin))) {
	pemwrite(state, buf, c);
    }
    c = pemclose(state);
    puts("");
    return c;
}

int
decode(void)
{
    struct pemstate *state;
    int c; char buf[5];

    state = pemopen(readbuf, stdin);
    while ((c = pemread(state, buf, sizeof buf)) != EOF)
	fwrite(buf, 1, c, stdout);
    return pemclose(state);
}

int
main(int argc, char **argv)
{
    return (argc > 1) ? decode() : encode();
}
