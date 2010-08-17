/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
 * Apache API calls required by libapacheafs.a
 */

#ifndef _APACHE_API_H_INCLUDED_
#define _APACHE_API_H_INCLUDED_

#ifdef APACHE_1_3

#ifdef APACHE_1_3_1
#include "1.3.1/httpd.h"
#include "1.3.1/http_conf_globals.h"
#include "1.3.1/ap_compat.h"
#elif defined(APACHE_1_3_6)
#include "1.3.6/httpd.h"
#include "1.3.6/http_conf_globals.h"
#include "1.3.6/ap_compat.h"
#else
#error No Apache subversion defined for APACHE_1_3 (such as APACHE_1_3_6)
#endif /* APACHE_1_3_1 */

#define OS_ESCAPE_PATH        ap_os_escape_path
#define TABLE_SET             ap_table_set
#define TABLE_GET             ap_table_get
#define PID_FNAME             ap_pid_fname
#define GETWORD               ap_getword
#define GETWORD_NULLS         ap_getword_nulls
#define GET_TIME              ap_get_time
#define UUDECODE              ap_uudecode
#define DOCUMENT_ROOT         ap_document_root
#define PSTRCAT               ap_pstrcat
#define LOG_REASON            ap_log_reason
#define FIND_LINKED_MODULE    ap_find_linked_module
#else
#include "1.2/httpd.h"
#include "1.2/http_conf_globals.h"	/* for pid_fname */

#define OS_ESCAPE_PATH        os_escape_path
#define TABLE_SET             table_set
#define TABLE_GET             table_get
#define PID_FNAME             pid_fname
#define GETWORD               getword
#define GETWORD_NULLS         getword_nulls
#define GET_TIME              get_time
#define UUDECODE              uudecode
#define DOCUMENT_ROOT         document_root
#define PSTRCAT               pstrcat
#define LOG_REASON            log_reason
#define FIND_LINKED_MODULE    find_linked_module
#endif
#endif /* _APACHE_API_H_INCLUDED_ */
