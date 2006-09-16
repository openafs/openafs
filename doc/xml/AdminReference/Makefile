BOOK = auarf000.xml
IDX = index.xml
SRCS = $(BOOK)
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
