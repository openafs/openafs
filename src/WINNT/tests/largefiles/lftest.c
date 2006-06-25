#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <process.h>

char * teststr = "This is a test string.";



void usage(void) {
    fprintf(stderr, "lftest <path>\n");
    exit(1);
}

int test_write(HANDLE hf, LARGE_INTEGER offset) {
    DWORD dwWritten;
    int ret = 0;

    if (!LockFile(hf, offset.u.LowPart, offset.u.HighPart, 
		   4096, 0)) {
	fprintf(stderr, "Unable to lock offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
	return -1;
    }

    if (!SetFilePointerEx(hf, offset, NULL, FILE_BEGIN)) {
	fprintf(stderr, "Unable to set file pointer to offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
	ret = -1;
	goto unlock;
    }

    if (!WriteFile(hf, teststr, strlen(teststr)+1, &dwWritten, NULL)) {
	fprintf(stderr, "Unable to write test string at offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
	ret = -1;
	goto unlock;
    } else {
	printf("wrote '%s' at offset 0x%08x:%08x\n", teststr, offset.u.HighPart, offset.u.LowPart);
    }

  unlock:
    if (!UnlockFile(hf, offset.u.LowPart, offset.u.HighPart, 
		   4096, 0)) {
	fprintf(stderr, "Unable to unlock offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
	ret = -1;
    }

#if 0
    if (!FlushFileBuffers(hf)) {
	fprintf(stderr, "Flush buffers fails at offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
    }
#endif
    return ret;
}

int test_read(HANDLE hf, LARGE_INTEGER offset) {
    char buffer[256];
    DWORD dwRead;
    int ret = 0;

    if (!LockFile(hf, offset.u.LowPart, offset.u.HighPart, 
		   4096, 0)) {
	fprintf(stderr, "Unable to lock offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
	return -1;
    }

    if (!SetFilePointerEx(hf, offset, NULL, FILE_BEGIN)) {
	fprintf(stderr, "Unable to set file pointer to offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
	ret = -1;
	goto unlock;
    }

    if (!ReadFile(hf, buffer, strlen(teststr)+1, &dwRead, NULL)) {
	fprintf(stderr, "Unable to read test string at offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
	ret = -1;
	goto unlock;
    } else {
	printf("read '%s' (%d bytes) at offset 0x%08x:%08x\n", buffer, dwRead, offset.u.HighPart, offset.u.LowPart);
    }

    if (strcmp(buffer, teststr)) {
	fprintf(stderr, "Test string comparison failure at offset 0x%08x:%08x\n", 
		 offset.u.HighPart, offset.u.LowPart);
	ret = -1;
	goto unlock;
    }

  unlock:
    if (!UnlockFile(hf, offset.u.LowPart, offset.u.HighPart,
		   4096, 0)) {
	fprintf(stderr, "Unable to unlock offset 0x%08x:%08x gle = 0x%08x\n", 
		 offset.u.HighPart, offset.u.LowPart, GetLastError());
	ret = -1;
    }

    return ret;
}

int 
main(int argc, char *argv[]) {
    HANDLE fh;
    __int64 i;
    char cmdline[512];
    LARGE_INTEGER large;

    if (argc == 1) 
	usage();

    if (!SetCurrentDirectory(argv[1])) {
	fprintf(stderr, "unable to set directory to %s\n", argv[1]);
	return 2;
    }

    fh = CreateFile("largefile.test", 
		     GENERIC_READ | GENERIC_WRITE | STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE,
		     FILE_SHARE_READ | FILE_SHARE_WRITE,
		     NULL /* default security */,
		     OPEN_ALWAYS,
		     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_WRITE_THROUGH,
		     NULL );
    
    if (fh == INVALID_HANDLE_VALUE) {
	fprintf(stderr, "unable to create/open the test file\n");
	return 3;
    }

    for ( i=0; i<7; i++ ) {

	large.QuadPart = i * (0x40000000-4);
	test_write(fh, large);

    }

    CloseHandle(fh);

    sprintf(cmdline, "fs.exe flushvolume %s", argv[1]);
    system(cmdline);

    fh = CreateFile("largefile.test", 
		     GENERIC_READ | GENERIC_WRITE | STANDARD_RIGHTS_READ | STANDARD_RIGHTS_WRITE,
		     FILE_SHARE_READ | FILE_SHARE_WRITE,
		     NULL /* default security */,
		     OPEN_ALWAYS,
		     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_WRITE_THROUGH,
		     NULL );
    
    if (fh == INVALID_HANDLE_VALUE) {
	fprintf(stderr, "unable to create/open the test file\n");
	return 3;
    }

    for ( i=0; i<7; i++ ) {

	large.QuadPart = i * (0x40000000-4);
	test_read(fh, large);

    }

    CloseHandle(fh);

    return 0;
}
