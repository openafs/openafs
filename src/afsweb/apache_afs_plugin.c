/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* Apache plugin for AFS authentication - should be archived to libapacheafs.a
 * contains calls to functions in apache_afs_client.o - and is the intermediary
 * between the module that plugs into apache's source code and the
 * apache_afs_client. Shares global variables with the module and the client.
 */

/*
 */

#include "apache_api.h"

#define afslog(level,str) if (level <= afsDebugLevel) (afsLogError str)
#define afsassert(str) if(!(str)) { fprintf(stderr, "afs module: assertion failed:%s\t%d\n",__FILE__,__LINE__) ; return SERVER_ERROR; }

#define AFS_AUTHTYPE                        "AFS"
#define AFS_DFS_AUTHTYPE                    "AFS-DFS"
#define ERRSTRLEN                           1024


#include <sys/types.h>
#include <sys/stat.h>


/* Global vars */

u_long afsDebugLevel;
const char module_name[] = "AFS Authentication Module";

typedef struct {
    char defaultCell[64];
    u_long cacheExpiration;
} apache_afs_glob;

static apache_afs_glob *afs_str;

/* Global file descriptors for pipes */
int readPipe, writePipe;

#ifdef AIX
/* Global temp lock file descriptor */
int tempfd;

/*
 * Create a temporary file and unlink it using the file descriptor for locking
 * as a means of synchronization for providing exclusive access to the pipe
 * for communicating with the weblog process
 */
static int
create_temp_file()
{
    char tmpFileName[L_tmpnam];
    int lockfd = 0;

    tmpnam(tmpFileName);
    unlink(tmpFileName);

    lockfd = open(tmpFileName, O_RDWR | O_CREAT);
    if (lockfd < 0) {
	perror("afs_plugin:Error creating temp file:");
	return lockfd;
    }
    unlink(tmpFileName);
    return lockfd;
}
#endif

/*
 * Initialization: start up the weblog process. Open the pipes and pass their
 * file descriptors to the weblog process
 */
void
afs_plugin_init(int tokenExpiration, char *weblogPath, char *error_fname,
		char *pf, char *cell, char *dir, int exp, char *loc,
		int shutdown)
{
    int childpid;
    int pipe1[2], pipe2[2];
    char weblogarg1[32];
    char weblogarg2[32];
    char weblogarg3[32];
    char weblogarg4[32];

    FILE *fp;			/* for pid_fname */
    char *afs_weblog_pidfile;
    char *httpd_pid_fname = (char *)malloc(strlen(pf) + 1);
    if (httpd_pid_fname == NULL) {
	fprintf(stderr,
		"%s: malloc failed - out of memory while allocating space for httpd_pid_fname\n",
		module_name);
	exit(-1);
    }
    strcpy(httpd_pid_fname, pf);
    afs_weblog_pidfile = (char *)malloc(strlen(httpd_pid_fname) + 5);
    if (httpd_pid_fname == NULL) {
	fprintf(stderr,
		"%s: malloc failed - out of memory while allocating space for afs_weblog_pidfile\n",
		module_name);
	exit(-1);
    }
    sprintf(afs_weblog_pidfile, "%s.afs", httpd_pid_fname);

    if (do_setpag()) {
	fprintf(stderr, "%s:Failed to set pag Error:%d\n", module_name,
		errno);
	exit(-1);
    }

    afs_str = (apache_afs_glob *) malloc(sizeof(apache_afs_glob));
    if (afs_str == NULL) {
	fprintf(stderr, "%s:malloc failed for afs_str\n", module_name);
	exit(-1);
    }

    if (cell)
	strcpy(afs_str->defaultCell, cell);
    else {
	fprintf(stderr, "%s: NULL argument in cell\n", module_name);
	exit(-1);
    }
    afs_str->cacheExpiration = exp;

    afslog(5,
	   ("Default Cell:%s\nCache Expiration:%d\nDebugLevel:%d",
	    afs_str->defaultCell, afs_str->cacheExpiration, afsDebugLevel));

#ifdef AIX
    /* Get a temp file fd for locking */
    tempfd = create_temp_file();
    if (tempfd < 0) {
	fprintf(stderr, "%s: Error creating temp file", module_name);
	exit(-1);
    }
#endif

    if (pipe(pipe1) < 0 || pipe(pipe2) < 0) {
	fprintf(stderr, "%s: Error creating pipes - %s", module_name,
		strerror(errno));
	exit(-1);
    }
    if ((childpid = fork()) < 0) {
	fprintf(stderr, "%s: Error forking - %s", module_name,
		strerror(errno));
	exit(-1);
    } else if (childpid > 0) {	/* parent */
	close(pipe1[0]);
	close(pipe2[1]);
	readPipe = pipe2[0];
	writePipe = pipe1[1];
    } else {			/* child */
	close(pipe1[1]);
	close(pipe2[0]);
	fp = fopen(afs_weblog_pidfile, "w");
	if (fp == NULL) {
	    perror("fopen");
	    fprintf(stderr, "%s: Error opening pidfile:%s - %s\n",
		    module_name, afs_weblog_pidfile, strerror(errno));
	    close(pipe1[0]);
	    close(pipe2[1]);
	    exit(-1);
	}
	fprintf(fp, "%ld\n", (long)getpid());
	fclose(fp);
	free(afs_weblog_pidfile);
	sprintf(weblogarg1, "%d", pipe1[0]);
	sprintf(weblogarg2, "%d", pipe2[1]);
	sprintf(weblogarg3, "%d", afs_str->cacheExpiration);
	sprintf(weblogarg4, "%d", tokenExpiration);
	sleep(5);
	execlp(weblogPath, "weblog_starter", weblogPath, error_fname,
	       weblogarg1, weblogarg2, weblogarg3, weblogarg4, NULL);
	fprintf(stderr, "%s: Error executing %s - %s\n", module_name,
		weblogPath, strerror(errno));
	perror("execlp");
	close(pipe1[0]);
	close(pipe2[1]);
	fclose(fp);

	/* exit by sending a SIGTERM to the httpd process (how to get the pid?)
	 * since at this point the pid file is outdated and only if we sleep for
	 * a while to allow httpd_main to put it's pid in the pidfile can we
	 * attempt to send it a SIGTERM for graceful shuttdown)
	 * so that everything is brought down - if you want to bring everything
	 * down!! Else continue with httpd without AFS authentication.
	 */
/*#ifdef SHUTDOWN_IF_AFS_FAILS in afs_module.c */
	if (shutdown) {
#define KILL_TIME_WAIT    1
#define MAX_KILL_ATTEMPTS 3
	    int attempts = 0;
	    fp = fopen(httpd_pid_fname, "r");
	    fscanf(fp, "%d", &childpid);
	    fclose(fp);
	  killagain:
	    sleep(KILL_TIME_WAIT);
	    if (kill(childpid, SIGTERM) == -1) {
		if ((errno == ESRCH) && (attempts < MAX_KILL_ATTEMPTS)) {
		    attempts++;
		    fprintf(stderr,
			    "%s:SIGTERM to process:%d FAILED attempt:%d\nRetrying "
			    " for %d more attempts every %d seconds\n",
			    module_name, childpid, attempts,
			    (MAX_KILL_ATTEMPTS - attempts), KILL_TIME_WAIT);
		    goto killagain;
		}
	    } else {
		fprintf(stderr, "%s:Shutdown complete ...\n", module_name);
	    }
	    if (attempts >= MAX_KILL_ATTEMPTS) {
		fprintf(stderr,
			"%s:httpd is still running-AFS authentication will fail "
			"because weblog startup failed\n", module_name);
	    }
	    exit(0);
	} else {
	    fprintf(stderr,
		    "%s:httpd running - AFS Authentication will not work! "
		    "Weblog startup failure", module_name);
	    exit(-1);
	}
    }
}

/*
 * Returns HTTP error codes based on the result of a stat error
 */
static int
sort_stat_error(request_rec * r)
{
    int status = 0;
    switch (errno) {
    case ENOENT:
	status = HTTP_NOT_FOUND;
	break;

    case EACCES:
	status = FORBIDDEN;
	break;

    case ENOLINK:
	status = HTTP_NOT_FOUND;
	break;

    case ENODEV:
	status = HTTP_NOT_FOUND;
	break;

    default:
	{
	    char error[ERRSTRLEN];
	    sprintf(error, "%s: stat error: %s", module_name,
		    strerror(errno));
	    status = SERVER_ERROR;
	    LOG_REASON(error, r->uri, r);
	    break;
	}
    }
    return status;
}

/*
 * recursively stats the path to see where we're going wrong and
 * chops off the path_info part of it -
 * Returns OK or an HTTP status code
 * Called if we get a ENOTDIR from the first stab at statting the
 * entire path - so we assume that we have some PATH_INFO and try to
 * chop it off the end and return the path itself
 * Side effects on request_rec
 - sets the filename field
 - sets the path_info field
 */
static int
remove_path_info(request_rec * r, char *path, struct stat *buf)
{
    char *cp;
    char *end;
    char *last_cp = NULL;
    int rc = 0;

    afsassert(r);
    afsassert(path);
    afsassert(buf);

    end = &path[strlen(path)];

    /* Advance over trailing slashes ... NOT part of filename */
    for (cp = end; cp > path && cp[-1] == '/'; --cp)
	continue;

    while (cp > path) {
	/* See if the pathname ending here exists... */
	*cp = '\0';
	errno = 0;
	rc = stat(path, buf);
	if (cp != end)
	    *cp = '/';

	if (!rc) {
	    if (S_ISDIR(buf->st_mode) && last_cp) {
		buf->st_mode = 0;	/* No such file... */
		cp = last_cp;
	    }
	    r->path_info = pstrdup(r->pool, cp);
	    *cp = '\0';
	    return OK;
	}

	else if (errno == ENOENT || errno == ENOTDIR) {
	    last_cp = cp;
	    while (--cp > path && *cp != '/')
		continue;
	    while (cp > path && cp[-1] == '/')
		--cp;
	} else if (errno != EACCES) {
	    /*
	     * this would be really bad since we checked the entire path
	     * earlier and got ENOTDIR instead of EACCES - so why are we getting
	     * it now? Anyway, we ought to return FORBIDDEN
	     */
	    return HTTP_FORBIDDEN;
	}
    }
    r->filename = pstrdup(r->pool, path);
    return OK;
}

/*
 * Checks to see if actual access to the URL is permitted or not
 * stats the URI first, if failure returns FORBIDDEN, if allowed then
 * checks to see if it is a file, dir or LINK (TEST), and accordingly does more
 */
static int
can_access(request_rec * r)
{
    int rc;
    char *doc_root = (char *)DOCUMENT_ROOT(r);
    struct stat buf;
    char path[MAX_STRING_LEN];

    afsassert(r->uri);
    afsassert(doc_root);

    if (r->filename) {
	afslog(10, ("%s: Found r->filename:%s", module_name, r->filename));
	sprintf(path, "%s", r->filename);
    } else {
	afslog(10,
	       ("%s: Composing path from doc_root:%s and r->uri:%s",
		module_name, doc_root, r->uri));
	sprintf(path, "%s/%s", doc_root, r->uri);
	afslog(10, ("%s: Path:%s", module_name, path));
    }
    rc = stat(path, &buf);
    if (rc == -1) {
	afslog(2,
	       ("%s: pid:%d\tpath:%s\tstat error:%s", module_name, getpid(),
		path, strerror(errno)));

	/*
	 * if the requested method is an HTTP PUT and the file does not
	 * exist then well, we'll get a stat error but we shouldn't return
	 * an error - we should try and open the file in append mode and then
	 * see if we get a permission denied error
	 */
	if ((strncmp(r->method, "PUT", 3) == 0) && (errno == ENOENT)) {
	    FILE *f = fopen(path, "a");
	    if (f == NULL) {
		if (errno == EACCES) {
		    afslog(2,
			   ("%s: Either AFS acls or other permissions forbid write"
			    " access to file %s for user %s", module_name,
			    path,
			    r->connection->user ? r->connection->
			    user : "UNKNOWN"));
		    return FORBIDDEN;
		} else {
		    log_reason
			("afs_module: Error checking file for PUT method",
			 r->uri, r);
		    return SERVER_ERROR;
		}
	    }
	} else if (errno == ENOTDIR) {
	    /*
	     * maybe the special case of CGI PATH_INFO to be translated to
	     * PATH_TRANSLATED - check each component of this path
	     * and stat it to see what portion of the url is actually
	     * the path and discard the rest for our purposes.
	     */
	    rc = remove_path_info(r, path, &buf);
	    afslog(10,
		   ("%s:remove_path_info returned %d path:%s", module_name,
		    rc, path));
	    if (rc)
		return rc;
	} else {
	    return sort_stat_error(r);
	}
    }
    /*
     * If we get here then we have something - either a file or a directory
     */
    else {
	if (S_IFREG == (buf.st_mode & S_IFMT)) {
	    /* regular file */
	    FILE *f;
	    char permissions[] = { 'r', '\0', '\0', '\0' };	/* room for +, etc... */

	    if ((strncmp(r->method, "PUT", 3) == 0)) {
		strcpy(permissions, "a");
	    }
	    if (!(f = fopen(path, permissions))) {
		if (errno == EACCES) {
		    afslog(2,
			   ("%s: Either AFS acls or other permissions"
			    " forbid access to file %s for user %s",
			    module_name, path,
			    r->connection->user ? r->connection->
			    user : "UNKNOWN"));
		    return FORBIDDEN;
		} else {
		    char error[ERRSTRLEN];
		    sprintf(error,
			    "%s: Error checking file %s for permissions:%s",
			    module_name, path, strerror(errno));
		    log_reason(error, r->uri, r);
		    return SERVER_ERROR;
		}
	    }
	    fclose(f);
	    return OK;
	}
	if (S_IFDIR == (buf.st_mode & S_IFMT)) {
	    /* path is a directory */

	    if (r->uri[strlen(r->uri) - 1] != '/') {
		/* if we don't have a trailing slash, return REDIRECT */
		char *ifile;
		if (r->args != NULL) {
		    ifile =
			PSTRCAT(r->pool, escape_uri(r->pool, r->uri), "/",
				"?", r->args, NULL);
		} else {
		    ifile =
			PSTRCAT(r->pool, escape_uri(r->pool, r->uri), "/",
				NULL);
		}
		TABLE_SET(r->headers_out, "Location", ifile);
		return REDIRECT;
	    } else {
		DIR *d;
		if (!(d = opendir(path))) {
		    if (errno == EACCES) {
			afslog(2,
			       ("%s: Error accessing dir %s - %s",
				module_name, path, strerror(errno)));
			return FORBIDDEN;
		    } else {
			char error[ERRSTRLEN];
			sprintf(error, "%s: opendir failed with Error:%s",
				module_name, strerror(errno));
			log_reason(error, r, r->uri);
			return SERVER_ERROR;
		    }
		}
		closedir(d);
		return OK;
	    }
	}
    }
}


/*
 * Logs requests which led to a FORBIDDEN return code provided a token
 * existed. Added feature (SetAFSAccessLog) in beta2
 */
static int
log_Access_Error(request_rec * r)
{
    if (FIND_LINKED_MODULE("afs_module.c") != NULL) {
	char err_msg[1024];
	int rc = 0;
	int len = 0;
	extern int logfd;

	if (r->connection->user)
	    sprintf(err_msg,
		    "[%s] AFS ACL's deny permission to "
		    "user: %s for URI:%s\n", GET_TIME(), r->connection->user,
		    r->uri);
	else
	    sprintf(err_msg,
		    "[%s] AFS ACL's deny permission to user - for URI:%s\n",
		    GET_TIME(), r->uri);

	len = strlen(err_msg);
	rc = write(logfd, err_msg, len);

	if (rc != len) {
	    afslog(0,
		   ("%s: Error logging message:%s - %s", module_name, err_msg,
		    strerror(errno)));
	    return -1;
	}
	return rc;
    }
}

/*
 * The interface - hook to obtain an AFS token if needed based on the
 * result of a stat (returned by can_access) or an open file
 */
int
afs_auth_internal(request_rec * r, char *cell)
{
    afsassert(r);
    afsassert(r->uri);
    afsassert(cell);
    if (FIND_LINKED_MODULE("afs_module.c") != NULL) {
	int rc, status;
	char *type;
	static int haveToken = 1;	/* assume that we always have a token */
	extern int logAccessErrors;

	/*
	 * Get afs_authtype directive value for that directory or location
	 */
	type = (char *)get_afsauthtype(r);

	/*
	 * UserDir (tilde) support
	 */
#ifndef APACHE_1_3
	if (FIND_LINKED_MODULE("mod_userdir.c") != NULL) {
	    rc = translate_userdir(r);
	    if ((rc != OK) && (rc != DECLINED)) {
		LOG_REASON("afs_module: Failure while translating userdir",
			   r->uri, r);
		return rc;
	    }
	}
#endif

	afslog(20, ("%s: pid:%d r->uri:%s", module_name, getpid(), r->uri));
	if (type)
	    afslog(20, ("%s: AFSAuthType: %s", module_name, type));
	else
	    afslog(20, ("%s: AFSAuthType NULL", module_name));

	/* if AuthType is not AFS, then unlog any existing tokens and DECLINE */
	if (type == NULL) {
	    if (haveToken)
		unlog();
	    return DECLINED;
	}

	if ((strcasecmp(type, AFS_AUTHTYPE))
	    && (strcasecmp(type, AFS_DFS_AUTHTYPE))) {
	    if (haveToken)
		unlog();
	    afslog(10,
		   ("%s: Error unknown AFSAuthType:%s returning DECLINED",
		    module_name, type));
	    return DECLINED;
	}

	if (cell)
	    status =
		authenticateUser(r, cell, afs_str->cacheExpiration, type);
	else
	    status =
		authenticateUser(r, afs_str->defaultCell,
				 afs_str->cacheExpiration, type);

	if (status != OK) {
	    afslog(10, ("%s: Returning status %d", module_name, status));
	    return status;
	}

	/* can we access this URL? */
	rc = can_access(r);

	if (rc == OK) {
	    return DECLINED;
	}

	if (rc == REDIRECT) {
	    return REDIRECT;
	}

	if (rc == FORBIDDEN) {
	    rc = forbToAuthReqd(r);
	    if (rc == FORBIDDEN) {
		if (logAccessErrors) {
		    log_Access_Error(r);
		}
	    }
	    return rc;
	}
	return DECLINED;
    }
}
