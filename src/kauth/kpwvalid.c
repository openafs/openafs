#include <afs/param.h>
#include <stdio.h>
#ifdef AFS_NT40_ENV
#include <WINNT/afsevent.h>
#endif
#include <afs/afsutil.h>

#include "AFS_component_version_number.c"
  
  /* returns 0 if the password is long enough, otherwise non-zero  */
main(int argc, char *argv[] )
{
  char oldpassword[512];
  char password[512];
  int rc;

  if (fgets(oldpassword, 512, stdin)) 
    while (fgets(password, 512, stdin)) {
       if (strlen(password) > 8) { /* password includes a newline */ 
	 rc = 0;
	 fputs("0\n",stdout);
	 fflush(stdout);
       }
       else {
	 rc = 1;
	 fputs("Passwords must contain at least 8 characters.\n",stderr);
	 fputs("1\n",stdout);
	 fflush(stdout);
       }
    }
}
