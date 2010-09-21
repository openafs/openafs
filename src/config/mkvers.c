/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Make the AFS_component_version_number.c file. Do it in C since there's no
 * guarantee of perl on an NT platform.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define VERINFO_STRING_CHARS_MAX  950	/* max chars in verinfo string */
#define CFILE_STRING_CHARS_MAX    2000	/* max chars in C file string */

#define DEPTH_MAX  6		/* maximum depth of src dir search (>= 3) */

#define CMLDIR_DFLT   "../../src/CML"	/* default starting point for search */
#define CMLDIR_BUFSZ  ((DEPTH_MAX * (sizeof("../") - 1)) + sizeof("src/CML"))
static char cmldir[CMLDIR_BUFSZ];

#define STATE "state"
#define STAMPS "stamps"

#define CML_STRING "cml_version_number[]=\"@(#)"
#define CML_VER_DECL "char " CML_STRING
char *cml_string = CML_VER_DECL;

#define VERINFO_BUILD_STRING  "#define AFS_VERINFO_BUILD "

#define AFS_STRING "char* AFSVersion = \""
#define VERS_FILE "AFS_component_version_number"

#define MAXDELTAS 128
typedef struct {
    char name[512];
    char type;
} deltaNames;

deltaNames stateDeltas[MAXDELTAS], stampDeltas[MAXDELTAS];
FILE *fpStamps;
FILE *fpState;
FILE *fpVers;
int nStates = 0;
int nStamps = 0;

enum {
    CF_DEFAULT,
    CF_VERINFO,
    CF_TEXT,
    CF_XML
} cfgFormat = CF_DEFAULT;

char *programName;

void PrintStamps(void);

void
Usage(void)
{
    printf
	("Usage: %s [-d directory] [-o output file name] [-c component] [-v | -t | -x]\n",
	 programName);
    printf("%s creates the AFS_component_version_number.c file.\n",
	   programName);
    printf("Options:\n");
    printf("-d directory - path of the CML directory, default is %s.\n",
	   CMLDIR_DFLT);
    printf("-o file name - alternate output file name.\n");
    printf
	("-c component - if not \"afs\" prefix for cml_version_number variable.\n");
    printf("-v generate NT versioninfo style declarations.\n");
    printf("-t generate text file style information.\n");
    printf("-x generate XML revision information.\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    char stampsFile[1024];
    char stateFile[1024];
    char s[1024];
    int i;
    char *baseDir;
    int argDir = 0;
    char *outputFile = NULL;
    char outputFileBuf[sizeof(VERS_FILE) + 2];
    struct stat sbuf;
    time_t versTime;
    int reBuild = 0;
    int code;
    char *cml_prefix = NULL;

    /* initialize cmldir buffer and set default starting directory */
    for (i = 0; i < DEPTH_MAX; i++) {
	strcat(cmldir, "../");
    }
    strcat(cmldir, "src/CML");

    baseDir = strstr(cmldir, CMLDIR_DFLT);

    programName = argv[0];

    for (i = 1; i < argc; i++) {
	if (!strcmp("-d", argv[i])) {
	    i++;
	    if (i >= argc || *argv[i] == '-') {
		printf("Missing directory name for -d option.\n");
		Usage();
	    }
	    baseDir = argv[i];
	    argDir = 1;
	} else if (!strcmp("-o", argv[i])) {
	    i++;
	    if (i >= argc || *argv[i] == '-') {
		printf("Missing output file name for -o option.\n");
		Usage();
	    }
	    outputFile = argv[i];
	} else if (!strcmp("-c", argv[i])) {
	    i++;
	    if (i >= argc || *argv[i] == '-') {
		printf("Missing component for -c option.\n");
		Usage();
	    }
	    cml_prefix = argv[i];
	} else if (!strcmp("-v", argv[i])) {
	    if (cfgFormat == CF_DEFAULT || cfgFormat == CF_VERINFO) {
		cfgFormat = CF_VERINFO;
	    } else {
		printf("Specify only one alternative output format\n");
		Usage();
	    }
	} else if (!strcmp("-t", argv[i])) {
	    if (cfgFormat == CF_DEFAULT || cfgFormat == CF_TEXT) {
		cfgFormat = CF_TEXT;
	    } else {
		printf("Specify only one alternative output format\n");
		Usage();
	    }
	} else if (!strcmp("-x", argv[i])) {
	    if (cfgFormat == CF_DEFAULT || cfgFormat == CF_XML) {
		cfgFormat = CF_XML;
	    } else {
		printf("Specify only one alternative output format\n");
		Usage();
	    }
	} else {
	    printf("%s: Unknown argument.\n", argv[i]);
	    Usage();
	}
    }

    /* set outputFile if not specified */

    if (outputFile == NULL) {
	strcpy(outputFileBuf, VERS_FILE);
	if (cfgFormat == CF_VERINFO) {
	    strcat(outputFileBuf, ".h");
	} else if (cfgFormat == CF_TEXT) {
	    strcat(outputFileBuf, ".txt");
	} else if (cfgFormat == CF_XML) {
	    strcat(outputFileBuf, ".xml");
	} else {
	    strcat(outputFileBuf, ".c");
	}
	outputFile = outputFileBuf;
    }

    /* Determine if we need to create the output file. */

    if ((code = stat(outputFile, &sbuf)) < 0) {
	reBuild = 1;
	versTime = (time_t) 0;	/* indicates no output file. */
    } else {
	versTime = sbuf.st_mtime;
    }

    sprintf(stampsFile, "%s/%s", baseDir, STAMPS);
    code = stat(stampsFile, &sbuf);

    while (code < 0 && errno == ENOENT && !argDir && baseDir > cmldir) {
	/* Try path at next level of depth. */
	baseDir -= sizeof("../") - 1;
	sprintf(stampsFile, "%s/%s", baseDir, STAMPS);
	code = stat(stampsFile, &sbuf);
    }
    if (code == 0 && versTime <= sbuf.st_mtime) {
	reBuild = 1;
    }

    sprintf(stateFile, "%s/%s", baseDir, STATE);

    if (!reBuild) {
	code = stat(stateFile, &sbuf);
	/* dont' check alternate base dir, since it would be reset above */
	if (code == 0 && versTime <= sbuf.st_mtime)
	    reBuild = 1;
    }

    if (!reBuild) {
	printf("Not rebuilding %s since it is up to date.\n", outputFile);
	exit(0);
    }

    if (cml_prefix) {
	cml_string =
	    (char *)malloc(strlen("char ") + strlen(cml_prefix) + strlen(CML_STRING) +
		   1);
	if (!cml_string) {
	    printf("No space to use prefix in cml string, ignoring it.\n");
	    cml_string = CML_VER_DECL;
	} else {
	    (void)sprintf(cml_string, "%s%s%s", "char ", cml_prefix,
			  CML_STRING);
	}
    }

    fpState = fopen(stateFile, "r");
    fpStamps = fopen(stampsFile, "r");
    fpVers = fopen(outputFile, "w");

    if (fpStamps == NULL || fpState == NULL || fpVers == NULL) {
	if (fpVers) {
	    if (cfgFormat == CF_VERINFO) {
		fprintf(fpVers,
			"%s \"No configuration information available\"\n",
			VERINFO_BUILD_STRING);
	    } else if (cfgFormat == CF_TEXT) {
		fprintf(fpVers, "No configuration information available.\n");
	    } else {
		fprintf(fpVers,
			"%sNo configuration information available\";\n",
			cml_string);
		fprintf(fpVers, "%safs??\";\n", AFS_STRING);
	    }
	    fclose(fpVers);
	} else {
	    fprintf(stderr, "Can't write version information to %s.\n",
		    outputFile);
	}
	fprintf(stderr,
		"No configuration information available, continuing...\n");
	exit(1);
    }


    nStates = 0;
    while (fgets(s, sizeof(s), fpState)) {
	if (*s == 'I' || *s == 'N' || *s == 'C' || *s == 'O') {
	    stateDeltas[nStates].type = *s;
	    s[strlen(s) - 1] = '\0';
	    (void)strcpy(stateDeltas[nStates].name, s + 2);
	    nStates++;
	}

    }
    fclose(fpState);

    PrintStamps();
    fclose(fpVers);

    return 0;
}


void
PrintStamps(void)
{
    char *s;
    char *c = NULL;
    int i;
    size_t outMax, outCount = 0;
    time_t t = time(NULL);

    if (cfgFormat == CF_VERINFO) {
	outMax = VERINFO_STRING_CHARS_MAX;
    } else if (cfgFormat == CF_TEXT) {
	outMax = 0;		/* signifies that there is no maximum */
    } else {
	outMax = CFILE_STRING_CHARS_MAX;
    }

    for (i = 0; i < nStates; i++) {
	if (stateDeltas[i].type == 'C') {
	    if (cfgFormat == CF_VERINFO) {
		fprintf(fpVers, "%s \"Base configuration %s",
			VERINFO_BUILD_STRING, stateDeltas[i].name);
	    } else if (cfgFormat == CF_TEXT) {
		fprintf(fpVers, "Base configuration %s\n",
			stateDeltas[i].name);
	    } else if (cfgFormat == CF_XML) {
                fprintf(fpVers,
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<revision>\n"
                        "<revnumber>\n"
                        "Base configuration %s\n"
                        "</revnumber>\n"
                        "<date>\n"
                        "%s"
                        "</date>\n"
                        "</revision>\n",
			stateDeltas[i].name, ctime(&t));
	    } else {
		fprintf(fpVers, "%sBase configuration %s", cml_string,
			stateDeltas[i].name);
	    }
	    c = stateDeltas[i].name;
	    break;
	}
    }

    for (i = 0; i < nStates; i++) {
	if (stateDeltas[i].type == 'I' || stateDeltas[i].type == 'N') {
	    outCount += strlen(stateDeltas[i].name) + 2;
	    if (outMax && outCount > outMax) {
		break;
	    }
	    if (cfgFormat == CF_TEXT) {
		fprintf(fpVers, "%c%s\n", stateDeltas[i].type,
			stateDeltas[i].name);
	    } else if (cfgFormat == CF_XML) {
		fprintf(fpVers,
                        "<revremark>\n"
                        ";%c%s"
                         "</revremark>\n",
                        stateDeltas[i].type,
			stateDeltas[i].name);
	    } else {
		fprintf(fpVers, ";%c%s", stateDeltas[i].type,
			stateDeltas[i].name);
	    }
	}
    }

    for (i = 0; i < nStates; i++) {
	if (stateDeltas[i].type == 'O') {
	    outCount += strlen(stateDeltas[i].name) + 2;
	    if (outMax && outCount > outMax) {
		break;
	    }
	    if (cfgFormat == CF_TEXT) {
		fprintf(fpVers, "%c%s\n", stateDeltas[i].type,
			stateDeltas[i].name);
	    } else if (cfgFormat == CF_XML) {
		fprintf(fpVers,
                        "<revremark>\n"
                        ";%c%s"
                         "</revremark>\n",
                        stateDeltas[i].type,
			stateDeltas[i].name);
	    } else {
		fprintf(fpVers, ";%c%s", stateDeltas[i].type,
			stateDeltas[i].name);
	    }
	}
    }

    if (outMax && outCount > outMax) {
	fprintf(fpVers, ";[LIST TRUNCATED]");
    }

    if (cfgFormat == CF_VERINFO) {
	fprintf(fpVers, "\"\n");
    } else if (cfgFormat == CF_DEFAULT) {
	fprintf(fpVers, "\";\n");

	if (c)
	    c += 3;
	s = (char *)strchr(c, ' ');
	if (s)
	    *s = '\0';
	fprintf(fpVers, "%s%s\";\n", AFS_STRING, c ? c : "Unknown");
    } else if (cfgFormat == CF_XML) {
        fprintf(fpVers, "</revision>\n");
    }
}
