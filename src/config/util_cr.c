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
#include "time.h"

void usuage()
{
	printf("util_cr file ;remove cr (from crlf)\n\
	OR util_cr } ProductVersion in_filename out_filename ; substitute for %1-%5 in file\n\
	   %1=Major version, %2=Minor version, %3=Patch(first digit) %4=(last two digits) %5=Version display string \n\
	   ProductVersion=maj.min.pat.pat2 ;maj=numeric, min=numeric pat,pat2 are not more than 3 digits or 1-2 digits and one alpha \n\
	   e.g 1.0.4.1, 1.0.4 a 1.0.401, 1.0.4a  all represent the same version\n\
	OR util_cr + file ;add cr\n \
	OR util_cr * \"+[register key value] x=y\" ; add register key value\n\
	OR util_cr * \"-[register key value]\" ; aremove register key value\n\
	OR util_cr @ file.ini \"[SectionKey]variable=value\" ; update ini-ipr-pwf file\n\
	OR util_cr @ file.ini \"[SectionKey]variable=value*DatE*\" ; update ini-ipr-pwf file, insert date\n\
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
 	if (strcmp(argv[1],"}")==0)
 	{
 		char v1[4],v2[4],v3[4],v4[4];
 		char v5[132];
 		char *ptr=NULL;
 		char *buf;
 		int maj;
 		int min;
 		int pat,pat2;
 		strcpy(v5,argv[2]);
 		if (argc<5)
 			usuage();
 		if ((ptr=strtok(argv[2],". \n"))==NULL)
 			return 0;
 		maj=atoi(ptr);
 		if ((ptr=strtok(NULL,". \n"))==NULL)
 			return 0;
 		min=atoi(ptr);
 		if ((ptr=strtok(NULL,". \n"))==NULL)
 			return 0;
 		pat2=-1;
 		switch (strlen(ptr))
 		{
 		case 0:
 			usuage();
 		case 1:
 			pat=atoi(ptr);
 			if (isdigit(*ptr)!=0)
 				break;
 			usuage();
 		case 2:	//ONLY 1.0.44 is interpreted as 1.0.4.4 or 1.0.4a as 1.0.4.a
 			if (isdigit(*ptr)==0)
 				usuage();
 			pat=*ptr-'0';
 			ptr++;
 			if (isalpha(*ptr)==0)
 			{
 				pat2=atoi(ptr);
 			} else if (isalpha(*ptr)!=0)
 			{
 				pat2=tolower(*ptr)-'a'+1;
 			} else 
 				usuage();
 			break;			
 		case 3://1.0.401 or 1.0.40a are the same; 
 			if ((isdigit(*ptr)==0)	// first 2 must be digit
 				|| (isdigit(*(ptr+1)==0))
 				|| (*(ptr+1)!='0' && isdigit(*(ptr+2))==0) // disallow 1.0.4b0  or 1.0.41a 
 				)
 				usuage();
 			pat=*ptr-'0';
 			ptr++;
 			pat2=atoi(ptr);
 			ptr++;
 			if (isalpha(*ptr))
 				pat2=tolower(*ptr)-'a'+1;
 			break;
 		default:
 			usuage();
 		}
 		// last can be 1-2 digits or one alpha (if pat2 hasn't been set)
 		if ((ptr=strtok(NULL,". \n"))!=NULL)
 		{
 			if (pat2>=0)
 				usuage();
 			switch (strlen(ptr))
 			{
 			case 1:
 				pat2=(isdigit(*ptr))?atoi(ptr):tolower(*ptr)-'a'+1;
 				break;
 			case 2:
 				if ( 
 					isdigit(*ptr)==0 
 					|| isdigit(*(ptr+1))==0
 					) 
 					usuage();
 				pat2=atoi(ptr);
 			default:
 				usuage();
 			}
 		}
 		file=fopen(argv[3],"r");
 		if (file==NULL)
 			usuage();
 		len=filelength(_fileno(file));
 		buf=(char *)malloc(len+1);
 		len=fread(buf,sizeof(char),len,file);
 		buf[len]=0;	//set eof
 		fclose(file);
 		file=fopen(argv[4],"w");
 		if (file==NULL)
 			usuage();
 		sprintf(v1,"%i",maj);
 		sprintf(v2,"%i",min);
 		sprintf(v3,"%i",pat);
 		sprintf(v4,"%02i",pat2);
 		while (1)
 		{
 			ptr=strstr(buf,"%");
 			fwrite(buf,1,(ptr)?ptr-buf:strlen(buf),file);	//write file if no % found or up to %
 			if (ptr==NULL)
 				break;
 			switch (*(ptr+1))	//skip first scan if buf="1...."
 			{
 			case '1':
 				fwrite(v1,1,strlen(v1),file);
 				ptr++;
 				break;				
 			case '2':
 				fwrite(v2,1,strlen(v2),file);
 				ptr++;
 				break;				
 			case '3':
 				fwrite(v3,1,strlen(v3),file);
 				ptr++;
 				break;				
 			case '4':
 				fwrite(v4,1,strlen(v4),file);
 				ptr++;
 				break;				
 			case '5':
 				fwrite(v5,1,strlen(v5),file);
 				ptr++;
 				break;
 			default:
 				fwrite("%",1,1,file);	//either % at end of file or no %1...
 				break;
 			}
 			buf=ptr+1;
 		}
 		fclose(file);
 		return 0;
 	}
	if (strcmp(argv[1],"~")==0)
	{	//check for file presence
		if (fopen(argv[2],"r"))
  			return(0);
 		if(argc<4)
 			printf("ERROR --- File not present %s\n",argv[2]);
 		else 
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
	if (strcmp(argv[1],"@")==0)
	{
		char msg[256],msgt[256];
		char *ptr;
		if (argc<4)
			usuage();
		for (i=3;argc>=4;i++)
		{

			char *ssect=strstr(argv[i],"[");
			char *skey=strstr(ssect,"]");
			char *sval;
			if ((ssect==NULL) || (skey==NULL))
			{
				printf("format error parameter %s\n",argv[i]);
				exit(0xc000);
			}
			ssect++;
			*skey=0;
			if ((strlen(skey+1)==0)||(strlen(ssect)==0))
			{
				printf("format error parameter %s\n",argv[i]);
				exit(0xc000);
			}
			while(*++skey==' ');
			sval=strstr(skey,"=");
			if (sval==NULL)
			{
				printf("format error parameter %s\n",argv[i]);
				exit(0xc000);
			}
			ptr=sval;
			while(*--ptr==' ') ;
			*(ptr+1)=0;
			while(*++sval==' ') ;
			if (ptr=strstr(sval,"*DatE*"))
			{// ok so lets substitute date in this string;
				char tmpbuf[32];
				*(ptr)=0;
				strcpy(msg,sval);
			    _tzset();
			    _strdate( tmpbuf );
				strcat(msg,tmpbuf);
				strcat(msg,ptr+6);
				sval=msg;
			}
			if (ptr=strstr(sval,"*TimE*"))
			{
				char tmpbuf[32];
				*(ptr)=0;
				strcpy(msgt,sval);
			    _strtime( tmpbuf );
				strncat(msgt,tmpbuf,5);
				strcat(msgt,ptr+6);
				sval=msgt;
			}
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
