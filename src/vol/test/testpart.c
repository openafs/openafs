#include <stdio.h>

Log(a, b, c, d, e, f, g, h, i, j, k)
{
printf(a,b,c,d,e,f,g,h,i,j,k);
}

iopen() {}

AssertionFailed()
{
printf("assertion failed\n");
exit(-1);
}

Abort()
{
printf("Aborting\n");
exit(-1);
}

main(argc, argv)
int argc;
char **argv; {
    VInitVolumePackage(1, 0, 0, 0, 0);
    VPrintDiskStats();

}

