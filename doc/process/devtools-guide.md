# Developer Tool Guide

This guide provides suggested settings or configurations for various developer
tools.

## Code formatting

The OpenAFS C style is described in [code-style-guide](code-style-guide.md).

### GNU indent options

If you have GNU indent 2.2.9 or later you can format new files for this style
with the following options:

    -nbad -bap -nbc -bbo -br -ce -cdw -brs -ncdb -cp1 -ncs -di2 -ndj
    -nfc1 -nfca -i4 -lp -npcs -nprs -psl -sc -nsob -ts8

In prose, the indent options correspond to:

* (-nbad) Do not require a blank line after function declarations
* (-bap) Require a blank line after each procedure's body
* (-nbc) Do not require a newline after commas in function declarations
* (-bbo) Prefer breaking long lines before boolean operators `&&` and `||`
* (-br) Format braces with the opening brace on the same line as the condition
* (-ce) Cuddle 'else' keywords on the same line as the preceding `}`
* (-cdw) Cuddle 'while' (of `do {} while ()`) keywords with preceding `}`
* (-brs) Put the opening '{' on the same line as the 'struct' keyword
* (-ncbd) Do not require comment delimiters (`/*` and `*/`) to always be on
  their own lines
* (-cp1) Put comments after preprocessor directives at the first tab stop after
  the directive
* (-ncs) Do not use a space after a cast operator
* (-di2) Place variable declarations immediately after (with one space
  separator) the type statement
* (-ndj) For comments after declarations, do not left justify them behind the
  declarations
* (-nfc1) Do not format comments in the first column as normal (i.e., allow
  them in contexts where comments would otherwise be indented)
* (-nfca) Do not format any comments (redundant with the former?)
* (-i4) Indentation is four spaces
* (-lp) Line up parentheses (on subsequent lines)
* (-npcs) Do not put a space after the function name in function calls
* (-nprs) Do not put a space after every `(` and before every `)`
* (-psl) Put the return type of a function on its own line
* (-sc) Use a `*` character at the left side of multiline comments
* (-nsob) Do not allow optional blank lines
* (-ts8) The tab stop is 8 columns

Since historic compliance with the style has been poor, caution is needed when
operating on existing files.  It is often suitable to do an initial style
cleanup commit before making sweeping changes to a given file, and otherwise
try to accommodate the prevailing style in the file when making minor changes.

GNU indent does not preserve the code style used for preprocessor indentation as
described in the style guide; restore the indentation by hand after running indent.
