#include "ktime.h"

main(argc, argv)
int argc;
char **argv; {
    long code, temp;

    if (argc <= 1) {
	printf("dtest: usage is 'dtest <time to interpret>'\n");
	exit(0);
    }

    code = ktime_DateToLong(argv[1], &temp);
    if (code) {
	printf("date parse failed with code %d.\n", code);
    }
    else {
	printf("returned %d, which, run through ctime, yields %s", temp, ctime(&temp));
    }
    exit(0);
}
