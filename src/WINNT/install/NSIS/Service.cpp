/*
      Service Installer for NSIS script
      
      Rob Murawski
      
      Released under terms of IBM Open Source agreement for OpenAFS
      
      */

#include <stdio.h>
#include <windows.h>
#include <tchar.h>

int main(int argc, char *argv[])
{
   if(argc<3)
   {
      printf("Insufficient arguments: Service ServiceName ServicePath DisplayName.\n");
      return 1;
   }
   
	SC_HANDLE hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	SC_HANDLE hService;

	if(hSCM == NULL)
	{
		printf("Could not open Service Control Manager. Aborting.\n");
		return 1;
	}


   if(*argv[1]!='u' && *argv[1]!='U')
   {
		hService = CreateService(hSCM, argv[1],
		_T(argv[3]),
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_IGNORE,
		argv[2],
		NULL,NULL,NULL, NULL, NULL );

		if (hService == NULL) 
		{
		    printf("Create Service failed (%d)\n", GetLastError() );
		    CloseServiceHandle(hSCM);
		}
   }
   else
   {
       hService = OpenService( hSCM, argv[2], DELETE);
       if(hService!=NULL)
	   DeleteService( hService );
   }
   
	CloseServiceHandle(hService);


	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return 0;
}
