BOOK = auqbg000.xml
IDX = auqbg009.xml
SRCS = $(BOOK) auqbg003.xml auqbg004.xml auqbg005.xml auqbg006.xml auqbg007.xml auqbg008.xml
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
