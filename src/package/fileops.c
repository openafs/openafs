/*
  *	(C) Copyright 10/12/86 by Carnegie Mellon University
  */

/*
 * Revision 1.2  89/09/13  11:36:48
 * Various fixes so it can compile under AIX.
 * 
 * Revision 1.1  89/06/14  11:06:03
 * Initial revision
 * 
 * Revision 1.3  88/07/28  13:41:05
 * working simulation moved over to the beta cell
 * 
 * Revision 1.2  88/02/26  05:07:56
 * simulation of package with a yacc parser
 * 
 * Revision 1.1  88/02/23  02:17:14
 * Initial revision
 * 
 */

#include <afs/param.h>
#include <sys/param.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <errno.h>
#ifdef	AFS_SUN5_ENV
#include <sys/fcntl.h>
#endif
#include "globals.h"
#include "package.h"


int mv(from,to)
register char *from;
register char *to;
{
    loudonly_message("mv %s %s",from,to);
    if (!opt_lazy && rename(from,to) < 0)
    {
#if defined(AFS_HPUX_ENV)
	char pnameBusy[512];

	if (errno == ETXTBSY) {
	    (void)strcpy(pnameBusy, to);
	    (void)strcat(pnameBusy, ".BUSY");
	    if (rename(to, pnameBusy) == 0) {
		if (rename(from, to) < 0) {
		    unlink(pnameBusy);
		    if (errno == ETXTBSY) {
			message("rename %s %s; %m (ignored)",from,to);				
			return 0;
		    }
		    message("rename %s %s; %m",from,to);	
		    return -1;
		}
		unlink(pnameBusy);
		return 0;
	    } else if (errno == ETXTBSY) {
		message("rename %s %s; %m (ignored)", to, pnameBusy);				
		return 0;
	    }
	}
#endif /* AFS_HPUX_ENV */
	message("rename %s %s; %m",from,to);	
	return -1;
    }
    return 0;
}

int rm(path)
register char *path;
{
    register char *endp;
    register struct dirent *de;
    register DIR *dp;
    struct stat stb;

    if (lstat(path,&stb) < 0)
    {
	/* message("lstat %s; %m",path); */
	return;
    }
#ifdef	KFLAG
    if (opt_kflag && (stb.st_mode & 0222) == 0)
    {
	loudonly_message("INHIBIT %s removal",path);
	return;
    }
#endif /* KFLAG */
    if ((stb.st_mode & S_IFMT) != S_IFDIR)
    {
	loudonly_message("rm %s",path);
	if (!opt_lazy && unlink(path) < 0)
	{
	    message("unlink %s; %m",path);
	    return;
	}
	return;
    }
    endp = path + strlen(path);
    if ((dp = opendir(path)) == 0)
    {
	message("opendir %s; %m",path);
	return;
    }
    *endp++ = '/';
    while ((de = readdir(dp)) != 0)
    {
	if (de->d_name[0] == '.')
	{
	    if (de->d_name[1] == 0)
		continue;
	    if (de->d_name[1] == '.' && de->d_name[2] == 0)
		continue;
	}
	(void)strcpy(endp,de->d_name);
	(void)rm(path);
    }
    *--endp = 0;
    (void)closedir(dp);
    loudonly_message("rmdir %s",path);
    if (!opt_lazy && rmdir(path) < 0)
    {
	message("rmdir %s; %m",path);
	return;
    }
    return;
}



int cp(from,to)
register char	*from;
register char	*to;
{
    register int ffd, tfd, cc;
    char buffer[8192];

    loudonly_message("cp %s %s",from,to);
    if (opt_lazy)
	return 0;
    if ((ffd = open(from,O_RDONLY)) < 0)
    {
	message("open %s; %m",from);
	return -1;
    }
    if ((tfd = open(to,O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0)
    {
	message("open %s; %m",to);
	(void)close(ffd);
	return -1;
    }
    for (;;)
    {
	if ((cc = read(ffd,buffer,sizeof(buffer))) < 0)
	{
	    message("read %s; %m",from);
	    (void)close(ffd);
	    (void)close(tfd);
	    return -1;
	}
	if (cc == 0)
	    break;
	if (cc != write(tfd,buffer,cc))
	{
	    message("write %s; %m",to);
	    (void)close(ffd);
	    (void)close(tfd);
	    return -1;
	}
    }
    if (close(ffd) < 0)
    {
	message("close %s; %m",from);
	(void)close(tfd);
	return -1;
    }
    if (close(tfd) < 0)
    {
	message("close %s; %m",to);
	return -1;
    }
    return 0;
}


int ln(from,to)
register char *from;
register char *to;
{
    loudonly_message("ln %s %s",from,to);
    if (!opt_lazy && link(from,to) < 0)
    {
	message("ln %s %s; %m",from,to);
	return -1;
    }
    return 0;
}




int mklostfound(path)
register char	*path;
{
    register char *u, *l, *endp;
    register int f;
    struct stat stb;

    loudonly_message("mklost+found %s",path);
    if (opt_lazy)
	return 0;
    endp = path + strlen(path);
    *endp++ = '/';
    endp[2] = 0;
    for (u = "0123456789abcdef"; *u; u++)
    {
	for (l = "0123456789abcdef"; *l; l++)
	{
	    endp[0] = *u;
	    endp[1] = *l;
	    f = open(path,O_CREAT|O_TRUNC|O_WRONLY,0666);
	    if (f < 0)
	    {
		message("open %s; %m",path);
		continue;
	    }
	    (void)close(f);
	}
    }
    for (u = "0123456789abcdef"; *u; u++)
    {
	for (l = "0123456789abcdef"; *l; l++)
	{
	    endp[0] = *u;
	    endp[1] = *l;
	    if (lstat(path,&stb) >= 0)
		if ((stb.st_mode & S_IFMT) != S_IFDIR)
		    if (unlink(path) < 0)
			message("unlink %s; %m",path);
	}
    }
    *--endp = 0;
    return 0;
}

