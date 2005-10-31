#include "winflock.h"

void _begin_log(void) 
{
    WaitForSingleObject(mutex_logfile, INFINITE);
    logfile << (isChild?"CHILD {\n":"PARENT {\n");
}

void _end_log(void) 
{
    logfile << "}\n";
    logfile.flush();
    ReleaseMutex(mutex_logfile);
}

void _sync_begin_parent(void)
{
    if(!isChild) {
        BEGINLOG();
    } else {
        WaitForSingleObject(event_child, INFINITE);
    }
}

void _sync_end_parent(void)
{
    if(!isChild) {
        ENDLOG();
        SetEvent(event_child);
    } else {
    }
}

void _sync_begin_child(void)
{
    if(!isChild) {
        WaitForSingleObject(event_parent, INFINITE);
    } else {
        BEGINLOG();
    }
}

void _sync_end_child(void)
{
    if(!isChild) {
    } else {
        ENDLOG();
        SetEvent(event_parent);
    }
}
