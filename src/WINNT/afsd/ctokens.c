/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afs/param.h>
#include <afs/stds.h>

#include <windows.h>
#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <afs/auth.h>

main(argc, argv)
        int argc;
        char **argv;
{
	int cellNum;
	int rc;
        time_t current_time;
        time_t tokenExpireTime;
	char *expireString;
	char userName[100];
	struct ktc_principal serviceName, clientName;
	struct ktc_token token;
	WSADATA WSAjunk;

	WSAStartup(0x0101, &WSAjunk);
        if (argc > 1) {
	   printf("%s [-help]\n", argv[0]);
	   return 0;
	}

	printf("\nTokens held by the Cache Manager:\n\n");
	cellNum = 0;
	current_time = time((void *) 0);

	while (1) {
		rc = ktc_ListTokens(cellNum, &cellNum, &serviceName);
		if (rc == KTC_NOENT) {
			/* end of list */
			printf("   --End of list --\n");
			break;
		}
		else if (rc == KTC_NOCM) {
			printf("AFS device may not have started\n");
			break;
		}
		else if (rc) {
			printf("Unexpected error, code %d\n", rc);
			exit(1);
		}
		else {
			rc = ktc_GetToken(&serviceName, &token, sizeof(token),
					  &clientName);
			if (rc) {
				printf("Unexpected error, service %s.%s.%s, code %d\n",
					serviceName.name, serviceName.instance,
					serviceName.cell, rc);
				continue;
			}
			tokenExpireTime = token.endTime;
			strcpy(userName, clientName.name);
			if (clientName.instance[0] != 0) {
				strcat(userName, ".");
				strcat(userName, clientName.instance);
			}
			if (userName[0] == '\0')
				printf("Tokens");
			else if (strncmp(userName, "AFS ID", 6) == 0)
				printf("User's (%s) tokens", userName);
			else if (strncmp(userName, "Unix UID", 8) == 0)
				printf("Tokens");
			else
				printf("User %s's tokens", userName);
			printf(" for %s%s%s@%s ",
				serviceName.name,
				serviceName.instance[0] ? "." : "",
				serviceName.instance,
				serviceName.cell);
			if (tokenExpireTime <= current_time)
				printf("[>> Expired <<]\n");
			else {
                                time_t t = tokenExpireTime;
				expireString = ctime(&t);
				expireString += 4;	 /* Skip day of week */
				expireString[12] = '\0'; /* Omit secs & year */
				printf("[Expires %s]\n", expireString);
			}
		}
	}
	return(0);
}
