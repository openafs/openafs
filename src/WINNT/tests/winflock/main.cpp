#include "winflock.h"

BOOL isChild = FALSE;
HANDLE h_child = NULL;
HANDLE event_child = NULL;
HANDLE event_parent = NULL;
HANDLE mutex_logfile = NULL;

BOOL tst_read_write = TRUE;
BOOL tst_pause = FALSE;

int show_usage(_TCHAR * pname)
{
    cerr << pname << " : WIN32 File Locking Test\n";
    cerr <<
        "Options:\n"
        "    -d <dir>: sets the directory where the test files are to be\n"
        "              created.\n"
        "    -nr     : disable read/write tests\n"
        "    -p      : Pause during the test with the test file locked\n"
        ;
    return 1;
}

int parse_cmd_line(int argc, _TCHAR * argv[])
{
    int i;

    if(argc == 1) {
        return show_usage(argv[0]);
    }

    for(i=1; i<argc; i++) {
        if(!_tcscmp(argv[i], _T("-d"))) {
            if(++i < argc && _tcslen(argv[i]) < MAX_PATH) {
                size_t len;
                StringCbCopy(test_dir, sizeof(test_dir), argv[i]);
                StringCbLength(test_dir, sizeof(test_dir), &len);
                if(len > 0 && test_dir[len-1] != _T('\\'))
                    StringCbCat(test_dir, sizeof(test_dir), _T("\\"));
            } else {
                return show_usage(argv[0]);
            }
        } else if (!_tcscmp(argv[i], _T("-nr"))) {
            tst_read_write = FALSE;        
        } else if(!_tcscmp(argv[i], _T("-child"))) {
            isChild = TRUE;
        } else if(!_tcscmp(argv[i], _T("-p"))) {
            tst_pause = TRUE;
        } else {
            cerr << "Invalid option : " << argv[i] << "\n";
            return show_usage(argv[0]);
        }
    }
    return 0;
}

int spawn_kids(int argc, _TCHAR *argv[])
{
    PROCESS_INFORMATION procinfo;
    STARTUPINFO startinfo;
    TCHAR cmd_line[MAX_PATH];
    size_t len;

    StringCbCopy(cmd_line, sizeof(cmd_line), _T("\""));
    StringCbCat(cmd_line, sizeof(cmd_line), argv[0]);
    StringCbCat(cmd_line, sizeof(cmd_line), _T("\""));
    StringCbCat(cmd_line, sizeof(cmd_line), _T(" -child"));
    if(!tst_read_write)
        StringCbCat(cmd_line, sizeof(cmd_line), _T(" -nr"));
    StringCbLength(test_dir, sizeof(test_dir), &len);
    if(len > 0) {
        StringCbCat(cmd_line, sizeof(cmd_line), _T(" -d "));
        StringCbCat(cmd_line, sizeof(cmd_line), test_dir);
        //_tcscat(cmd_line, _T("\""));
    }

    startinfo.cb = sizeof(startinfo);
    startinfo.lpReserved = NULL;
    startinfo.lpDesktop = NULL;
    startinfo.lpTitle = NULL;
    startinfo.dwFlags = 0;
    startinfo.cbReserved2 = 0;
    
    cerr << "PARENT: Process ID:" << GetCurrentProcessId() << "\n";
    cerr << "PARENT: Spawning child process: " << cmd_line << "\n";

    if(!CreateProcess(
        NULL,
        cmd_line,
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &startinfo,
        &procinfo))
        return 1;

    h_child = procinfo.hProcess;

    if(procinfo.hThread)
        CloseHandle(procinfo.hThread);

    cerr << "PARENT: Waiting for child process...\n";
    cerr.flush();

    WaitForSingleObject(event_parent, INFINITE);

    cerr << "PARENT: Done.\n";
    cerr << "PARENT: Created child process ID: " << procinfo.dwProcessId << "\n";
    cerr.flush();

    return 0;
}

int run_tests(void)
{
    int rv = 0;
    int rvt = 0;

#define PC_CALL(f)      \
    if(!isChild) {      \
        BEGINLOG();     \
        rvt = f;        \
        ENDLOG();       \
        SetEvent(event_child);                          \
        WaitForSingleObject(event_parent, INFINITE);    \
    } else {                                            \
        WaitForSingleObject(event_child, INFINITE);     \
        BEGINLOG();     \
        rvt = f;        \
        ENDLOG();       \
        SetEvent(event_parent);                         \
    }                   \
    rv = (rv | rvt)

#define PCINT_CALL(f)   \
    rvt = f;            \
    rv = (rv | rvt)
    
    PC_CALL(begin_tests());

    PC_CALL(test_create());
    
    if(tst_read_write)
        PC_CALL(test_lock_prep());

    PCINT_CALL(testint_lock_excl_beof());

    if(tst_read_write)
        PCINT_CALL(testint_lock_excl_rw_beof());

    if(tst_read_write)
        PCINT_CALL(testint_lock_excl_eeof());

    if(tst_pause) {
        TCHAR c;
        cin >> c;
    }

    PCINT_CALL(testint_unlock());

    PCINT_CALL(testint_lock_escalation());

    PC_CALL(end_tests());

#undef PC_CALL
#undef PCINT_CALL

    return rv;
}

void cleanup(void)
{
    if(h_child)
        CloseHandle(h_child);
}

void create_sync_objects(void)
{
    event_child = CreateEvent(
        NULL,
        FALSE,
        FALSE,
        _T("Local\\WinFLockChildEvent"));

    assert(event_child != NULL);

    event_parent = CreateEvent(
        NULL,
        FALSE,
        FALSE,
        _T("Local\\WinFLockParentEvent"));

    assert(event_parent != NULL);

    mutex_logfile = CreateMutex(
        NULL,
        FALSE,
        _T("Local\\WinFLockLogFileMutex"));

    assert(mutex_logfile != NULL);
}

void free_sync_objects(void)
{
    if(event_child)
        CloseHandle(event_child);
    if(event_parent)
        CloseHandle(event_parent);
    if(mutex_logfile)
        CloseHandle(mutex_logfile);
}

int _tmain(int argc, _TCHAR * argv[])
{
    int rv;

    rv = parse_cmd_line(argc, argv);
    if(rv != 0)
        return rv;

    create_sync_objects();

    if(!isChild) {
        if(spawn_kids(argc, argv))
            return 1;
    } else {
        SetEvent(event_parent);
    }

    rv = run_tests();

    free_sync_objects();

    cleanup();
    return rv;
}
