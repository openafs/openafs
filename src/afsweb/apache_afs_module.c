/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * afs_module  Version 1.3     < for Apache Server version 1.2.6 and 1.3.1 >
 *
 * Add to Configuration file (before any of the Authentication modules)
 * Module afs_module    afs_module.o
 *
 * Add to server configuration file:
 * SetAFSDefaultCell <cell name>
 * SetAFSCacheExpiration <seconds for AFS client cache expiration>
 * SetAFSTokenExpiration <seconds for AFS token expiration>
 * SetAFSWeblogPath <path (full or relative to ServerRoot to the weblog_starter binary>
 * SetAFSDebugLevel <positive integer for verbosity level of error logs>
 * Per directory configuration for locations that AFS access is desired may
 * be specified as follows Eg.
 * <Location /afs>
 * ## Optionally set default cell for this location AFSDefaultCell <cell name>
 * AFSAuthType AFS
 * AFSLoginPrompt AFS Login for <%c/%d>
 * </Location>
 */

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_conf_globals.h"


/*
 * default parameters - if none are specified in the config file
 * these are used
 */
#define DEFAULT_WEBLOG_PATH                   "./weblog_starter"
#define DEFAULT_CACHE_EXPIRATION              300	/* 5 minutes */
#define DEFAULT_TOKEN_EXPIRATION              600	/* 10 minutes */
#define DEFAULT_DEBUG_LEVEL                   0
#define null ((void*)0)

extern u_long afsDebugLevel;

/* forward declaration, see defination at end of file */
module afs_module;

/* Data type for server configuration */
typedef struct {
    char *afs_defaultCell;
    u_long afs_cacheExpiration;
    u_long afs_tokenExpiration;
    char *afs_weblogPath;
    char *afs_accessLog;
    u_long afs_debugLevel;
} afs_server_config_rec;


/* Data type for per directory configuration */
typedef struct {
    char *afs_authtype;
    char *afs_prompt;
    char *afs_cell;
} afs_dir_config_rec;


static int initialized = 0;

static char defaultCell[64];
static u_long cacheExpiration;
int logAccessErrors = 0;
int logfd = -1;

static int afsAccessFile_flags = (O_WRONLY | O_APPEND | O_CREAT);
#ifdef __EMX__
/* OS/2 dosen't support users and groups */
static mode_t afsAccessFile_mode = (S_IREAD | S_IWRITE);
#else
static mode_t afsAccessFile_mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#endif

void
afs_init(server_rec * s, pool * p)
{
    if (!initialized) {
	char *weblogPath;
	afs_server_config_rec *src =
	    get_module_config(s->module_config, &afs_module);
	int tokenExpiration = src->afs_tokenExpiration;

	initialized = 1;
	if (src->afs_weblogPath)
	    weblogPath = pstrdup(p, src->afs_weblogPath);
	else
	    weblogPath = pstrdup(p, DEFAULT_WEBLOG_PATH);

	if (src->afs_defaultCell)
	    strcpy(defaultCell, src->afs_defaultCell);
	else {
	    fprintf(stderr,
		    "AFS Module: SetAFSDEfaultCell <cellname> "
		    "is a required directive for module to work. Exiting\n");
	    exit(1);
	}
	tokenExpiration = src->afs_tokenExpiration;
	cacheExpiration = src->afs_cacheExpiration;
	afsDebugLevel = src->afs_debugLevel;

	if ((src->afs_accessLog == NULL) || (src->afs_accessLog == "")) {
	    logAccessErrors = 0;
	} else {
	    char *filename = server_root_relative(p, src->afs_accessLog);
	    logfd = open(filename, afsAccessFile_flags, afsAccessFile_mode);
	    if (logfd < 0) {
		fprintf(stderr,
			"afs_module:Error opening afs access log file:%s\n"
			"Error:%s,Exiting\n", strerror(errno), filename);
		exit(-1);
	    }
	    logAccessErrors = 1;
	}

	if (cacheExpiration < 0) {
	    cacheExpiration = DEFAULT_CACHE_EXPIRATION;
	}
	if (tokenExpiration < 0) {
	    tokenExpiration = DEFAULT_TOKEN_EXPIRATION;
	}

	if (afsDebugLevel < 0) {
	    afsDebugLevel = DEFAULT_DEBUG_LEVEL;
	}

	if (cacheExpiration > tokenExpiration) {
	    fprintf(stderr,
		    "tokenExpiration:%d is less than cacheExpiration:%d\n",
		    tokenExpiration, cacheExpiration);
	    fprintf(stderr, "\n\n");
	    fprintf(stderr,
		    "Ensure that this does not happen, since you will "
		    "not be allowed access to files if the cached token has expired "
		    "because the token lifetime is less than the cache expiration "
		    "time. Restart the server after editing the config file\n");
	    exit(1);
	}

	if (!cacheExpiration && tokenExpiration) {
	    fprintf(stderr,
		    "cacheExpiration is set to infinity (0), but "
		    "tokenExpiration:%d is finite\n", tokenExpiration);
	    fprintf(stderr, "\n\n");
	    fprintf(stderr,
		    "Ensure that this does not happen, since you will "
		    "not be allowed access to files if the cached token has expired "
		    "because the token lifetime is less than the cache expiration "
		    "time (infinite). "
		    "Restart the server after editing the config file\n");
	}
#ifdef SHUTDOWN_IF_AFS_FAILS
	afs_plugin_init(tokenExpiration, server_root_relative(p, weblogPath),
			server_root_relative(p, s->error_fname),
			server_root_relative(p, pid_fname), defaultCell, NULL,
			cacheExpiration, NULL, 1);
#else
	afs_plugin_init(tokenExpiration, server_root_relative(p, weblogPath),
			server_root_relative(p, s->error_fname),
			server_root_relative(p, pid_fname), defaultCell, NULL,
			cacheExpiration, NULL, 0);
#endif
    }
}

static int
check_weblog(char *path)
{
    struct stat fs;
    int rc = stat(path, &fs);
    if (rc)
	return rc;

    if (!((fs.st_mode & S_IXUSR) || (fs.st_mode & S_IXGRP)
	  || (fs.st_mode & S_IXOTH)))
	return 2;
    return 0;
}

/*
 * per-server configuration creator
 */
void *
create_afs_server_config(pool * p, server_rec * s)
{
    afs_server_config_rec *scr =
	(afs_server_config_rec *) pcalloc(p, sizeof(afs_server_config_rec));
    scr->afs_cacheExpiration = 0;
    scr->afs_tokenExpiration = 0;
    scr->afs_debugLevel = 0;
    return (void *)scr;
}

/*
 * per-dir configuration record creator
 */
void *
create_afs_dir_config(pool * p, char *dummy)
{
    afs_dir_config_rec *new =
	(afs_dir_config_rec *) pcalloc(p, sizeof(afs_dir_config_rec));
    return (void *)new;
}

/*
 * Handlers for server config lines - set afs default cell, weblog path, access log,
 * token and cache expiration time to the server configuration. As well as per-dir
 * configs like AFSAuthentication.
 */
const char *
set_afscell(cmd_parms * cmd, void *dummy, char *arg)
{
    void *sc = cmd->server->module_config;
    afs_server_config_rec *sr = get_module_config(sc, &afs_module);

    if ((arg == NULL) || (arg[0] == '\0'))
	return "AFS Default cell is a required directive";

    sr->afs_defaultCell = arg;
    return NULL;
}

const char *
set_afsweblogpath(cmd_parms * cmd, void *dummy, char *arg)
{
    void *sc = cmd->server->module_config;
    afs_server_config_rec *sr = get_module_config(sc, &afs_module);
    int rc = check_weblog(arg);
    if (rc) {
	switch (rc) {
	case 1:
	    return
		"Weblog Path should be the full path to the weblog_starter binary";
	case 2:
	    return "weblog_starter not executable";
	}
    }
    sr->afs_weblogPath = arg;
    return NULL;
}

const char *
set_afsacceslog(cmd_parms * cmd, void *dummy, char *arg)
{
    void *sc = cmd->server->module_config;
    afs_server_config_rec *sr = get_module_config(sc, &afs_module);
    sr->afs_accessLog = arg;
    return NULL;
}

static const char *
set_afs_CacheExpiration(cmd_parms * cmd, void *dummy, char *expTime)
{
    afs_server_config_rec *sr =
	get_module_config(cmd->server->module_config, &afs_module);
    sr->afs_cacheExpiration = atol(expTime);
    return NULL;
}

static const char *
set_afs_TokenExpiration(cmd_parms * cmd, void *dummy, char *expTime)
{
    afs_server_config_rec *sr =
	get_module_config(cmd->server->module_config, &afs_module);
    sr->afs_tokenExpiration = atol(expTime);
    return NULL;
}

static const char *
set_afs_DebugLevel(cmd_parms * cmd, void *dummy, char *debugLevel)
{
    afs_server_config_rec *sr = get_module_config(cmd->server->module_config,
						  &afs_module);
    sr->afs_debugLevel = atol(debugLevel);
    return NULL;
}

/* Commands this module needs to handle */
command_rec afs_cmds[] = {
    {"SetAFSDefaultCell", set_afscell, NULL,
     RSRC_CONF, TAKE1, "the AFS cell you want to access by default"}
    ,

    {"SetAFSCacheExpiration", set_afs_CacheExpiration, NULL, RSRC_CONF, TAKE1,
     "time for tokens in the client cache to live"}
    ,

    {"SetAFSWeblogPath", set_afsweblogpath, NULL, RSRC_CONF, TAKE1,
     "full or relative to ServerRoot path to the weblog_starter binary"}
    ,

    {"SetAFSTokenExpiration", set_afs_TokenExpiration, NULL, RSRC_CONF, TAKE1,
     "time for tokens in the kernel cache to live"}
    ,

    {"SetAFSAccessLog", set_afsacceslog, NULL, RSRC_CONF, TAKE1,
     "log afs access errors to this file"}
    ,

    {"SetAFSDebugLevel", set_afs_DebugLevel, NULL, RSRC_CONF, TAKE1,
     "Set Verbosity level for logging stuff to the error log"}
    ,

    {"AFSLoginPrompt", set_string_slot,
     (void *)XtOffsetOf(afs_dir_config_rec, afs_prompt), ACCESS_CONF, TAKE1,
     "Prompt to show on Authenticating Window - similar to AuthName"},

    {"AFSAuthType", set_string_slot,
     (void *)XtOffsetOf(afs_dir_config_rec, afs_authtype), ACCESS_CONF, TAKE1,
     "Authentication type for AFS"},

    {"AFSDefaultCell", set_string_slot,
     (void *)XtOffsetOf(afs_dir_config_rec, afs_cell), ACCESS_CONF, TAKE1,
     "Default AFS cell for this location"},

    {NULL}
};

char *
get_afsauthtype(request_rec * r)
{
    afs_dir_config_rec *dr = (afs_dir_config_rec *)
	get_module_config(r->per_dir_config, &afs_module);
    if (!dr->afs_authtype) {
	/* Try to unite CGI subprocess configuration info */
	if (r->main) {
	    dr = (afs_dir_config_rec *)
		get_module_config(r->main->per_dir_config, &afs_module);
	}
    }
    return dr->afs_authtype;
}

char *
get_afs_authprompt(request_rec * r)
{
    afs_dir_config_rec *dr =
	(afs_dir_config_rec *) get_module_config(r->per_dir_config,
						 &afs_module);
    if (!dr->afs_prompt) {
	/* Try to unite CGI subprocess configuration info */
	if (r->main) {
	    dr = (afs_dir_config_rec *)
		get_module_config(r->main->per_dir_config, &afs_module);
	}
    }
    return dr->afs_prompt;
}

static char *
get_afs_cell(request_rec * r)
{
    afs_dir_config_rec *dr =
	(afs_dir_config_rec *) get_module_config(r->per_dir_config,
						 &afs_module);
    if (!dr->afs_cell) {
	/* Try to unite CGI subprocess configuration info */
	if (r->main) {
	    dr = (afs_dir_config_rec *)
		get_module_config(r->main->per_dir_config, &afs_module);
	}
    }
    return dr->afs_cell;
}

int
afs_auth(request_rec * r)
{
    char *cell = get_afs_cell(r);
    return afs_auth_internal(r, cell ? cell : defaultCell, cacheExpiration);
}



module afs_module = {
    STANDARD_MODULE_STUFF,
    afs_init,
    create_afs_dir_config,
    NULL,
    create_afs_server_config,
    NULL,
    afs_cmds,
    NULL,
    afs_auth,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};
