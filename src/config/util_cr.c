/* crlf.c : Defines the entry point for the console application.*/

/* Copyright 2000, International Business Machines Corporation and others.
	All Rights Reserved.
 
	This software has been released under the terms of the IBM Public
	License.  For details, see the LICENSE file in the top-level source
	directory or online at http://www.openafs.org/dl/license10.html
*/

#include "stdio.h"
#include "io.h"
#include <assert.h>
#include "string.h"
#include "process.h"
#include "windows.h"
#include "malloc.h"

void usuage()
{
	printf("util_cr file ;remove cr (from crlf)\n\
	OR util_cr + file ;add cr\n \
	OR util_cr * \"+[register key value] x=y\" ; add register key value\n\
	OR util_cr * \"-[register key value]\" ; aremove register key value\n\
	OR util_cr & file.ini \"SectionKey=value\" ; update ini-ipr-pwf file\n\
	OR util_cr ~  ;force error\n");
	exit(0xc000);
}


void Addkey (const char *hkey,const char *subkey,const char *stag,const char *sval)
{
	DWORD disposition,result;
	HKEY kPkey,kHkey=0;
	if (strcmp(hkey,"HKEY_CLASSES_ROOT")==0) kHkey=HKEY_CLASSES_ROOT;
	if (strcmp(hkey,"HKEY_CURRENT_USER")==0) kHkey=HKEY_CURRENT_USER;
	if (strcmp(hkey,"HKEY_LOCAL_MACHINE")==0) kHkey=HKEY_LOCAL_MACHINE;
	if (kHkey==0)
		usuage();
	result=(RegCreateKeyEx(kHkey	/*HKEY_LOCAL_MACHINE*/
		,subkey
		,0,NULL
		,REG_OPTION_NON_VOLATILE
		,KEY_ALL_ACCESS,NULL
		,&kPkey
		,&disposition)==ERROR_SUCCESS);
	if(!result)
	{
		printf("AFS Error - Could Not create a registration key\n");
		exit(0xc000);
	}
	if (stag==NULL) return;
	if ((sval)&&(strlen(sval)))
	{
		if (*stag=='@')
			result=RegSetValueEx(kPkey,"",0,REG_SZ,(CONST BYTE *)sval,strlen(sval));
		else
			result=RegSetValueEx(kPkey,stag,0,REG_SZ,(CONST BYTE *)sval,strlen(sval));
	} else {

		if (*stag=='@')
			result=(RegSetValueEx(kPkey,"",0,REG_SZ,(CONST BYTE *)"",0));
		else
			result=(RegSetValueEx(kPkey,stag,0,REG_SZ,(CONST BYTE *)"",0));
	}
	if(result!=ERROR_SUCCESS)
	{
		printf("AFS Error - Could Not create a registration key\n");
		exit(0xc000);
	}
}

void Subkey(const char *hkey,const char *subkey)
{
	DWORD result;
	HKEY kHkey=0;
	if (strcmp(hkey,"HKEY_CLASSES_ROOT")==0) kHkey=HKEY_CLASSES_ROOT;
	if (strcmp(hkey,"HKEY_CURRENT_USER")==0) kHkey=HKEY_CURRENT_USER;
	if (strcmp(hkey,"HKEY_LOCAL_MACHINE")==0) kHkey=HKEY_LOCAL_MACHINE;
	if (kHkey==0)
		usuage();
	result=RegDeleteKey(
		kHkey,
		subkey
	);
	if(result!=ERROR_SUCCESS)
	{
		printf("AFS Error - Could Not create a registration key\n");
		exit(0xc000);
	}
}

int main(int argc, char* argv[])
{
	char fname[128];
	FILE *file;
	int l,i;
	char **pvar,*ch;
	long len;
	typedef char * CHARP;

	if (argc<3)
		usuage();
	if (strcmp(argv[1],"~")==0)
	{	//check for file presence
		if (fopen(argv[2],"r"))
			return(0);
		printf("Error---%s\n",argv[3]);
		exit(0xc000);
	}
	if (strcmp(argv[1],"*")==0)
	{		/* "[HKEY_CLASSES_ROOT\CLSID\{DC515C27-6CAC-11D1-BAE7-00C04FD140D2}]  @=AFS Client Shell Extension" */
		if (argc<3)
			usuage();
		for (i=2;argc>=3;i++)
		{
			char *ssub=strtok(argv[i],"[");
			BOOL option;
			char *skey=strtok(NULL,"]");
			char *sval,*stag;
			if ((ssub==NULL) || (skey==NULL))
			{
				printf("format error parameter %s\n",argv[i]);
				exit(0xc000);
			}
			option=(*ssub=='-');
			stag=strtok(NULL,"\0");
			if (stag)
			while(*stag==' ') 
				stag++;
			ssub=strtok(skey,"\\");
			ssub=strtok(NULL,"\0");
			sval=strtok(stag,"=");
			sval=strtok(NULL,"\0");
			switch (option)
			{
			case 0:
				Addkey (skey,ssub,stag,sval);
				break;
			default :
				if (stag)
					Addkey (skey,ssub,stag,"");
				else
					Subkey(skey,ssub);
				break;
			}

			argc-=1;
		}
		return 0;
	}
	if (strcmp(argv[1],"&")==0)
	{
		if (argc<4)
			usuage();
		for (i=3;argc>=4;i++)
		{
			char *ssect=strtok(argv[i],"[");
			char *skey=strtok(argv[i],"]");
			char *sval;
			skey=strtok(NULL,"=");
			if ((ssect==NULL) || (skey==NULL))
			{
				printf("format error parameter %s\n",argv[i]);
				exit(0xc000);
			}
			while(*skey==' ') 
				skey++;

			sval=strtok(NULL,"=");
			if (sval==NULL)
			{
				printf("format error parameter %s\n",argv[i]);
				exit(0xc000);
			}
//			printf("parameters %s %s %s %s\n",ssect,skey,sval,argv[2]);
			if (WritePrivateProfileString(ssect,skey,sval,argv[2])==0)
			{
				LPVOID lpMsgBuf;
				FormatMessage( 
					FORMAT_MESSAGE_ALLOCATE_BUFFER | 
					FORMAT_MESSAGE_FROM_SYSTEM | 
					FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					GetLastError(),
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR) &lpMsgBuf,
					0,
					NULL 
				);
				printf("Error writing profile string - %s",lpMsgBuf);
				LocalFree( lpMsgBuf );
				exit(0xc000);
			}
			argc-=1;
		}
		return 0;
	}
	strcpy(fname,argv[2]);
	if (strcmp(argv[1],"+")==0)
	{
		file=fopen(fname,"rb");
		if (file==NULL)
			exit(0xc000);
		len=filelength(_fileno(file));
		ch=(char *)malloc(len+2);
		*ch++=0;	/* a small hack to allow matching /r/n if /n is first character*/
		len=fread(ch,sizeof(char),len,file);
		file=freopen(fname,"wb",file);
		while (len-->0)
		{
			if ((*ch=='\n') && (*(ch-1)!='\r')) /*line feed alone*/
			{
				fputc('\r',file);
			}
			fputc(*ch,file);
			ch++;
		}
		fclose(file);
		return 0;
	}
	if (strcmp(argv[1],"-")==0)
	{
		strcpy(fname,argv[2]);
		file=fopen(fname,"rb");
		if (file==NULL)
			exit(0xc000);
		len=filelength(_fileno(file));
		ch=(char *)malloc(len+1);
		len=fread(ch,sizeof(char),len,file);
		file=freopen(fname,"wb",file);
		while (len-->0)
		{
			if (*ch!='\r')
				fputc(*ch,file);
			ch++;
		}
		fclose(file);
		return 0;
	}
	if (strstr(fname,".et")==NULL)
		strcat(fname,".et");
	file=fopen(fname,"rb");
	if (file==NULL)
		exit(0xc000);
	len=filelength(_fileno(file));
	ch=(char *)malloc(len+1);
	len=fread(ch,sizeof(char),len,file);
	file=freopen(fname,"wb",file);
	while (len-->0)
	{
		if (*ch!='\r')
			fputc(*ch,file);
		ch++;
	}
	fclose(file);
	pvar=(CHARP *)malloc(argc*sizeof(CHARP));
	for (i=1;i<argc-1;i++)
		pvar[i]=argv[i+1];
	pvar[argc-1]=NULL;
	pvar[0]=argv[1];
	l=_spawnvp(_P_WAIT,argv[1],pvar);
	if (ch)
		free(ch);
	if (pvar)
		free(pvar);
	return 0;
}
