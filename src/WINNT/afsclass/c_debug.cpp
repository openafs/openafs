/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <windows.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdarg.h>
#include <WINNT/c_debug.h>

#ifdef DEBUG

#ifndef THIS_HINST
#define THIS_HINST (GetModuleHandle (NULL))
#endif

#define WM_OUTSTRING  (WM_USER + 0x99)



/*
 * VARIABLES __________________________________________________________________
 *
 */

               Debugstr    debug;

               int         Debugstr::fRegistered = 0;
               int         Debugstr::fInit = 0;
               HWND        Debugstr::hwnd  = 0;

               ushort      Debugstr::gx;
               ushort      Debugstr::gy;
               ushort      Debugstr::gcX;
               ushort      Debugstr::gcY;
               char        Debugstr::gdata[ yMAX ][ xMAX ];

               BOOL        Debugstr::fAngles = FALSE;

               ushort      nRefr;

               ushort      cxAvgWidth    (void);


/*
 * ROUTINES ___________________________________________________________________
 *
 */

int AssertFn (int b, char *expr, int line, char *name)
{
   char  szLine1[ 256 ];
   char  szLine2[ 256 ];

   if (b)
      return 1;

   wsprintf (szLine1, "Assertion failed: \"%s\"", expr);
   wsprintf (szLine2, "Line %u of module %s.", line, name);

#ifdef DEBUG
debug << szLine1 << "\n";
debug << szLine2 << "\n";
#endif

   MessageBox (NULL, szLine1, szLine2, MB_ICONEXCLAMATION);
   return 0;
}


/*
 * OPERATORS __________________________________________________________________
 *
 */

Debugstr & Debugstr::operator<< (char *str)
{
   if (! strcmp (str, ANGLES_ON))
      fAngles = TRUE;
   else if (! strcmp (str, ANGLES_OFF))
      fAngles = FALSE;
   else if (! strcmp (str, LASTERROR))
      {
      LPVOID lp;

      FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                     NULL, GetLastError(),
                     MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                     (LPTSTR)&lp, 0, NULL);

      (*this) << "#" << (LONG)GetLastError();
      (*this) << " (" << (char *)lp << ")";

      LocalFree (lp);
      }
   else
      {
      Register();	// does nothing unless first time
      Initialize();	// does nothing unless first time

      char *strToSend = (char *)Allocate(1+strlen(str));
      strcpy (strToSend, str);

      PostMessage (Debugstr::hwnd, WM_OUTSTRING, 0, (LPARAM)strToSend);
      }
   return (*this);
}

Debugstr & Debugstr::operator<< (void *addr)
{
   char  szTemp[40];
   if (HIWORD(addr) == 0x0000)
      wsprintf (szTemp, "0x%04X", (ushort)LOWORD(PtrToLong(addr)));
   else
      wsprintf (szTemp, "0x%08lX", PtrToLong(addr));
   return (*this << szTemp);
}

Debugstr & Debugstr::operator<< (uchar ch)
{
   char  szTemp[2];
   szTemp[0] = ch;
   szTemp[1] = 0;
   return (*this << szTemp);
}

Debugstr & Debugstr::operator<< (char ch)
{
   char  szTemp[2];
   szTemp[0] = ch;
   szTemp[1] = 0;
   return (*this << szTemp);
}

Debugstr & Debugstr::operator<< (size_t l)
{
   char  szTemp[40];
   _ltoa ((LONG)l, szTemp, 10);
   return (*this << szTemp);
}

Debugstr & Debugstr::operator<< (long l)
{
   char  szTemp[40];
   _ltoa (l, szTemp, 10);
   return (*this << szTemp);
}

Debugstr & Debugstr::operator<< (ushort s)
{
   char  szTemp[40];
   _itoa (s, szTemp, 10);
   return (*this << szTemp);
}

Debugstr & Debugstr::operator<< (short s)
{
   char  szTemp[40];
   _itoa (s, szTemp, 10);
   return (*this << szTemp);
}

Debugstr & Debugstr::operator<< (double f)
{
   char *psz;	// (may cross segments)
   char  szTemp[ 64 ];
   long  l;

   if (fAngles)
      {
      f *= 180.0 / 3.1415926535897;
      while (f >= 360)
         f -= 360;
      while (f < 0)
         f += 360;
      }

   l = (long)f;
   *this << l;

   f -= (double)l;
   f *= 1000000.0;	// add 6 zeroes...
   l = (long)f;

   wsprintf (szTemp, "%06ld", l);

   for (psz = &szTemp[ strlen(szTemp)-1 ]; psz >= szTemp; psz--)
      {
      if (*psz != '0')
         break;
      }
   *(1+psz) = 0;

   if (szTemp[0] != 0)
      {
      *this << "." << szTemp;
      }

   return *this;
}

Debugstr & Debugstr::operator<< (RECT r)
{
   *this << "{ x=" << (LONG)r.left << ".." << (LONG)r.right;
   *this << ", y=" << (LONG)r.top << ".." << (LONG)r.bottom << " }";
   return (*this);
}

Debugstr & Debugstr::operator<< (LPIDENT lpi)
{
   if (!lpi)
      {
      *this << "{ invalid ident }";
      }
   else if (lpi->fIsCell())
      {
      TCHAR szCell[ cchNAME ];
      lpi->GetCellName (szCell);
      *this << "{ cell " << szCell << " }";
      }
   else if (lpi->fIsServer())
      {
      TCHAR szServer[ cchNAME ];
      lpi->GetServerName (szServer);
      *this << "{ server " << szServer << " }";
      }
   else if (lpi->fIsAggregate())
      {
      TCHAR szServer[ cchNAME ];
      lpi->GetServerName (szServer);
      TCHAR szAggregate[ cchNAME ];
      lpi->GetAggregateName (szAggregate);
      *this << "{ aggregate " << szServer << ":" << szAggregate << " }";
      }
   else if (lpi->fIsFileset())
      {
      TCHAR szServer[ cchNAME ];
      lpi->GetServerName (szServer);
      TCHAR szAggregate[ cchNAME ];
      lpi->GetAggregateName (szAggregate);
      TCHAR szFileset[ cchNAME ];
      lpi->GetFilesetName (szFileset);
      *this << "{ fileset " << szFileset << " on " << szServer << ":" << szAggregate << " }";
      }
   else if (lpi->fIsServer())
      {
      TCHAR szServer[ cchNAME ];
      lpi->GetServerName (szServer);
      TCHAR szService[ cchNAME ];
      lpi->GetServiceName (szService);
      *this << "{ service " << szServer << ":" << szService << " }";
      }
   return (*this);
}


/*
 * STATICS ____________________________________________________________________
 *
 */

Debugstr::Debugstr (void)
{
   hfNew = NULL;
   brBack = NULL;
}

Debugstr::~Debugstr (void)
{
   if (hfNew != NULL)
      {
      DeleteObject (hfNew);
      hfNew = NULL;
      }

   if (brBack != NULL)
      {
      DeleteObject (brBack);
      brBack = NULL;
      }
}


#define szDebugCLASS  "DebugClass"

void Debugstr::Register (void)
{
   WNDCLASS  wc;

   if (fRegistered)
      return;
   fRegistered = TRUE;

   wc.style         = CS_HREDRAW | CS_VREDRAW;
   wc.lpfnWndProc   = DebugWndProc;
   wc.cbClsExtra    = 0;
   wc.cbWndExtra    = 0;
   wc.hInstance     = THIS_HINST;
   wc.hIcon         = LoadIcon(NULL, IDI_EXCLAMATION);
   wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
   wc.hbrBackground = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ));
   wc.lpszMenuName  = NULL;
   wc.lpszClassName = szDebugCLASS;

   (void)RegisterClass(&wc);
}


void Debugstr::Initialize (void)
{
   HWND       hWnd;
   int        x, y, w, h;

   if (fInit)
      return;
   fInit = TRUE;

   w = cxAvgWidth() * 58;
// w = cxAvgWidth() * 160;
   h = GetSystemMetrics (SM_CYSCREEN);
   x = GetSystemMetrics (SM_CXSCREEN) - w;
   y = 0;

   hWnd = CreateWindow(
              szDebugCLASS,
              "debug++",	// Title
              WS_OVERLAPPEDWINDOW |	// Window style
                 WS_THICKFRAME,
              x,	// Default horizontal position
              y +35,	// Default vertical position
              w,	// Default width
              h -70,	// Default height
              NULL,	// Overlapped windows have no parent
              NULL,	// Use the window class menu
              THIS_HINST,	// This instance owns this window
              NULL	// Pointer not needed
              );

   if (!hWnd)
      return;

          /*
           * Make the window visible and update its client area.
           *
           */

   for (y = 0; y < yMAX; y++)
      gdata[y][0] = 0;

   gx  = minX;
   gy  = minY;
   gcX = 0;
   gcY = 0;
   nRefr = 0;

   ShowWindow (hWnd, SW_SHOWNOACTIVATE);
   UpdateWindow (hWnd);

   Debugstr::hwnd = hWnd;
}


/*** OutString - Translates "\n" 's, and calls Output to display text
 *
 * ENTRY:     char   *str     - string to display
 *            BOOL    fRecord - FALSE if inside WM_PAINT message
 *
 * EXIT:      none
 *
 */

void Debugstr::OutString (char *str, BOOL fRecord)
{
   char      *psz, *pch;
   HDC        hdc;
   DWORD      fg, bk;
   RECT       r;
   RECT       r2;
   HFONT      hfOld;
   LOGFONT    lf;
   SIZE       siz;

   Register();	// does nothing unless first time
   Initialize();	// does nothing unless first time

   if (str == NULL)
      str = "(null)";

   OutputDebugString (str);

   if (Debugstr::hwnd == NULL)
      return;

   if ((hdc = GetDC (Debugstr::hwnd)) == NULL)
      return;

   fg = SetTextColor (hdc, GetSysColor( COLOR_BTNTEXT ));
   bk = SetBkColor (hdc, GetSysColor( COLOR_BTNFACE ));

   if (brBack == NULL)
      {
      brBack = CreateSolidBrush (GetBkColor(hdc));
      }

   if (hfNew == NULL)
      {
      memset (&lf, 0, sizeof(lf));

      lf.lfWeight = FW_NORMAL;
      lf.lfHeight = -MulDiv (8, GetDeviceCaps(hdc, LOGPIXELSY), 72);
      strcpy (lf.lfFaceName, "Arial");

      hfNew = CreateFontIndirect(&lf);
      }

   hfOld = (HFONT)SelectObject(hdc, hfNew);
   GetTextMetrics (hdc, &tm);

   GetClientRect (Debugstr::hwnd, &r);

   if (!strcmp (str, "CLS"))
      {
      gx  = 0;
      gy  = minY;
      gcX = 0;
      gcY = 0;
      gdata[gcY][0] = 0;
      gdata[gcY+1][0] = 0;

      FillRect (hdc, &r, brBack);
      }
   else for (psz = str; *psz; )
      {
      if ((pch = strchr(psz, '\n')) == NULL)
         {
         Output (hdc, psz, fRecord);
         break;
         }

      *pch = 0;
      Output (hdc, psz, fRecord);

      gy += (ushort)tm.tmHeight;
      gcY++;
      gx  = minX;
      gcX = 0;

      if (fRecord)
         {
         if ((gy + tm.tmHeight) > (ushort)r.bottom)
            {
            gy = minY;
            gcY = 0;

            GetTextExtentPoint (hdc, gdata[gcY], (int)strlen(gdata[gcY]), &siz);

            r2.top = gy;
            r2.bottom = gy + tm.tmHeight;
            r2.left = gx -1;
            r2.right = siz.cx;
            r2.right += gx;
            FillRect (hdc, &r2, brBack);

            gdata[gcY][0] = 0;
            }
         }

      nRefr = max( nRefr, gcY );

      *pch = '\n';
      psz = 1+pch;

      if (fRecord)
         {
         GetTextExtentPoint (hdc, gdata[gcY+1], (int)strlen(gdata[gcY+1]), &siz);

         r2.top = gy +tm.tmHeight;
         r2.bottom = gy +tm.tmHeight +tm.tmHeight;
         r2.left = gx;
         r2.right = siz.cx;
         r2.right += gx;
         FillRect (hdc, &r2, brBack);

         gdata[gcY+1][0] = 0;
         }
      }

   SelectObject (hdc, hfOld);
   SetTextColor (hdc, fg);
   SetBkColor   (hdc, bk);
   ReleaseDC    (Debugstr::hwnd, hdc);
}


void Debugstr::Output (HDC hdc, char *psz, BOOL fRec)
{
   SIZE   siz;

   TextOut (hdc, gx, gy, psz, (int)strlen(psz));

   if (fRec)
      strcat (gdata[gcY], psz);

   GetTextExtentPoint (hdc, psz, (int)strlen(psz), &siz);

   gx += (ushort)siz.cx;
   gcX += (int)strlen(psz);
}



/*** DebugWndProc - Main window callback
 *
 * ENTRY:  
 * EXIT:      As WNDPROC
 *
 * MESSAGES:  WM_COMMAND    - application menu (About dialog box)
 *            WM_DESTROY    - destroy window
 *
 */

LRESULT APIENTRY Debugstr::DebugWndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   ushort   x, y, cX, cY;

   switch (message)
      {
      case WM_COMMAND:
           break;

      case WM_DESTROY:
            if (Debugstr::hwnd != 0)
               {
               CloseWindow (Debugstr::hwnd);
               Debugstr::hwnd = 0;	// Turn off debugging--no window!
               }
           break;

      case WM_OUTSTRING:
            {
            char *str = (char*)lParam;
            debug.OutString (str, TRUE);
            Free (str);
            }
           break;

      case WM_PAINT:
            {
            PAINTSTRUCT ps;
            HDC         hdc;
            hdc = BeginPaint (hWnd, &ps);
            EndPaint (hWnd, &ps);

            x = gx; y = gy; cX = gcX; cY = gcY;

            gx = minX;
            gy = minY;
            gcX = 0;
            gcY = 0;

            for (gcY = 0; gcY < nRefr; )
               {
               TCHAR szLine[] = TEXT("\n");
               debug.OutString (gdata[gcY], FALSE);
               debug.OutString (szLine, FALSE);
               }
            debug.OutString (gdata[gcY], FALSE);

            gx = x; gy = y; gcX = cX; gcY = cY;
            return 0;
            }
           break;

      default:
           break;
      }

   return (DefWindowProc(hWnd, message, wParam, lParam));
}


/*** cxAvgWidth - Returns the average width of the chars in Arial/8point.
 *
 * ENTRY:     none
 *
 * EXIT:      ushort pels
 *
 * ERROR:     returns 8
 *
 */

ushort cxAvgWidth (void)
{
   HFONT      hfnt, hfntOld = NULL;
   LOGFONT    lf;
   TEXTMETRIC tm;
   HDC        hdc;


   hdc = GetDC( GetDesktopWindow() );

   memset (&lf, 0, sizeof(lf));
   lf.lfHeight = -MulDiv (8, GetDeviceCaps(hdc, LOGPIXELSY), 72);
   strcpy (lf.lfFaceName, "Arial");

   if ((hfnt = CreateFontIndirect(&lf)) != NULL)
      {
      hfntOld = (HFONT)SelectObject(hdc, hfnt);
      }

   if (! GetTextMetrics (hdc, &tm))
      return 8;

   if (hfntOld != NULL)
      {
      SelectObject (hdc, hfntOld);
      DeleteObject (hfnt);
      }

   ReleaseDC (GetDesktopWindow(), hdc);

   return (ushort)tm.tmAveCharWidth;
}


/*
 * LOG ROUTINES
 *
 */

#define LONG_TYPE    0x0001
#define SHORT_TYPE   0x0002
#define INT_TYPE     0x0004
#define CHAR_TYPE    0x0008
#define STRING_TYPE  0x0010
#define FLOAT_TYPE   0x0020
#define COMMA_TYPE   0x0040
#define MSG_TYPE     0x0080

cdecl LogOut::LogOut (char *psz, ...)
{
   char     *pch;
   ushort    n;
   BOOL      fBreak;
   va_list   arg;
   va_start (arg, psz);

   strcpy (pszFormat, psz);
   nArgs = 0;

   for (pch = psz; *pch; pch++)
      {
      if (*pch != '%')
         continue;

      n = 0;
      fBreak = FALSE;

      while (!fBreak)
         {
         if (!* (++pch))
            break;

         switch (*pch)
            {
            case 'F':    n |= LONG_TYPE;    break;
            case 'l':    n |= LONG_TYPE;    break;
            case 'h':    n |= SHORT_TYPE;   break;
            case 'X':    n |= INT_TYPE;     break;
            case 'x':    n |= INT_TYPE;     break;
            case 'O':    n |= INT_TYPE;     break;
            case 'o':    n |= INT_TYPE;     break;
            case 'd':    n |= INT_TYPE;     break;
            case 'u':    n |= INT_TYPE;     break;
            case 'c':    n |= CHAR_TYPE;    break;
            case 's':    n |= STRING_TYPE;  break;
            default:     fBreak = TRUE;     break;
            }
         }

      if (nArgs == MAX_ARGS)
         break;

      aPtr[nArgs] = va_arg (arg, char *);

      nArgs++;

      if (! *pch)
         break;
      }
}

LogOut::~LogOut (void)
{
   char      tmpfmt[ 64 ];
   char      text[ 256 ];
   char     *pszOut;
   char     *pszTmpFmt;
   char     *pszFmt;
   char     *pch;
   ushort    i = 0;
   ushort    n;
   BOOL      fBreak;

   pszOut = text;

   for (pszFmt = pszFormat; *pszFmt; pszFmt++)
      {
      if (*pszFmt != '%')
         {
         *pszOut++ = *pszFmt;
         continue;
         }

      n = 0;
      fBreak = FALSE;

      for (pszTmpFmt = tmpfmt; !fBreak; )
         {
         *pszTmpFmt++ = *pszFmt;

         if (!* (++pszFmt))
            break;

         switch (*pszFmt)
            {
            case 'F':    n |= LONG_TYPE;    break;   // (far)
            case 'l':    n |= LONG_TYPE;    break;
            case 'h':    n |= SHORT_TYPE;   break;
            case 'X':    n |= INT_TYPE;     break;
            case 'x':    n |= INT_TYPE;     break;
            case 'O':    n |= INT_TYPE;     break;
            case 'o':    n |= INT_TYPE;     break;
            case 'd':    n |= INT_TYPE;     break;
            case 'u':    n |= INT_TYPE;     break;
            case 'c':    n |= CHAR_TYPE;    break;
            case 's':    n |= STRING_TYPE;  break;
            default:     fBreak = TRUE;     break;
            }
         }

      *pszTmpFmt = 0;

      pch = aPtr[ i ];
      i++;


      if (n & STRING_TYPE)
         {
         wsprintf (pszOut, tmpfmt, (char far *)pch);
         }
      else if (n & LONG_TYPE)
         wsprintf (pszOut, tmpfmt, *(long *)pch);
      else if (n & SHORT_TYPE)
         wsprintf (pszOut, tmpfmt, *(short *)pch);
      else if (n & INT_TYPE)
         wsprintf (pszOut, tmpfmt, *(int *)pch);
      else if (n & CHAR_TYPE)
         wsprintf (pszOut, tmpfmt, (char)*(char *)pch);
      else
         *pszOut = 0;

      pszOut = &pszOut[ strlen(pszOut) ];

      if (! *pszFmt)
         break;
      }

   *pszOut = 0;

   debug << text << "\n";
}

#endif // DEBUG
