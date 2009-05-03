/*
 * definitions common to the source files of the error table compiler
 */

enum lang {
    lang_C,			/* ANSI C (default) */
    lang_KRC,			/* C: ANSI + K&R */
    lang_CPP			/* C++ */
};

int count_of_tables;		/* count of tables */
char *cot_suffix();		/* convert to table suffix */
int debug;			/* dump debugging info? */
char *filename;			/* error table source */
enum lang language;
const char *whoami;
/* prototypes for functions from the yacc/lex generated src */
int yyparse(void);
