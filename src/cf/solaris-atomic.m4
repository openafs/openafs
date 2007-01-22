dnl Copyright 2006-2007, Sine Nomine Associates and others.
dnl All Rights Reserved.
dnl 
dnl This software has been released under the terms of the IBM Public
dnl License.  For details, see the LICENSE file in the top-level source
dnl directory or online at http://www.openafs.org/dl/license10.html

dnl
dnl $Id$
dnl 

dnl this file defines test cases for the solaris atomic operations library.
dnl unfortunately, these interfaces have been implemented over a series
dnl of incremental updates spanning numerous update releases to the OS

AC_DEFUN([SOLARIS_HAS_ATOMIC_INC_8], [
AC_MSG_CHECKING(for atomic_inc_8)
AC_CACHE_VAL(ac_cv_solaris_atomic_inc_8,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
atomic_inc_8(&x);], 
ac_cv_solaris_atomic_inc_8=yes,
ac_cv_solaris_atomic_inc_8=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_inc_8)
if test "$ac_cv_solaris_atomic_inc_8" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_INC_8, 1, [define if atomic_inc_8() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_INC_16], [
AC_MSG_CHECKING(for atomic_inc_16)
AC_CACHE_VAL(ac_cv_solaris_atomic_inc_16,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
atomic_inc_16(&x);], 
ac_cv_solaris_atomic_inc_16=yes,
ac_cv_solaris_atomic_inc_16=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_inc_16)
if test "$ac_cv_solaris_atomic_inc_16" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_INC_16, 1, [define if atomic_inc_16() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_INC_32], [
AC_MSG_CHECKING(for atomic_inc_32)
AC_CACHE_VAL(ac_cv_solaris_atomic_inc_32,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
atomic_inc_32(&x);], 
ac_cv_solaris_atomic_inc_32=yes,
ac_cv_solaris_atomic_inc_32=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_inc_32)
if test "$ac_cv_solaris_atomic_inc_32" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_INC_32, 1, [define if atomic_inc_32() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_INC_64], [
AC_MSG_CHECKING(for atomic_inc_64)
AC_CACHE_VAL(ac_cv_solaris_atomic_inc_64,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
atomic_inc_64(&x);], 
ac_cv_solaris_atomic_inc_64=yes,
ac_cv_solaris_atomic_inc_64=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_inc_64)
if test "$ac_cv_solaris_atomic_inc_64" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_INC_64, 1, [define if atomic_inc_64() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_DEC_8], [
AC_MSG_CHECKING(for atomic_dec_8)
AC_CACHE_VAL(ac_cv_solaris_atomic_dec_8,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
atomic_dec_8(&x);], 
ac_cv_solaris_atomic_dec_8=yes,
ac_cv_solaris_atomic_dec_8=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_dec_8)
if test "$ac_cv_solaris_atomic_dec_8" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_DEC_8, 1, [define if atomic_dec_8() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_DEC_16], [
AC_MSG_CHECKING(for atomic_dec_16)
AC_CACHE_VAL(ac_cv_solaris_atomic_dec_16,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
atomic_dec_16(&x);], 
ac_cv_solaris_atomic_dec_16=yes,
ac_cv_solaris_atomic_dec_16=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_dec_16)
if test "$ac_cv_solaris_atomic_dec_16" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_DEC_16, 1, [define if atomic_dec_16() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_DEC_32], [
AC_MSG_CHECKING(for atomic_dec_32)
AC_CACHE_VAL(ac_cv_solaris_atomic_dec_32,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
atomic_dec_32(&x);], 
ac_cv_solaris_atomic_dec_32=yes,
ac_cv_solaris_atomic_dec_32=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_dec_32)
if test "$ac_cv_solaris_atomic_dec_32" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_DEC_32, 1, [define if atomic_dec_32() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_DEC_64], [
AC_MSG_CHECKING(for atomic_dec_64)
AC_CACHE_VAL(ac_cv_solaris_atomic_dec_64,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
atomic_dec_64(&x);], 
ac_cv_solaris_atomic_dec_64=yes,
ac_cv_solaris_atomic_dec_64=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_dec_64)
if test "$ac_cv_solaris_atomic_dec_64" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_DEC_64, 1, [define if atomic_dec_64() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_8], [
AC_MSG_CHECKING(for atomic_add_8)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_8,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
int8_t y;
atomic_add_8(&x, y);], 
ac_cv_solaris_atomic_add_8=yes,
ac_cv_solaris_atomic_add_8=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_8)
if test "$ac_cv_solaris_atomic_add_8" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_8, 1, [define if atomic_add_8() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_16], [
AC_MSG_CHECKING(for atomic_add_16)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_16,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
int16_t y;
atomic_add_16(&x, y);], 
ac_cv_solaris_atomic_add_16=yes,
ac_cv_solaris_atomic_add_16=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_16)
if test "$ac_cv_solaris_atomic_add_16" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_16, 1, [define if atomic_add_16() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_32], [
AC_MSG_CHECKING(for atomic_add_32)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_32,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
int32_t y;
atomic_add_32(&x, y);], 
ac_cv_solaris_atomic_add_32=yes,
ac_cv_solaris_atomic_add_32=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_32)
if test "$ac_cv_solaris_atomic_add_32" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_32, 1, [define if atomic_add_32() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_64], [
AC_MSG_CHECKING(for atomic_add_64)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_64,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
int64_t y;
atomic_add_64(&x, y);], 
ac_cv_solaris_atomic_add_64=yes,
ac_cv_solaris_atomic_add_64=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_64)
if test "$ac_cv_solaris_atomic_add_64" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_64, 1, [define if atomic_add_64() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_PTR], [
AC_MSG_CHECKING(for atomic_add_ptr)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_ptr,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile void *a, *x;
ssize_t y;
x = &a;
atomic_add_ptr(x, y);], 
ac_cv_solaris_atomic_add_ptr=yes,
ac_cv_solaris_atomic_add_ptr=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_ptr)
if test "$ac_cv_solaris_atomic_add_ptr" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_PTR, 1, [define if atomic_add_ptr() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_OR_8], [
AC_MSG_CHECKING(for atomic_or_8)
AC_CACHE_VAL(ac_cv_solaris_atomic_or_8,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
uint8_t y;
atomic_or_8(&x, y);], 
ac_cv_solaris_atomic_or_8=yes,
ac_cv_solaris_atomic_or_8=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_or_8)
if test "$ac_cv_solaris_atomic_or_8" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_OR_8, 1, [define if atomic_or_8() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_OR_16], [
AC_MSG_CHECKING(for atomic_or_16)
AC_CACHE_VAL(ac_cv_solaris_atomic_or_16,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
uint16_t y;
atomic_or_16(&x, y);], 
ac_cv_solaris_atomic_or_16=yes,
ac_cv_solaris_atomic_or_16=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_or_16)
if test "$ac_cv_solaris_atomic_or_16" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_OR_16, 1, [define if atomic_or_16() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_OR_32], [
AC_MSG_CHECKING(for atomic_or_32)
AC_CACHE_VAL(ac_cv_solaris_atomic_or_32,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
uint32_t y;
atomic_or_32(&x, y);], 
ac_cv_solaris_atomic_or_32=yes,
ac_cv_solaris_atomic_or_32=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_or_32)
if test "$ac_cv_solaris_atomic_or_32" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_OR_32, 1, [define if atomic_or_32() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_OR_64], [
AC_MSG_CHECKING(for atomic_or_64)
AC_CACHE_VAL(ac_cv_solaris_atomic_or_64,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
uint64_t y;
atomic_or_64(&x, y);], 
ac_cv_solaris_atomic_or_64=yes,
ac_cv_solaris_atomic_or_64=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_or_64)
if test "$ac_cv_solaris_atomic_or_64" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_OR_64, 1, [define if atomic_or_64() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_AND_8], [
AC_MSG_CHECKING(for atomic_and_8)
AC_CACHE_VAL(ac_cv_solaris_atomic_and_8,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
uint8_t y;
atomic_and_8(&x, y);], 
ac_cv_solaris_atomic_and_8=yes,
ac_cv_solaris_atomic_and_8=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_and_8)
if test "$ac_cv_solaris_atomic_and_8" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_AND_8, 1, [define if atomic_and_8() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_AND_16], [
AC_MSG_CHECKING(for atomic_and_16)
AC_CACHE_VAL(ac_cv_solaris_atomic_and_16,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
uint16_t y;
atomic_and_16(&x, y);], 
ac_cv_solaris_atomic_and_16=yes,
ac_cv_solaris_atomic_and_16=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_and_16)
if test "$ac_cv_solaris_atomic_and_16" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_AND_16, 1, [define if atomic_and_16() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_AND_32], [
AC_MSG_CHECKING(for atomic_and_32)
AC_CACHE_VAL(ac_cv_solaris_atomic_and_32,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
uint32_t y;
atomic_and_32(&x, y);], 
ac_cv_solaris_atomic_and_32=yes,
ac_cv_solaris_atomic_and_32=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_and_32)
if test "$ac_cv_solaris_atomic_and_32" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_AND_32, 1, [define if atomic_and_32() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_AND_64], [
AC_MSG_CHECKING(for atomic_and_64)
AC_CACHE_VAL(ac_cv_solaris_atomic_and_64,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
uint64_t y;
atomic_and_64(&x, y);], 
ac_cv_solaris_atomic_and_64=yes,
ac_cv_solaris_atomic_and_64=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_and_64)
if test "$ac_cv_solaris_atomic_and_64" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_AND_64, 1, [define if atomic_and_64() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_INC_8_NV], [
AC_MSG_CHECKING(for atomic_inc_8_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_inc_8_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
uint8_t z;
z = atomic_inc_8_nv(&x);], 
ac_cv_solaris_atomic_inc_8_nv=yes,
ac_cv_solaris_atomic_inc_8_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_inc_8_nv)
if test "$ac_cv_solaris_atomic_inc_8_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_INC_8_NV, 1, [define if atomic_inc_8_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_INC_16_NV], [
AC_MSG_CHECKING(for atomic_inc_16_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_inc_16_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
uint16_t z;
z = atomic_inc_16_nv(&x);], 
ac_cv_solaris_atomic_inc_16_nv=yes,
ac_cv_solaris_atomic_inc_16_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_inc_16_nv)
if test "$ac_cv_solaris_atomic_inc_16_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_INC_16_NV, 1, [define if atomic_inc_16_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_INC_32_NV], [
AC_MSG_CHECKING(for atomic_inc_32_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_inc_32_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
uint32_t z;
z = atomic_inc_32_nv(&x);], 
ac_cv_solaris_atomic_inc_32_nv=yes,
ac_cv_solaris_atomic_inc_32_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_inc_32_nv)
if test "$ac_cv_solaris_atomic_inc_32_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_INC_32_NV, 1, [define if atomic_inc_32_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_INC_64_NV], [
AC_MSG_CHECKING(for atomic_inc_64_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_inc_64_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
uint64_t z;
z = atomic_inc_64_nv(&x);], 
ac_cv_solaris_atomic_inc_64_nv=yes,
ac_cv_solaris_atomic_inc_64_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_inc_64_nv)
if test "$ac_cv_solaris_atomic_inc_64_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_INC_64_NV, 1, [define if atomic_inc_64_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_DEC_8_NV], [
AC_MSG_CHECKING(for atomic_dec_8_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_dec_8_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
uint8_t z;
z = atomic_dec_8_nv(&x);], 
ac_cv_solaris_atomic_dec_8_nv=yes,
ac_cv_solaris_atomic_dec_8_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_dec_8_nv)
if test "$ac_cv_solaris_atomic_dec_8_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_DEC_8_NV, 1, [define if atomic_dec_8_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_DEC_16_NV], [
AC_MSG_CHECKING(for atomic_dec_16_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_dec_16_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
uint16_t z;
z = atomic_dec_16_nv(&x);], 
ac_cv_solaris_atomic_dec_16_nv=yes,
ac_cv_solaris_atomic_dec_16_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_dec_16_nv)
if test "$ac_cv_solaris_atomic_dec_16_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_DEC_16_NV, 1, [define if atomic_dec_16_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_DEC_32_NV], [
AC_MSG_CHECKING(for atomic_dec_32_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_dec_32_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
uint32_t z;
z = atomic_dec_32_nv(&x);], 
ac_cv_solaris_atomic_dec_32_nv=yes,
ac_cv_solaris_atomic_dec_32_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_dec_32_nv)
if test "$ac_cv_solaris_atomic_dec_32_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_DEC_32_NV, 1, [define if atomic_dec_32_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_DEC_64_NV], [
AC_MSG_CHECKING(for atomic_dec_64_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_dec_64_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
uint64_t z;
z = atomic_dec_64_nv(&x);], 
ac_cv_solaris_atomic_dec_64_nv=yes,
ac_cv_solaris_atomic_dec_64_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_dec_64_nv)
if test "$ac_cv_solaris_atomic_dec_64_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_DEC_64_NV, 1, [define if atomic_dec_64_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_8_NV], [
AC_MSG_CHECKING(for atomic_add_8_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_8_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
int8_t y;
uint8_t z;
z = atomic_add_8_nv(&x, y);], 
ac_cv_solaris_atomic_add_8_nv=yes,
ac_cv_solaris_atomic_add_8_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_8_nv)
if test "$ac_cv_solaris_atomic_add_8_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_8_NV, 1, [define if atomic_add_8_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_16_NV], [
AC_MSG_CHECKING(for atomic_add_16_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_16_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
int16_t y;
uint16_t z;
z = atomic_add_16_nv(&x, y);], 
ac_cv_solaris_atomic_add_16_nv=yes,
ac_cv_solaris_atomic_add_16_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_16_nv)
if test "$ac_cv_solaris_atomic_add_16_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_16_NV, 1, [define if atomic_add_16_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_32_NV], [
AC_MSG_CHECKING(for atomic_add_32_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_32_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
int32_t y;
uint32_t z;
z = atomic_add_32_nv(&x, y);], 
ac_cv_solaris_atomic_add_32_nv=yes,
ac_cv_solaris_atomic_add_32_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_32_nv)
if test "$ac_cv_solaris_atomic_add_32_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_32_NV, 1, [define if atomic_add_32_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_64_NV], [
AC_MSG_CHECKING(for atomic_add_64_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_64_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
int64_t y;
uint64_t z;
z = atomic_add_64_nv(&x, y);], 
ac_cv_solaris_atomic_add_64_nv=yes,
ac_cv_solaris_atomic_add_64_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_64_nv)
if test "$ac_cv_solaris_atomic_add_64_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_64_NV, 1, [define if atomic_add_64_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_ADD_PTR_NV], [
AC_MSG_CHECKING(for atomic_add_ptr_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_add_ptr_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile void *a, *x;
ssize_t y;
void * z;
x = &a;
z = atomic_add_ptr_nv(x, y);], 
ac_cv_solaris_atomic_add_ptr_nv=yes,
ac_cv_solaris_atomic_add_ptr_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_add_ptr_nv)
if test "$ac_cv_solaris_atomic_add_ptr_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_ADD_PTR_NV, 1, [define if atomic_add_ptr_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_OR_8_NV], [
AC_MSG_CHECKING(for atomic_or_8_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_or_8_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
uint8_t y, z;
z = atomic_or_8_nv(&x, y);], 
ac_cv_solaris_atomic_or_8_nv=yes,
ac_cv_solaris_atomic_or_8_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_or_8_nv)
if test "$ac_cv_solaris_atomic_or_8_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_OR_8_NV, 1, [define if atomic_or_8_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_OR_16_NV], [
AC_MSG_CHECKING(for atomic_or_16_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_or_16_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
uint16_t y, z;
z = atomic_or_16_nv(&x, y);], 
ac_cv_solaris_atomic_or_16_nv=yes,
ac_cv_solaris_atomic_or_16_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_or_16_nv)
if test "$ac_cv_solaris_atomic_or_16_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_OR_16_NV, 1, [define if atomic_or_16_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_OR_32_NV], [
AC_MSG_CHECKING(for atomic_or_32_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_or_32_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
uint32_t y, z;
z = atomic_or_32_nv(&x, y);], 
ac_cv_solaris_atomic_or_32_nv=yes,
ac_cv_solaris_atomic_or_32_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_or_32_nv)
if test "$ac_cv_solaris_atomic_or_32_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_OR_32_NV, 1, [define if atomic_or_32_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_OR_64_NV], [
AC_MSG_CHECKING(for atomic_or_64_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_or_64_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
uint64_t y, z;
z = atomic_or_64_nv(&x, y);], 
ac_cv_solaris_atomic_or_64_nv=yes,
ac_cv_solaris_atomic_or_64_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_or_64_nv)
if test "$ac_cv_solaris_atomic_or_64_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_OR_64_NV, 1, [define if atomic_or_64_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_AND_8_NV], [
AC_MSG_CHECKING(for atomic_and_8_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_and_8_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
uint8_t y, z;
z = atomic_and_8_nv(&x, y);], 
ac_cv_solaris_atomic_and_8_nv=yes,
ac_cv_solaris_atomic_and_8_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_and_8_nv)
if test "$ac_cv_solaris_atomic_and_8_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_AND_8_NV, 1, [define if atomic_and_8_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_AND_16_NV], [
AC_MSG_CHECKING(for atomic_and_16_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_and_16_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
uint16_t y, z;
z = atomic_and_16_nv(&x, y);], 
ac_cv_solaris_atomic_and_16_nv=yes,
ac_cv_solaris_atomic_and_16_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_and_16_nv)
if test "$ac_cv_solaris_atomic_and_16_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_AND_16_NV, 1, [define if atomic_and_16_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_AND_32_NV], [
AC_MSG_CHECKING(for atomic_and_32_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_and_32_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
uint32_t y, z;
z = atomic_and_32_nv(&x, y);], 
ac_cv_solaris_atomic_and_32_nv=yes,
ac_cv_solaris_atomic_and_32_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_and_32_nv)
if test "$ac_cv_solaris_atomic_and_32_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_AND_32_NV, 1, [define if atomic_and_32_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_AND_64_NV], [
AC_MSG_CHECKING(for atomic_and_64_nv)
AC_CACHE_VAL(ac_cv_solaris_atomic_and_64_nv,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
uint64_t y, z;
z = atomic_and_64_nv(&x, y);], 
ac_cv_solaris_atomic_and_64_nv=yes,
ac_cv_solaris_atomic_and_64_nv=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_and_64_nv)
if test "$ac_cv_solaris_atomic_and_64_nv" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_AND_64_NV, 1, [define if atomic_and_64_nv() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_SWAP_8], [
AC_MSG_CHECKING(for atomic_swap_8)
AC_CACHE_VAL(ac_cv_solaris_atomic_swap_8,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
uint8_t y, z;
z = atomic_swap_8(&x, y);], 
ac_cv_solaris_atomic_swap_8=yes,
ac_cv_solaris_atomic_swap_8=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_swap_8)
if test "$ac_cv_solaris_atomic_swap_8" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_SWAP_8, 1, [define if atomic_swap_8() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_SWAP_16], [
AC_MSG_CHECKING(for atomic_swap_16)
AC_CACHE_VAL(ac_cv_solaris_atomic_swap_16,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
uint16_t y, z;
z = atomic_swap_16(&x, y);], 
ac_cv_solaris_atomic_swap_16=yes,
ac_cv_solaris_atomic_swap_16=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_swap_16)
if test "$ac_cv_solaris_atomic_swap_16" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_SWAP_16, 1, [define if atomic_swap_16() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_SWAP_32], [
AC_MSG_CHECKING(for atomic_swap_32)
AC_CACHE_VAL(ac_cv_solaris_atomic_swap_32,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
uint32_t y, z;
z = atomic_swap_32(&x, y);], 
ac_cv_solaris_atomic_swap_32=yes,
ac_cv_solaris_atomic_swap_32=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_swap_32)
if test "$ac_cv_solaris_atomic_swap_32" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_SWAP_32, 1, [define if atomic_swap_32() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_SWAP_64], [
AC_MSG_CHECKING(for atomic_swap_64)
AC_CACHE_VAL(ac_cv_solaris_atomic_swap_64,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
uint64_t y, z;
z = atomic_swap_64(&x, y);], 
ac_cv_solaris_atomic_swap_64=yes,
ac_cv_solaris_atomic_swap_64=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_swap_64)
if test "$ac_cv_solaris_atomic_swap_64" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_SWAP_64, 1, [define if atomic_swap_64() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_SWAP_PTR], [
AC_MSG_CHECKING(for atomic_swap_ptr)
AC_CACHE_VAL(ac_cv_solaris_atomic_swap_ptr,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile void *a, *x;
void *y, *z;
x = &a;
y = NULL;
z = atomic_swap_ptr(x, y);], 
ac_cv_solaris_atomic_swap_ptr=yes,
ac_cv_solaris_atomic_swap_ptr=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_swap_ptr)
if test "$ac_cv_solaris_atomic_swap_ptr" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_SWAP_PTR, 1, [define if atomic_swap_ptr() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_CAS_8], [
AC_MSG_CHECKING(for atomic_cas_8)
AC_CACHE_VAL(ac_cv_solaris_atomic_cas_8,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint8_t x;
uint8_t y, z, c;
z = atomic_cas_8(&x, c, y);], 
ac_cv_solaris_atomic_cas_8=yes,
ac_cv_solaris_atomic_cas_8=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_cas_8)
if test "$ac_cv_solaris_atomic_cas_8" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_CAS_8, 1, [define if atomic_cas_8() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_CAS_16], [
AC_MSG_CHECKING(for atomic_cas_16)
AC_CACHE_VAL(ac_cv_solaris_atomic_cas_16,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint16_t x;
uint16_t y, z, c;
z = atomic_cas_16(&x, c, y);], 
ac_cv_solaris_atomic_cas_16=yes,
ac_cv_solaris_atomic_cas_16=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_cas_16)
if test "$ac_cv_solaris_atomic_cas_16" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_CAS_16, 1, [define if atomic_cas_16() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_CAS_32], [
AC_MSG_CHECKING(for atomic_cas_32)
AC_CACHE_VAL(ac_cv_solaris_atomic_cas_32,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint32_t x;
uint32_t y, z, c;
z = atomic_cas_32(&x, c, y);], 
ac_cv_solaris_atomic_cas_32=yes,
ac_cv_solaris_atomic_cas_32=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_cas_32)
if test "$ac_cv_solaris_atomic_cas_32" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_CAS_32, 1, [define if atomic_cas_32() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_CAS_64], [
AC_MSG_CHECKING(for atomic_cas_64)
AC_CACHE_VAL(ac_cv_solaris_atomic_cas_64,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile uint64_t x;
uint64_t y, z, c;
z = atomic_cas_64(&x, c, y);], 
ac_cv_solaris_atomic_cas_64=yes,
ac_cv_solaris_atomic_cas_64=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_cas_64)
if test "$ac_cv_solaris_atomic_cas_64" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_CAS_64, 1, [define if atomic_cas_64() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_ATOMIC_CAS_PTR], [
AC_MSG_CHECKING(for atomic_cas_ptr)
AC_CACHE_VAL(ac_cv_solaris_atomic_cas_ptr,
[
AC_TRY_LINK(
[#include <atomic.h>],
[volatile void *a, *x;
void *y, *z, *c;
x = &a;
c = (void *) &a;
y = NULL;
z = atomic_cas_ptr(x, c, y);], 
ac_cv_solaris_atomic_cas_ptr=yes,
ac_cv_solaris_atomic_cas_ptr=no)])
AC_MSG_RESULT($ac_cv_solaris_atomic_cas_ptr)
if test "$ac_cv_solaris_atomic_cas_ptr" = "yes"; then
  AC_DEFINE(HAVE_ATOMIC_CAS_PTR, 1, [define if atomic_cas_ptr() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_MEMBAR_ENTER], [
AC_MSG_CHECKING(for membar_enter)
AC_CACHE_VAL(ac_cv_solaris_membar_enter,
[
AC_TRY_LINK(
[#include <atomic.h>],
[membar_enter();], 
ac_cv_solaris_membar_enter=yes,
ac_cv_solaris_membar_enter=no)])
AC_MSG_RESULT($ac_cv_solaris_membar_enter)
if test "$ac_cv_solaris_membar_enter" = "yes"; then
  AC_DEFINE(HAVE_MEMBAR_ENTER, 1, [define if membar_enter() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_MEMBAR_EXIT], [
AC_MSG_CHECKING(for membar_exit)
AC_CACHE_VAL(ac_cv_solaris_membar_exit,
[
AC_TRY_LINK(
[#include <atomic.h>],
[membar_exit();], 
ac_cv_solaris_membar_exit=yes,
ac_cv_solaris_membar_exit=no)])
AC_MSG_RESULT($ac_cv_solaris_membar_exit)
if test "$ac_cv_solaris_membar_exit" = "yes"; then
  AC_DEFINE(HAVE_MEMBAR_EXIT, 1, [define if membar_exit() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_MEMBAR_PRODUCER], [
AC_MSG_CHECKING(for membar_producer)
AC_CACHE_VAL(ac_cv_solaris_membar_producer,
[
AC_TRY_LINK(
[#include <atomic.h>],
[membar_producer();], 
ac_cv_solaris_membar_producer=yes,
ac_cv_solaris_membar_producer=no)])
AC_MSG_RESULT($ac_cv_solaris_membar_producer)
if test "$ac_cv_solaris_membar_producer" = "yes"; then
  AC_DEFINE(HAVE_MEMBAR_PRODUCER, 1, [define if membar_producer() exists])
fi
])

AC_DEFUN([SOLARIS_HAS_MEMBAR_CONSUMER], [
AC_MSG_CHECKING(for membar_consumer)
AC_CACHE_VAL(ac_cv_solaris_membar_consumer,
[
AC_TRY_LINK(
[#include <atomic.h>],
[membar_consumer();], 
ac_cv_solaris_membar_consumer=yes,
ac_cv_solaris_membar_consumer=no)])
AC_MSG_RESULT($ac_cv_solaris_membar_consumer)
if test "$ac_cv_solaris_membar_consumer" = "yes"; then
  AC_DEFINE(HAVE_MEMBAR_CONSUMER, 1, [define if membar_consumer() exists])
fi
])



AC_DEFUN([SOLARIS_USERSPACE_ATOMIC_OPS], [
SOLARIS_HAS_ATOMIC_INC_8
SOLARIS_HAS_ATOMIC_INC_16
SOLARIS_HAS_ATOMIC_INC_32
SOLARIS_HAS_ATOMIC_INC_64
SOLARIS_HAS_ATOMIC_DEC_8
SOLARIS_HAS_ATOMIC_DEC_16
SOLARIS_HAS_ATOMIC_DEC_32
SOLARIS_HAS_ATOMIC_DEC_64
SOLARIS_HAS_ATOMIC_ADD_8
SOLARIS_HAS_ATOMIC_ADD_16
SOLARIS_HAS_ATOMIC_ADD_32
SOLARIS_HAS_ATOMIC_ADD_64
SOLARIS_HAS_ATOMIC_ADD_PTR
SOLARIS_HAS_ATOMIC_OR_8
SOLARIS_HAS_ATOMIC_OR_16
SOLARIS_HAS_ATOMIC_OR_32
SOLARIS_HAS_ATOMIC_OR_64
SOLARIS_HAS_ATOMIC_AND_8
SOLARIS_HAS_ATOMIC_AND_16
SOLARIS_HAS_ATOMIC_AND_32
SOLARIS_HAS_ATOMIC_AND_64
SOLARIS_HAS_ATOMIC_INC_8_NV
SOLARIS_HAS_ATOMIC_INC_16_NV
SOLARIS_HAS_ATOMIC_INC_32_NV
SOLARIS_HAS_ATOMIC_INC_64_NV
SOLARIS_HAS_ATOMIC_DEC_8_NV
SOLARIS_HAS_ATOMIC_DEC_16_NV
SOLARIS_HAS_ATOMIC_DEC_32_NV
SOLARIS_HAS_ATOMIC_DEC_64_NV
SOLARIS_HAS_ATOMIC_ADD_8_NV
SOLARIS_HAS_ATOMIC_ADD_16_NV
SOLARIS_HAS_ATOMIC_ADD_32_NV
SOLARIS_HAS_ATOMIC_ADD_64_NV
SOLARIS_HAS_ATOMIC_ADD_PTR_NV
SOLARIS_HAS_ATOMIC_OR_8_NV
SOLARIS_HAS_ATOMIC_OR_16_NV
SOLARIS_HAS_ATOMIC_OR_32_NV
SOLARIS_HAS_ATOMIC_OR_64_NV
SOLARIS_HAS_ATOMIC_AND_8_NV
SOLARIS_HAS_ATOMIC_AND_16_NV
SOLARIS_HAS_ATOMIC_AND_32_NV
SOLARIS_HAS_ATOMIC_AND_64_NV
SOLARIS_HAS_ATOMIC_SWAP_8
SOLARIS_HAS_ATOMIC_SWAP_16
SOLARIS_HAS_ATOMIC_SWAP_32
SOLARIS_HAS_ATOMIC_SWAP_64
SOLARIS_HAS_ATOMIC_SWAP_PTR
SOLARIS_HAS_ATOMIC_CAS_8
SOLARIS_HAS_ATOMIC_CAS_16
SOLARIS_HAS_ATOMIC_CAS_32
SOLARIS_HAS_ATOMIC_CAS_64
SOLARIS_HAS_ATOMIC_CAS_PTR
SOLARIS_HAS_MEMBAR_ENTER
SOLARIS_HAS_MEMBAR_EXIT
SOLARIS_HAS_MEMBAR_PRODUCER
SOLARIS_HAS_MEMBAR_CONSUMER
])
