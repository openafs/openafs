#include "winflock.h"

TCHAR test_dir[MAX_PATH] = _T("");
TCHAR fn_base[MAX_PATH] = _T("");
TCHAR fn_aux[MAX_PATH] = _T("");

HANDLE h_file_base = NULL;
HANDLE h_file_aux = NULL;

void log_last_error(void)
{
    logfile << "GetLastError() == " << GetLastError() << "\n";
}

int begin_tests()
{
    TCHAR file_name[MAX_PATH];

    if(!isChild)
    logfile << "-------Starting tests-----------------------------\n";

    StringCbCopy(file_name, sizeof(file_name), test_dir);
    StringCbCat(file_name, sizeof(file_name), _T("FLTST000"));
    StringCbCopy(fn_base, sizeof(fn_base), file_name);

    logfile << "h_file_base = CreateFile(" << file_name << ") shared\n";

    h_file_base = CreateFile(
        file_name,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        0,
        NULL);

    if(h_file_base == INVALID_HANDLE_VALUE) {
        log_last_error();
        return 1;
    }

    StringCbCopy(file_name, sizeof(file_name), test_dir);
    StringCbCat(file_name, sizeof(file_name), _T("FLTST001"));
    StringCbCopy(fn_aux, sizeof(fn_aux), file_name);

    if(!isChild) {

        logfile << "h_file_aux = CreateFile(" << file_name << ") exclusive\n";

        h_file_aux = CreateFile(
            file_name,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            0,
            NULL);

        if(h_file_aux == INVALID_HANDLE_VALUE) {
            log_last_error();
            return 1;
        }
    }

    return 0;
}

/*  CreateFile:
   - Requesting a sharing mode that conflicts with the access mode specified in a previous
     open reqeuest whose handle is still open should be an error (should return ERROR_SHARING_VIOLATION)
   - If sharing mode is 0, the file cannot be opened again until the handle is closed.
   - Sharing modes should be tested:
     - FILE_SHARE_DELETE
     - FILE_SHARE_READ
     - FILE_SHARE_WRITE
*/
int test_create(void)
{
    HANDLE h;

    if(isChild) {
        logfile << "----Begin CreateFile tests ----\n";

        cerr << 
            "TEST:CREATE:001 Requesting a sharing mode that conflicts with the access mode "
            "specified in a previous open requestion whose handle is still open should be an error.\n";

        cerr <<
            "TEST:CREATE:001:01 Attempt exclusive open of a file which is already opened exclusively\n";

        logfile << "CreateFile(" << fn_aux << ")... exclusive\n";
        h = CreateFile(
            fn_aux,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            0,
            NULL);

        if(h != INVALID_HANDLE_VALUE) {
            logfile << "BAD : CreateFile(" << fn_aux << ") should have failed but didn't\n";
            cerr << "TEST:CREATE:001:01 ***FAILED***\n";
            CloseHandle(h);
        } else {
            logfile << "good: CreateFile(" << fn_aux << ") failed\n";
            cerr << "TEST:CREATE:001:01 PASS (LastError=" << GetLastError() << ")\n";
            if(GetLastError() != ERROR_SHARING_VIOLATION)
                cerr << "TEST:CREATE:001:01 **WARN** LastError != ERROR_SHARING_VIOLATION\n";
        }

        cerr <<
            "TEST:CREATE:001:02 Attempt to open a file with shared read which is already opened exclusively\n";

        logfile << "CreateFile(" << fn_aux << ")... share read\n";

        h = CreateFile(
            fn_aux,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            0,
            NULL);

        if(h != INVALID_HANDLE_VALUE) {
            logfile << "BAD : CreateFile(" << fn_aux << ") should have failed but didn't\n";
            cerr << "TEST:CREATE:001:02 ***FAILED***\n";
            CloseHandle(h);
        } else {
            logfile << "good\n";
            cerr << "TEST:CREATE:001:02 PASS (LastError=" << GetLastError() << ")\n";
            if(GetLastError() != ERROR_SHARING_VIOLATION)
                cerr << "TEST:CREATE:001:02 **WARN** LastError != ERROR_SHARING_VIOLATION\n";
        }

        cerr <<
            "TEST:CREATE:001:03 Attempt to open a file exclusively which is already opened shared\n";

        logfile << "CreateFile(" << fn_base << ")... exclusive\n";

        h = CreateFile(
            fn_base,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            0,
            NULL);

        if(h != INVALID_HANDLE_VALUE) {
            logfile << "BAD : CreateFile(" << fn_base << ") should have failed but didn't\n";
            cerr << "TEST:CREATE:001:03 ***FAILED***\n";
            CloseHandle(h);
        } else {
            logfile << "good\n";
            cerr << "TEST:CREATE:001:03 PASS (LastError=" << GetLastError() << ")\n";
            if(GetLastError() != ERROR_SHARING_VIOLATION)
                cerr << "TEST:CREATE:001:03 **WARN** LastError != ERROR_SHARING_VIOLATION\n";
        }

        cerr <<
            "TEST:CREATE:001:04 Attempt to open a file shared write which is already opened shared r/w\n";

        logfile << "CreateFile(" << fn_base << ")... share write\n";

        h = CreateFile(
            fn_base,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            0,
            NULL);

        if(h != INVALID_HANDLE_VALUE) {
            logfile << "BAD : CreateFile(" << fn_base << ") should have failed but didn't\n";
            cerr << "TEST:CREATE:001:04 ***FAILED***\n";
            CloseHandle(h);
        } else {
            logfile << "good\n";
            cerr << "TEST:CREATE:001:04 PASS (LastError=" << GetLastError() << ")\n";
            if(GetLastError() != ERROR_SHARING_VIOLATION)
                cerr << "TEST:CREATE:001:04 **WARN** LastError != ERROR_SHARING_VIOLATION\n";
        }

        cerr <<
            "TEST:CREATE:001:05 Attempt to open a file shared r/w which is already opened shared r/w\n";

        logfile << "CreateFile(" << fn_base << ")... share r/w\n";

        h = CreateFile(
            fn_base,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            0,
            NULL);

        if(h != INVALID_HANDLE_VALUE) {
            logfile << "good\n";
            cerr << "TEST:CREATE:001:05 PASS\n";
            CloseHandle(h);
        } else {
            logfile << "BAD : CreateFile(" << fn_base << ") failed!\n";
            cerr << "TEST:CREATE:001:05 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        }

        logfile << "----End CreateFile tests ----\n";
    }

    return 0;
}

/* prepare the test file by filling it in with the test pattern */
int test_lock_prep(void)
{
    int i,j;
    DWORD * buffer;
    DWORD nb;
    if(!isChild) {
        logfile << "----Preparing test file----\n";
        /* buffer is 4k */
        buffer = (DWORD *) malloc(sizeof(DWORD) * 1024);

        assert(buffer != NULL);
        logfile << "SetFilePointer(h_file_base, 0, NULL, FILE_BEGIN)\n";
        SetFilePointer(h_file_base, 0, NULL, FILE_BEGIN);
        for(j=0; j<256; j++) {
            for(i=0; i<1024; i++) {
                buffer[i] = j;
            }

            logfile << "WriteFile(h_file_base, (LPCVOID) buffer, sizeof(DWORD) * 1024, &nb, NULL)\n";

            if(!WriteFile(h_file_base, (LPCVOID) buffer, sizeof(DWORD) * 1024, &nb, NULL)) {
                logfile << "WARNING**: WriteFile error=" << GetLastError() << "\n";
            } else if(nb != sizeof(DWORD) * 1024) {
                logfile << "WARNING**: WriteFile underrun (j=" << j << ",nb=" << nb << ")\n";
            }
        }

        free(buffer);

        SetEndOfFile(h_file_base);
        FlushFileBuffers(h_file_base);

        logfile << "----End Preparing test file---\n";
    }

    return 0;
}

/* Test exclusive locks below EOF

   testint_* functions manage their own cross process synchronization
*/
int testint_lock_excl_beof(void)
{
    /* at this point, test_lock_prep() has already run, and the length
       of h_file_base is set at 1M */
    SYNC_BEGIN_PARENT {
        logfile << "----Begin Lock Test Exclusive BEOF----\n";

        cerr << "TEST:LOCK:001 Exclusive byte-range locks below EOF\n";

        /* parent takes three non-overlapping locks */
        logfile << "LockFile(h_file_base, PAGE_BEGIN(10), 0, PAGE_LEN(10), 0)\n";
        if(!LockFile(h_file_base, PAGE_BEGIN(10), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: LockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:001 ***ERROR*** Setup failed!\n";
        }

        logfile << "LockFile(h_file_base, 4096 * 30, 0, 4096 * 10, 0)\n";
        if(!LockFile(h_file_base, PAGE_BEGIN(30), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: LockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:001 ***ERROR*** Setup failed!\n";
        }

        logfile << "LockFile(h_file_base, 4096 * 62, 0, 4096 * 1, 0)\n";
        if(!LockFile(h_file_base, PAGE_BEGIN(62), 0, PAGE_LEN(1), 0)) {
            logfile << "ERROR**: LockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:001 ***ERROR*** Setup failed!\n";
        }
    } SYNC_END_PARENT;

    SYNC_BEGIN_PARENT {
        logfile << "--Test using same handle, same process--\n";
        cerr <<
            "TEST:LOCK:001:01 Test overlapping locks within same process (complete overlap)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(15), 0, PAGE_LEN(3), 0)) {
            cerr << "TEST:LOCK:001:01 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:001:01 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(15), 0, PAGE_LEN(3), 0))
                cerr << "TEST:LOCK:001:01 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:001:02 Test overlapping locks within same process (partial overlap A)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(15), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:001:02 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:001:02 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(15), 0, PAGE_LEN(10), 0))
                cerr << "TEST:LOCK:001:02 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:001:03 Test overlapping locks within same process (partial overlap B)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(25), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:001:03 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:001:03 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(25), 0, PAGE_LEN(10), 0))
                cerr << "TEST:LOCK:001:03 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:001:04 Test non-overlapping locks with same process\n";
        cerr <<
            "TEST:LOCK:001:04 PASS (iff Setup succeeded)\n";

    } SYNC_END_PARENT;

    SYNC_BEGIN_CHILD {
        logfile << "--Test using child process--\n";
        cerr <<
            "TEST:LOCK:001:05 Test overlapping locks with different process (complete overlap)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(15), 0, PAGE_LEN(3), 0)) {
            cerr << "TEST:LOCK:001:05 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:001:05 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(15), 0, PAGE_LEN(3), 0))
                cerr << "TEST:LOCK:001:05 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:001:06 Test overlapping locks with different process (partial overlap A)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(15), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:001:06 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:001:06 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(15), 0, PAGE_LEN(10), 0))
                cerr << "TEST:LOCK:001:06 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:001:07 Test overlapping locks with different process (partial overlap B)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(25), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:001:07 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:001:07 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(25), 0, PAGE_LEN(10), 0))
                cerr << "TEST:LOCK:001:07 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:001:08 Test non-overlapping lock with different process\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(50), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:001:08 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:001:08 PASS\n";
            /* leave lock held */
        }
    } SYNC_END_CHILD;

    return 0;
}

/* by now we have the following exclusive locks:

pages 10-19 : exclusive by parent
pages 30-39 : exclusive by parent
pages 50-59 : exclusive by child
pages 0-9   : free-for-all
pages 20-29 : free-for-all
pages 40-49 : free-for-all
pages 60-61 : free-for-all
page  62    : exclusive by parent
pages 63-255: free-for-all
pages 256-  : non-existent
*/


int testint_lock_excl_rw_beof(void)
{
    DWORD * read_buf;
    DWORD * write_buf;
    DWORD nbytes;

    /* each of read_buf and write_buf are 10 pages long */
    read_buf = (DWORD *) malloc(sizeof(DWORD) * 10240);
    assert(read_buf != NULL);

    write_buf = (DWORD *) malloc(sizeof(DWORD) * 10240);
    assert(write_buf != NULL);

    SYNC_BEGIN_PARENT {
        logfile << "----Test writes and reads on exclusive locks----\n";
        logfile << "--Read tests--\n";

        cerr << "TEST:LOCK:002 Read tests in the presence of locks\n";

        cerr << "TEST:LOCK:002:01 Read in unlocked area\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(42), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(42), NULL, FILE_BEGIN) != PAGE_BEGIN(42)) {
            cerr << "TEST:LOCK:002:01 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!ReadFile(h_file_base, (LPVOID) read_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:002:01 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:002:01 PASS\n";
        }
        
        cerr << "TEST:LOCK:002:02 Read in partially locked area (A)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(27), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(27), NULL, FILE_BEGIN) != PAGE_BEGIN(27)) {
            cerr << "TEST:LOCK:002:02 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!ReadFile(h_file_base, (LPVOID) read_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:002:02 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:002:02 PASS\n";
        }
        
        cerr << "TEST:LOCK:002:03 Read in locked area\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(32), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(32), NULL, FILE_BEGIN) != PAGE_BEGIN(32)) {
            cerr << "TEST:LOCK:002:03 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!ReadFile(h_file_base, (LPVOID) read_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:002:03 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:002:03 PASS\n";
        }
        
        cerr << "TEST:LOCK:002:04 Read in partially locked area (B)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(37), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(37), NULL, FILE_BEGIN) != PAGE_BEGIN(37)) {
            cerr << "TEST:LOCK:002:04 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!ReadFile(h_file_base, (LPVOID) read_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:002:04 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:002:04 PASS\n";
        }
         
        cerr << "TEST:LOCK:002:05 Read in partially unowned area (A)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(47), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(47), NULL, FILE_BEGIN) != PAGE_BEGIN(47)) {
            cerr << "TEST:LOCK:002:05 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!ReadFile(h_file_base, (LPVOID) read_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:002:05 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:002:05 ***FAILED***\n";
        }
        
        cerr << "TEST:LOCK:002:06 Read in fully unowned area\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(52), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(52), NULL, FILE_BEGIN) != PAGE_BEGIN(52)) {
            cerr << "TEST:LOCK:002:06 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!ReadFile(h_file_base, (LPVOID) read_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:002:06 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:002:06 ***FAILED***\n";
        }
        
        cerr << "TEST:LOCK:002:07 Read in partially unowned area (B)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(56), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(56), NULL, FILE_BEGIN) != PAGE_BEGIN(56)) {
            cerr << "TEST:LOCK:002:07 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!ReadFile(h_file_base, (LPVOID) read_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:002:07 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:002:07 ***FAILED***\n";
        }

        cerr << "TEST:LOCK:002:08 Read in partially unowned area (C)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(59), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(59), NULL, FILE_BEGIN) != PAGE_BEGIN(59)) {
            cerr << "TEST:LOCK:002:08 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!ReadFile(h_file_base, (LPVOID) read_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:002:08 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:002:08 ***FAILED***\n";
        }
    } SYNC_END_PARENT;

    SYNC_BEGIN_CHILD {
        int i;
        int j;

        for(j=0; j<10; j++) {
            for(i=0; i<1024; i++) {
                write_buf[j*1024 + i] = 0x0d0d0d0d;
            }
        }

        cerr << "TEST:LOCK:003 Write tests in the presence of locks\n";
        logfile << "--Write tests--\n";

        cerr << "TEST:LOCK:003:01 Write in unlocked area\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(42), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(42), NULL, FILE_BEGIN) != PAGE_BEGIN(42)) {
            cerr << "TEST:LOCK:003:01 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!WriteFile(h_file_base, (LPCVOID) write_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:003:01 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:003:01 PASS\n";
        }

        cerr << "TEST:LOCK:003:02 Write in partially owned area (A)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(47), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(47), NULL, FILE_BEGIN) != PAGE_BEGIN(47)) {
            cerr << "TEST:LOCK:003:02 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!WriteFile(h_file_base, (LPCVOID) write_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:003:02 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:003:02 PASS\n";
        }

        cerr << "TEST:LOCK:003:03 Write in fully owned area\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(52), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(52), NULL, FILE_BEGIN) != PAGE_BEGIN(52)) {
            cerr << "TEST:LOCK:003:03 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!WriteFile(h_file_base, (LPCVOID) write_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:003:03 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:003:03 PASS\n";
        }

        cerr << "TEST:LOCK:003:04 Write in partially owned area (B)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(56), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(56), NULL, FILE_BEGIN) != PAGE_BEGIN(56)) {
            cerr << "TEST:LOCK:003:04 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!WriteFile(h_file_base, (LPCVOID) write_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:003:04 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:003:04 PASS\n";
        }

        cerr << "TEST:LOCK:003:05 Write in partially unowned area (A)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(27), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(27), NULL, FILE_BEGIN) != PAGE_BEGIN(27)) {
            cerr << "TEST:LOCK:003:05 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!WriteFile(h_file_base, (LPCVOID) write_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:003:05 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:003:05 ***FAILED***\n";
        }

        cerr << "TEST:LOCK:003:06 Write in fully unowned area\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(32), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(32), NULL, FILE_BEGIN) != PAGE_BEGIN(32)) {
            cerr << "TEST:LOCK:003:06 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!WriteFile(h_file_base, (LPCVOID) write_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:003:06 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:003:06 ***FAILED***\n";
        }

        cerr << "TEST:LOCK:003:07 Write in partially unowned area (B)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(37), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(37), NULL, FILE_BEGIN) != PAGE_BEGIN(37)) {
            cerr << "TEST:LOCK:003:07 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!WriteFile(h_file_base, (LPCVOID) write_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:003:07 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:003:07 ***FAILED***\n";
        }

        cerr << "TEST:LOCK:003:08 Write in partially unowned area (C)\n";

        logfile << "SetFilePointer(h_file_base, PAGE_BEGIN(59), NULL, FILE_BEGIN)\n";
        if(SetFilePointer(h_file_base, PAGE_BEGIN(59), NULL, FILE_BEGIN) != PAGE_BEGIN(59)) {
            cerr << "TEST:LOCK:003:08 ERROR** Setup Failed!\n";
            logfile << "**ERROR** Last error=" << GetLastError() << "\n";
        }
        if(!WriteFile(h_file_base, (LPCVOID) write_buf, PAGE_LEN(5), &nbytes, NULL)) {
            cerr << "TEST:LOCK:003:08 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:003:08 ***FAILED***\n";
        }

        FlushFileBuffers(h_file_base);

    } SYNC_END_CHILD;

    free(write_buf);
    free(read_buf);

    return 0;
}

int testint_lock_excl_eeof(void)
{
    /* at this point, test_lock_prep() has already run, and the length
       of h_file_base is set at 1M */
    SYNC_BEGIN_PARENT {
        logfile << "----Begin Lock Test Exclusive EEOF----\n";

        cerr << "TEST:LOCK:004 Exclusive byte-range locks above EOF\n";

        /* parent takes three non-overlapping locks */
        logfile << "LockFile(h_file_base, PAGE_BEGIN(256+10), 0, PAGE_LEN(10), 0)\n";
        if(!LockFile(h_file_base, PAGE_BEGIN(256+10), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: LockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:004 ***ERROR*** Setup failed!\n";
        }

        logfile << "LockFile(h_file_base, PAGE_BEGIN(256+30), 0, 4096 * 10, 0)\n";
        if(!LockFile(h_file_base, PAGE_BEGIN(256+30), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: LockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:004 ***ERROR*** Setup failed!\n";
        }

        logfile << "LockFile(h_file_base, PAGE_BEGIN(256+62), 0, 4096 * 1, 0)\n";
        if(!LockFile(h_file_base, PAGE_BEGIN(256+62), 0, PAGE_LEN(1), 0)) {
            logfile << "ERROR**: LockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:004 ***ERROR*** Setup failed!\n";
        }
    } SYNC_END_PARENT;

    SYNC_BEGIN_PARENT {
        logfile << "--Test using same handle, same process--\n";
        cerr <<
            "TEST:LOCK:004:01 Test overlapping locks within same process (complete overlap)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(256+15), 0, PAGE_LEN(3), 0)) {
            cerr << "TEST:LOCK:004:01 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:004:01 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(256+15), 0, PAGE_LEN(3), 0))
                cerr << "TEST:LOCK:004:01 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:004:02 Test overlapping locks within same process (partial overlap A)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(256+15), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:004:02 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:004:02 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(256+15), 0, PAGE_LEN(10), 0))
                cerr << "TEST:LOCK:004:02 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:004:03 Test overlapping locks within same process (partial overlap B)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(256+25), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:004:03 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:004:03 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(256+25), 0, PAGE_LEN(10), 0))
                cerr << "TEST:LOCK:004:03 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:004:04 Test non-overlapping locks with same process\n";
        cerr <<
            "TEST:LOCK:004:04 PASS (iff Setup succeeded)\n";

    } SYNC_END_PARENT;

    SYNC_BEGIN_CHILD {
        logfile << "--Test using child process--\n";
        cerr <<
            "TEST:LOCK:004:05 Test overlapping locks with different process (complete overlap)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(256+15), 0, PAGE_LEN(3), 0)) {
            cerr << "TEST:LOCK:004:05 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:004:05 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(256+15), 0, PAGE_LEN(3), 0))
                cerr << "TEST:LOCK:004:05 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:004:06 Test overlapping locks with different process (partial overlap A)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(256+15), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:004:06 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:004:06 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(256+15), 0, PAGE_LEN(10), 0))
                cerr << "TEST:LOCK:004:06 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:004:07 Test overlapping locks with different process (partial overlap B)\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(256+25), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:004:07 PASS (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:004:07 ***FAILED***\n";
            if(!UnlockFile(h_file_base, PAGE_BEGIN(256+25), 0, PAGE_LEN(10), 0))
                cerr << "TEST:LOCK:004:07 ****BADNESS**** UnlockFile failed with " << GetLastError() << "\n";
        }

        cerr <<
            "TEST:LOCK:004:08 Test non-overlapping lock with different process\n";

        if(!LockFile(h_file_base, PAGE_BEGIN(256+50), 0, PAGE_LEN(10), 0)) {
            cerr << "TEST:LOCK:004:08 ***FAILED*** (LastError=" << GetLastError() << ")\n";
        } else {
            cerr << "TEST:LOCK:004:08 PASS\n";
            /* leave lock held */
        }
    } SYNC_END_CHILD;

    return 0;
}

/* not necessary */
int testint_lock_excl_rw_eeof(void)
{
    return 0;
}

/* unlock all the remaining locks */
int testint_unlock(void)
{
    SYNC_BEGIN_PARENT {
        logfile << "----Begin Unlock test----\n";

        cerr << "TEST:LOCK:005 Unlocks\n";

        cerr << "TEST:LOCK:005:01 Unlock parent locks\n";

        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(10), 0, PAGE_LEN(10), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(10), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:01 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:01 PASS\n";

        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(30), 0, PAGE_LEN(10), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(30), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:02 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:02 PASS\n";


        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(62), 0, PAGE_LEN(1), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(62), 0, PAGE_LEN(1), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:03 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:03 PASS\n";


        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(256+10), 0, PAGE_LEN(10), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(256+10), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:04 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:04 PASS\n";


        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(256+30), 0, PAGE_LEN(10), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(256+30), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:05 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:05 PASS\n";


        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(256+62), 0, PAGE_LEN(1), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(256+62), 0, PAGE_LEN(1), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:06 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:06 PASS\n";

    } SYNC_END_PARENT;

    SYNC_BEGIN_CHILD {
        cerr << "TEST:LOCK:005:02 Unlock parent locks\n";

        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(50), 0, PAGE_LEN(10), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(50), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:07 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:07 PASS\n";


        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(256+50), 0, PAGE_LEN(10), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(256+50), 0, PAGE_LEN(10), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:08 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:08 PASS\n";

    } SYNC_END_CHILD;

    SYNC_BEGIN_PARENT {
        cerr << "TEST:LOCK:006 Check if unlocked really worked\n";

        logfile << "LockFile(h_file_base, PAGE_BEGIN(0), 0, PAGE_LEN(256+60), 0)\n";
        if(!LockFile(h_file_base, PAGE_BEGIN(0), 0, PAGE_LEN(256+60), 0)) {
            logfile << "ERROR**: LockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:006:01 ***ERROR*** Lock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:006:01 PASS\n";

        logfile << "UnlockFile(h_file_base, PAGE_BEGIN(0), 0, PAGE_LEN(256+60), 0)\n";
        if(!UnlockFile(h_file_base, PAGE_BEGIN(0), 0, PAGE_LEN(256+60), 0)) {
            logfile << "ERROR**: UnockFile Failed (last error=" << GetLastError() << ")\n";
            cerr << "TEST:LOCK:005:08 ***ERROR*** Unlock Failed! Error=" << GetLastError() << "\n";
        } else
            cerr << "TEST:LOCK:005:08 PASS\n";
    } SYNC_END_PARENT;

    return 0;
}

int testint_lock_escalation(void)
{
    OVERLAPPED ov;

    ZeroMemory(&ov, sizeof(ov));

    SYNC_BEGIN_PARENT {
        cerr << "TEST:LOCK:006 Lock Escalation\n";
        logfile << "-----------Lock escalation------\n";

        ov.Offset = PAGE_BEGIN(10);
        ov.OffsetHigh = 0;
        if(!LockFileEx(h_file_base, LOCKFILE_FAIL_IMMEDIATELY, 0, PAGE_LEN(10), 0, &ov)) {
            logfile << "ERROR**: Last error = " << GetLastError() << "\n";
            cerr << "TEST:LOCK:006:01 ***FAILED*** Error=" << GetLastError() << "\n";
        } else {
            cerr << "TEST:LOCK:006:01 PASS\n";
        }

        ov.Offset = PAGE_BEGIN(30);
        ov.OffsetHigh = 0;
        if(!LockFileEx(h_file_base, LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY, 0, PAGE_LEN(10), 0, &ov)) {
            logfile << "ERROR**: Last error = " << GetLastError() << "\n";
            cerr << "TEST:LOCK:006:02 ***FAILED*** Error=" << GetLastError() << "\n";
        } else {
            cerr << "TEST:LOCK:006:02 PASS\n";
        }

        ov.Offset = PAGE_BEGIN(50);
        ov.OffsetHigh = 0;
        if(!LockFileEx(h_file_base, LOCKFILE_FAIL_IMMEDIATELY, 0, PAGE_LEN(10), 0, &ov)) {
            logfile << "ERROR**: Last error = " << GetLastError() << "\n";
            cerr << "TEST:LOCK:006:03 ***FAILED*** Error=" << GetLastError() << "\n";
        } else {
            cerr << "TEST:LOCK:006:03 PASS\n";
        }

        ov.Offset = PAGE_BEGIN(50);
        ov.OffsetHigh = 0;
        if(!UnlockFileEx(h_file_base, 0, PAGE_LEN(10), 0, &ov)) {
            logfile << "ERROR**: Last error = " << GetLastError() << "\n";
            cerr << "TEST:LOCK:006:04 ***FAILED*** Error=" << GetLastError() << "\n";
        } else {
            cerr << "TEST:LOCK:006:04 PASS\n";
        }

        ov.Offset = PAGE_BEGIN(30);
        ov.OffsetHigh = 0;
        if(!UnlockFileEx(h_file_base, 0, PAGE_LEN(10), 0, &ov)) {
            logfile << "ERROR**: Last error = " << GetLastError() << "\n";
            cerr << "TEST:LOCK:006:05 ***FAILED*** Error=" << GetLastError() << "\n";
        } else {
            cerr << "TEST:LOCK:006:05 PASS\n";
        }

        ov.Offset = PAGE_BEGIN(10);
        ov.OffsetHigh = 0;
        if(!UnlockFileEx(h_file_base, 0, PAGE_LEN(10), 0, &ov)) {
            logfile << "ERROR**: Last error = " << GetLastError() << "\n";
            cerr << "TEST:LOCK:006:06 ***FAILED*** Error=" << GetLastError() << "\n";
        } else {
            cerr << "TEST:LOCK:006:06 PASS\n";
        }

    } SYNC_END_PARENT;

    return 0;
}

int end_tests()
{
    if(h_file_base != NULL) {
        logfile << "Closing h_file_base\n";
        CloseHandle(h_file_base);
    }
    if(h_file_aux != NULL) {
        logfile << "Closing h_file_aux\n";
        CloseHandle(h_file_aux);
    }

    logfile << "--------Ending tests\n";

    return 0;
}
