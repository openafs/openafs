/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include "stdafx.h"
#include <winsock2.h>
#include <ws2tcpip.h>

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <tchar.h>
#include <stdarg.h>

#include "msgs.h"
#include <WINNT\talocale.h>



/*
  ShowMessageBox:

  This function takes three main arguements, the stringtable ID, the button types
  to be displayed (default = MB_OK) and the help table reference (default = 0, no
  help) and then a variable amount of arguements. The variable list does not need
  a special ending flag/character/number. The list is read only as needed, which
  is defined by the string table and the presence of any "%X" characters, where X
  is one of the printf format types. The order of the variable list MUST
  correspond to the order of types in the string table entry. If the string table
  calls for INT INT UINT CHAR* DOUBLE, then the arguement list had better be INT
  INT UINT CHAR* DOUBLE or else there will be serious problems (stack will be
  misread, general protection faults, garbage output, and other errors).

  This function takes the arguements passed in the list and inserts them by
  parsing and pszcut/pszpaste the string table entry to add all the arguements passed
  in. This allows for any generic	message to be created.

  %i,d 	  = Integer
  %u 	  = unsigned int
  %x,X    = Hex (takes an integer arguement, pszconverts it)
  %g,f,e  = Double
  %s	  = String (char*)
  %l
  d       = Long int
  x	  = long hex
  %c	  = character (one)
  %a	  = String Table Ref. (UINT)
  %o	  = CString object (prints the string of the object)
  default = prints out string so far, with error message attached at place of error.

  Return type is the button pressed in the message box.

*/

UINT ShowMessageBox (UINT Id, UINT Button, UINT Help, ...) {

    CString temp;
    TCHAR *pszstring,
    *pszpaste,
    *pszcut,
    *pszdone,
    *pszconvert;
    TCHAR chread;
    va_list params;
    int x;

    pszconvert = new TCHAR[255];
    va_start(params, Help);
    LoadString (temp, Id);
    pszstring = temp.GetBuffer(512);
    _tcscpy(pszstring,pszstring);
    temp.ReleaseBuffer();
    // Look and see - is there a need to insert chars (95% of the time, there won't)
    if (!_tcsstr(pszstring, _T("%"))) {
	delete pszconvert;
	return AfxMessageBox(pszstring, Button, Help);
    }

    x = _tcscspn(pszstring, _T("%"));
    pszdone = new TCHAR[512];
    pszcut = new TCHAR[512];
    pszpaste = new TCHAR[512];
    _tcscpy(pszcut, &pszstring[x+2]);
    _tcsncpy(pszpaste, pszstring, x);
    pszpaste[x] = _T('\0');
    chread = pszstring[x+1];

    for ( ; ; ) {

	switch (chread) {
	case	_T('i') :
	case	_T('d') :
	{
	    int anint = va_arg(params, int);
	    _itot( anint, pszconvert, 10);
	    break;
	}
	case	_T('u') :
	{
	    UINT anuint = va_arg(params, UINT);
	    _itot( anuint, pszconvert, 10);
	    break;
	}

	case	_T('x') :
	case	_T('X') :
	{
	    int ahex = va_arg(params, int);
	    _itot( ahex, pszconvert, 16);
	    break;
	}
	case	_T('g') :
	case	_T('f') :
	case	_T('e') :
	{
	    double adbl = va_arg(params, double);
            _stprintf(pszconvert, _T("%g"), adbl);
	    break;
	}
	case	_T('s') :
	{
	    TCHAR *pStr = va_arg(params, TCHAR*);
	    ASSERT(_tcslen(pStr) <= 255);
	    _tcscpy(pszconvert, pStr);
	    break;
	}
	case	_T('l') :
	{
	    chread = pszdone[x+2];
	    switch(chread) {
	    case	_T('x')	:
	    {
		long int alhex = va_arg(params, long int);
		_ltot(alhex, pszconvert, 16);
		_tcscpy(pszcut, &pszcut[1]);
		break;
	    }
	    case 	_T('d')	:
		default 	:
		{
		    long int along = va_arg(params, long int);
		    _ltot( along, pszconvert, 10);
		    // For the L, there will be one character after it,
		    //   so move ahead another letter
		    _tcscpy(pszcut, &pszcut[1]);
		    break;
		}
	    }
	    break;
	}

	case	_T('c') :
	{
	    int letter = va_arg(params, int);
	    pszconvert[0] = (TCHAR)letter;
	    pszconvert[1] = '\0';
	    break;
	}
	case 	_T('a')	:
	{
	    CString zeta;
	    TCHAR* lsc;
	    UINT ls = va_arg(params, UINT);
	    LoadString (zeta, ls);
	    lsc = zeta.GetBuffer(255);
	    _tcscpy(pszconvert, lsc);
	    zeta.ReleaseBuffer();
	    break;
	}
	case	_T('o')	:
	{
	    CString get = va_arg(params, CString);
	    TCHAR* ex = get.GetBuffer(255);
	    _tcscpy(pszconvert,ex);
	    get.ReleaseBuffer();
	    break;
	}
	    default 	:
	    {
		_tcscpy(pszconvert, _T(" Could not load message. Invalid %type in string table entry. "));
		delete pszdone;
		pszdone = new TCHAR[_tcslen(pszpaste)+_tcslen(pszcut)+_tcslen(pszconvert)+5];
		_tcscpy(pszdone, pszpaste);
		_tcscat(pszdone, pszconvert);
		_tcscat(pszdone, pszcut);
		AfxMessageBox(pszdone, Button, Help);
		delete pszcut;
		delete pszpaste;
		delete pszconvert;
		delete pszdone;
		ASSERT(FALSE);
		return 0;
	    }
	} // case

	delete pszdone;
	pszdone = new TCHAR[_tcslen(pszpaste)+_tcslen(pszcut)+_tcslen(pszconvert)+5];
	_tcscpy(pszdone, pszpaste);
	_tcscat(pszdone, pszconvert);
	_tcscat(pszdone, pszcut);
	// Now pszdone holds the entire message.
	// Check to see if there are more insertions to be made or not

	if (!_tcsstr(pszdone, _T("%")))	{
	    UINT rt_type = AfxMessageBox(pszdone, Button, Help);
	    delete pszcut;
	    delete pszpaste;
	    delete pszconvert;
	    delete pszdone;
	    return rt_type;
	} // if

	// there are more insertions to make, prepare the strings to use.
	x = _tcscspn(pszdone, _T("%"));
	_tcscpy(pszcut, &pszdone[x+2]);
	_tcsncpy(pszpaste, pszdone, x);
	pszpaste[x] = _T('\0');
	chread = pszdone[x+1];

    } // for
    ASSERT(FALSE);
    return 0;

} // ShowMessageBox

CString GetMessageString(UINT Id, ...)
{
    CString temp;
    TCHAR *pszstring,
    *pszpaste,
    *pszcut,
    *pszdone,
    *pszconvert;
    TCHAR chread;
    va_list params;
    int x;
    CString strMsg;

    pszconvert = new TCHAR[255];
    va_start(params, Id);
    LoadString (temp, Id);
    pszstring = temp.GetBuffer(512);
    _tcscpy(pszconvert,pszstring);
    temp.ReleaseBuffer();

    // Look and see - is there a need to insert chars (95% of the time, there won't)
    if (!_tcsstr(pszstring, _T("%"))) {
	strMsg = pszconvert;
	delete pszconvert;
	return strMsg;
    }

    x = _tcscspn(pszstring, _T("%"));
    pszdone = new TCHAR[512];
    pszcut = new TCHAR[512];
    pszpaste = new TCHAR[512];
    _tcscpy(pszcut, &pszstring[x+2]);
    _tcsncpy(pszpaste, pszstring, x);
    pszpaste[x] = _T('\0');
    chread = pszstring[x+1];

    for ( ; ; ) {

	switch (chread) {
	case	_T('i') :
	case	_T('d') :
	{
	    int anint = va_arg(params, int);
	    _itot( anint, pszconvert, 10);
	    break;
	}
	case	_T('u') :
	{
	    UINT anuint = va_arg(params, UINT);
	    _itot( anuint, pszconvert, 10);
	    break;
	}

	case	_T('x') :
	case	_T('X') :
	{
	    int ahex = va_arg(params, int);
	    _itot( ahex, pszconvert, 16);
	    break;
	}
	case	_T('g') :
	case	_T('f') :
	case	_T('e') :
	{
	    double adbl = va_arg(params, double);
            _stprintf(pszconvert, _T("%g"), adbl);
	    break;
	}
	case	_T('s') :
	{
	    TCHAR *pStr = va_arg(params, TCHAR*);
	    ASSERT(_tcslen(pStr) <= 255);
	    _tcscpy(pszconvert, pStr);
	    break;
	}
	case	_T('l') :
	{
	    chread = pszdone[x+2];
	    switch(chread) {
	    case	_T('x')	:
	    {
		long int alhex = va_arg(params, long int);
		_ltot(alhex, pszconvert, 16);
		_tcscpy(pszcut, &pszcut[1]);
		break;
	    }
	    case 	_T('d')	:
		default 	:
		{
		    long int along = va_arg(params, long int);
		    _ltot( along, pszconvert, 10);
		    // For the L, there will be one character after it,
		    //   so move ahead another letter
		    _tcscpy(pszcut, &pszcut[1]);
		    break;
		}
	    }
	    break;
	}

	case	_T('c') :
	{
	    int letter = va_arg(params, int);
	    pszconvert[0] = (TCHAR)letter;
	    pszconvert[1] = _T('\0');
	    break;
	}
	case 	_T('a')	:
	{
	    CString zeta;
	    TCHAR* lsc;
	    UINT ls = va_arg(params, UINT);
	    LoadString (zeta, ls);
	    lsc = zeta.GetBuffer(255);
	    _tcscpy(pszconvert, lsc);
	    zeta.ReleaseBuffer();
	    break;
	}
	case	_T('o')	:
	{
	    CString get = va_arg(params, CString);
	    TCHAR* ex = get.GetBuffer(255);
	    _tcscpy(pszconvert,ex);
	    get.ReleaseBuffer();
	    break;
	}
	default:
	    {
		_tcscpy(pszconvert, _T(" Could not load message. Invalid %type in string table entry. "));
		delete pszdone;
		pszdone = new TCHAR[_tcslen(pszpaste)+_tcslen(pszcut)+_tcslen(pszconvert)+5];
		_tcscpy(pszdone, pszpaste);
		_tcscat(pszdone, pszconvert);
		_tcscat(pszdone, pszcut);
		strMsg = pszdone;
		delete pszcut;
		delete pszpaste;
		delete pszconvert;
		delete pszdone;
		ASSERT(FALSE);
		return strMsg;
	    }
	} // case

	delete pszdone;
	pszdone = new TCHAR[_tcslen(pszpaste)+_tcslen(pszcut)+_tcslen(pszconvert)+5];
	_tcscpy(pszdone, pszpaste);
	_tcscat(pszdone, pszconvert);
	_tcscat(pszdone, pszcut);
	// Now pszdone holds the entire message.
	// Check to see if there are more insertions to be made or not

	if (!_tcsstr(pszdone, _T("%")))	{
	    strMsg = pszdone;
	    delete pszcut;
	    delete pszpaste;
	    delete pszconvert;
	    delete pszdone;
	    return strMsg;
	} // if

	// there are more insertions to make, prepare the strings to use.
	x = _tcscspn(pszdone, _T("%"));
	_tcscpy(pszcut, &pszdone[x+2]);
	_tcsncpy(pszpaste, pszdone, x);
	pszpaste[x] = _T('\0');
	chread = pszdone[x+1];

    } // for
    ASSERT(FALSE);
    return strMsg;
}

void LoadString (CString &Str, UINT id)
{
    TCHAR szString[ 256 ];
    GetString (szString, id);
#ifdef UNICODE
    CString wstr(szString);
    Str = wstr;
#else
    Str = szString;
#endif
}

