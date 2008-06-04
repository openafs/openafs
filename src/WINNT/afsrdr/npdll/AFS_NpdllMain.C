
#include <windows.h>
#include <process.h>

// NOTE:
//
// Function:	DllMain
//
// Return:	TRUE  => Success
//		    FALSE => Failure

BOOL 
WINAPI 
DllMain( HINSTANCE hDLLInst, 
         DWORD fdwReason, 
         LPVOID lpvReserved)
{
    BOOL	bStatus = TRUE;
    WORD	wVersionRequested;

    switch( fdwReason)    
    {

        case DLL_PROCESS_ATTACH:
            break;

        case DLL_PROCESS_DETACH:
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
    }

    return( bStatus);
}
