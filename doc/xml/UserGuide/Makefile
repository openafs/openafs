BOOK = auusg000.xml
IDX = auusg013.xml
SRCS = $(BOOK) auusg003.xml auusg004.xml auusg005.xml auusg006.xml auusg007.xml \
	auusg008.xml auusg009.xml auusg010.xml auusg011.xml auusg012.xml
FLAGS = -e no-idref

all: pdf html

html: $(SRCS)
	docbook2html $(FLAGS) -d mystyle.dsl#html $(BOOK)

pdf: $(SRCS)
	docbook2pdf $(FLAGS) -d mystyle.dsl#print $(BOOK)

index:
	@echo "Building index..."
	@/usr/bin/collateindex.pl -N -o $(IDX)
	@jw -d mystyle.dsl#html -V html-index -o tmpindex $(BOOK)
	@/usr/bin/collateindex.pl -g -o $(IDX) tmpindex/HTML.index
	@rm -rf tmpindex

check:
	xmllint --noout --valid $(BOOK)
