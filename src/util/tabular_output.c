/*
 * Copyright (c) 2010, Christof Hanke,
 * RZG, Max-Planck-Institut f. Plasmaphysik.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include <afs/afsutil.h>
#include <afs/tabular_output.h>

/* private structures */

struct util_TableRow {
    char **CellContents;
};

struct util_Table {
    int Type;
    int numColumns,sortByColumn;
    int numRows, numAllocatedRows;
    int *ColumnWidths;
    char **ColumnHeaders;
    int  *ColumnContentTypes;
    int RowLength; /* number of character per Row */
    /* Basic subentities */
    struct util_TableRow *Header;
    struct util_TableRow **Body;
    struct util_TableRow *Footer;
    /* output methods provided for this table */
    int (*printHeader) (struct util_Table *Table);
    int (*printFooter) (struct util_Table *Table);
    int (*printBody) (struct util_Table *Table);
    int (*printRow) (struct util_Table *, struct util_TableRow *);
};

/* private functions */

struct util_TableRow* newTableRow(struct util_Table *);
int printTableFooter_CSV(struct util_Table* Table);
int printTableHeader_CSV(struct util_Table* Table);
int printTableRow_CSV(struct util_Table* Table, struct util_TableRow *aTableRow);
int printTableFooter_ASCII(struct util_Table* Table);
int printTableHeader_ASCII(struct util_Table* Table);
int printTableRow_ASCII(struct util_Table* Table, struct util_TableRow *aTableRow);
int printTableFooter_HTML(struct util_Table* Table);
int printTableHeader_HTML(struct util_Table* Table);
int printTableRow_HTML(struct util_Table* Table, struct util_TableRow *aTableRow);
int findRowIndex(struct util_Table* Table, struct util_TableRow *aRow);
int do_setTableRow(struct util_Table *Table, struct util_TableRow *aRow, char **Contents);


/*
Public Interface
*/

int
util_setTableBodyRow(struct util_Table *Table, int RowIndex, char **Contents) {
    struct util_TableRow *aRow;

    if (RowIndex >= Table->numRows) {
        return -1;
    }
    aRow=Table->Body[RowIndex];
    return do_setTableRow(Table,aRow,Contents);
}

int util_setTableFooter(struct util_Table * Table, char ** Contents) {
    if (Table->Footer != NULL)
        Table->Footer = newTableRow(Table);
    return do_setTableRow(Table,Table->Footer,Contents);
}


int util_setTableHeader(struct util_Table *Table, char ** Contents) {
    return do_setTableRow(Table,Table->Header,Contents);
}

int
util_addTableBodyRow(struct util_Table *Table, char **Contents) {
    struct util_TableRow *aRow;
    int indx,i,row,col;
    int thisRowLength=0;

    /* Allocate more Rows if required. */
    if (Table->numRows >= Table->numAllocatedRows) {
        Table->numAllocatedRows += UTIL_T_NUMALLOC_ROW;
        Table->Body=realloc(Table->Body,\
                    Table->numAllocatedRows*sizeof(struct util_TableRow*));
        for (i=0;i<UTIL_T_NUMALLOC_ROW;i++) {
            Table->Body[Table->numRows+i]=newTableRow(Table);
        }
    }
    aRow=newTableRow(Table);
    do_setTableRow(Table,aRow,Contents);
    if (Table->sortByColumn >= 0)  {
        indx=findRowIndex(Table,aRow);
        for (row=Table->numRows;row>indx;row--) {
            for (col=0;col<Table->numColumns;col++) {
                 strncpy(Table->Body[row]->CellContents[col],
                         Table->Body[row-1]->CellContents[col],
                         UTIL_T_MAX_CELLCONTENT_LEN);
            }
        }
    } else {
      indx=Table->numRows;
    }
    Table->numRows += 1;
    for (i=0;i<Table->numColumns;i++) {
        strncpy(Table->Body[indx]->CellContents[i],Contents[i],\
                UTIL_T_MAX_CELLCONTENT_LEN);
        thisRowLength += min(strlen(Contents[i]),UTIL_T_MAX_CELLCONTENT_LEN);
    }
    if (thisRowLength > Table->RowLength)
        Table->RowLength = thisRowLength;
    return Table->numRows-1;
}

int
util_printTableBody(struct util_Table *Table) {
    int i;

    for (i=0;i<Table->numRows;i++)
	Table->printRow(Table,Table->Body[i]);
    return 0;
}

int
util_printTable(struct util_Table *Table) {
    Table->printHeader(Table);
    Table->printBody(Table);
    Table->printFooter(Table);
    return 0;
}

int
util_printTableHeader(struct util_Table *Table) {
    Table->printHeader(Table);
    return 0;
}

int
util_printTableFooter(struct util_Table *Table) {
    Table->printFooter(Table);
    return 0;
}

/* private functions */

int
do_setTableRow(struct util_Table *Table, struct util_TableRow *aRow, char **Contents) {
    int i;
    int thisRowLength=0;
    if ( Contents == NULL )
        return -1;
    for (i=0;i<Table->numColumns;i++) {
        strcpy(aRow->CellContents[i],Contents[i]);
        thisRowLength += min(strlen(Contents[i]),UTIL_T_MAX_CELLCONTENT_LEN);
    }
    if (thisRowLength > Table->RowLength)
        Table->RowLength = thisRowLength;
    return 0;
}


/* ASCII output functions */

int
printTableRow_ASCII(struct util_Table *Table, struct util_TableRow *aRow) {
    int i;

    if (!aRow)
	return 1;

    printf("%c",UTIL_T_CELLSEPARATOR);

    for (i=0;i< Table->numColumns-1;i++) {
        if ( Table->ColumnContentTypes[i] == UTIL_T_CONTENTTYPE_STRING)
            printf("%-*s%c",Table->ColumnWidths[i],aRow->CellContents[i],\
                   UTIL_T_CELLSEPARATOR);
        else
            printf("%*s%c",Table->ColumnWidths[i],aRow->CellContents[i],\
                   UTIL_T_CELLSEPARATOR);
    }
    if ( Table->ColumnContentTypes[i] == UTIL_T_CONTENTTYPE_STRING)
        printf("%-*s %c\n",Table->ColumnWidths[i],aRow->CellContents[i],\
               UTIL_T_CELLSEPARATOR);
    else
        printf("%*s %c\n",Table->ColumnWidths[i],aRow->CellContents[i],UTIL_T_CELLSEPARATOR);
    return 0;
}

int
printTableHeader_ASCII(struct util_Table *Table) {
    int i;

    printf("%c",UTIL_T_CELLSEPARATOR);
    for(i=0;i<Table->RowLength;i++)
        printf("%c",UTIL_T_ROWSEPARATOR);
    printf("%c\n",UTIL_T_CELLSEPARATOR);

    printTableRow_ASCII(Table,Table->Header);

    printf("%c",UTIL_T_CELLSEPARATOR);
    for(i=0;i<Table->RowLength;i++)
        printf("%c",UTIL_T_ROWSEPARATOR);
    printf("%c",UTIL_T_CELLSEPARATOR);
    printf("\n");
    return 0;
}


int
printTableFooter_ASCII(struct util_Table *Table) {
    int i;

    printf("%c",UTIL_T_CELLSEPARATOR);
    for(i=0;i<Table->RowLength;i++)
        printf("%c",UTIL_T_ROWSEPARATOR);
    printf("%c",UTIL_T_CELLSEPARATOR);
    printf( "\n");
    if (Table->Footer) {
        printTableRow_ASCII(Table,Table->Footer);
        printf("%c",UTIL_T_CELLSEPARATOR);
        for(i=0;i<Table->RowLength;i++)
            printf("%c",UTIL_T_ROWSEPARATOR);
        printf("%c",UTIL_T_CELLSEPARATOR);
        printf( "\n");
    }
    return 0;
}

/* HTML output functions */

int
printTableRow_HTML(struct util_Table *Table, struct util_TableRow *aRow) {
    int i;

    if (!aRow)
	return 1;

    if (aRow == Table->Header)
	printf("\t\t<th>\n");
    else
	printf("\t\t<tr>\n");

    for (i=0;i< Table->numColumns;i++) {
        printf("\t\t<td>");
        printf("%s",aRow->CellContents[i]);
        printf("\t\t</td>\n");
    }
    if (aRow == Table->Header)
	printf("\t\t</th>\n");
    else
	printf("\t\t</tr>\n");
    printf("\n");
    return 0;
}

int
printTableFooter_HTML(struct util_Table *Table) {

    printf("</tbody>\n");
    if (Table->Footer) {
        printf("<tfooter>\n");
        printTableRow_HTML(Table,Table->Footer);
        printf("</tfooter>\n");
    }
    printf("</table>\n");
    return 0;
}

int
printTableHeader_HTML (struct util_Table *Table) {

    printf("<table>\n");
    printf("<thead>\n");
    printTableRow_HTML(Table,Table->Header);
    printf("</thead>\n");
    printf("<tbody>\n");
    return 0;
}


/* CSV output functions */

int
printTableRow_CSV(struct util_Table *Table, struct util_TableRow *aRow) {
    int i;

    if (!aRow)
	return 1;
    for (i=0;i<Table->numColumns-1;i++) {
        printf("%s,",aRow->CellContents[i]);
    }
    printf("%s\n",aRow->CellContents[i]);
    return 0;
}

int
printTableHeader_CSV (struct util_Table *Table) {
    return printTableRow_CSV(Table,Table->Header);
}

int
printTableFooter_CSV (struct util_Table *Table) {
    return printTableRow_CSV(Table,Table->Footer);
}


/* Constructors */

char **
util_newCellContents(struct util_Table* Table) {
    char **CellContents=NULL;
    int i;

    if ( (CellContents=malloc( sizeof(char *) * Table->numColumns))== NULL ) {
        fprintf(stderr,"Internal Error. Cannot allocate memory for new CellContents-array.\n");
        exit(EXIT_FAILURE);
    }
    for (i=0;i<Table->numColumns;i++) {
        if ( (CellContents[i]=malloc(UTIL_T_MAX_CELLCONTENT_LEN)) == NULL)  {
            fprintf(stderr,\
                    "Internal Error. Cannot allocate memory for new CellContents-array.\n");
            exit(EXIT_FAILURE);
        }
        CellContents[i][0]='\0';
    }
    return CellContents;
}


struct util_Table*
util_newTable(int Type, int numColumns, char **ColumnHeaders, int *ColumnContentTypes, int *ColumnWidths, int sortByColumn) {
    struct util_Table *Table=NULL;
    int i;

    if ( (Table=malloc(sizeof(struct util_Table))) == NULL) {
          fprintf(stderr,\
                  "Internal Error. Cannot allocate memory for new Table.\n");
          exit(EXIT_FAILURE);
    }
    Table->Type=Type;
    Table->numColumns=numColumns;
    Table->numRows=0;
    Table->numAllocatedRows=0;
    if (sortByColumn < 0 || sortByColumn > numColumns) {
        fprintf(stderr,"Invalid Table Sortkey: %d.\n", sortByColumn);
	errno=EINVAL;
	free(Table);
	return NULL;
    }
    if (sortByColumn > 0 )
        Table->sortByColumn=sortByColumn-1; /* externally, we treat the first
                                             column as 1, internally as 0 */
    Table->ColumnHeaders=ColumnHeaders;
    Table->ColumnContentTypes=ColumnContentTypes;
    Table->ColumnWidths=ColumnWidths;
    Table->RowLength=0;
    for (i=0; i< numColumns;i++)
        Table->RowLength += ColumnWidths[i]+1;
    switch (Table->Type) {
        case UTIL_T_TYPE_ASCII :
                Table->printHeader=printTableHeader_ASCII;
		Table->printFooter=printTableFooter_ASCII;
		Table->printRow=printTableRow_ASCII;
                break;
        case UTIL_T_TYPE_CSV :
                Table->printHeader=printTableHeader_CSV;
		Table->printFooter=printTableFooter_CSV;
		Table->printRow=printTableRow_CSV;
                break;
        case UTIL_T_TYPE_HTML :
                Table->printHeader=printTableHeader_HTML;
		Table->printFooter=printTableFooter_HTML;
		Table->printRow=printTableRow_HTML;
                break;
        default :
                fprintf(stderr,"Error. Invalid TableType: %d.\n", Table->Type);
		free(Table);
		errno=EINVAL;
                return NULL;
    }
    Table->printBody=util_printTableBody;
    Table->Header=newTableRow(Table);
    do_setTableRow(Table,Table->Header,ColumnHeaders);
    Table->Body=NULL;
    Table->Footer=NULL;
    return Table;
}


/* private Constructors */

struct util_TableRow*
newTableRow(struct util_Table* Table) {
    struct util_TableRow *aRow =NULL;

    if ( (aRow = malloc(sizeof(struct util_TableRow))) == NULL) {
        fprintf(stderr,\
                "Internal Error. Cannot allocate memory for new TableRow.\n");
        exit(EXIT_FAILURE);
    }
    aRow->CellContents=util_newCellContents(Table);
    return aRow;
}

int
freeTableRow( struct util_Table* Table, struct util_TableRow *aRow) {
    int i;

    for (i=0;i<Table->numColumns;i++) {
        free(aRow->CellContents[i]);
    }
    free(aRow->CellContents);
    return 0;
}

int
util_freeTable(struct util_Table *Table) {
    int i;

    freeTableRow(Table, Table->Header);
    freeTableRow(Table, Table->Footer);
    for(i=0;i<Table->numRows;i++) {
        freeTableRow(Table, Table->Body[i]);
    }
    free(Table);
    return 0;
}


afs_int64
compareBodyRow(struct util_Table *Table, int RowIndx, struct util_TableRow *aRow) {

    afs_int64 value1,value2;
    if (Table->ColumnContentTypes[Table->sortByColumn] == UTIL_T_CONTENTTYPE_STRING) {
        return strncmp(Table->Body[RowIndx]->CellContents[Table->sortByColumn],\
               aRow->CellContents[Table->sortByColumn],UTIL_T_MAX_CELLCONTENT_LEN);
    } else {
        util_GetInt64(Table->Body[RowIndx]->CellContents[Table->sortByColumn],\
               &value1);
        util_GetInt64(aRow->CellContents[Table->sortByColumn],&value2);
        return ( value1 - value2 );
    }
}

/* find correct index for new row by bi-secting the table */
int
findRowIndex(struct util_Table* Table, struct util_TableRow *aRow){
    int cmp,lower,middle,upper;

    /* empty Table */
    if (Table->numRows == 0)  {
        return 0;
    }
    /* Entry smaller than smallest so far */
    if (compareBodyRow(Table,0,aRow) > 0)  {
        return 0;
    }
    /* Entry larger than largest so far */
    if (compareBodyRow(Table,Table->numRows-1,aRow) < 0)  {
        return Table->numRows;
    }

    lower =  0;
    upper= Table->numRows-1;
    do {
        middle=(upper-lower)/2+lower;
        cmp=compareBodyRow(Table,middle,aRow);
	if (cmp > 0)  {
            upper=middle;
        }
        if (cmp < 0)  {
            lower=middle;
        }
        if (cmp == 0) {
            return middle;
        }
        if (upper - lower < 2) {
            if ( compareBodyRow(Table,lower,aRow) < 0 )
                return upper;
            else
                return lower;
        }
    }  while(1);
    AFS_UNREACHED(return(0));
}
