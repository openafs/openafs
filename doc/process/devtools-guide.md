# Developer Tool Guide

This guide provides suggested settings or configurations for various developer
tools.

## Code formatting

Since historic compliance with the style has been poor, caution is needed when
operating on existing files.  It is often suitable to do an initial style
cleanup commit before making sweeping changes to a given file, and otherwise
try to accommodate the prevailing style in the file when making minor changes.

Formatting tools may not match or preserve the proper coding style (see any
cautions for the specific tools).  After running a formatting tool, you may need
to manually restore or modify the formatting to match the code-style-guide.

The OpenAFS code style is described in [code-style-guide](code-style-guide.md).

### GNU indent options

If you have GNU indent 2.2.10 or later, the following options can format new
files that will closely match the [code-style-guide](code-style-guide.md) (see
below for cautions when using indent).

    -nbad -nbfda -bap -nbc -bbo -br -ce -cdw -brs -ncdb -cp1 -ncs -di2 -ndj
    -nfc1 -nfca -lp -npcs -nprs -psl -sc -nsob -lps -l200 -i4 -ts8 -il1

The options correspond to:

* (-nbad) Do not require a blank line after variable declarations
* (-nbfda) Do not put each argument in a function's declaration on a separate
  line
* (-bap) Require a blank line after each procedure's body
* (-nbc) Do not require a newline after commas in variable declarations
* (-bbo) Prefer breaking long lines before boolean operators `&&` and `||`
* (-br) Format braces with the opening brace on the same line as the condition
* (-ce) Cuddle 'else' keywords on the same line as the preceding `}`
* (-cdw) Cuddle 'while' (of `do {} while ()`) keywords with preceding `}`
* (-brs) Put the opening '{' on the same line as the 'struct' keyword
* (-ncdb) Do not require comment delimiters (`/*` and `*/`) to always be on
  their own lines
* (-cp1) Put comments one space after preprocessor `#else`, `#elif` and `#endif`
  directives
* (-ncs) Do not use a space after a cast operator
* (-di2) Place variable declarations immediately after (with one space
  separator) the type statement
* (-ndj) For comments after declarations, do not left justify them behind the
  declarations
* (-nfc1) Do not format comments in the first column as normal (i.e., allow
  them in contexts where comments would otherwise be indented)
* (-nfca) Do not format any comments
* (-lp) Line up parentheses (on subsequent lines)
* (-npcs) Do not put a space after the function name in function calls
* (-nprs) Do not put a space after every `(` and before every `)`
* (-psl) Put the return type of a function definition on its own line
* (-sc) Use a `*` character at the left side of multiline comments
* (-nsob) Keep optional blank lines
* (-lps) Leave preprocessor spaces between `#` and the preprocessor directive
* (-l200) Wrap lines over 200 characters
* (-i4) Indentation is four spaces
* (-ts8) The tab stop is 8 columns
* (-il1) Indent labels one space

#### Cautions with indent

GNU indent cannot fully match the style as defined in
[code-style-guide](code-style-guide.md), and can break existing formatting.

* Empty loop bodies are left as-is
* While preprocessor spacing is preserved (-lps), indent does not correct
  the existing spacing
* Trailing comments are aligned to a fixed column with tabs
* Lines broken by hand can be rejoined
* Spacing inside initializer braces, e.g. `{ .a = 1 }` becomes `{.a = 1 }`
* Braces are not added
* Multi-line comments aren't put into the required format
