/*
 *	(C) Copyright 10/17/86 by Carnegie Mellon University
 */
#include <stdio.h>
#include <unistd.h>

extern char *malloc();

#define	maxinputdepth	16
#define	maxlinesize	1024
#define	macrohashsize	1023
#define	macronamesize	32
#define	maxnestdepth	128

#define	inline	xxinline

struct nest {
    int status;
};

struct file {
    char *name;
    FILE *stream;
    int lineno;
};

struct macro {
    struct macro *next;
    enum macromode { rdwr, rdonly } mode;
    char *name;
    char *value;
};

static stripcomments = 0;
static specialchar = '%';
static struct nest nests[maxnestdepth];
static struct nest *nestp = &nests[0];
static struct nest *lastnestp = &nests[maxnestdepth - 1];

static struct file files[maxinputdepth];
static struct file *lastfilep = &files[maxinputdepth - 1];
static struct file *filep = &files[0];

static char inline[maxlinesize];
static char outline[maxlinesize];
static struct macro *macrohashtable[macrohashsize];

static
error(a0, a1)
     char *a0, *a1;
{
    fprintf(stderr, a0, a1);
    putc('\n', stderr);
    exit(1);
}

static
fileerror(a0, a1)
     char *a0, *a1;
{
    fprintf(stderr, "%s; line %d: ", filep->name, filep->lineno);
    error(a0, a1);
}

static char *
strsav(s)
     char *s;
{
    char *p;

    if ((p = malloc(strlen(s) + 1)) == NULL)
	error("Out of Memory");
    strcpy(p, s);
    return p;
}

static struct macro **
macrolookup(name)
     char *name;
{
    register struct macro **mpp, *mp;
    register char *cp;
    register unsigned hv;

    for (cp = name, hv = 0; *cp; hv += *cp++);
    mpp = &macrohashtable[hv % macrohashsize];
    while ((mp = *mpp) && strcmp(mp->name, name))
	mpp = &mp->next;
    return mpp;
}

static
macroundefine(name)
     char *name;
{
    register struct macro **mpp, *mp;

    mpp = macrolookup(name);
    if (mp = *mpp) {
	*mpp = mp->next;
	free(mp->value);
	free(mp->name);
	free(mp);
    }
}

static
macrodefine(name, value, mode)
     char *name;
     char *value;
     enum macromode mode;
{
    register struct macro **mpp, *mp;

    mpp = macrolookup(name);
    if (mp = *mpp) {
	if (mp->mode == rdonly)
	    return;
	free(mp->value);
    } else {
	if ((mp = (struct macro *)malloc(sizeof(struct macro))) == 0)
	    error("Out of memory");
	mp->name = strsav(name);
	mp->next = 0;
	*mpp = mp;
    }
    mp->mode = mode;
    mp->value = strsav(value);
}


static char *
macroexpand(dst, src)
     register char *dst, *src;
{
    char name[macronamesize];
    register char *np;
    register struct macro *mp;

    while (*src) {
	if (*src != '$') {
	    *dst++ = *src++;
	    continue;
	}
	src++;
	if (*src == '$') {
	    *dst++ = '$';
	    src++;
	    continue;
	}
	np = name;
	if (*src == '{' || *src == '(') {
	    src++;
	    while (*src) {
		if (*src == '}' || *src == ')') {
		    src++;
		    break;
		}
		if (np >= &name[macronamesize])
		    src++;
		else
		    *np++ = *src++;
	    }
	} else {
	    *np++ = *src++;
	}
	*np = 0;
	if (mp = *macrolookup(name))
	    dst = macroexpand(dst, mp->value);
    }
    *dst = 0;
    return dst;
}



static
readline(line)
     char *line;
{
    while (filep >= &files[0]) {
	filep->lineno++;
	if (fgets(line, maxlinesize, filep->stream) != NULL)
	    return -1;
	if (fclose(filep->stream) == EOF)
	    error("Error closing %s", filep->name);
	free(filep->name);
	if (filep == &files[0])
	    return 0;
	filep--;
    }
    return 0;
}

static
writeline(line)
     char *line;
{
    fputs(line, stdout);
}


static
directive(what)
     char *what;
{
    char *arg[3], *cp;
    int n;

    if (*what++ != specialchar)
	return nestp->status;
    if (cp = strrchr(what, '\n'))
	*cp = 0;
    for (n = 0; n < 2; n++) {
	while (*what == ' ' || *what == '\t')
	    what++;
	arg[n] = what;
	while (*what != ' ' && *what != '\t' && *what != 0)
	    what++;
	if (*what)
	    *what++ = 0;
    }
    while (*what == ' ' || *what == '\t')
	what++;
    arg[2] = what;
    if (strcmp(arg[0], "ifdef") == 0) {
	if (nestp == lastnestp)
	    fileerror("If Depth overflow");
	if (nestp->status == 2 || nestp->status == 1) {
	    nestp++;
	    nestp->status = 2;
	    return 1;
	}
	nestp++;
	nestp->status = (*macrolookup(arg[1])) ? 0 : 1;
	return 1;
    }
    if (strcmp(arg[0], "ifndef") == 0) {
	if (nestp == lastnestp)
	    fileerror("If Depth overflow");
	if (nestp->status == 2 || nestp->status == 1) {
	    nestp++;
	    nestp->status = 2;
	    return 1;
	}
	nestp++;
	nestp->status = (*macrolookup(arg[1])) ? 1 : 0;
	return 1;
    }
    if (strcmp(arg[0], "else") == 0) {
	if (nestp->status == 2)
	    return 1;
	if (nestp == &nests[0])
	    fileerror("If less else");
	nestp->status = nestp->status ? 0 : 1;
	return 1;
    }
    if (strcmp(arg[0], "endif") == 0) {
	if (nestp == &nests[0])
	    fileerror("If less endif");
	nestp--;
	return 1;
    }
    if (nestp->status)
	return 1;
    if (strcmp(arg[0], "include") == 0) {
	if (filep == lastfilep)
	    fileerror("Include file overflow");
	filep++;
	if ((filep->stream = fopen(arg[1], "r")) == NULL) {
	    filep--;
	    fileerror("Can't open %s", arg[1]);
	}
	filep->name = strsav(arg[1]);
	filep->lineno = 0;
	return 1;
    }
    if (strcmp(arg[0], "define") == 0) {
	macrodefine(arg[1], arg[2], rdwr);
	return 1;
    }
    if (strcmp(arg[0], "undef") == 0) {
	macroundefine(arg[1]);
	return 1;
    }
    fileerror("Unknown directive %s", arg[0]);
}

expandfile(name)
     char *name;
{
    if (strcmp(name, "-") == 0) {
	filep->stream = stdin;
	filep->name = strsav("(stdin)");
    } else {
	if ((filep->stream = fopen(name, "r")) == NULL) {
	    fileerror("Can't open %s", name);
	    exit(1);
	}
	filep->name = strsav(name);
    }
    filep->lineno = 0;
    while (readline(inline)) {
	if (stripcomments) {
	    char *cp;
	    for (cp = inline; *cp != 0 && *cp != '#'; cp++)
		continue;
	    *cp = 0;
	    if (cp == inline)
		continue;
	}
	(void)macroexpand(outline, inline);
	if (directive(outline))
	    continue;
	writeline(outline);
    }
}

static
usage()
{
    fprintf(stderr, "Usage: mpp [-cC][-s][-Dname=value][-Uname][-][files]\n");
    exit(1);
}

#include "AFS_component_version_number.c"

main(argc, argv)
     int argc;
     char **argv;
{
    argv++, argc--;
    if (argc == 0)
	usage();
    while (argc > 0) {
	if (**argv == '-') {
	    if (strcmp(*argv, "-s") == 0) {
		stripcomments++;
		argv++, argc--;
		continue;
	    }
	    if (strncmp(*argv, "-c", sizeof("-c") - 1) == 0) {
		specialchar = argv[0][sizeof("-c")];
		argv++, argc--;
		continue;
	    }
	    if (strncmp(*argv, "-U", sizeof("-U") - 1) == 0) {
		macroundefine(&argv[0][sizeof("-U")]);
		argv++, argc--;
		continue;
	    }
	    if (strncmp(*argv, "-D", sizeof("-D") - 1) == 0) {
		char *cp, *cp2;

		cp = &argv[0][sizeof("-D") - 1];
		if (cp2 = strrchr(cp, '='))
		    *cp2++ = 0;
		if (cp2 == 0)
		    cp2 = "";
		macrodefine(cp, cp2, rdonly);
		argv++, argc--;
		continue;
	    }
	    if (strcmp(*argv, "-"))
		usage();
	}
	expandfile(*argv);
	argv++, argc--;
    }
    exit(0);
}
