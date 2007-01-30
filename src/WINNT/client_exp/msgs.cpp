/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

extern "C" {
#include <afs/param.h>
#include <afs/stds.h>
}

#include <string.h>
#include <stdarg.h>

#include "stdafx.h"
#include "msgs.h"



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
    char *pszstring, 
    *pszpaste, 
    *pszcut, 
    *pszdone,
    *pszconvert;
    char chread;
    va_list params;
    int x;

    pszconvert = new char[255];    	
    va_start(params, Help);
    LoadString (temp, Id);
    pszstring = temp.GetBuffer(512);
    strcpy(pszstring,pszstring);
    temp.ReleaseBuffer();
    // Look and see - is there a need to insert chars (95% of the time, there won't)
    if (!strstr(pszstring, "%")) {
	delete pszconvert;
	return AfxMessageBox(pszstring, Button, Help);
    }   

    x = strcspn(pszstring, "%");
    pszdone = new char[512];
    pszcut = new char[512];
    pszpaste = new char[512];
    strcpy(pszcut, &pszstring[x+2]);
    strncpy(pszpaste, pszstring, x);
    pszpaste[x] = '\0';
    chread = pszstring[x+1];

    for ( ; ; ) {

	switch (chread) { 
	case	'i' :
	case	'd' :
	{ 	    
	    int anint = va_arg(params, int);
	    _itoa( anint, pszconvert, 10);
	    break;
	}
	case	'u' :
	{	
	    UINT anuint = va_arg(params, UINT);
	    _itoa( anuint, pszconvert, 10);
	    break;
	}

	case	'x' :
	case	'X' :   
	{
	    int ahex = va_arg(params, int);
	    _itoa( ahex, pszconvert, 16);
	    break;
	}
	case	'g' :
	case	'f' :
	case	'e' :   
	{
	    double adbl = va_arg(params, double);
	    _gcvt( adbl, 10, pszconvert);
	    break;
	}
	case	's' :	
	{
	    char *pStr = va_arg(params, char*);
	    ASSERT(strlen(pStr) <= 255);
	    strcpy(pszconvert, pStr);
	    break;
	}
	case	'l' :	
	{
	    chread = pszdone[x+2];
	    switch(chread) {
	    case	'x'	:
	    {
		long int alhex = va_arg(params, long int);
		_ltoa(alhex, pszconvert, 16);
		strcpy(pszcut, &pszcut[1]);
		break;
	    }
	    case 	'd'	:
		default 	:
		{
		    long int along = va_arg(params, long int);
		    _ltoa( along, pszconvert, 10);
		    // For the L, there will be one character after it,
		    //   so move ahead another letter
		    strcpy(pszcut, &pszcut[1]);
		    break;
		}
	    }
	    break;
	}

	case	'c' :	
	{
	    int letter = va_arg(params, int);
	    pszconvert[0] = (char)letter;
	    pszconvert[1] = '\0'; 
	    break;
	}
	case 	'a'	:
	{
	    CString zeta;
	    char* lsc;
	    UINT ls = va_arg(params, UINT);
	    LoadString (zeta, ls);
	    lsc = zeta.GetBuffer(255);
	    strcpy(pszconvert, lsc);
	    zeta.ReleaseBuffer();
	    break;
	}
	case	'o'	:
	{
	    CString get = va_arg(params, CString);
	    char* ex = get.GetBuffer(255);
	    strcpy(pszconvert,ex);
	    get.ReleaseBuffer();
	    break;
	}
	    default 	:
	    {	
		strcpy(pszconvert, " Could not load message. Invalid %type in string table entry. ");
		delete pszdone;
		pszdone = new char[strlen(pszpaste)+strlen(pszcut)+strlen(pszconvert)+5];
		strcpy(pszdone, pszpaste);
		strcat(pszdone, pszconvert);
		strcat(pszdone, pszcut);
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
	pszdone = new char[strlen(pszpaste)+strlen(pszcut)+strlen(pszconvert)+5];
	strcpy(pszdone, pszpaste);
	strcat(pszdone, pszconvert);
	strcat(pszdone, pszcut);
	// Now pszdone holds the entire message.
	// Check to see if there are more insertions to be made or not
	
	if (!strstr(pszdone, "%"))	{
	    UINT rt_type = AfxMessageBox(pszdone, Button, Help);
	    delete pszcut;
	    delete pszpaste;
	    delete pszconvert;
	    delete pszdone;
	    return rt_type;
	} // if

	// there are more insertions to make, prepare the strings to use.
	x = strcspn(pszdone, "%");
	strcpy(pszcut, &pszdone[x+2]);
	strncpy(pszpaste, pszdone, x); 
	pszpaste[x] = '\0';
	chread = pszdone[x+1];
	
    } // for
    ASSERT(FALSE);		
    return 0;

} // ShowMessageBox

CString GetMessageString(UINT Id, ...)
{
    CString temp;
    char *pszstring, 
    *pszpaste, 
    *pszcut, 
    *pszdone,
    *pszconvert;
    char chread;
    va_list params;
    int x;
    CString strMsg;

    pszconvert = new char[255];    	
    va_start(params, Id);
    LoadString (temp, Id);
    pszstring = temp.GetBuffer(512);
    strcpy(pszconvert,pszstring);
    temp.ReleaseBuffer();

    // Look and see - is there a need to insert chars (95% of the time, there won't)
    if (!strstr(pszstring, "%")) {
	strMsg = pszconvert;
	delete pszconvert;
	return strMsg;
    }   

    x = strcspn(pszstring, "%");
    pszdone = new char[512];
    pszcut = new char[512];
    pszpaste = new char[512];
    strcpy(pszcut, &pszstring[x+2]);
    strncpy(pszpaste, pszstring, x);
    pszpaste[x] = '\0';
    chread = pszstring[x+1];

    for ( ; ; ) {

	switch (chread) { 
	case	'i' :
	case	'd' :
	{ 	    
	    int anint = va_arg(params, int);
	    _itoa( anint, pszconvert, 10);
	    break;
	}
	case	'u' :
	{	
	    UINT anuint = va_arg(params, UINT);
	    _itoa( anuint, pszconvert, 10);
	    break;
	}

	case	'x' :
	case	'X' :   
	{
	    int ahex = va_arg(params, int);
	    _itoa( ahex, pszconvert, 16);
	    break;
	}
	case	'g' :
	case	'f' :
	case	'e' :   
	{
	    double adbl = va_arg(params, double);
	    _gcvt( adbl, 10, pszconvert);
	    break;
	}
	case	's' :	
	{
	    char *pStr = va_arg(params, char*);
	    ASSERT(strlen(pStr) <= 255);
	    strcpy(pszconvert, pStr);
	    break;
	}
	case	'l' :	
	{
	    chread = pszdone[x+2];
	    switch(chread) {
	    case	'x'	:
	    {
		long int alhex = va_arg(params, long int);
		_ltoa(alhex, pszconvert, 16);
		strcpy(pszcut, &pszcut[1]);
		break;
	    }
	    case 	'd'	:
		default 	:
		{
		    long int along = va_arg(params, long int);
		    _ltoa( along, pszconvert, 10);
		    // For the L, there will be one character after it,
		    //   so move ahead another letter
		    strcpy(pszcut, &pszcut[1]);
		    break;
		}
	    }
	    break;
	}	

	case	'c' :	
	{
	    int letter = va_arg(params, int);
	    pszconvert[0] = (char)letter;
	    pszconvert[1] = '\0'; 
	    break;
	}
	case 	'a'	:
	{
	    CString zeta;
	    char* lsc;
	    UINT ls = va_arg(params, UINT);
	    LoadString (zeta, ls);
	    lsc = zeta.GetBuffer(255);
	    strcpy(pszconvert, lsc);
	    zeta.ReleaseBuffer();
	    break;
	}
	case	'o'	:
	{
	    CString get = va_arg(params, CString);
	    char* ex = get.GetBuffer(255);
	    strcpy(pszconvert,ex);
	    get.ReleaseBuffer();
	    break;
	}
	default:
	    {	
		strcpy(pszconvert, " Could not load message. Invalid %type in string table entry. ");
		delete pszdone;
		pszdone = new char[strlen(pszpaste)+strlen(pszcut)+strlen(pszconvert)+5];
		strcpy(pszdone, pszpaste);
		strcat(pszdone, pszconvert);
		strcat(pszdone, pszcut);
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
	pszdone = new char[strlen(pszpaste)+strlen(pszcut)+strlen(pszconvert)+5];
	strcpy(pszdone, pszpaste);
	strcat(pszdone, pszconvert);
	strcat(pszdone, pszcut);
	// Now pszdone holds the entire message.
	// Check to see if there are more insertions to be made or not
	
	if (!strstr(pszdone, "%"))	{
	    strMsg = pszdone;
	    delete pszcut;
	    delete pszpaste;
	    delete pszconvert;
	    delete pszdone;
	    return strMsg;
	} // if

	// there are more insertions to make, prepare the strings to use.
	x = strcspn(pszdone, "%");
	strcpy(pszcut, &pszdone[x+2]);
	strncpy(pszpaste, pszdone, x); 
	pszpaste[x] = '\0';
	chread = pszdone[x+1];
	
    } // for
    ASSERT(FALSE);		
    return strMsg;
}

void LoadString (CString &Str, UINT id)
{
    TCHAR szString[ 256 ];
    GetString (szString, id);
    Str = szString;
}

