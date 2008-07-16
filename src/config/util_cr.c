/* crlf.c : Defines the entry point for the console application.*/

/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html

*/

#undef _CRTDBG_MAP_ALLOC
#include "stdio.h"
#include "io.h"
#include <assert.h>
#include "string.h"
#include "process.h"
#include "windows.h"
#include "malloc.h"
#include "time.h"
#include "stdlib.h"

#ifndef  intptr_t
#define intptr_t INT_PTR
#endif

void
usage()
{
    printf("util_cr file ;remove cr (from crlf)\n\
	OR util_cr } ProductVersion in_filename out_filename ; substitute for %%1-%%5 in file\n\
	   %%1=Major version, %%2=Minor version, %%3=Patch(first digit) %%4=(last two digits) %%5=Version display string \n\
	   ProductVersion=maj.min.pat.pat2 ;maj=numeric, min=numeric pat,pat2 are not more than 3 digits or 1-2 digits and one alpha \n\
	   e.g 1.0.4.1, 1.0.4 a 1.0.401, 1.0.4a  all represent the same version\n\
	OR util_cr + file ;add cr\n \
	OR util_cr * \"+[register key value] x=y\" ; add register key value\n\
	OR util_cr * \"-[register key value]\" ; aremove register key value\n\
	OR util_cr @ file.ini \"[SectionKey]variable=value\" ; update ini-ipr-pwf file\n\
	OR util_cr @ file.ini \"[SectionKey]variable=value*DatE*\" ; update ini-ipr-pwf file, insert date\n\
	OR util_cr ~  ;force error\n\
	OR util_cr _del  [/q=quiet /s=recurese] [*. *. *.] ;delete \n\
	OR util_cr _cpy [/q=quiet ] [*. *. *.] destinationFolder;\n\
	OR util_cr _isOS [nt xp 98 9x w2] ;test for OS, return 1 if match else 0\n\
	OR util_cr _dir !build type!source!object! set current directory in file->home base is used for offset\n\
	OR unil_cr _ver return compiler version\n");
    exit(0xc000);
}

struct TRANSLATION {
    WORD langID;		// language ID
    WORD charset;		// character set (code page)
};

int
CheckVersion(int argc, char *argv[])
{
    OSVERSIONINFO VersionInfo;
    int i;
    memset(&VersionInfo, 0, sizeof(VersionInfo));
    VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&VersionInfo)) {
	return 0XC000;
    }
    for (i = 2; i < argc; i++) {
	if (stricmp(argv[i], "nt") == 0) {
	    if ((VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		&& (VersionInfo.dwMajorVersion == 4)
		&& (VersionInfo.dwMinorVersion == 0))
		return 1;
	}
	if (stricmp(argv[i], "xp") == 0) {
	    if ((VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		&& (VersionInfo.dwMajorVersion == 5)
		&& (VersionInfo.dwMinorVersion == 1))
		return 1;
	}
	if (stricmp(argv[i], "w2") == 0) {
	    if ((VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
		&& (VersionInfo.dwMajorVersion == 5)
		&& (VersionInfo.dwMinorVersion == 0))
		return 1;
	}
	if (stricmp(argv[i], "98") == 0) {
	    if ((VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		&& (VersionInfo.dwMinorVersion == 10))
		return 1;
	}
	if (stricmp(argv[i], "95") == 0) {
	    if ((VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		&& (VersionInfo.dwMinorVersion == 0))

		return 1;
	}
	if (stricmp(argv[i], "9x") == 0) {
	    if (VersionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
		return 1;
	}
	if (stricmp(argv[i], "_") == 0)
	    return 0;
    }
    return 0;
}

void
Addkey(const char *hkey, const char *subkey, const char *stag,
       const char *sval)
{
    DWORD disposition, result;
    HKEY kPkey, kHkey = 0;
    if (strcmp(hkey, "HKEY_CLASSES_ROOT") == 0)
	kHkey = HKEY_CLASSES_ROOT;
    if (strcmp(hkey, "HKEY_CURRENT_USER") == 0)
	kHkey = HKEY_CURRENT_USER;
    if (strcmp(hkey, "HKEY_LOCAL_MACHINE") == 0)
	kHkey = HKEY_LOCAL_MACHINE;
    if (kHkey == 0)
	usage();
    result = (RegCreateKeyEx(kHkey	/*HKEY_LOCAL_MACHINE */
			     , subkey, 0, NULL, REG_OPTION_NON_VOLATILE,
			     KEY_ALL_ACCESS, NULL, &kPkey,
			     &disposition) == ERROR_SUCCESS);
    if (!result) {
	printf("AFS Error - Could Not create a registration key\n");
	exit(0xc000);
    }
    if (stag == NULL)
	return;
    if ((sval) && (strlen(sval))) {
	if (*stag == '@')
	    result =
		RegSetValueEx(kPkey, "", 0, REG_SZ, (CONST BYTE *) sval,
			      (DWORD)strlen(sval));
	else
	    result =
		RegSetValueEx(kPkey, stag, 0, REG_SZ, (CONST BYTE *) sval,
			      (DWORD)strlen(sval));
    } else {

	if (*stag == '@')
	    result =
		(RegSetValueEx(kPkey, "", 0, REG_SZ, (CONST BYTE *) "", 0));
	else
	    result =
		(RegSetValueEx(kPkey, stag, 0, REG_SZ, (CONST BYTE *) "", 0));
    }
    if (result != ERROR_SUCCESS) {
	printf("AFS Error - Could Not create a registration key\n");
	exit(0xc000);
    }
}

void
Subkey(const char *hkey, const char *subkey)
{
    DWORD result;
    HKEY kHkey = 0;
    if (strcmp(hkey, "HKEY_CLASSES_ROOT") == 0)
	kHkey = HKEY_CLASSES_ROOT;
    if (strcmp(hkey, "HKEY_CURRENT_USER") == 0)
	kHkey = HKEY_CURRENT_USER;
    if (strcmp(hkey, "HKEY_LOCAL_MACHINE") == 0)
	kHkey = HKEY_LOCAL_MACHINE;
    if (kHkey == 0)
	usage();
    result = RegDeleteKey(kHkey, subkey);
    if (result != ERROR_SUCCESS) {
	printf("AFS Error - Could Not create a registration key\n");
	exit(0xc000);
    }
}

void
doremove(BOOL bRecurse, BOOL bQuiet, char *argv)
{
    char *pParm;
    char parm[MAX_PATH + 1];
    char basdir[MAX_PATH + 1];
    strcpy(parm, argv);
    pParm = parm;
    GetCurrentDirectory(sizeof(basdir), basdir);
    if (strrchr(parm, '\\') != NULL) {	/*jump to base directory */
	pParm = strrchr(parm, '\\');
	*pParm = 0;
	if (!SetCurrentDirectory(parm))
	    return;
	pParm++;
    }
    if (!bRecurse) {
	struct _finddata_t fileinfo;
	intptr_t hfile;
	BOOL bmore;
	char basdir[MAX_PATH + 1];
	GetCurrentDirectory(sizeof(basdir), basdir);
	hfile = _findfirst(pParm, &fileinfo);
	bmore = (hfile != -1);
	while (bmore) {
	    if ((DeleteFile(fileinfo.name) == 1) && (!bQuiet))
		printf("Remove %s\\%s\n", basdir, fileinfo.name);
	    bmore = (_findnext(hfile, &fileinfo) == 0);
	}
	_findclose(hfile);
    } else {
	/*RECURSIVE LOOP - SCAN directories */
	struct _finddata_t fileinfo;
	intptr_t hfile;
	BOOL bmore;
	doremove(FALSE, bQuiet, pParm);
	hfile = _findfirst("*.*", &fileinfo);
	bmore = (hfile != -1);
	while (bmore) {
	    if (fileinfo.attrib & _A_SUBDIR) {
		if ((strcmp(fileinfo.name, ".") != 0)
		    && (strcmp(fileinfo.name, "..") != 0)) {
		    if (SetCurrentDirectory(fileinfo.name))
			doremove(TRUE, bQuiet, pParm);
		    SetCurrentDirectory(basdir);
		}
	    }
	    bmore = (_findnext(hfile, &fileinfo) == 0);
	}
	_findclose(hfile);
    }
    SetCurrentDirectory(basdir);
}

void
gencurdir(char *val)
{
    char bld[MAX_PATH + 1], parm[MAX_PATH + 1], src[MAX_PATH + 1],
	obj[MAX_PATH + 1], dir[MAX_PATH + 1], curdir[MAX_PATH + 1];
    char *p, *po;
    FILE *f;
    BOOL isObjAbs, isSrcAbs;	/* two flags to determine if either string is not relative */
    strcpy(bld, val + 1);	/*it better be checked or free */
    strcpy(src, strchr(bld, '!') + 1);
    strcpy(obj, strchr(src, '!') + 1);
    *strchr(bld, '!') = 0;
    *strchr(obj, '!') = 0;
    *strchr(src, '!') = 0;
    isObjAbs = ((*obj == '\\') || (strstr(obj, ":")));
    isSrcAbs = ((*src == '\\') || (strstr(src, ":")));
    GetCurrentDirectory(MAX_PATH, dir);
    strlwr(bld);
    strlwr(dir);
    strlwr(src);
    strlwr(obj);
    strcpy(curdir, dir);
    strcat(curdir, "\\ \n");
    if (GetTempPath(MAX_PATH, parm) == 0)
	exit(0xc000);
    strcat(parm, "home");
    if ((f = fopen(parm, "w")) == NULL)
	exit(0xc000);
    __try {
	__try {
	    if (obj[strlen(obj) - 1] != '\\')
		strcat(obj, "\\");
	    if (src[strlen(src) - 1] != '\\')
		strcat(src, "\\");
	    do {		/* try to match src or obj */
		if ((p = strstr(dir, src)) && isSrcAbs) {
		    po = p;
		    p += strlen(src);
		} else if ((p = strstr(dir, src)) && !isSrcAbs
			   && *(p - 1) == '\\') {
		    po = p;
		    p += strlen(src);
		} else if ((p = strstr(dir, obj)) && isObjAbs) {
		    po = p;
		    p += strlen(obj);
		} else if ((p = strstr(dir, obj)) && !isObjAbs
			   && *(p - 1) == '\\') {
		    po = p;
		    p += strlen(obj);
		}
	    } while (strstr(p, src) || strstr(p, obj));
	    if (isObjAbs) {
		sprintf(parm, "OJT=%s%s\\%s \n", obj, bld, p);
	    } else {
		*po = 0;
		sprintf(parm, "OJT=%s%s%s\\%s \n", dir, obj, bld, p);
	    }
	    if (strcmp(curdir, parm + 4) == 0)	/* current directory is object */
		strcpy(parm, "OJT= \n");
	    fwrite(parm, strlen(parm), 1, f);
	    if (isSrcAbs) {
		sprintf(parm, "SRT=%s%s\\ \n", src, p);
	    } else {
		*(p - strlen(src)) = 0;
		sprintf(parm, "SRT=%s%s%s\\ \n", dir, src, p);
	    }
	    if (strcmp(curdir, parm + 4) == 0)	/* current directory is object */
		strcpy(parm, "SRT= \n");
	    fwrite(parm, strlen(parm), 1, f);
	    /* now lets set the AFS_LASTCMP environment variable */
	    sprintf(parm, "AFS_LASTCMP=%s\\%s", src, p);
	}
	__except(EXCEPTION_EXECUTE_HANDLER) {
	    exit(0xc000);
	}
    }
    __finally {
	fclose(f);
    }
}

int
isequal(char *msg1, char *msg2, char *disp)
{
    strlwr(msg1);
    strlwr(msg2);
    if (strcmp(msg1, msg2) != 0)
	return 0;
    printf("ERROR -- %s \n", disp);
    exit(0xc000);
}

int
SetSysEnv(int argc, char *argv[])
{
    DWORD_PTR dwResult;
    printf("assignment %s %s\n", argv[2], argv[3]);
    Addkey("HKEY_LOCAL_MACHINE",
	   "System\\CurrentControlSet\\Control\\Session Manager\\Environment",
	   argv[2]
	   , argv[3]
	);
    SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
		       (DWORD) "Environment", SMTO_NORMAL, 1, &dwResult);
    return 0;
}

int
main(int argc, char *argv[])
{
    char fname[128];
    FILE *file;
    int i;
    char **pvar, *ch, *save;
    size_t len;
    BOOL bRecurse = FALSE;
    BOOL bQuiet = FALSE;
    if (argc < 2)
	usage();

   /* RSM4: Add an "ECHO" that doesn't append a new line... */
   if (strcmp(argv[1], "_echo") == 0) {
      if(argc<3)
         usage();
      printf("%s",argv[2]);
      return 0;
   }

    if (strcmp(argv[1], "_sysvar") == 0) {
	if (argc < 4)
	    usage();
	return (SetSysEnv(argc, argv));

    }
    if (strnicmp(argv[1], "_dir", 4) == 0) {	/*get current directory routine */
	gencurdir(argv[1]);
	return 0;
    }
    if (strnicmp(argv[1], "_isequal", 4) == 0) {	/*get current directory routine */
	return isequal(argv[2], argv[3], argv[4]);
    }
    if (stricmp(argv[1], "_del") == 0) {	/*DELETE routine */
	int iargc = 2;
	if ((argc > iargc) && (stricmp(argv[iargc], "/s") == 0)) {
	    iargc++;
	    bRecurse = TRUE;
	}
	if ((argc > iargc) && (stricmp(argv[iargc], "/q") == 0)) {
	    iargc++;
	    bQuiet = TRUE;
	}
	if ((argc > iargc) && (stricmp(argv[iargc], "/s") == 0)) {
	    iargc++;
	    bRecurse = TRUE;
	}
	while (iargc < argc) {
	    doremove(bRecurse, bQuiet, argv[iargc]);
	    iargc++;
	}
	return 0;
    }
    if (strcmp(argv[1], "_ver") == 0) {
	return _MSC_VER;
    }
    if (argc < 3)
	usage();
    if (strcmp(argv[1], "_isOS") == 0)
	return CheckVersion(argc, argv);
    if (strcmp(argv[1], "}") == 0) {
	char v1[4], v2[4], v3[4], v4[4];
	char v5[132];
	char *ptr = NULL;
	char *buf;
	int maj;
	int min;
	int pat, pat2;
	strcpy(v5, argv[2]);
	if (argc < 5)
	    usage();
	if ((ptr = strtok(argv[2], ". \n")) == NULL)
	    return 0;
	maj = atoi(ptr);
	if ((ptr = strtok(NULL, ". \n")) == NULL)
	    return 0;
	min = atoi(ptr);
	if ((ptr = strtok(NULL, ". \n")) == NULL)
	    return 0;
	pat2 = -1;
	switch (strlen(ptr)) {
	case 0:
	    usage();
	case 1:
	    pat = atoi(ptr);
	    if (isdigit(*ptr) != 0)
		break;
	    usage();
	case 2:		//ONLY 1.0.44 is interpreted as 1.0.4.4 or 1.0.4a as 1.0.4.a
	    if (isdigit(*ptr) == 0)
		usage();
	    pat = *ptr - '0';
	    ptr++;
	    if (isalpha(*ptr) == 0) {
		pat2 = atoi(ptr);
	    } else if (isalpha(*ptr) != 0) {
		pat2 = tolower(*ptr) - 'a' + 1;
	    } else
		usage();
	    break;
	case 3:		//1.0.401 or 1.0.40a are the same; 
	    if ((isdigit(*ptr) == 0)	// first 2 must be digit
		|| (isdigit(*(ptr + 1)) == 0)
		|| (*(ptr + 1) != '0' && isdigit(*(ptr + 2)) == 0)	// disallow 1.0.4b0  or 1.0.41a 
		)
		usage();
	    pat = *ptr - '0';
	    ptr++;
	    pat2 = atoi(ptr);
	    ptr++;
	    if (isalpha(*ptr))
		pat2 = tolower(*ptr) - 'a' + 1;
	    break;
	default:
	    usage();
	}
	// last can be 1-2 digits or one alpha (if pat2 hasn't been set)
	if ((ptr = strtok(NULL, ". \n")) != NULL) {
	    if (pat2 >= 0)
		usage();
	    switch (strlen(ptr)) {
	    case 1:
		pat2 = (isdigit(*ptr)) ? atoi(ptr) : tolower(*ptr) - 'a' + 1;
		break;
	    case 2:
		if (isdigit(*ptr) == 0 || isdigit(*(ptr + 1)) == 0)
		    usage();
		pat2 = atoi(ptr);
		break;
	    default:
		usage();
	    }
	}
	file = fopen(argv[3], "r");
	if (file == NULL)
	    usage();
	len = filelength(_fileno(file));
	save = (char *)malloc(len + 1);
	buf = save;
	len = fread(buf, sizeof(char), len, file);
	buf[len] = 0;		//set eof
	fclose(file);
	file = fopen(argv[4], "w");
	if (file == NULL)
	    usage();
	sprintf(v1, "%i", maj);
	sprintf(v2, "%i", min);
	sprintf(v3, "%i", pat);
	sprintf(v4, "%02i", pat2);
	while (1) {
	    ptr = strstr(buf, "%");
	    fwrite(buf, 1, (ptr) ? ptr - buf : strlen(buf), file);	//write file if no % found or up to %
	    if (ptr == NULL)
		break;
	    switch (*(ptr + 1))	//skip first scan if buf="1...."
	    {
	    case '1':
		fwrite(v1, 1, strlen(v1), file);
		ptr++;
		break;
	    case '2':
		fwrite(v2, 1, strlen(v2), file);
		ptr++;
		break;
	    case '3':
		fwrite(v3, 1, strlen(v3), file);
		ptr++;
		break;
	    case '4':
		fwrite(v4, 1, strlen(v4), file);
		ptr++;
		break;
	    case '5':
		fwrite(v5, 1, strlen(v5), file);
		ptr++;
		break;
	    default:
		fwrite("%", 1, 1, file);	//either % at end of file or no %1...
		break;
	    }
	    buf = ptr + 1;
	}
	fclose(file);
	free(save);
	return 0;
    }
    if (strcmp(argv[1], "~") == 0) {	//check for file presence
	if (fopen(argv[2], "r"))
	    return (0);
	if (argc < 4)
	    printf("ERROR --- File not present %s\n", argv[2]);
	else
	    printf("Error---%s\n", argv[3]);
	exit(0xc000);
    }
    if (strcmp(argv[1], "*") == 0) {	/* "[HKEY_CLASSES_ROOT\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}]  @=AFS Client Shell Extension" */
	if (argc < 3)
	    usage();
	for (i = 2; argc >= 3; i++) {
	    char *ssub = strtok(argv[i], "[");
	    BOOL option;
	    char *skey = strtok(NULL, "]");
	    char *sval, *stag;
	    if ((ssub == NULL) || (skey == NULL)) {
		printf("format error parameter %s\n", argv[i]);
		exit(0xc000);
	    }
	    option = (*ssub == '-');
	    stag = strtok(NULL, "\0");
	    if (stag)
		while (*stag == ' ')
		    stag++;
	    ssub = strtok(skey, "\\");
	    ssub = strtok(NULL, "\0");
	    sval = strtok(stag, "=");
	    sval = strtok(NULL, "\0");
	    switch (option) {
	    case 0:
		Addkey(skey, ssub, stag, sval);
		break;
	    default:
		if (stag)
		    Addkey(skey, ssub, stag, "");
		else
		    Subkey(skey, ssub);
		break;
	    }

	    argc -= 1;
	}
	return 0;
    }
    if (strcmp(argv[1], "@") == 0) {
	char msg[256], msgt[256];
	char *ptr;
	if (argc < 4)
	    usage();
	for (i = 3; argc >= 4; i++) {

	    char *ssect = strstr(argv[i], "[");
	    char *skey = strstr(ssect, "]");
	    char *sval;
	    if ((ssect == NULL) || (skey == NULL)) {
		printf("format error parameter %s\n", argv[i]);
		exit(0xc000);
	    }
	    ssect++;
	    *skey = 0;
	    if ((strlen(skey + 1) == 0) || (strlen(ssect) == 0)) {
		printf("format error parameter %s\n", argv[i]);
		exit(0xc000);
	    }
	    while (*++skey == ' ');
	    sval = strstr(skey, "=");
	    if (sval == NULL) {
		printf("format error parameter %s\n", argv[i]);
		exit(0xc000);
	    }
	    ptr = sval;
	    while (*--ptr == ' ');
	    *(ptr + 1) = 0;
	    while (*++sval == ' ');
	    if (ptr = strstr(sval, "*DatE*")) {	// ok so lets substitute date in this string;
		char tmpbuf[32];
		*(ptr) = 0;
		strcpy(msg, sval);
		_tzset();
		_strdate(tmpbuf);
		strcat(msg, tmpbuf);
		strcat(msg, ptr + 6);
		sval = msg;
	    }
	    if (ptr = strstr(sval, "*TimE*")) {
		char tmpbuf[32];
		*(ptr) = 0;
		strcpy(msgt, sval);
		_strtime(tmpbuf);
		strncat(msgt, tmpbuf, 5);
		strcat(msgt, ptr + 6);
		sval = msgt;
	    }
	    if (WritePrivateProfileString(ssect, skey, sval, argv[2]) == 0) {
		LPVOID lpMsgBuf;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			      FORMAT_MESSAGE_FROM_SYSTEM |
			      FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
			      GetLastError(), MAKELANGID(LANG_NEUTRAL,
							 SUBLANG_DEFAULT),
			      (LPTSTR) & lpMsgBuf, 0, NULL);
		printf("Error writing profile string - %s", lpMsgBuf);
		LocalFree(lpMsgBuf);
		exit(0xc000);
	    }
	    argc -= 1;
	}
	return 0;
    }
    strcpy(fname, argv[2]);
    if (strcmp(argv[1], "+") == 0) {
	file = fopen(fname, "rb");
	if (file == NULL)
	    exit(0xc000);
	len = filelength(_fileno(file));
	save = (char *)malloc(len + 2);
	ch = save;
	*ch++ = 0;		/* a small hack to allow matching /r/n if /n is first character */
	len = fread(ch, sizeof(char), len, file);
	file = freopen(fname, "wb", file);
	while (len-- > 0) {
	    if ((*ch == '\n') && (*(ch - 1) != '\r')) {	/*line feed alone */
		fputc('\r', file);
	    }
	    fputc(*ch, file);
	    ch++;
	}
	fclose(file);
	free(save);
	return 0;
    }
    if (strcmp(argv[1], "-") == 0) {
	strcpy(fname, argv[2]);
	file = fopen(fname, "rb");
	if (file == NULL)
	    exit(0xc000);
	len = filelength(_fileno(file));
	save = (char *)malloc(len + 1);
	ch = save;
	len = fread(ch, sizeof(char), len, file);
	file = freopen(fname, "wb", file);
	while (len-- > 0) {
	    if (*ch != '\r')
		fputc(*ch, file);
	    ch++;
	}
	fclose(file);
	free(save);
	return 0;
    }
    if (strstr(fname, ".et") == NULL)
	strcat(fname, ".et");
    file = fopen(fname, "rb");
    if (file == NULL)
	exit(0xc000);
    len = filelength(_fileno(file));
    save = (char *)malloc(len + 1);
    ch = save;
    len = fread(ch, sizeof(char), len, file);
    file = freopen(fname, "wb", file);
    while (len-- > 0) {
	if (*ch != '\r')
	    fputc(*ch, file);
	ch++;
    }
    fclose(file);
    pvar = (char **)malloc(argc * sizeof(char *));
    for (i = 1; i < argc - 1; i++)
	pvar[i] = argv[i + 1];
    pvar[argc - 1] = NULL;
    pvar[0] = argv[1];
    (void)_spawnvp(_P_WAIT, argv[1], pvar);
    if (save)
	free(save);
    if (pvar)
	free(pvar);
    return 0;
}
