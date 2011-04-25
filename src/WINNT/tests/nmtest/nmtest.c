/*
 * Copyright (c) 2009 - Secure Endpoints Inc.
 *
 * Author: Asanka Herath <asanka@secure-endpoints.com>
 */

#include<windows.h>
#include<wincrypt.h>
#include<stdlib.h>
#include<stdio.h>

#define BLOCKSIZE 1024
#define MAX_PARAM 1

typedef __int64 offset_t;

HCRYPTPROV h_prov = 0;

offset_t N = 0;
offset_t M = 0;
const char * filename = "";
BOOL  verify = FALSE;
BOOL  show_offsets = FALSE;

typedef struct hash_data {
    offset_t offset;
    DWORD    param;
} hash_data;

#define ROUNDUP(x, align) (((((x) - 1) / (align)) + 1) * (align))

void dump_hex(BYTE * buffer, int cb)
{
    static const char *htable[] = {
        "0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"
    };
    int i;

    for (i=0; i < cb; i++) {
        if ((i % 16) == 0) {
            fprintf(stderr, "\n%08X : ", i);
        }

        fprintf(stderr, "%s%s",
                htable[(buffer[i] >> 4) & 0xf],
                htable[(buffer[i] & 0xf)]);

        if ((i % 16) == 8)
            fprintf(stderr, "-");
        else
            fprintf(stderr, " ");
    }

    fprintf(stderr, "\n");
}

#define CCall(f) if (!f) goto done;

BOOL init_test_data(void)
{
    return CryptAcquireContext(&h_prov, NULL, MS_DEF_PROV, PROV_RSA_FULL,
                               CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
}

void exit_test_data(void)
{
    if (h_prov)
        CryptReleaseContext(h_prov, 0);
    h_prov = 0;
}

BOOL generate_test_data_block(offset_t offset, DWORD param, BYTE buffer[BLOCKSIZE])
{
    HCRYPTHASH h_hash = 0;
    HCRYPTKEY  h_key = 0;
    hash_data  d;
    DWORD      cb_data;
    BOOL rv = FALSE;

    ZeroMemory(&d, sizeof(d));

    d.offset = offset;
    d.param = param;

    CCall(CryptCreateHash(h_prov, CALG_SHA1, 0, 0, &h_hash));
    CCall(CryptHashData(h_hash, (BYTE *) &d, sizeof(d), 0));
    CCall(CryptDeriveKey(h_prov, CALG_RC4, h_hash, CRYPT_NO_SALT, &h_key));

    ZeroMemory(buffer, BLOCKSIZE);
    cb_data = BLOCKSIZE;
    CCall(CryptEncrypt(h_key, 0, TRUE, 0, buffer, &cb_data, cb_data));

    //dump_hex(buffer, 32);

    rv = TRUE;
 done:

    if (h_hash)
        CryptDestroyHash(h_hash);
    if (h_key)
        CryptDestroyKey(h_key);

    return rv;
}

BOOL verify_test_data_block(offset_t offset, DWORD param, BYTE buffer[BLOCKSIZE])
{
    BYTE expected[BLOCKSIZE];

    if (!generate_test_data_block(offset, param, expected)) {
        return FALSE;
    }
    if (!memcmp(expected, buffer, BLOCKSIZE)) {
        if (param > 0 && show_offsets)
            printf("... [%I64d]\n", offset, param);
        return TRUE;
    }

    return FALSE;
}

typedef struct bitmap {
    DWORD    magic;
    offset_t length;
    offset_t o_data;
    BYTE     bits[1];
} bitmap;

#define BMAGIC 0xbeefface

#define NBITMAPBYTES(n) (ROUNDUP((n)/BLOCKSIZE, 8)/8)
#define BITMAPSIZE(n) (sizeof(bitmap) + NBITMAPBYTES(n) - 1)
#define FILESIZE(n)   (DATAOFFSET(n) + (n))
#define DATAOFFSET(n) ROUNDUP(BITMAPSIZE(n), BLOCKSIZE)

bitmap * allocbits(offset_t N)
{
    bitmap * b;

    b = malloc(BITMAPSIZE(N));
    if (b == NULL)
        return NULL;

    memset(b, 0, BITMAPSIZE(N));

    b->magic = BMAGIC;
    b->length = N;
    b->o_data = DATAOFFSET(N);

    return b;
}

void freebits(bitmap * b)
{
    if (b)
        free(b);
}

void setbit(bitmap * b, offset_t o)
{
    if (o < b->o_data || o >= b->o_data + b->length || (o % BLOCKSIZE) != 0) {
        fprintf(stderr, "Internal error. Invalid offset\n");
        exit(1);
    }

    o = (o - b->o_data) / BLOCKSIZE;

    b->bits[o / 8] |= (1 << (o & 7));
}

BOOL getbit(bitmap * b, offset_t o)
{
    if (o < b->o_data || o >= b->o_data + b->length || (o % BLOCKSIZE) != 0) {
        fprintf(stderr, "Internal error. Invalid offset\n");
        exit(1);
    }

    o = (o - b->o_data) / BLOCKSIZE;

    return !!(b->bits[o / 8] & (1 << (o & 7)));
}

BOOL write_bitmap(HANDLE h, bitmap * b)
{
    LARGE_INTEGER li;
    OVERLAPPED ov;
    offset_t b_size, o;
    BOOL success = FALSE;

    ZeroMemory(&ov, sizeof(ov));

    b_size = BITMAPSIZE(b->length);

    li.QuadPart = 0;

    if (!SetFilePointerEx(h, li, &li, FILE_BEGIN)) {
        fprintf(stderr, "Can't set file pointer. GLE=%d\n", GetLastError());
        goto done;
    }

    for (o = 0; o < b_size; o += BLOCKSIZE) {
        BYTE buffer[BLOCKSIZE];
        DWORD n_written = 0;

        if (o + BLOCKSIZE <= b_size) {
            memcpy(buffer, ((BYTE *) b) + o, BLOCKSIZE);
        } else {
            memcpy(buffer, ((BYTE *) b) + o, (b_size - o));
            memset(buffer + (b_size - o), 0, BLOCKSIZE - (b_size - o));
        }

        li.QuadPart = o;
        ov.Offset = li.LowPart;
        ov.OffsetHigh = li.HighPart;

        if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, BLOCKSIZE, 0, &ov)) {
            fprintf(stderr, "Can't lock file. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!WriteFile(h, buffer, BLOCKSIZE, &n_written, NULL) || n_written != BLOCKSIZE) {
            fprintf(stderr, "Can't write data. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!UnlockFileEx(h, 0, BLOCKSIZE, 0, &ov)) {
            fprintf(stderr, "Can't unlock file. GLE=%d\n", GetLastError());
            goto done;
        }
    }

    li.QuadPart = 0;
    if (!SetFilePointerEx(h, li, &li, FILE_CURRENT)) {
        fprintf(stderr, "Can't set file pointer. GLE=%d\n", GetLastError());
        goto done;
    }

    if (li.QuadPart != b->o_data) {
        fprintf(stderr, "Current file pointer is not at start of data.\n");
        goto done;
    }

    success = TRUE;

 done:

    return success;
}

bitmap* read_bitmap(HANDLE h)
{
    LARGE_INTEGER li;
    OVERLAPPED ov;
    bitmap * bfile = NULL;
    bitmap * bprep = NULL;
    bitmap * brv = NULL;
    BYTE buffer[BLOCKSIZE];
    DWORD n_read = 0;
    offset_t o;

    ZeroMemory(&ov, sizeof(ov));

    li.QuadPart = 0;

    if (!SetFilePointerEx(h, li, &li, FILE_BEGIN)) {
        fprintf(stderr, "Can't set file pointer. GLE=%d\n", GetLastError());
        goto done;
    }

    ov.Offset = 0;
    ov.OffsetHigh = 0;

    if (!LockFileEx(h, 0, 0, BLOCKSIZE, 0, &ov)) {
        fprintf(stderr, "Can't lock file. GLE=%d\n", GetLastError());
        goto done;
    }

    if (!ReadFile(h, buffer, BLOCKSIZE, &n_read, NULL) || n_read != BLOCKSIZE) {
        fprintf(stderr, "Can't read data. GLE=%d\n", GetLastError());
        goto done;
    }

    if (!UnlockFileEx(h, 0, BLOCKSIZE, 0, &ov)) {
        fprintf(stderr, "Can't unlock file. GLE=%d\n", GetLastError());
        goto done;
    }

    bfile = (bitmap *)buffer;

    if (bfile->magic != BMAGIC) {
        fprintf(stderr, "Corrupt data. Magic number is invalid\n");
        goto done;
    }

    if (!GetFileSizeEx(h, &li)) {
        fprintf(stderr, "Can't get file size. GLE=%d\n", GetLastError());
        goto done;
    }

    if (li.QuadPart != FILESIZE(bfile->length)) {
        fprintf(stderr, "Corrupt data. Invalid file size.\n");
        goto done;
    }

    if (bfile->o_data != DATAOFFSET(bfile->length)) {
        fprintf(stderr, "Corrupt data. Invalid data offset.\n");
        goto done;
    }

    bprep = allocbits(bfile->length);

    if (bprep == NULL) {
        fprintf(stderr, "Can't allocate memory for bitmap\n");
        goto done;
    }

    memcpy(bprep, bfile, __min(BITMAPSIZE(bfile->length), BLOCKSIZE));

    for (o = BLOCKSIZE; o < BITMAPSIZE(bprep->length); o += BLOCKSIZE) {
        li.QuadPart = o;
        ov.Offset = li.LowPart;
        ov.OffsetHigh = li.HighPart;

        if (!LockFileEx(h, 0, 0, BLOCKSIZE, 0, &ov)) {
            fprintf(stderr, "Can't lock file. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!ReadFile(h, buffer, BLOCKSIZE, &n_read, NULL) || n_read != BLOCKSIZE) {
            fprintf(stderr, "Can't read data. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!UnlockFileEx(h, 0, BLOCKSIZE, 0, &ov)) {
            fprintf(stderr, "Can't unlock file. GLE=%d\n", GetLastError());
            goto done;
        }

        memcpy(((BYTE *) bprep) + o, buffer, __min(BITMAPSIZE(bprep->length) - o, BLOCKSIZE));
    }

    li.QuadPart = 0;
    if (!SetFilePointerEx(h, li, &li, FILE_CURRENT)) {
        fprintf(stderr, "Can't set file pointer. GLE=%d\n", GetLastError());
        goto done;
    }

    if (li.QuadPart != bprep->o_data) {
        fprintf(stderr, "Current file pointer not at start of data.\n");
        goto done;
    }

    brv = bprep;

 done:
    if (brv == NULL && bprep != NULL) {
        freebits(bprep);
    }
    return brv;
}

int do_verify_test(void)
{
    HANDLE h_file = NULL;
    int rv = 1;
    offset_t offset;
    LARGE_INTEGER li;
    OVERLAPPED ov;
    bitmap * b = NULL;

    printf("Verifying test data file [%s]\n", filename);

    h_file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Can't open file [%s] GLE=%d\n", filename, GetLastError());
        goto done;
    }

    b = read_bitmap(h_file);

    if (b == NULL) {
        goto done;
    }

    N = b->length;

    printf("File size N = %I64d\n", N);

    if (!init_test_data()) {
        fprintf(stderr, "Initialize crypto. GLE=%d\n", GetLastError());
        goto done;
    }

    printf("Verifying data ... \n");

    ZeroMemory(&ov, sizeof(ov));

    for (offset = b->o_data; offset < b->o_data+N; offset += BLOCKSIZE) {
        BYTE buffer[BLOCKSIZE];
        DWORD n_read = 0;

        li.QuadPart = offset;
        ov.Offset = li.LowPart;
        ov.OffsetHigh = li.HighPart;

        if (!LockFileEx(h_file, 0, 0, BLOCKSIZE, 0, &ov)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't lock file. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!ReadFile(h_file, buffer, BLOCKSIZE, &n_read, NULL) || n_read != BLOCKSIZE) {
            printf("ERROR\n");
            fprintf(stderr, "Can't read data. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!verify_test_data_block(offset, ((getbit(b, offset))? 1 : 0), buffer)) {
            printf("VERIFY FAILED\n");
            fprintf(stderr, "Verification failed at offset %I64d\n", offset);
            goto done;
        }

        if (!UnlockFileEx(h_file, 0, BLOCKSIZE, 0, &ov)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't unlock file. GLE=%d\n", GetLastError());
            goto done;
        }
    }

    printf("Verify succeeded!\n");

    rv = 0;

 done:
    if (b)
        freebits(b);

    if (h_file)
        CloseHandle(h_file);

    exit_test_data();

    return rv;
}

int do_write_test(void)
{
    HANDLE h_file = NULL;
    int rv = 1;
    offset_t offset;
    OVERLAPPED ov;
    bitmap * b = NULL;

    printf("Generating test data file [%s]\n", filename);

    N = (((N - 1)/BLOCKSIZE) + 1) * BLOCKSIZE;
    M = (((M - 1)/BLOCKSIZE) + 1) * BLOCKSIZE;

    printf("Using N = %I64d and M = %I64d\n", N, M);

    h_file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                        CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Can't create file [%s] GLE=%d\n", filename, GetLastError());
        goto done;
    }

    b = allocbits(N);
    if (b == NULL) {
        fprintf(stderr, "Can't allocate bitmap.\n");
        goto done;
    }

    if (!init_test_data()) {
        fprintf(stderr, "Initialize crypto. GLE=%d\n", GetLastError());
        goto done;
    }

    printf("Phase 1: Generating test data ... ");

    if (!write_bitmap(h_file, b)) {
        goto done;
    }

    ZeroMemory(&ov, sizeof(ov));

    for (offset = b->o_data; offset < b->o_data + N; offset += BLOCKSIZE) {
        BYTE buffer[BLOCKSIZE];
        DWORD n_written = 0;
        LARGE_INTEGER li;

        if (!generate_test_data_block(offset, 0, buffer)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't generate test data. GLE=%d\n", GetLastError());
            goto done;
        }

        li.QuadPart = offset;
        ov.Offset = li.LowPart;
        ov.OffsetHigh = li.HighPart;

        if (!LockFileEx(h_file, LOCKFILE_EXCLUSIVE_LOCK, 0, BLOCKSIZE, 0, &ov)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't lock file. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!WriteFile(h_file, buffer, BLOCKSIZE, &n_written, NULL) || n_written != BLOCKSIZE) {
            printf("ERROR\n");
            fprintf(stderr, "Can't write data. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!UnlockFileEx(h_file, 0, BLOCKSIZE, 0, &ov)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't unlock file. GLE=%d\n", GetLastError());
            goto done;
        }
    }

    printf("Done\n");

    if (!FlushFileBuffers(h_file)) {
        fprintf(stderr, "Can't flush file. GLE=%d\n", GetLastError());
        goto done;
    }

    CloseHandle(h_file);

    h_file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                        OPEN_ALWAYS, FILE_FLAG_RANDOM_ACCESS, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Can't create file [%s] GLE=%d\n", filename, GetLastError());
        goto done;
    }

    printf("Phase 2: Overwriting test data ... \n");

    for (offset = 0; offset < M; offset += BLOCKSIZE) {
        offset_t orandom;
        BYTE buffer[BLOCKSIZE];
        LARGE_INTEGER li;
        DWORD n_written = 0;

        if (!CryptGenRandom(h_prov, sizeof(orandom), (BYTE *) &orandom)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't generate random number. GLE=%d\n", GetLastError());
            goto done;
        }

        if (orandom < 0)
            orandom = -orandom;
        orandom = ((orandom % N) / BLOCKSIZE) * BLOCKSIZE;
        orandom += b->o_data;

        if (show_offsets)
            printf("... [%I64d]\n", orandom);

        if (!generate_test_data_block(orandom, 1, buffer)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't generate test data. GLE=%d\n", GetLastError());
            goto done;
        }

        li.QuadPart = orandom;
        ov.Offset = li.LowPart;
        ov.OffsetHigh = li.HighPart;

        if (!LockFileEx(h_file, LOCKFILE_EXCLUSIVE_LOCK, 0, BLOCKSIZE, 0, &ov)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't lock file. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!SetFilePointerEx(h_file, li, NULL, FILE_BEGIN)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't set file pointer. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!WriteFile(h_file, buffer, BLOCKSIZE, &n_written, NULL) || n_written != BLOCKSIZE) {
            printf("ERROR\n");
            fprintf(stderr, "Can't write data. GLE=%d\n", GetLastError());
            goto done;
        }

        if (!UnlockFileEx(h_file, 0, BLOCKSIZE, 0, &ov)) {
            printf("ERROR\n");
            fprintf(stderr, "Can't unlock file. GLE=%d\n", GetLastError());
            goto done;
        }

        setbit(b, orandom);
    }

    if (!write_bitmap(h_file, b)) {
        goto done;
    }

    printf("Done.\n");

    rv = 0;

 done:
    if (b)
        freebits(b);

    if (h_file)
        CloseHandle(h_file);

    exit_test_data();

    return rv;
}

BOOL show_usage(const char * fn)
{
    fprintf(stderr,
            "%s : Generate or verify test data file.\n"
            "\n"
            "Usage: %s [-r <filename> | -w <N> <M> <filename>]\n"
            "\n"
            "  -w <N> <M> <filename> :\n"
            "     First writes N bytes of random data into <filename> and then\n"
            "     overwrites M bytes with different random data.\n"
            "\n"
            "  -r <filename> : \n"
            "     Verify the contents of <filename>.  Verification succeeds if\n"
            "     the contents of <filename> was generated using the -w option\n",
            fn, fn);
    return FALSE;
}

BOOL parse_cmdline(int argc, char ** argv)
{
    if (argc == 3) {
        if (strcmp(argv[1], "-r"))
            return show_usage(argv[0]);
        verify = TRUE;
        N = 0;
        M = 0;
        filename = argv[2];
        return TRUE;
    }

    if (argc == 5) {
        if (strcmp(argv[1], "-w"))
            return show_usage(argv[0]);
        verify = FALSE;
        N = atol(argv[2]);
        if (N == 0)
            return show_usage(argv[0]);
        M = atol(argv[3]);
        if (M == 0)
            return show_usage(argv[0]);
        filename = argv[4];
        return TRUE;
    }

    return show_usage(argv[0]);
}

int do_tests(void)
{
    if (verify)
        return do_verify_test();
    else
        return do_write_test();
}

int main(int argc, char ** argv)
{
    if (!parse_cmdline(argc, argv))
        return 1;

    return do_tests();
}
