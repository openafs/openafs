######################################################################
# 		Copyright IBM Corporation 1987
# 
#                       All Rights Reserved
# 
# Permission to use, copy, modify, and distribute this software and its 
# documentation for any purpose and without fee is hereby granted, 
# provided that the above copyright notice appear in all copies and that
# both that copyright notice and this permission notice appear in 
# supporting documentation, and that the name of IBM not be
# used in advertising or publicity pertaining to distribution of the
# software without specific, written prior permission.  
# 
# IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
# ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
# IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
# ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
# WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
# ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
# SOFTWARE.
######################################################################

# Make modified install
SHELL = /bin/sh
COMPONENT=pinstall
include ../config/Makefile.${SYS_NAME}

LIBS= ${XLIBS}
CFLAGS= ${OPTMZ} ${XCFLAGS}

include ../config/Makefile.version

pinstall: install.c  AFS_component_version_number.c 
	case ${SYS_NAME} in \
                sgi_64 | sgi_65) \
                        ${CC} -g -I${DESTDIR}include ${CFLAGS} install.c -o pinstall ${LIBS} ;; \
                sgi_* ) \
                        $(CC) -g -I${DESTDIR}include ${CFLAGS} install.c -o pinstall ${LIBS} -lmld ;; \
                *) \
                        $(CC) -g -I${DESTDIR}include ${CFLAGS} install.c -o pinstall ${LIBS} ;; \
        esac
	- [ -d test ] && ( cd test; $(MAKE) )
	- [ -d test ] && ./test/test_install

${DESTDIR}bin/install: pinstall
	./pinstall -f pinstall ${DESTDIR}bin/install

# Note: Some rule needs to appear here so that OSF's make does not try to
# compile install from install.c. A ";" will not work since other platforms
# complain. That's why we use 'echo'.
install: ${DESTDIR}bin/install
	@echo -n

noversion: install

kinstall ukinstall webinstall:
	@echo No kernel source here.

system: install

clean:
	rm -f pinstall AFS_component_version_number.c



