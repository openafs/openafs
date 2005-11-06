/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <WINNT/afsclass.h>

#ifndef DEBUG // building retail?

#ifndef ASSERT
#define ASSERT(b)  ((b) ? TRUE : FALSE)  // take failure paths if retail asserts
#endif

#else // building debug?

#ifndef ASSERT
#define ASSERT(b)  AssertFn(b, #b, __LINE__, __FILE__)
#endif

#include <windows.h>


         typedef unsigned char    uchar;
         typedef unsigned short   ushort;

#define ANGLES_ON   "[AnglesOn]"
#define ANGLES_OFF  "[AnglesOff]"
#define LASTERROR   "[LastError]"


/*
 * DEFINITIONS ________________________________________________________________
 *
 */

#define minX 2
#define minY 2

#define yMAX 60
#define xMAX 300


/*
 * TYPEDEFS ___________________________________________________________________
 *
 */


         class Debugstr
            {
            public:

               Debugstr & operator<< (char   *psz);
               Debugstr & operator<< (void   *psz);
               Debugstr & operator<< (long    v);
               Debugstr & operator<< (size_t  v);
               Debugstr & operator<< (ushort  v);
               Debugstr & operator<< (short   v);
               Debugstr & operator<< (uchar   ch);
               Debugstr & operator<< (char    ch);
               Debugstr & operator<< (double  f);
               Debugstr & operator<< (RECT    r);
               Debugstr & operator<< (LPIDENT lpi);

               Debugstr  (void);
               ~Debugstr (void);

               static LRESULT APIENTRY DebugWndProc (HWND, UINT, WPARAM, LPARAM);

               void          OutString   (char *str, BOOL fRecord);

            private:

               HBRUSH        brBack;
               HFONT         hfNew;
               TEXTMETRIC    tm;

               static void   Register    (void);
               static void   Initialize  (void);
               void          Output      (HDC hdc, char *str, BOOL fRecord);

               static int    fRegistered;
               static BOOL   fInit;
               static HWND   hwnd;

               static ushort gx, gcX;
               static ushort gy, gcY;
               static char gdata[ yMAX ][ xMAX ];

               static BOOL   fAngles;
            };


#define MAX_ARGS 10

         class LogOut
            {
            public:

               cdecl LogOut  (char *format, ...);
               ~LogOut (void);

            private:
               char     pszFormat[ 256 ];

               char    *aPtr[ MAX_ARGS ];
               ushort   nArgs;
            };



/*
 * VARIABLES __________________________________________________________________
 *
 */

         extern  Debugstr  debug;


/*
 * PROTOTYPES _________________________________________________________________
 *
 */

         extern  int   AssertFn   (int, char *, int, char *);


#endif // DEBUG

#endif // DEBUG_H

