#include "ktime.h"

main(argc, argv)
int argc;
char **argv; {
    struct ktime ttime;
    long ntime, code;

    if (argc <= 1) {
	printf("ktest: usage is 'ktest <periodic date to evaluate>'\n");
	exit(1);
    }

    code = ktime_ParsePeriodic(argv[1], &ttime);
    if (code) {
	printf("got error code %d from ParsePeriodic.\n", code);
	exit(1);
    }

    ntime = ktime_next(&ttime, 0);
    printf("time is %d, %s", ntime, ctime(&ntime));
    exit(0);
}
