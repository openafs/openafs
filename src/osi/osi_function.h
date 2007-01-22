/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_OSI_FUNCTION_H
#define	_OSI_OSI_FUNCTION_H


/*
 * osi common function support package
 */


/*
 * abstract function prologue and epilogue support
 */


/*
 * internal support macros
 */
#define _osi_Function_Args0() (void)
#define _osi_Function_Args1(t1, v1) (t1 v1)
#define _osi_Function_Args2(t1, v1, t2, v2) (t1 v1, t2 v2)
#define _osi_Function_Args3(t1, v1, t2, v2, t3, v3) (t1 v1, t2 v2, t3 v3)
#define _osi_Function_Args4(t1, v1, t2, v2, t3, v3, t4, v4) (t1 v1, t2 v2, t3 v3, t4 v4)
#define _osi_Function_Args5(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5) (t1 v1, t2 v2, t3 v3, t4 v4, t5 v5)
#define _osi_Function_Args6(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6) (t1 v1, t2 v2, t3 v3, t4 v4, t5 v5, t6 v6)
#define _osi_Function_Args7(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6, t7, v7) (t1 v1, t2 v2, t3 v3, t4 v4, t5 v5, t6 v6, t7 v7)
#define _osi_Function_Args8(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6, t7, v7, t8, v8) (t1 v1, t2 v2, t3 v3, t4 v4, t5 v5, t6 v6, t7 v7, t8 v8)


#define _osi_Function_TracePrologue(f) osi_tracepoint_table.##f##_entry_id, osi_TraceFunc_FunctionPrologue
#define _osi_Function_TraceEpilogue(f) osi_tracepoint_table.##f##_return_id, osi_TraceFunc_FunctionEpilogue


#define _osi_Function_TracePrologue0(f) \
    (_osi_Function_TracePrologue(f), 0)
#define _osi_Function_TracePrologue1(f, v1) \
    (_osi_Function_TracePrologue(f), 1, _osi_TraceCast(v1))
#define _osi_Function_TracePrologue2(f, v1, v2) \
    (_osi_Function_TracePrologue(f), 2, _osi_TraceCast(v1), _osi_TraceCast(v2))
#define _osi_Function_TracePrologue3(f, v1, v2, v3) \
    (_osi_Function_TracePrologue(f), 3, _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3))
#define _osi_Function_TracePrologue4(f, v1, v2, v3, v4) \
    (_osi_Function_TracePrologue(f), 4, _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), \
     _osi_TraceCast(v4))
#define _osi_Function_TracePrologue5(f, v1, v2, v3, v4, v5) \
    (_osi_Function_TracePrologue(f), 5, _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), \
     _osi_TraceCast(v4), _osi_TraceCast(v5))
#define _osi_Function_TracePrologue6(f, v1, v2, v3, v4, v5, v6) \
    (_osi_Function_TracePrologue(f), 6, _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), \
     _osi_TraceCast(v4), _osi_TraceCast(v5), _osi_TraceCast(v6))
#define _osi_Function_TracePrologue7(f, v1, v2, v3, v4, v5, v6, v7) \
    (_osi_Function_TracePrologue(f), 7, _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), \
     _osi_TraceCast(v4), _osi_TraceCast(v5), _osi_TraceCast(v6), _osi_TraceCast(v7))
#define _osi_Function_TracePrologue8(f, v1, v2, v3, v4, v5, v6, v7, v8) \
    (_osi_Function_TracePrologue(f), 8, _osi_TraceCast(v1), _osi_TraceCast(v2), _osi_TraceCast(v3), \
     _osi_TraceCast(v4), _osi_TraceCast(v5), _osi_TraceCast(v6), _osi_TraceCast(v7), _osi_TraceCast(v8))


#define _osi_Function_TraceEpilogue0(f) (_osi_Function_TraceEpilogue(f), 0)
#define _osi_Function_TraceEpilogue1(f, ret) (_osi_Function_TraceEpilogue(f), 1, _osi_TraceCast(ret))


/*
 * stack variable declaration macros
 *
 * sample usage:
 * osi_Function_Vars1(int, code, = 0)
 */

#define osi_Function_Vars1(t1, v1, i1) \
    t1 v1 i1;

#define osi_Function_Vars2(t1, v1, i1, t2, v2, i2) \
    t1 v1 i1; \
    t2 v2 i2;

#define osi_Function_Vars3(t1, v1, i1, t2, v2, i2, t3, v3, i3) \
    t1 v1 i1; \
    t2 v2 i2; \
    t3 v3 i3;

#define osi_Function_Vars4(t1, v1, i1, t2, v2, i2, t3, v3, i3, t4, v4, i4) \
    t1 v1 i1; \
    t2 v2 i2; \
    t3 v3 i3; \
    t4 v4 i4;

#define osi_Function_Vars5(t1, v1, i1, t2, v2, i2, t3, v3, i3, t4, v4, i4, t5, v5, i5) \
    t1 v1 i1; \
    t2 v2 i2; \
    t3 v3 i3; \
    t4 v4 i4; \
    t5 v5 i5;

#define osi_Function_Vars6(t1, v1, i1, t2, v2, i2, t3, v3, i3, t4, v4, i4, t5, v5, i5, t6, v6, i6) \
    t1 v1 i1; \
    t2 v2 i2; \
    t3 v3 i3; \
    t4 v4 i4; \
    t5 v5 i5; \
    t6 v6 i6;

#define osi_Function_Vars7(t1, v1, i1, t2, v2, i2, t3, v3, i3, t4, v4, i4, t5, v5, i5, t6, v6, i6, t7, v7, i7) \
    t1 v1 i1; \
    t2 v2 i2; \
    t3 v3 i3; \
    t4 v4 i4; \
    t5 v5 i5; \
    t6 v6 i6; \
    t7 v7 i7;

#define osi_Function_Vars8(t1, v1, i1, t2, v2, i2, t3, v3, i3, t4, v4, i4, t5, v5, i5, t6, v6, i6, t7, v7, i7, t8, v8, i8) \
    t1 v1 i1; \
    t2 v2 i2; \
    t3 v3 i3; \
    t4 v4 i4; \
    t5 v5 i5; \
    t6 v6 i6; \
    t7 v7 i7; \
    t8 v8 i8;


/*
 * function prologue macros
 *
 * sample usage:
 * osi_Function_Prologue2(int, main, osi_Function_Vars1(int, code,), int, argc, char **, argv) {
 *    ...
 */

#define osi_Function_Prologue0(ret, name, vars) \
    ret \
    name _osi_Function_Args0() \
    { \
        vars \
        osi_TracePoint_FunctionPrologue0 _osi_Function_TracePrologue0(name);

#define osi_Function_Prologue1(ret, name, vars, t1, v1)
    ret \
    name _osi_Function_Args1(t1, v1) \
    { \
        vars \
        osi_TracePoint_FunctionPrologue _osi_Function_TracePrologueN(name, 1, v1);

#define osi_Function_Prologue2(ret, name, vars, t1, v1, t2, v2) \
    ret \
    name _osi_Function_Args2(t1, v1, t2, v2) \
    { \
        vars \
        osi_TracePoint_FunctionPrologue _osi_Function_TracePrologueN(name, 2, v1, v2);

#define osi_Function_Prologue3(ret, name, vars, t1, v1, t2, v2, t3, v3) \
    ret \
    name _osi_Function_Args3(t1, v1, t2, v2, t3, v3) \
    { \
        vars \
        osi_TracePoint_FunctionPrologue _osi_Function_TracePrologueN(name, 3, v1, v2, v3);

#define osi_Function_Prologue4(ret, name, vars, t1, v1, t2, v2, t3, v3, t4, v4) \
    ret \
    name _osi_Function_Args4(t1, v1, t2, v2, t3, v3, t4, v4) \
    { \
        vars \
        osi_TracePoint_FunctionPrologue _osi_Function_TracePrologueN(name, 4, v1, v2, v3, v4);

#define osi_Function_Prologue5(ret, name, vars, t1, v1, t2, v2, t3, v3, t4, v4, t5, v5) \
    ret \
    name _osi_Function_Args5(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5) \
    { \
        vars \
        osi_TracePoint_FunctionPrologue _osi_Function_TracePrologueN(name, 5, v1, v2, v3, v4, v5);

#define osi_Function_Prologue6(ret, name, vars, t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6) \
    ret \
    name _osi_Function_Args6(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6) \
    { \
        vars \
        osi_TracePoint_FunctionPrologue _osi_Function_TracePrologueN(name, 6, v1, v2, v3, v4, v5, v6);

#define osi_Function_Prologue7(ret, name, vars, t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6, t7, v7) \
    ret \
    name _osi_Function_Args7(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6, t7, v7) \
    { \
        vars \
        osi_TracePoint_FunctionPrologue _osi_Function_TracePrologueN(name, 7, v1, v2, v3, v4, v5, v6, v7);

#define osi_Function_Prologue8(ret, name, vars, t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6, t7, v7, t8, v8) \
    ret \
    name _osi_Function_Args8(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5, t6, v6, t7, v7, t8, v8) \
    { \
        vars \
        osi_TracePoint_FunctionPrologue _osi_Function_TracePrologueN(name, 8, v1, v2, v3, v4, v5, v6, v7, v8);



/*
 * function epilogue macros
 *
 * sample usage:
 *     ...
 * } osi_Function_Epilogue(main, code)
 */

#define osi_Function_Epilogue0(name) \
        osi_TracePoint_FunctionEpilogue0 _osi_Function_TraceEpilogue0(name); \
    }

#define osi_Function_Epilogue(name, ret) \
        osi_TracePoint_FunctionEpilogue _osi_Function_TraceEpilogue1(name, ret); \
        return (ret); \
    }


#endif /* _OSI_OSI_FUNCTION_H */
