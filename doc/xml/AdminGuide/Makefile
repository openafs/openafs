BOOK = auagd000.xml
IDX = auagd026.xml
SRCS = $(BOOK) auagd005.xml auagd006.xml auagd007.xml auagd008.xml auagd009.xml \
	auagd010.xml auagd011.xml auagd012.xml auagd013.xml auagd014.xml auagd015.xml \
	auagd016.xml auagd017.xml auagd018.xml auagd019.xml auagd020.xml auagd021.xml \
	auagd022.xml auagd023.xml auagd024.xml auagd025.xml
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
