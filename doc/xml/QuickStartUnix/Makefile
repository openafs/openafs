# Makefile to build the AFS QuickStart guide for Unix.
#
# This makefile assumes that various utilities are available on the system.
# On Debian etch, installing the packages:
#
#     docbook-utils
#     docbook-dsssl
#     docbook-xsl
#     libxml2-utils
#     xsltproc
#     dblatex
#
# gave me all the utilities needed.  You may need to install fewer packages as
# several depend on the others.
#
# HTML_XSL is possibly specific to Debian and may need to be modified on other
# systems.

BOOK     = auqbg000.xml
SRCS     = $(BOOK) auqbg003.xml auqbg004.xml auqbg005.xml auqbg006.xml \
	   auqbg007.xml auqbg008.xml appendix.xml
HTML_XSL = /usr/share/xml/docbook/stylesheet/nwalsh/html/chunk.xsl

all: pdf html

html: $(SRCS)
	xsltproc --param navig.graphics 1 \
	    --stringparam navig.graphics.path ../ $(HTML_XSL) $(BOOK)

pdf: $(SRCS)
	dblatex $(BOOK)

check:
	xmllint --noout --valid $(BOOK)

clean:
	rm -f *.html *.pdf $(IDX)
