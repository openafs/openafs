/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

* /
/* usd_test.c: Tests the usd library by opening a tape device,
 *         writing a few blocks of data to it, doing a fsf, bsf
 */
#include <afsconfig.h>
#include <afs/param.h>

#include <stdio.h>
#include <afs/usd.h>
#ifdef AFS_NT40_ENV
#include <windows.h>
#include <winbase.h>
#endif

static char *whoami;
usd_handle_t hTape;

#define USDTEST_DEBUG        0	/* set to 1, to print tape pos info */

static int err_exit(char *action, char *msg, int rcode);
static int bufeql(char *buf1, char *buf2, int size);
static int Rewind(usd_handle_t hTape);
static int WriteEOF(usd_handle_t hTape, int cnt);
static int ForwardSpace(usd_handle_t hTape, int cnt);
static int BackSpace(usd_handle_t hTape, int cnt);
static int PrepTape(usd_handle_t hTape);
static int ShutdownTape(usd_handle_t hTape);
static int PrintTapePos(usd_handle_t hTape);

int
main(int argc, char **argv)
{
    int rcode;
    afs_uint32 xferd;
    char *filename = argv[1];
    char buf1[1024], buf2[1024];
    int i;

    whoami = argv[0];
    if (argc < 2) {
	printf(" Usage: %s <tape device name>\n", whoami);
	exit(1);
    }
    rcode =
	usd_Open(filename, (USD_OPEN_RDWR | USD_OPEN_WLOCK), 0777, &hTape);

    if (rcode) {
	printf("%s:Can't open device. Err %d\n", whoami, rcode);
	return 1;
    }
    PrepTape(hTape);

    Rewind(hTape);
    Rewind(hTape);
    Rewind(hTape);
    PrintTapePos(hTape);
    /* Write test - write 10 blocks of random data */
    printf("%s: Beginning Write Test\n", whoami);
    i = 10;
    for (i = 0; i < 10; i++) {
	rcode = USD_WRITE(hTape, buf1, 1024, &xferd);
	if (rcode || xferd != 1024) {
	    printf("%s: Write test failed. Err %d\n", whoami, rcode);
	    return 1;
	}
	PrintTapePos(hTape);
    }

    WriteEOF(hTape, 1);

    PrintTapePos(hTape);
    printf("%s: Write Test Passed.\n", whoami);

    Rewind(hTape);
    printf("%s: Rewind Test passed\n", whoami);
    PrintTapePos(hTape);

    /* Read test - read the 10 blocks written */
    printf("%s: Beginning read test\n", whoami);
    for (i = 0; i < 10; i++) {
	rcode = USD_READ(hTape, buf2, 1024, &xferd);
	err_exit("read", "Read Test Failed", rcode);
	PrintTapePos(hTape);
	if (xferd != 1024)
	    err_exit("read", "Read Test Failed. Wrong no. of bytes read.", 1);
	if (bufeql(buf1, buf2, 1024) == 0) {
	    printf("%s: Read test failed\n", whoami);
	    exit(1);
	}
    }
    printf("%s: Read Test passed\n", whoami);

    Rewind(hTape);
    PrintTapePos(hTape);
    /* WEOF - Test */
    for (i = 0; i < 5; i++) {
	/* write data block */
	*buf1 = i;
	rcode = USD_WRITE(hTape, buf1, 1024, &xferd);
	err_exit("Write", "Write EOF Test Failed", rcode);

	/* write EOF mark */
	rcode = WriteEOF(hTape, 1);
	err_exit("WEOF", "Write EOF Test Failed", rcode);
	PrintTapePos(hTape);
    }
    printf("%s: WEOF Test passed\n", whoami);

    Rewind(hTape);
    /* fsf/bsf test */
    ForwardSpace(hTape, 2);
    PrintTapePos(hTape);
    rcode = USD_READ(hTape, buf2, 1024, &xferd);
    err_exit("read", "FSF Test Failed", rcode);
    if (*buf2 != 2)
	err_exit("FSF", "FSF Test Failed", 1);

    ForwardSpace(hTape, 2);
    PrintTapePos(hTape);
    rcode = USD_READ(hTape, buf2, 1024, &xferd);
    err_exit("read", "FSF Test Failed", rcode);
    if (*buf2 != 4)
	err_exit("FSF", "FSF Test Failed", 1);

    printf("%s: FSF Test passed\n", whoami);

    BackSpace(hTape, 4);
    ForwardSpace(hTape, 1);
    rcode = USD_READ(hTape, buf2, 1024, &xferd);
    err_exit("read", "FSF Test Failed", rcode);
    if (*buf2 != 1)
	err_exit("BSF", "FSF Test Failed", 1);

    printf("%s: BSF Test Passed\n", whoami);

    ShutdownTape(hTape);
    rcode = USD_CLOSE(hTape);
    if (rcode) {
	printf("%s:Can't close device. Err %d\n", whoami, rcode);
	return 1;
    }
    printf("%s: usd library, all tests passed!\n", whoami);
}


static int
err_exit(char *action, char *msg, int rcode)
{
    if (!rcode)
	return 0;
    printf("%s:Can't %s device. Err %d\n %s \n", whoami, action, rcode, msg);

    /* Now shutdown and close the tape */
    ShutdownTape(hTape);
    rcode = USD_CLOSE(hTape);
    if (rcode) {
	printf("%s:Can't close device. Err %d\n", whoami, rcode);
	return 1;
    }
    exit(1);
}


static int
bufeql(char *buf1, char *buf2, int size)
{
    while (size--) {
	if (*buf1 != *buf2)
	    return 0;

	++buf1;
	++buf2;
    }
    return 1;
}


static int
Rewind(usd_handle_t hTape)
{
    usd_tapeop_t tapeop;
    int rcode;

    tapeop.tp_op = USDTAPE_REW;
    tapeop.tp_count = 1;
    rcode = USD_IOCTL(hTape, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    err_exit("rewind", "Rewind Test Failed", rcode);
    return 0;
}

static int
WriteEOF(usd_handle_t hTape, int cnt)
{
    usd_tapeop_t tapeop;
    int rcode;

    tapeop.tp_op = USDTAPE_WEOF;
    tapeop.tp_count = cnt;
    rcode = USD_IOCTL(hTape, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    err_exit("rewind", "Rewind Test Failed", rcode);
    return 0;
}

static int
ForwardSpace(usd_handle_t hTape, int cnt)
{
    usd_tapeop_t tapeop;
    int rcode;

    tapeop.tp_op = USDTAPE_FSF;
    tapeop.tp_count = cnt;
    rcode = USD_IOCTL(hTape, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    err_exit("FSF", "FSF Test Failed", rcode);
    return 0;
}

static int
BackSpace(usd_handle_t hTape, int cnt)
{
    usd_tapeop_t tapeop;
    int rcode;

    tapeop.tp_op = USDTAPE_BSF;
    tapeop.tp_count = cnt;
    rcode = USD_IOCTL(hTape, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    err_exit("BSF", "BSF Test Failed", rcode);
    return 0;
}

static int
PrepTape(usd_handle_t hTape)
{
    usd_tapeop_t tapeop;
    int rcode;

    tapeop.tp_op = USDTAPE_PREPARE;
    tapeop.tp_count = 0;
    rcode = USD_IOCTL(hTape, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    err_exit("PREPARE", "Tape Prepare Test Failed", rcode);
    return 0;
}


static int
ShutdownTape(usd_handle_t hTape)
{
    usd_tapeop_t tapeop;
    int rcode;

    tapeop.tp_op = USDTAPE_SHUTDOWN;
    tapeop.tp_count = 0;
    rcode = USD_IOCTL(hTape, USD_IOCTL_TAPEOPERATION, (void *)&tapeop);
    if (rcode) {		/* can't call err_exit() here for fear of recursion */
	printf("%s: Tape Shutdown Failed with code %d\n", whoami, rcode);
	exit(1);
    }
    return 0;
}

#ifdef AFS_NT40_ENV
static int
PrintTapePos(usd_handle_t hTape)
{
    DWORD rcode;
    DWORD part, offLow, offHi;

    rcode =
	GetTapePosition(hTape->handle, TAPE_ABSOLUTE_POSITION, &part, &offLow,
			&offHi);
    if (rcode != NO_ERROR)
	err_exit("GetTapePosition", "Get Tape Pos test Failed", 1);

    if (USDTEST_DEBUG)
	printf("%s: Cur Tape Block : %d \n", whoami, offLow);

    return (offLow);
}
#else
static int
PrintTapePos(usd_handle_t hTape)
{
    afs_hyper_t startOff, stopOff;
    int rcode;

    hset64(startOff, 0, 0);
    hset64(stopOff, 0, 0);

    rcode = USD_SEEK(hTape, startOff, SEEK_CUR, &stopOff);
    err_exit("Seek", "Tape Seek Test Failed", rcode);

    if (USDTEST_DEBUG)
	printf("%s: Cur Tape Pos : %d bytes\n", whoami, stopOff.low);

    return (stopOff.low);
}
#endif
