#ifndef _WEBLOG_ERRORS_H_INCLUDED_
#define _WEBLOG_ERRORS_H_INCLUDED_

/* error code definitions */
#define PIPEREAD 1 
#define PIPESEND 2
#define KA       3
#define PARSE    4
#define NULLARGS 5
#define RESTART  6

#define WEBLOGMINERROR  ((10<<16))
#define PIPEREADERROR   (WEBLOGMINERROR + PIPEREAD)
#define PIPESENDERROR   (WEBLOGMINERROR + PIPESEND) 
#define KAERROR         (WEBLOGMINERROR + KA)
#define NULLARGSERROR   (WEBLOGMINERROR + PARSE)
#define PARSEERROR      (WEBLOGMINERROR + NULLARGS)
#define RESTARTERROR    (WEBLOGMINERROR + RESTART)

#define PIPEREADMSG "Error reading from pipe"
#define KAMSG       "Kerberos Authentication error from ka_init"
#define PIPESENDMSG "Error sending through pipe"
#define PARSEMSG    "Error parsing data recieved from pipe"
#define NULLARGSMSG "Null arguments"
#define RESTARTMSG  "Error restarting"


#define WEBLOGEXIT(code) \
rx_Finalize(); \
(!code ? exit(0) : exit((code)-WEBLOGMINERROR))

#endif /* _WEBLOG_ERRORS_H_INCLUDED_ */

