/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)glob.c	5.7 (Berkeley) 12/14/88";
#endif /* not lint */

/*
 * C-shell glob for random programs.
 */

#include <afs/param.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>

#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#if defined (NLS) || defined (KJI)
#include <NLchar.h>
#include <NLctype.h>
#endif
/*
#ifdef MSG
#include "ftpd_msg.h" 
extern nl_catd catd;
#define MSGSTR(n,s) NLcatgets(catd,MS_FTPD,n,s) 
#else*/
#define MSGSTR(n,s) s
/*#endif*/

#define	QUOTE 0200
#define	TRIM 0177
#define	eq(a,b)		(strcmp(a, b)==0)
#define	GAVSIZ		(NCARGS/6)
#define	isdir(d)	((d.st_mode & S_IFMT) == S_IFDIR)

static	char **gargv;		/* Pointer to the (stack) arglist */
static	int gargc;		/* Number args in gargv */
static	int gnleft;
static	short gflag;
static int tglob();
char	**glob();
char	*globerr;
char	*home;
struct	passwd *getpwnam();
extern	int errno;
static	char *strspl(), *strend();
char	*malloc(), *strcpy(), *strcat();
char	**copyblk();

static	int globcnt;

char	*globchars = "`{[*?";

static	char *gpath, *gpathp, *lastgpathp;
static	int globbed;
static	char *entp;
static	char **sortbas;
#if defined(AFS_AIX32_ENV) || defined(AFS_LINUX20_ENV)
static ginit(char **);
static collect(register char *);
static acollect(register char *);
static sort();
static expand(char *);
static matchdir(char *);
static execbrc(char *, char *);
static match(char *, char *);
static amatch(char *, char *);
static Gmatch(char *, char *);
static Gcat(register char *, register char *);

int any(register int, register char *);
int blklen(register char **);
char **blkcpy(char **, register char **);
static char *strspl(register char *, register char *);
char **copyblk(register char **);
static char *strend(register char *);
int gethdir(char *);


#ifdef notdef
static addpath(char);
static rscan(register char **, int (*)(char));
int letter(register char);
int digit(register char);
void blkfree(char **);
#endif
#endif

char **
glob(v)
	register char *v;
{
	char agpath[BUFSIZ];
	char *agargv[GAVSIZ];
	char *vv[2];

	vv[0] = strcpy(malloc((unsigned)strlen(v) + 1), v);
	vv[1] = 0;
	gflag = 0;
	rscan(vv, tglob);
	if (gflag == 0)
		return (copyblk(vv));

	globerr = 0;
	gpath = agpath; gpathp = gpath; *gpathp = 0;
	lastgpathp = &gpath[sizeof agpath - 2];
	ginit(agargv); globcnt = 0;
	collect(v);
	if (globcnt == 0 && (gflag&1)) {
/*		blkfree(gargv); */
	        gargv = 0; 
		return (0);
	} else
		return (gargv = copyblk(gargv));
}

static
ginit(agargv)
	char **agargv;
{

	agargv[0] = 0; gargv = agargv; sortbas = agargv; gargc = 0;
	gnleft = NCARGS - 4;
}

static
collect(as)
	register char *as;
{
	if (eq(as, "{") || eq(as, "{}")) {
		Gcat(as, "");
		sort();
	} else
		acollect(as);
}

static
acollect(as)
	register char *as;
{
	register int ogargc = gargc;

	gpathp = gpath; *gpathp = 0; globbed = 0;
	expand(as);
	if (gargc != ogargc)
		sort();
}

static
sort()
{
	register char **p1, **p2, *c;
	char **Gvp = &gargv[gargc];

	p1 = sortbas;
	while (p1 < Gvp-1) {
		p2 = p1;
		while (++p2 < Gvp)
#if defined (NLS) || defined (KJI)
			if (NLstrcmp(*p1, *p2) > 0)
#else
			if (strcmp(*p1, *p2) > 0)
#endif
				c = *p1, *p1 = *p2, *p2 = c;
		p1++;
	}
	sortbas = Gvp;
}

static
expand(as)
	char *as;
{
	register char *cs;
	register char *sgpathp, *oldcs;
	struct stat stb;
#if defined (NLS) || defined (KJI)
	int c, c1, two_bytes;
#endif

	sgpathp = gpathp;
	cs = as;
	if (*cs == '~' && gpathp == gpath) {
		addpath('~');
#if defined (NLS) || defined (KJI)
		while (c = *++cs) {
			two_bytes = 0;
			if (NCisshift (c)) {
				two_bytes++;
				c1 = *++cs;
				_NCdec2 (c, c1, c);
				cs--; /* need to pass 1st char to addpath */
			}
			if (NCisalpha (c) || NCisdigit (c) || *cs == '-') {
				addpath (*cs);
				if (two_bytes)
					addpath (*cs++);
			}
			else{
				break;
			}
		}
#else
		for (cs++; letter(*cs) || digit(*cs) || *cs == '-';)
			addpath(*cs++);
#endif
		if (!*cs || *cs == '/') {
			if (gpathp != gpath + 1) {
				*gpathp = 0;
				if (gethdir(gpath + 1))
					globerr = "Unknown user name after ~";
				(void) strcpy(gpath, gpath + 1);
			} else
				(void) strcpy(gpath, home);
			gpathp = strend(gpath);
		}
	}
	while (!any(*cs, globchars)) {
		if (*cs == 0) {
			if (!globbed)
				Gcat(gpath, "");
			else if (stat(gpath, &stb) >= 0) {
				Gcat(gpath, "");
				globcnt++;
			}
			goto endit;
		}
#if defined (NLS) || defined (KJI)
		/* need extra call to addpath for two byte NLS char */
		if (NCisshift (*cs))
			addpath (*cs++);
#endif
		addpath(*cs++);
	}
	oldcs = cs;
	while (cs > as && *cs != '/')
		cs--, gpathp--;
	if (*cs == '/')
		cs++, gpathp++;
	*gpathp = 0;
	if (*oldcs == '{') {
		(void) execbrc(cs, ((char *)0));
		return;
	}
	matchdir(cs);
endit:
	gpathp = sgpathp;
	*gpathp = 0;
}

#ifndef AFS_LINUX20_ENV
#define dirfd(D) (D)->dd_fd
#endif

static
matchdir(pattern)
	char *pattern;
{
	struct stat stb;
	register struct dirent *dp;
	DIR *dirp;

	/*
	 * This fixes the problem of using the local
	 * path on the remote machine.  (PTM #35291).
	 */
	if (strcmp(gpath, "") == 0)
		dirp = opendir(".");
	else
		dirp = opendir(gpath);
	if (dirp == NULL) {
		if (globbed)
			return;
		goto patherr2;
	}
	if (fstat(dirfd(dirp), &stb) < 0)
		goto patherr1;
	if (!isdir(stb)) {
		errno = ENOTDIR;
		goto patherr1;
	}
	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_ino == 0)
			continue;
		if (match(dp->d_name, pattern)) {
			Gcat(gpath, dp->d_name);
			globcnt++;
		}
	}
	closedir(dirp);
	return;

patherr1:
	closedir(dirp);
patherr2:
	globerr = "Bad directory components";
}

static
execbrc(p, s)
	char *p, *s;
{
	char restbuf[BUFSIZ + 2];
	register char *pe, *pm, *pl;
	int brclev = 0;
	char *lm, savec, *sgpathp;
#if defined (NLS) || defined (KJI)
	int tc;
#endif

	for (lm = restbuf; *p != '{'; *lm++ = *p++) {
#if defined (NLS) || defined (KJI)
	    if (NCisshift (*p)) {
		*lm++ = *p++;
		if (_NCdec2(*lm, *p, *lm) == 1) {
		    lm--;
		    p--;
		}
	    }
#endif
	    continue;
	}
	for (pe = ++p; *pe; pe++)
	switch (*pe) {

	case '{':
		brclev++;
		continue;

	case '}':
		if (brclev == 0)
			goto pend;
		brclev--;
		continue;

	case '[':
		for (pe++; *pe && *pe != ']'; pe++) {
#if defined (NLS) || defined (KJI)
			if (NCisshift (*pe)) {
				tc = *p++;
				if (_NCdec2 (tc, *pe, tc) == 1) pe--;
			}
#endif
			continue;
		    }
		continue;
#if defined (NLS) || defined (KJI)
	default:
		if (NCisshift (*pe)) {
			tc = *pe++;
			if (_NCdec2 (tc, *pe, tc) == 1) pe--;
		}
	continue;
#endif

	}
pend:
	brclev = 0;
	for (pl = pm = p; pm <= pe; pm++)
#if defined (NLS) || defined (KJI)
	switch (*pm) {
#else
	switch (*pm & (QUOTE|TRIM)) {
#endif
	case '{':
		brclev++;
		continue;

	case '}':
		if (brclev) {
			brclev--;
			continue;
		}
		goto doit;

#if defined (NLS) || defined (KJI)
	case ',':
#else
	case ','|QUOTE:
	case ',':
#endif
		if (brclev)
			continue;
doit:
		savec = *pm;
		*pm = 0;
		(void) strcpy(lm, pl);
		(void) strcat(restbuf, pe + 1);
		*pm = savec;
		if (s == 0) {
			sgpathp = gpathp;
			expand(restbuf);
			gpathp = sgpathp;
			*gpathp = 0;
		} else if (amatch(s, restbuf))
			return (1);
		sort();
		pl = pm + 1;
		if (brclev)
			return (0);
		continue;

	case '[':
		for (pm++; *pm && *pm != ']'; pm++) {
#if defined (NLS) || defined (KJI)
			if (NCisshift (*pm)) {
				tc = *pm++;
				if (_NCdec2 (tc, *pm, tc) == 1) pm--;
			}
#endif
			continue;
		    }
		if (!*pm)
			pm--;
		continue;
#if defined (NLS) || defined (KJI)
	default:
		if (NCisshift (*pm)) {
			tc = *pm++;
			if (_NCdec2 (tc, *pm, tc) == 1) pm--;
		}
		continue;
#endif
       }
	if (brclev)
		goto doit;
	return (0);
}

static
match(s, p)
	char *s, *p;
{
	register int c;
	register char *sentp;
	char sglobbed = globbed;

	if (*s == '.' && *p != '.')
		return (0);
	sentp = entp;
	entp = s;
	c = amatch(s, p);
	entp = sentp;
	globbed = sglobbed;
	return (c);
}

static
amatch(s, p)
	register char *s, *p;
{
	register int scc;
	int ok, lc;
	char *sgpathp;
	struct stat stb;
	int c, cc;
#if defined (NLS) || defined (KJI)
	register int cc1, scc1;
	static int s_is2 = 0;
#endif

	globbed = 1;
	for (;;) {
#if defined (NLS) || defined (KJI)
	s_is2 = 0;
	scc = *s++;
	if (NCisshift (scc)) {
		scc1 = *s++;
		if (_NCdec2(scc, scc1, scc) == 1) 
			s--;
		else
			s_is2 = 1;
	}
#else
		scc = *s++ & TRIM;
#endif
		switch (c = *p++) {

		case '{':
#if defined (NLS) || defined (KJI)
		if (s_is2)
			return (execbrc(p -1, s - 2));
		else
#endif
			return (execbrc(p - 1, s - 1));

		case '[':
			ok = 0;
			lc = 077777;
			while (cc = *p++) {
#if defined (NLS) || defined (KJI)
				if (NCisshift (cc))
					if (_NCdec2(cc, *p, cc) == 2) p++;
#endif
				if (cc == ']') {
					if (ok)
						break;
					return (0);
				}
#if defined (NLS) || defined (KJI)
				if (cc == '[')
					if (p[1] == ':')
					{
						if (!strncmp(p,"[:alpha:]",9)) {
					   	ok |= (isascii(scc) && 
							isalpha(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:upper:]",9)) {
					   	ok |= (isascii(scc) && 
							isupper(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:lower:]",9)) {
					   	ok |= (isascii(scc) && 
							islower(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:digit:]",9)) {
					   	ok |= (isascii(scc) && 
							isdigit(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:alnum:]",9)) {
					   	ok |= (isascii(scc) && 
							isalnum(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:print:]",9)) {
					   	ok |= (isascii(scc) && 
							isprint(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:punct:]",9)) {
					   	ok |= (isascii(scc) && 
							ispunct(scc));
					   	p += 8;
					   	break;
					   	}
#ifdef KJI
						if (!strncmp(p,"[:jalpha:]",10)) {
					   	ok |= isjalpha(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jdigit:]",10)) {
					   	ok |= isjdigit(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jpunct:]",10)) {
					   	ok |= isjpunct(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jparen:]",10)) {
					   	ok |= isjparen(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jkanji:]",10)) {
					   	ok |= isjkanji(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jhira:]",9)) {
					   	ok |= isjhira(scc);
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:jkata:]",9)) {
					   	ok |= isjkata(scc);
					   	p += 8;
					   	break;
					   	}

#endif /*KJI*/
					}
				if ((cc == '-') && (lc > 0)) {
					int cu1, lcu, scu1, tco;

					cc1 = *p++;
					if (NCisshift (cc1))
						if (_NCdec2(cc1, *p, cc1) == 
								1)
							p--;
					cu1 = NCcoluniq(cc1);
					if (((tco = NCcollate(cc1)) < 0 ) &&
					   (tco = _NCxcolu(tco, &p, 0, 
							&cu1)));

					lcu = NCcoluniq(lc);
					if ( (tco = NCcollate(lc)) < 0 ) {
						char tb[3], *tbp = tb;

						tb[0] = tb[1] = tb[2] = '\0';
                                        	_NCe2(lc,tb[0], tb[1]);
						tco = _NCxcolu(tco, &tbp, 0, 
								&lcu);
				    	}
					scu1 = NCcoluniq(scc);
					if (((tco = NCcollate(scc)) < 0 ) &&
                                           (tco = _NCxcolu(tco, &s,0,&scu1)));

					/* if we have nonzero collate vals */
					if (lcu && cu1 && scu1 )
						ok += (lcu <= scu1 && 
							scu1 <= cu1);
					else
						ok += (lc == scc || 
							scc == cu1);
#else
				if (cc == '-') {
					if (lc <= scc && scc <= *p++)
						ok++;
#endif /* KJI || NLS */
				} else
					if (scc == (lc = cc))
						ok++;
			}
			if (cc == 0)
				if (ok)
					p--;
				else
					return 0;
			continue;

		case '*':
			if (!*p)
				return (1);
			if (*p == '/') {
				p++;
				goto slash;
			}
#if defined (NLS) || defined (KJI)
			if (s_is2) s--;
#endif
			s--;
			do {
				if (amatch(s, p))
					return (1);
			} while (*s++);
			return (0);

		case 0:
			return (scc == 0);

		default:
#if defined (NLS) || defined (KJI)
			if (NCisshift (c))
				if (_NCdec2(c, *p, c) == 2) p++;
#endif
			if (c != scc)
				return (0);
			continue;

		case '?':
			if (scc == 0)
				return (0);
			continue;

		case '/':
			if (scc)
				return (0);
slash:
			s = entp;
			sgpathp = gpathp;
			while (*s)
				addpath(*s++);
			addpath('/');
			if (stat(gpath, &stb) == 0 && isdir(stb))
				if (*p == 0) {
					Gcat(gpath, "");
					globcnt++;
				} else
					expand(p);
			gpathp = sgpathp;
			*gpathp = 0;
			return (0);
		}
	}
}

static
Gmatch(s, p)
	register char *s, *p;
{
	register int scc;
	int ok, lc;
	int c, cc;
#if defined (NLS) || defined (KJI)
	register int scc1, cc1;
	int s_is2;
#endif

	for (;;) {
#if defined (NLS) || defined (KJI)
		s_is2 = 0;
		scc = *s++;
		if (NCisshift (scc)) {
			scc1 = *s++;
			if (_NCdec2(scc, scc1, scc) == 1)
				s--;
			else
				s_is2 = 1;
		}
#else
		scc = *s++ & TRIM;
#endif
		switch (c = *p++) {

		case '[':
			ok = 0;
			lc = 077777;
			while (cc = *p++) {
#if defined (NLS) || defined (KJI)
				if (NCisshift (cc))
					if (_NCdec2(cc, *p, cc)  == 2) p++;
#endif
				if (cc == ']') {
					if (ok)
						break;
					return (0);
				}

#if defined (NLS) || defined (KJI)
				if (cc == '[')
					if (p[1] == ':')
					{
						if (!strncmp(p,"[:alpha:]",9)) {
					   	ok |= (isascii(scc) && 
							isalpha(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:upper:]",9)) {
					   	ok |= (isascii(scc) && 
							isupper(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:lower:]",9)) {
					   	ok |= (isascii(scc) && 
							islower(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:digit:]",9)) {
					   	ok |= (isascii(scc) && 
							isdigit(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:alnum:]",9)) {
					   	ok |= (isascii(scc) && 
							isalnum(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:print:]",9)) {
					   	ok |= (isascii(scc) && 
							isprint(scc));
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:punct:]",9)) {
					   	ok |= (isascii(scc) && 
							ispunct(scc));
					   	p += 8;
					   	break;
					   	}
#ifdef KJI
						if (!strncmp(p,"[:jalpha:]",10)) {
					   	ok |= isjalpha(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jdigit:]",10)) {
					   	ok |= isjdigit(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jpunct:]",10)) {
					   	ok |= isjpunct(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jparen:]",10)) {
					   	ok |= isjparen(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jkanji:]",10)) {
					   	ok |= isjkanji(scc);
					   	p += 9;
					   	break;
					   	}
						if (!strncmp(p,"[:jhira:]",9)) {
					   	ok |= isjhira(scc);
					   	p += 8;
					   	break;
					   	}
						if (!strncmp(p,"[:jkata:]",9)) {
					   	ok |= isjkata(scc);
					   	p += 8;
					   	break;
					   	}
#endif /* KJI */
					}
				if ((cc == '-') && (lc > 0)) {
					int cu1, scu1, lcu, tco;

					cc1 = *p++;
					if (NCisshift (cc1))
						if (_NCdec2(cc1, *p, cc1) == 
								1)
							p--;
					cu1 = NCcoluniq(cc1);
					if (((tco = NCcollate(cc1)) < 0 ) &&
					   (tco = _NCxcolu(tco, &p, 0,&cu1)));

					lcu = NCcoluniq(lc);
					if ( (tco = NCcollate(lc)) < 0 ) {
						char tb[3], *tbp = tb;

						tb[0] = tb[1] = tb[2] = '\0';
                                        	_NCe2(lc,tb[0], tb[1]);
						tco = _NCxcolu(tco, &tbp, 0, 
								&lcu);
				    	}
					scu1 = NCcoluniq(scc);
					if (((tco = NCcollate(scc)) < 0 ) &&
                                           (tco = _NCxcolu(tco, &s,0,&scu1)));

					/* if we have nonzero collate vals */
					if (lcu && cu1 && scu1 )
						ok += (lcu <= scu1 && 
							scu1 <= cu1);
					else
						ok += (lc == scc || 
							scc == cu1);
#else 
				if (cc == '-') {
					if (lc <= scc && scc <= *p++)
						ok++;
#endif /* KJI || NLS */
				} else
					if (scc == (lc = cc))
						ok++;
			}
			if (cc == 0)
				if (ok)
					p--;
				else
					return 0;
			continue;

		case '*':
			if (!*p)
				return (1);
#if defined (NLS) || defined (KJI)
			if (s_is2) s--;
#endif
			for (s--; *s; s++)
				if (Gmatch(s, p))
					return (1);
			return (0);

		case 0:
			return (scc == 0);

		default:
#if defined (NLS) || defined (KJI)
			if (NCisshift(c))
				if (_NCdec2 (c, *p, c) == 2) p++;
			if (c != scc)
#else
			if ((c & TRIM) != scc)
#endif
				return (0);
			continue;

		case '?':
			if (scc == 0)
				return (0);
			continue;

		}
	}
}

static
Gcat(s1, s2)
	register char *s1, *s2;
{
	register int len = strlen(s1) + strlen(s2) + 1;

	if (len >= gnleft || gargc >= GAVSIZ - 1)
		globerr = "Arguments too long";
	else {
		gargc++;
		gnleft -= len;
		gargv[gargc] = 0;
		gargv[gargc - 1] = strspl(s1, s2);
	}
}

static
addpath(c)
	char c;
{

	if (gpathp >= lastgpathp)
		globerr = "Pathname too long";
	else {
		*gpathp++ = c;
		*gpathp = 0;
	}
}

static
rscan(t, f)
	register char **t;
	int (*f)();
{
	register char *p, c;

	while (p = *t++) {
		if (f == tglob)
			if (*p == '~')
				gflag |= 2;
			else if (eq(p, "{") || eq(p, "{}"))
				continue;
		while (c = *p++)
			(*f)(c);
	}
}
/*
static
scan(t, f)
	register char **t;
	int (*f)();
{
	register char *p, c;

	while (p = *t++)
		while (c = *p)
			*p++ = (*f)(c);
} */

static
tglob(c)
	register char c;
{

	if (any(c, globchars))
		gflag |= c == '{' ? 2 : 1;
	return (c);
}
/*
static
trim(c)
	char c;
{

	return (c & TRIM);
} */


letter(c)
	register char c;
{

	return (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_');
}

digit(c)
	register char c;
{

	return (c >= '0' && c <= '9');
}

any(c, s)
	register int c;
	register char *s;
{

	while (*s)
		if (*s++ == c)
			return(1);
	return(0);
}
blklen(av)
	register char **av;
{
	register int i = 0;

	while (*av++)
		i++;
	return (i);
}

char **
blkcpy(oav, bv)
	char **oav;
	register char **bv;
{
	register char **av = oav;

	while (*av++ = *bv++)
		continue;
	return (oav);
}

blkfree(av0)
	char **av0;
{
	register char **av = av0;

	while (*av)
		free(*av++);
}

static
char *
strspl(cp, dp)
	register char *cp, *dp;
{
	register char *ep = malloc((unsigned)(strlen(cp) + strlen(dp) + 1));

	if (ep == (char *)0)
		fatal("Out of memory");
	(void) strcpy(ep, cp);
	(void) strcat(ep, dp);
	return (ep);
}

char **
copyblk(v)
	register char **v;
{
	register char **nv = (char **)malloc((unsigned)((blklen(v) + 1) *
						sizeof(char **)));
	if (nv == (char **)0)
		fatal("Out of memory");

	return (blkcpy(nv, v));
}

static
char *
strend(cp)
	register char *cp;
{

	while (*cp)
		cp++;
	return (cp);
}
/*
 * Extract a home directory from the password file
 * The argument points to a buffer where the name of the
 * user whose home directory is sought is currently.
 * We write the home directory of the user back there.
 */
gethdir(home)
	char *home;
{
	register struct passwd *pp = getpwnam(home);

	if (!pp || home + strlen(pp->pw_dir) >= lastgpathp)
		return (1);
	(void) strcpy(home, pp->pw_dir);
	return (0);
}
