#ifndef _TABULAR_OUTPUT_H
#define  _TABULAR_OUTPUT_H

#define UTIL_T_MAX_CELLS 20
#define UTIL_T_MAX_CELLCONTENT_LEN 30
#define UTIL_T_TYPE_ASCII 0
#define UTIL_T_TYPE_CSV 1
#define UTIL_T_TYPE_HTML 2
#define UTIL_T_TYPE_MAX 2
#define UTIL_T_CELLSEPARATOR '|'
#define UTIL_T_ROWSEPARATOR '-'
#define UTIL_T_NUMALLOC_ROW 10
#define UTIL_T_CONTENTTYPE_STRING 0
#define UTIL_T_CONTENTTYPE_NUMERIC 1

/* private structures */

struct util_Table;

/* public accessor functions */


extern struct util_Table* util_newTable(int Type, int numColumns,
              char **ColumnHeaders, int *ColumnContentTypes,
              int *ColumnWidths, int sortByColumn);
extern char ** util_newCellContents(struct util_Table*);
extern int util_printTableHeader(struct util_Table *Table);
extern int util_printTableBody(struct util_Table *Table);
extern int util_printTableFooter(struct util_Table *Table);
extern int util_printTable(struct util_Table *Table);
extern int util_addTableBodyRow(struct util_Table *Table, char **Contents);
extern int util_setTableBodyRow(struct util_Table *Table, int RowIndex,
                        char **Contents);
extern int util_setTableHeader(struct util_Table *Table, char **Contents);
extern int util_setTableFooter(struct util_Table *Table, char **Contents);
extern int util_freeTable(struct util_Table* Table);

#define UTIL_T_TYPE_HELP "output-format of table: 0: ASCII, 1: CSV, 2 : HTML"
#define UTIL_T_SORT_HELP "table-column to sort"

#endif /* _TABULAR_OUTPUT_H */
