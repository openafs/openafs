# Makefile to build the AFS QuickStart guide for Unix.
#
# This makefile assumes that various utilities are available on the system.
# On Debian etch, installing the packages:
#
#     docbook-utils
#     docbook-dsssl
#     libxml2-utils
#
# gave me all the utilities needed.  You may need to install fewer packages as
# several depend on the others.

BOOK = auqbg000.xml
IDX = auqbg009.xml
SRCS = $(BOOK) auqbg003.xml auqbg004.xml auqbg005.xml auqbg006.xml \
	auqbg007.xml auqbg008.xml
FLAGS = -e no-idref

all: pdf html

html: $(SRCS) $(IDX)
	docbook2html $(FLAGS) -d mystyle.dsl\#html $(BOOK)

pdf: $(SRCS) $(IDX)
	docbook2pdf $(FLAGS) -d mystyle.dsl\#print $(BOOK)

$(IDX): $(SRCS)
	@echo "Building index..."
	/usr/bin/collateindex.pl -N -o $(IDX)
	jw -d mystyle.dsl\#html -V html-index -o tmpindex $(BOOK)
	/usr/bin/collateindex.pl -g -o $(IDX) tmpindex/HTML.index
	rm -rf tmpindex

check:
	xmllint --noout --valid $(BOOK)

clean:
	rm -f *.html *.pdf $(IDX)
