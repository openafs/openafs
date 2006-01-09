#pragma once

#include<windows.h>
#include<tchar.h>
#include<iostream>
#include<cassert>

#include<strsafe.h>

using namespace std;

extern _TCHAR test_dir[MAX_PATH];
extern BOOL isChild;
extern HANDLE h_child;
extern HANDLE event_child;
extern HANDLE event_parent;
extern HANDLE mutex_logfile;

#define logfile cout

void _begin_log(void);
void _end_log(void);
void _sync_begin_parent(void);
void _sync_end_parent(void);
void _sync_begin_child(void);
void _sync_end_child(void);

#define BEGINLOG()  _begin_log()
#define ENDLOG()    _end_log()

/*
SYNC_BEGIN_PARENT {
    ... parent code here ...
} SYNC_END_PARENT;
*/

#define SYNC_BEGIN_PARENT   \
    _sync_begin_parent();   \
    if(!isChild)

#define SYNC_END_PARENT   \
    _sync_end_parent()

/*
SYNC_BEGIN_CHILD {
    ... child code here ...
} SYNC_END_CHILD;
*/
#define SYNC_BEGIN_CHILD    \
    _sync_begin_child();    \
    if(isChild)

#define SYNC_END_CHILD      \
    _sync_end_child()

int begin_tests(void);

int test_create(void);
int test_lock_prep(void);
int testint_lock_excl_beof(void);
int testint_lock_excl_rw_beof(void);
int testint_lock_excl_eeof(void);
int testint_waitlock(void);
int test_waitlock_parent(void);
int test_waitlock_child(void);
int testint_unlock(void);
int testint_lock_escalation(void);

int end_tests(void);

#define PAGE_BEGIN(x) ((x) * 4096)
#define PAGE_LEN(x)   ((x) * 4096)