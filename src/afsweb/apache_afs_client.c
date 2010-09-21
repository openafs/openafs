/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Implements most of the client side web authentication protocol
 */

/*
 */

#include "apache_afs_utils.h"
#include "apache_afs_cache.h"

#include "AFS_component_version_number.c"
#include "apache_api.h"

#define afsassert(str) if(!(str)) { fprintf(stderr, "afs module: assertion failed:%s\t%d\n",__FILE__,__LINE__) ; return SERVER_ERROR; }

#define MAXBUFF 1024

#define APACHEAFS_MAX_PATH	     1024	/* Maximum path length */
#define APACHEAFS_USERNAME_MAX	     64	/* Maximum username length */
#define APACHEAFS_PASSWORD_MAX	     64	/* Maximum password length */
#define APACHEAFS_CELLNAME_MAX	     64	/* Maximum cellname length */
#define APACHEAFS_MOUNTPTLEN_MAX     64	/* Max mountpoint length */

#ifndef MAX
#define MAX(A,B)		((A)>(B)?(A):(B))
#endif /* !MAX */
#ifndef MIN
#define MIN(A,B)		((A)<(B)?(A):(B))
#endif /* !MAX */

/* */
static int setCellAuthHeader(request_rec * r);

/* Module name for logging stuff */
extern const char module_name[];

/* file descriptors for the pipes for communication with weblog */
extern int readPipe;
extern int writePipe;

#ifdef AIX
/* lock file descriptor */
extern int tempfd;
#endif

/* do we have an authentication field */
static int haveAuth = 0;

/* local cache stuff */
static int cache_initialized;

/* we need the defult cell in several places */
static char global_default_cell[APACHEAFS_CELLNAME_MAX];

/* buffers to keep track of the last authenticated user */
static char lastUser[APACHEAFS_USERNAME_MAX];
static char lastCell[APACHEAFS_CELLNAME_MAX];
static char lastCksum[SHA_HASH_BYTES];

/* do I have my own PAG */
static int doneSETPAG = 0;

/*
 * Parse the authorization header for username and password
 */
static int
parse_authhdr(request_rec * r, char *user, char *passwd, char *cell,
	      char *defaultCell)
{
    int i;
    char *p, *t;
    const char *auth_line = TABLE_GET(r->headers_in, "Authorization");

    if ((r == NULL) || (auth_line == NULL) || (user == NULL)
	|| (passwd == NULL) || (cell == NULL)) {
	LOG_REASON("AFSAUTH_CLIENT: NULL request record, auth_line, cell,"
		   "user or passwd while parsing authentication header",
		   r->uri, r);
	return SERVER_ERROR;
    }

    user[0] = '\0';
    passwd[0] = '\0';
    cell[0] = '\0';

    /*
     * check for basic authentication
     */
    if (strncasecmp
	(GETWORD(r->pool, (const char **)&auth_line, ' '), "basic", 6) != 0) {
	/* Client tried to authenticate using some other auth scheme */
	LOG_REASON
	    ("AFSAUTH_CLIENT:client used other than Basic authentication"
	     "scheme", r->uri, r);
	return SERVER_ERROR;
    }

    /*
     * Username and password are base64 encoded
     */
    t = UUDECODE(r->pool, auth_line);

    if (t == NULL) {
	LOG_REASON("AFSAUTH_CLIENT:uudecode failed", r->uri, r);
	return SERVER_ERROR;
    }

    /*
     * Format is user@cell:passwd. The user, cell or passwd may be missing
     */
    r->connection->user = GETWORD_NULLS(r->pool, (const char **)&t, ':');
    r->connection->auth_type = "Basic";
    strcpy(passwd, t);

    p = r->connection->user;

    for (i = 0; *p != '@' && *p != '\0'; p++, i++) {
	user[i] = *p;
    }
    user[i] = '\0';
    if (*p == '@') {
	for (i = 0, p++; *p != '\0'; p++, i++) {
	    cell[i] = *p;
	}
	cell[i] = '\0';
    }

    if (cell[0] == '\0') {
	strcpy(cell, defaultCell);
    }
    return OK;
}

/*
 * send a buffer to the weblog process over the pipe. Used for sending
 * authentication credentials to weblog
 */
static int
sendBuffer(char *buf, int len)
{
    afsassert(buf);
    if (write(writePipe, buf, len) != len) {
	afslog(5,
	       ("%s: Error writing to pipe - %s", module_name,
		strerror(errno)));
	return -1;
    }
    return 0;
}

/*
 * packs user credentials into a buffer seperated by newlines and
 * sends them to weblog
 */
static int
sendTo_afsAuthenticator(char *user, char *passwd, char *cell, char *type)
{
    char buf[MAXBUFF];

    afsassert(user);
    afsassert(passwd);
    afsassert(cell);
    afsassert(type);

    sprintf(buf, "%s\n%s\n%s\n%s", type, user, cell, passwd);
    return sendBuffer(buf, strlen(buf));
}

/*
 * reads the response from weblog over the pipe
 */
static int
recvFrom_afsAuthenticator(char *buf)
{
    int n;

    afsassert(buf);
    n = read(readPipe, buf, MAXBUFF);
    if (n < 0) {
	afslog(5,
	       ("%s: Error reading from pipe - %s", module_name,
		strerror(errno)));
	return -1;
    }
    return n;
}

#ifndef NO_AFSAPACHE_CACHE
/*
 * check local cache for the token associated with user crds.
 */
static int
check_Cache(char *user, char *passwd, char *cell, char *tokenBuf)
{
    char cksum[SHA_HASH_BYTES];	/* for sha checksum for caching */

    /* look up local cache - function in apache_afs_cach.c */
    weblog_login_checksum(user, cell, passwd, cksum);
    return weblog_login_lookup(user, cell, cksum, &tokenBuf[0]);
}

/*
 * put the token and the user credentials in the local cache
 */
static int
updateCache(char *user, char *passwd, char *cell, char *tokenBuf,
	    int cacheExpiration)
{
    long expires = 0, testExpires = 0;
    char cksum[SHA_HASH_BYTES];	/* for sha checksum for caching */

    /* put the token in local cache with the expiration date */
    expires = getExpiration(tokenBuf);
    if (expires < 0) {
	afslog(5,
	       ("%s: Error getting expiration time for cache. Expires %d",
		module_name, expires));
	return -1;
    }

    weblog_login_checksum(user, cell, passwd, cksum);

    if (cacheExpiration == 0) {
	weblog_login_store(user, cell, cksum, &tokenBuf[0], sizeof(tokenBuf),
			   expires);
    } else {
	testExpires = cacheExpiration + time(NULL);
	weblog_login_store(user, cell, cksum, &tokenBuf[0], sizeof(tokenBuf),
			   MIN(expires, testExpires));
    }
    return 0;
}
#endif /* NO_APACHEAFS_CACHE */


/*
 * locking routines to provide exclusive access to the pipes
 */
static int
start_lock(int fd, int cmd, int type)
{
    struct flock lock;
    lock.l_type = type;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;
    return (fcntl(fd, cmd, &lock));
}

static int
test_lock(int fd, int type)
{
    struct flock lock;
    lock.l_type = type;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    if (fcntl(fd, F_GETLK, &lock) < 0) {
	return -1;
    }
    if (lock.l_type == F_UNLCK) {
	return 0;		/* not locked */
    }
    return (lock.l_pid);	/* return pid of locking process */
}


#define Read_lock(fd) \
            start_lock(fd, F_SETLK, F_RDLCK)
#define Readw_lock(fd) \
            start_lock(fd, F_SETLKW, F_RDLCK)
#define Write_lock(fd) \
            start_lock(fd, F_SETLK, F_WRLCK)
#define Writew_lock(fd) \
            start_lock(fd, F_SETLKW, F_WRLCK)
#define Unlock(fd) \
            start_lock(fd, F_SETLK, F_UNLCK)
#define Is_readlock(fd) \
            test_lock(fd, F_RDLCK)
#define Is_writelock(fd) \
            test_lock(fd, F_WRLCK)

/*
 * communication between this process and weblog - sends user credentials
 * over a shared pipe (mutex provided using locks) and recieves either a
 * token or an error message
 */
static int
request_Authentication(char *user, char *passwd, char *cell, char *type,
		       char *tokenbuf, char *reason)
{
    int len = 0;
    int pid;
    char *temp;
    int lockfd = 0;

    afsassert(user);
    afsassert(passwd);
    afsassert(cell);
    afsassert(type);
    afsassert(tokenbuf);

/*
 * lock the pipe before beginning communication or in case of AIX it is an
 * error to attempt to lock a pipe or FIFO (EINVAL) therefore we have to create
 * a temporary file and use that fd instead
 */
#ifdef AIX
    lockfd = tempfd;
#else
    lockfd = writePipe;
#endif

    while ((pid = Is_writelock(lockfd)) != 0) {
	if (pid < 0) {
	    afslog(5,
		   ("%s: pid:%d Error locking pipe - %s", module_name,
		    getpid(), strerror(errno)));
	    return -1;
	}
	afslog(40,
	       ("%s: pid %d waiting for lock held by pid %d", module_name,
		getpid(), pid));
    }

    if (Write_lock(lockfd) == -1) {
	afslog(5,
	       ("%s: pid:%d Error write lock - %s. Retrying with WriteW",
		module_name, getpid(), strerror(errno)));
	if (Writew_lock(lockfd) == -1) {
	    afslog(5,
		   ("%s: pid:%d Error write lock - %s", module_name, getpid(),
		    strerror(errno)));
	    return -1;
	}
    }

    if (sendTo_afsAuthenticator(user, passwd, cell, type) == -1) {
	Unlock(lockfd);
	afslog(5, ("%s: Error sending authentication info", module_name));
	return -1;
    }

    len = recvFrom_afsAuthenticator(tokenbuf);

/* release the lock */
    if (Unlock(lockfd)) {
	afslog(5, ("%s: pid:%d Error unlocking", module_name, getpid()));
	return -1;
    }

    if (len > 0) {
	if (strncmp(tokenbuf, "FAILURE", 7) == 0) {
	    temp = &tokenbuf[7];
	    strncpy(reason, temp, len);
	    return -2;
	}
    }
    return len;
}

/*
 * pioctl setting token
 */
static int
setToken(char *tokenBuf, int tokenLen)
{
    char *temp;
    afs_int32 i = 0;

    afsassert(tokenBuf);

    /*
     * set the primary flag only if we haven't done a SETPAG previoulsy
     * by flipping this bit
     */
    if (!doneSETPAG) {
#ifdef OLDSETPAG
	/* skip over the secret token */
	temp = tokenBuf;
	memcpy(&i, temp, sizeof(afs_int32));
	temp += (i + sizeof(afs_int32));

	/* skip over the clear token */
	memcpy(&i, temp, sizeof(afs_int32));
	temp += (i + sizeof(afs_int32));

	doneSETPAG = 1;
	memcpy(&i, temp, sizeof(afs_int32));
	i |= 0x8000;
	memcpy(temp, &i, sizeof(afs_int32));
	temp += sizeof(afs_int32);
#endif

	if (do_setpag()) {
	    return -1;
	}
	doneSETPAG = 1;
    }
    return do_pioctl(tokenBuf, tokenLen, tokenBuf, tokenLen, VIOCSETTOK, NULL,
		     0);
}

/*
 * Get the token for the primary cell from the cache manager for this
 * process. Primary cell is the cell at the first index (index 0)
 */
static int
getToken(char *buf, int bufsize)
{
    /* get just the ONE token for this PAG from cache manager */
    afs_int32 i = 0;
    memcpy((void *)buf, (void *)&i, sizeof(afs_int32));
    return do_pioctl(buf, sizeof(afs_int32), buf, bufsize, VIOCGETTOK, NULL,
		     0);
}


/*
 * discard all authentication information for this PAG ie. this process
 */
int
unlog()
{
    return do_pioctl(0, 0, 0, 0, VIOCUNPAG, NULL, 0);
}


/*
 * Does the following things:
 * Checks whether there is a Basic Authentication header - obtains creds.
 * Checks local cache for the token associated with the user creds.
 * - if no token in cache - obtains token from weblog using pipes
 * - sets the token and returns appropriate return code
 * Return values: OK, SERVER_ERROR, AUTH_REQUIRED, FORBIDDEN
 */
int
authenticateUser(request_rec * r, char *defaultCell, int cacheExpiration,
		 char *type)
{
    char user[APACHEAFS_USERNAME_MAX];
    char passwd[APACHEAFS_PASSWORD_MAX];
    char cell[APACHEAFS_CELLNAME_MAX];
    char tokenbuf[MAXBUFF];
    char cksum[SHA_HASH_BYTES];
    int rc = 0;
    int i = 0;
    const char *auth_line;
    char reason[MAXBUFF];	/* if authentication failed - this is why */
    char err_msg[MAXBUFF];
    int userChanged = 0;

    afsassert(r);
    afsassert(r->uri);
    afsassert(defaultCell);
    afsassert(type);

    auth_line = TABLE_GET(r->headers_in, "Authorization");

    if (strcmp(global_default_cell, defaultCell)) {
	strcpy(global_default_cell, defaultCell);
    }

    memset(user, 0, APACHEAFS_USERNAME_MAX);
    memset(passwd, 0, APACHEAFS_PASSWORD_MAX);
    memset(cell, 0, APACHEAFS_CELLNAME_MAX);

    if (auth_line == NULL) {	/* No Authorization field - we don't do anything */
	/*
	 * No Authorization field recieved - that's fine by us.
	 * go ahead and attempt to service the request and if we get
	 * back FORBIDDEN then we'll take care of it then
	 */
	afslog(15, ("%s: No authline recieved", module_name));
	haveAuth = 0;
	userChanged = 1;
	memset(lastUser, 0, APACHEAFS_USERNAME_MAX);
	memset(lastCell, 0, APACHEAFS_CELLNAME_MAX);
	memset(lastCksum, 0, SHA_HASH_BYTES);
	rc = unlog();
	afslog(25,
	       ("%s: pid:%d No Authorization field. Unlogging ...",
		module_name, getpid()));
	if (rc) {
	    sprintf(err_msg,
		    "%s: Error unlogging from AFS cell - rc: %d, errno:%d",
		    module_name, rc, errno);
	    LOG_REASON(err_msg, r->uri, r);
	    return SERVER_ERROR;
	}
	return OK;
    }

    /*
     * We should get here only if there IS an Authorization field
     */

    if ((rc = parse_authhdr(r, user, passwd, cell, defaultCell)) != 0) {
	sprintf(err_msg, "%s: Error parsing Authorization Header rc:%d",
		module_name, rc);
	LOG_REASON(err_msg, r->uri, r);
	return rc;		/* SERVER ERROR */
    }

    /*
     *  should get here only after obtaining the username and password and cell
     *  check to make sure anyway
     */
    if ((user[0] == '\0') || (cell[0] == '\0') || (passwd[0] == '\0')) {
	afslog(15,
	       ("%s: pid:%d No username or password or cell. Unlogging.",
		module_name, getpid()));
	haveAuth = 0;
	userChanged = 1;
	memset(lastUser, 0, APACHEAFS_USERNAME_MAX);
	memset(lastCell, 0, APACHEAFS_CELLNAME_MAX);
	memset(lastCksum, 0, SHA_HASH_BYTES);
	rc = unlog();
	if (rc) {
	    sprintf(err_msg,
		    "%s: Error unlogging from AFS cell - rc: %d, errno:%d",
		    module_name, rc, errno);
	    LOG_REASON(err_msg, r->uri, r);
	    return SERVER_ERROR;
	}
	setCellAuthHeader(r);
	return AUTH_REQUIRED;
    }
#ifdef DEBUG
    fprintf(stderr, "Cell:%s\tUser:%s\tPasswd:%s\n", cell, user, passwd);
#endif

    /*
     * compare with previous username/cell/cksum - update it
     * unlog if required
     */

    weblog_login_checksum(user, cell, passwd, cksum);
    if (!haveAuth) {
	haveAuth = 1;
	strcpy(lastUser, user);
	strcpy(lastCksum, cksum);
	strcpy(lastCell, cell);
    }
    if (strcmp(user, lastUser) || strcmp(cell, lastCell)
	|| strcmp(cksum, lastCksum)) {
	/*
	 * unlog the old user from the cell if a new username/passwd is recievd
	 */

	userChanged = 1;
	afslog(25,
	       ("%s: pid:%d\tUnlogging user %s from cell%s", module_name,
		getpid(), lastUser, lastCell));
	afslog(25, ("%s:New user:%s\t New Cell:%s", module_name, user, cell));
	afslog(25, ("%s:Trying to get URL:%s", module_name, r->uri));
	afslog(25, ("%s: Unlogging ....", module_name));

	if (unlog()) {
	    sprintf(err_msg,
		    "%s: Error unlogging from AFS cell - rc: %d, errno:%d",
		    module_name, rc, errno);
	    LOG_REASON(err_msg, r->uri, r);
	    return SERVER_ERROR;
	}
	/* change  lastUser to this user */
	strcpy(lastUser, user);
	strcpy(lastCksum, cksum);
	strcpy(lastCell, cell);
    }

    /* strcmp checksums - ie. did the user change */
#ifndef NO_AFSAPACHE_CACHE
    if (!cache_initialized) {
	token_cache_init();
	cache_initialized = 1;
    }

    /* have to check local cache for this name/passwd */

    rc = check_Cache(user, passwd, cell, tokenbuf);
    if (rc) {
	/* if found then just send the request without going through
	 * weblog - this means the user has already been authenticated
	 * once and we have a valid token just need to set it -
	 * only if it is different from the token already set. No need to
	 * even unlog because this token is set for the entire PAG which
	 * of course consists of just this child process
	 */
	afslog(35,
	       ("%s: pid:%d found user %s's token (expires:%d) in cache",
		module_name, getpid(), user,
		(getExpiration(tokenbuf) - time(NULL))));

	/* if the user changed then set this token else leave it since it should
	 * be set */
	if (userChanged) {
	    /* set this token obtained from the local cache */
	    afslog(15,
		   ("%s:pid:%d\t Setting cached token", module_name,
		    getpid()));
	    if (setToken(tokenbuf, rc)) {
		afslog(5,
		       ("%s: pid:%d Failed to set token obtained from cache."
			"rc:%d errno:%d Token Expiration:%d", module_name,
			getpid(), rc, errno,
			(getExpiration(tokenbuf) - time(NULL))));
#ifdef DEBUG_CACHE
		parseToken(tokenbuf);
#endif
		/*
		 * BUG WORKAROUND: sometimes we fail while setting token
		 * with errno ESRCH indicating the named cell
		 * in the last field of the token is not recognized - but that's
		 * not quite true according to parseToken()!! Possibly corrupted
		 * tokens from the cache?
		 * Anyway we just get a new token from weblog
		 */
		goto reqAuth;
	    }
	} /* if userChanged */
	else {
	    /* if this is a child process getting the request for the first time
	     * then there's no way this guy's got a token for us in which case
	     * getToken should fail with EDOM and that means we should set the token
	     * first and maybe set a static variable saying we have set a token?
	     */
	    char temp[MAXBUFF];
	    if (getToken(temp, sizeof(temp))) {
		if (errno == EDOM) {
		    /* try setting the cached token */
		    if (setToken(tokenbuf, rc)) {
			/*
			 * same bug workaround here. ie. go to weblog if setting
			 * the cached token fails.
			 */
			sprintf(err_msg,
				"%s: pid:%d Failed to set cached token."
				"errno:%d rc:%d", module_name, getpid(),
				errno, rc);
			LOG_REASON(err_msg, r->uri, r);
			goto reqAuth;
		    }
		} else {
		    /* and again for any getToken failure other than EDOM */
		    sprintf(err_msg,
			    "%s: Failed to get token: errno:%d rc:%d",
			    module_name, errno, rc);
		    LOG_REASON(err_msg, r->uri, r);
		    goto reqAuth;
		}
	    }			/* so we already have a token set since the gettoken succeeded */
	}

	/* to set the REMOTE_USER environment variable */
	strcpy(r->connection->user, user);
	return OK;
    }
    /*
     * else - request afs_Authenticator's for it and update local cache
     * then go about serving the request URI
     */
    else {
      reqAuth:
#endif /* NO_AFSAPACHE_CACHE */

	rc = request_Authentication(user, passwd, cell, type, tokenbuf,
				    reason);
	if (rc > 0) {
	    /* we got back a token from weblog */
	    /* set the token with setToken */
	    if (setToken(tokenbuf, rc)) {
		sprintf(err_msg,
			"%s: Failed to set token given by weblog. errno:%d",
			module_name, errno);
		LOG_REASON(err_msg, r->uri, r);
		return SERVER_ERROR;
	    }
#ifdef DEBUG_TOKENS
	    system("/usr/afsws/bin/tokens");
#endif

#ifndef NO_AFSAPACHE_CACHE
	    /* update local cache */
	    if (updateCache(user, passwd, cell, tokenbuf, cacheExpiration)) {
		sprintf(err_msg, "%s: Error updating cache", module_name);
		LOG_REASON(err_msg, r->uri, r);
		return SERVER_ERROR;
	    }
	    afslog(15,
		   ("%s: pid:%d\t put user:%s tokens in cache", module_name,
		    getpid(), user));
#endif /* NO_AFSAPACHE_CACHE */

	    /* now we've got a token, updated the cache and set it so we should
	     * have no problems accessing AFS files - however if we do then
	     * we handle it in afs_accessCheck() when the error comes back
	     */

	    /* to set the REMOTE_USER environment variable to the username */
	    strcpy(r->connection->user, user);
	    return OK;
	} else if (rc == -2) {
	    sprintf(err_msg,
		    ":%s: AFS authentication failed for %s@%s because %s",
		    module_name, user, cell, reason);
	    LOG_REASON(err_msg, r->uri, r);
	    setCellAuthHeader(r);
	    return AUTH_REQUIRED;
	} else if (rc == -1) {
	    sprintf(err_msg, "%s: Error readiong from pipe. errno:%d",
		    module_name, errno);
	    LOG_REASON(err_msg, r->uri, r);
	    return SERVER_ERROR;
	}

	else {
	    /*
	     * unknown error from weblog - this should not occur
	     * if afs_Authenticator can't authenticate you, then return FORBIDDEN
	     */
	    sprintf(err_msg,
		    "%s: AFS could not authenticate user %s in cell %s."
		    "Returning FORBIDDEN", module_name, user, cell);
	    LOG_REASON(err_msg, r->uri, r);
	    return FORBIDDEN;
	}
#ifndef NO_AFSAPACHE_CACHE
    }
#endif
    /* should never get here */
    LOG_REASON("AFS Authentication: WE SHOULD NEVER GET HERE", r->uri, r);
    return SERVER_ERROR;
}


/*
 * pioctl call to get the cell name hosting the object specified by path.
 * returns 0 if successful -1 if failure. Assumes memory has been allocated
 * for cell. Used to set the www-authenticate header.
 */
static int
get_cellname_from_path(char *apath, char *cell)
{
    int rc;

    afsassert(apath);
    afsassert(cell);

    rc = do_pioctl(NULL, 0, cell, APACHEAFS_CELLNAME_MAX, VIOC_FILE_CELL_NAME,
		   apath, 1);
    if (rc)
	afslog(30,
	       ("%s: Error getting cell from path %s. errno:%d rc:%d",
		module_name, apath, errno, rc));
    else
	afslog(30,
	       ("%s: Obtained cell %s from path %s", module_name, cell,
		apath));

    return rc;
}

/*
 * obtains the path to the file requested and sets things up to
 * call get_cell_by_name.
 * TODO: These could well be combined into one single function.
 */
static int
getcellname(request_rec * r, char *buf)
{
    int rc = 0;

    afsassert(r);
    afsassert(buf);

    if (r->filename) {
	rc = get_cellname_from_path(r->filename, buf);
    } else {
	char path[1024];
	sprintf(path, "%s/%s", DOCUMENT_ROOT(r), r->uri);
	rc = get_cellname_from_path(path, buf);
    }
    return rc;
}

/*
 * Returns a part of the url upto the second slash in the buf
 */
static int
geturi(request_rec * r, char *buf)
{
    int rc = 0;
    char *pos;
    char *end;
    int i = 0;
    int max = 2;

    afsassert(r);
    afsassert(buf);

    memset(buf, 0, APACHEAFS_CELLNAME_MAX);
    pos = strchr(r->uri, '/');
    if (pos != NULL) {
	pos++;
	for (i = 0; i < max; i++) {
	    end = strchr(pos, '/');
	    if (end != NULL) {
		int len = strlen(pos) - strlen(end);
		strcat(buf, "/");
		strncat(buf, pos, len);
		afslog(35,
		       ("%s: Getting URI upto second slash buf:%s",
			module_name, buf));
		pos += (len + 1);
		end = strchr(pos, '/');
		if (end == NULL) {
		    break;
		}
	    } else {
		strcat(buf, pos);
		break;
	    }
	}
    } else {
	strcpy(buf, " ");
    }
    return rc;
}

/*
 * This function recursively parses buf and places the output in msg
 * Eg. <%c%uUnknown> gets translated to the cellname that the file
 * resides in, failing which the first part of the uri failing which the
 * string Unknown
 */
static int
parseAuthName_int(request_rec * r, char *buf, char *msg)
{
    char *pos;
    char *end;
    int len = 0;
    int rc = 0;
    char blank[APACHEAFS_CELLNAME_MAX];

    afsassert(r);
    afsassert(buf);
    afsassert(msg);

    memset(blank, 0, sizeof(blank));
    afslog(50,
	   ("%s: Parsing Authorization Required reply. buf:%s", module_name,
	    buf));

    pos = strchr(buf, '<');
    if (pos) {
	len = strlen(pos);
	pos++;
	end = strchr(pos, '>');
	if (end == NULL) {
	    afslog(0,
		   ("%s:Parse error for AUTH_REQUIRED reply - mismatched <",
		    module_name));
	    fprintf(stderr, "Parse Error: mismatched <\n");
	    strncpy(msg, buf, strlen(buf) - len);
	    afslog(0, ("%s: msg:%s", msg));
	    return -1;
	}
	end++;
	if (pos[0] == '%') {
	    pos++;
	    switch (pos[0]) {
	    case 'c':
		rc = getcellname(r, blank);
		if (!rc) {
		    strncpy(msg, buf, strlen(buf) - len);
		    strcat(msg, blank);
		    strcat(msg, end);
		    return 0;
		}
		break;

	    case 'u':
		rc = geturi(r, blank);
		if (!rc) {
		    strncpy(msg, buf, strlen(buf) - len);
		    strcat(msg, blank);
		    strcat(msg, end);
		    return 0;
		}
		break;

	    case 'd':
		if (global_default_cell != NULL) {
		    strncpy(msg, buf, strlen(buf) - len);
		    strcat(msg, global_default_cell);
		    strcat(msg, end);
		    return 0;
		}
		break;
	    }
	    strncpy(msg, buf, strlen(buf) - len);
	    strcat(msg, "<");
	    pos++;
	    strcat(msg, pos);
	    strcpy(buf, msg);
	    memset(msg, 0, 1024);
	    parseAuthName_int(r, buf, msg);
	    return 0;
	} else {
	    strncpy(msg, buf, strlen(buf) - len);
	    strncat(msg, pos, strlen(pos) - strlen(end) - 1);
	    strcat(msg, end);
	    return 0;
	}
    }
}

/*
 * Parses the entire auth_name string - ie. takes care of multiple
 * <%...> <%...>
 */
static int
parseAuthName(request_rec * r, char *buf)
{
    char *pos;
    int rc;
    char msg[1024];

    afsassert(r);
    afsassert(buf);

    memset(msg, 0, sizeof(msg));

    pos = strchr(buf, '<');
    while (pos != NULL) {
	rc = parseAuthName_int(r, buf, msg);
	if (rc) {
	    strcpy(buf, msg);
	    afslog(35,
		   ("%s: Failed to parse Auth Name. buf:%s", module_name,
		    buf));
	    return -1;
	}
	strcpy(buf, msg);
	memset(msg, 0, sizeof(msg));
	pos = strchr(buf, '<');
    }
    afslog(50,
	   ("%s: Parsing WWW Auth required reply. final message:%s",
	    module_name, buf));
    return 0;
}


/*
 * Set the www-authenticate header - this is the login prompt the users see
 */
static int
setCellAuthHeader(request_rec * r)
{
    char *name;
    char buf[1024];
    int rc = 0;

    afsassert(r);

    name = (char *)get_afs_authprompt(r);
    if (name != NULL) {
	strcpy(buf, name);
	rc = parseAuthName(r, buf);
    } else {
	strcpy(buf, " ");
    }
    TABLE_SET(r->err_headers_out, "WWW-Authenticate",
	      PSTRCAT(r->pool, "Basic realm=\"", buf, "\"", NULL));
    return rc;
}


/*
 * Checks if we have some authentication credentials, if we do returns
 * FORBIDDEN and if we don't then returns AUTH_REQUIRED with the appropriate
 * www-authenticate header. Should be called if we can't access a file because
 * permission is denied.
 */
int
forbToAuthReqd(request_rec * r)
{
    if (haveAuth) {
	return FORBIDDEN;
    } else {
	setCellAuthHeader(r);
	return AUTH_REQUIRED;
    }
}
