/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef GETLANA_H
#define GETLANA_H

typedef BYTE lana_number_t;
struct LANAINFO
 {
   lana_number_t lana_number;
   TCHAR lana_name[MAX_PATH];
 };

 LANAINFO * GetLana(TCHAR* msg, const char *LanaName);

 void GetAfsName(int lanaNumber, BOOL isGateway, TCHAR* name);

#endif