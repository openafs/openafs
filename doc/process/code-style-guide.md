# Notes on Coding Standards/Requirements for OpenAFS Source

This document covers the rules for coding style, lists the dependencies
needed to build from source, covers how we use git and gerrit for
development and code review, gives guidelines for what we expect code
review to cover, and discusses how we handle compiler warnings.

# Prototyping and Style

We have an official style for C code.  Please use it.  If you have GNU indent
2.2.9 or later you can format new files for this style with the following
options:

    -npro -nbad -bap -nbc -bbo -br -ce -cdw -brs -ncdb -cp1 -ncs -di2 -ndj
    -nfc1 -nfca -i4 -lp -npcs -nprs -psl -sc -nsob -ts8

In prose, the indent options correspond to:

* (-npro) Do not read .indent.pro files
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

The style for non-C code varies.  Makefiles must use literal tabs to
indent rule bodies, but use spaces for any additional indentation
needed.  Plain text files (such as this one) should use spaces for
indentation, with a 4-space indent as the default when one is needed.
Other cases will be added here as they are encountered and a consensus
determined for how to handle them.

Prototypes for all source files in a given dir DDD should be placed
in the file `DD/DDD_prototypes.h`. All externally used (either API
or used by other source files) routines and variables should be
prototyped in this file.

The prototypes should be a full prototype, with argument and return
types. (That is, they should not generate a warning with
`gcc -Wstrict-prototypes`.)

Format of the prototype files should look like:

    Standard Copyright Notice

    #ifndef AFS_SRC_DDD_PROTO_H
    #define AFS_SRC_DDD_PROTO_H

    /* filename.c */
    prototypes

    /* filename.c */
    prototypes

    #endif /* AFS_SRC_DDD_PROTO_H */

In most of the existing prototypes, the define is `DDD_PROTOTYPES_H`, which is
probably ok as well.

The declaration of the routines should be done in ANSI style. If at some
later date, it is determined that prototypes don't work on some platform
properly, we can use ansi2knr during the compile.

    rettype
    routine(argtype arg)
    {

    }

All routines should have a return type specified, void if nothing returned,
and should have `(void)` if no arguments are taken.

Header files should not contain macros or other definitions unless they
are used across multiple source files.

All routines should be declared static if they are not used outside that
source file.

Compiles on gcc-using machines should strive to handle using
`-Wstrict-prototypes -Werror` (this may take a while).

Routines shall be defined in source prior to use if possible, and
prototyped in block at top of file if static.

API documentation in the code should be done using Qt-style Doxygen
comments.

If you make a routine or variable static, be sure and remove it from
the AIX .exp files.

It's recommended to configure with `--enable-checking` to activate the
compiler warning flags that the codebase complies with.

Multiple line comment blocks should begin with only `/*` on one line and
end with only `*/` on one line.

Example:

    /*
     * Multiple line comment blocks should be formatted
     * like this example.
     */

Always use braces for the bodies of conditionals and loops.  This makes
it easier to follow the logic flow for complicated nested constructs,
and reduces the risk of bugs.

Use:

    if (some_condition) {
        do_some_action();
    } else {
        do_something_else();
    }

    while (some_condition) {
        do_something();
    }

Instead of:

    if (some_condition)
        do_some_action();

    while (some_condition)
        do_something();

In switch statements, to fall through from one case statement to another, use
`AFS_FALLTHROUGH` to mark the intentional fall through.  Do not use fall through
comments (e.g. `/* fallthrough */`), as some compilers do not recognize them and
will flag the case statement with an implied fallthrough warning.

Use:

    switch (x) {
    case 1:
        do_something();
        AFS_FALLTHROUGH;
    case 2:
        do_something_else();
        AFS_FALLTHROUGH;
    default:
        do_some_action();
    }

Instead of using fallthrough comments:

    switch (x) {
    case 1:
        do_something();
        /* fallthrough */
    case 2:
        do_something_else();
        /* fallthrough */
    default:
        do_some_action();
    }

Or not marking the fall through:

    switch (x) {
    case 1:
        do_something();
    case 2:
        do_something_else();
    default:
        do_some_action();
    }
