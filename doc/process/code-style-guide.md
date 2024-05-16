# Code Style Guide

## Non-C style

Makefiles must use literal tabs to indent rule bodies, but use spaces for any
additional indentation needed.

Plain text files should use spaces for indentation, with a 4-space indent as
the default when one is needed.

The rest of this file concerns the style used for C code, since that is most of
the code in the tree.

## C Style

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

## C Standard

All cross-platform code adheres to C89, plus a few extensions. (Or, you can
think of it as C99, minus a few features.) Some extensions we allow over C89
include:

* The `long long int` type
* Designated initializers for structs (`struct Foo val = { .field = 1 };`)
* A trailing comma for the last element of an `enum`
* The `__func__` predefined macro

The prototypes should be a full prototype, with argument and return
types. (That is, they should not generate a warning with
`gcc -Wstrict-prototypes`.)

Some post-C89 features we do not allow:

* Declaring variables in the middle of a block (all declarations must go at the
  beginning of a block)
* `static inline` (use the `static_inline` abstraction instead)

Code in platform-specific sections or source files can use additional features,
as long as you know they are supported for that platform. Linux-specific kernel
code frequently uses other features, for example.

## Prototypes

Prototypes for all source files in a given dir DDD should be placed in the file
`DDD/DDD_prototypes.h` All externally used (either API or used by other source
files) routines and variables should be prototyped in this file.

The prototypes should be a full prototype, with argument and return types.
(That is, they should not generate a warning with `gcc -Wstrict-prototypes`.)

Format of the prototype files should look like:

    /* Copyright notice */

    #ifndef AFS_SRC_DDD_PROTO_H
    #define AFS_SRC_DDD_PROTO_H

    /* filename.c */
    prototypes

    /* filename.c */
    prototypes

    #endif /* AFS_SRC_DDD_PROTO_H */

In most of the existing prototypes, the define is `DDD_PROTOTYPES_H`, which is
probably ok as well.

## Functions

The declaration of the routines should be done in ANSI style. For example:

    rettype
    routine(argtype arg)
    {
    }

All routines should have a return type specified, void if nothing returned, and
should have `(void)` if no arguments are taken.

Header files should not contain macros or other definitions unless they are
used across multiple source files.

All routines should be declared static if they are not used outside that source
file.

Compilation on gcc-using machines should strive to handle using
`-Wstrict-prototypes -Werror` (this may take a while).

Routines shall be defined in source prior to use if possible; if a static
function cannot be defined before use, declare prototypes in a block near the
top of the file.

## Comments

Do not use C++-style `//` comments.

For non-doxygen comment blocks, multi-line comment blocks should begin with
only `/*` on one line and end with only `*/` on one line. Example:

    /*
     * Multiple line comment blocks should be formatted
     * like this example.
     */

API documentation in the code should be done using Qt-style or Javadoc-style
Doxygen comments in a comment block before the implementing function. Example:

    /**
     * Foo a bar.
     *
     * @param[in] foo_in    The foo to use.
     * @param[out] bar_out  On success, set to a newly allocated bar. The
     *                      caller must free this when done.
     *
     * @returns errno error codes
     * @retval EINVAL	The given foo is invalid.
     */
    int
    foo_bar(struct foo *foo_in, struct bar **bar_out)
    {
    }

# Conditional/Loop Braces

Always use braces for the bodies of conditionals and loops. This makes it
easier to follow the logic flow for complicated nested constructs, and reduces
the risk of bugs.

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
    else
        do_something_else();

    while (some_condition)
        do_something();

If a conditional or loop has an empty body, still show the braces on separate
lines, possibly with a comment. That is, do this:

    while (do_something()) {
        /* empty body */
    }

Not this:

    while (do_something);

Nor this:

    while (do_something) {}

## Switch

In switch statements, to fall through from one case statement to another, use
`AFS_FALLTHROUGH` to mark the intentional fall through.  Do not use fall
through comments (e.g. `/* fallthrough */`), as some compilers do not recognize
them and will flag the case statement with an implied fallthrough warning.

Indent the `case` statements aligned with the parent `switch`.

Use:

    switch (x) {
    case 1:
        do_something();
        break;
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
        break;
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
	break;
    case 2:
        do_something_else();
    default:
        do_some_action();
    }

## Labels

When declaring a label, indent the label by a single space. If the label is not
indented at all, `git` and other diff-producing tools can make mistakes when
showing what function a changed line is in.

Using `goto` is perfectly fine when consolidating returning code paths from a
function.

Use:

    int
    func(void)
    {
        if (code != 0) {
            goto done;
        }
     done:
        return code;
    }

Instead of:

    int
    func(void)
    {
        if (code != 0) {
            goto done;
        }
    done:
        return code;
    }

## Comparisons

Compare integers and pointers against specific values, even if you're just
checking that they are zero or nonzero. Don't rely on the compiler to do this
for you.

Use:

    int var, *ptr;
    if (var != 0) {
        use_var();
    }
    if (ptr == NULL) {
        no_ptr();
    }

Instead of:

    int var, *ptr;
    if (var) {
        use_var();
    }
    if (!ptr) {
        no_ptr();
    }

If a value is of type `int` but actually represents a boolean value, then use
the value directly. For example:

    int foo_exists = 0;
    if (result == 0) {
        foo_exists = 1;
    }
    if (foo_exists) {
        use_foo();
    }

This helps make it immediately obvious when reading a section of code whether a
value actually represents an integer or pointer, or is a boolean value.

## NULL

Use `NULL` with pointers; never assign `0` to a pointer or compare a pointer to
`0`.

When passing `NULL` to a variadic function, you must cast it to a pointer
(typically `(void*)`). Platforms can define `NULL` to be the integer `0`, which
is normally fine when the compiler knows to cast it to a pointer. But for
variadic functions, the compiler doesn't know the type of the argument at
compile-time, and so a `0` may not get cast to a pointer, causing problems if
pointer types are larger than integers on that platform. This has actually
caused bugs!
