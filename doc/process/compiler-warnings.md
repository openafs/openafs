# Warnings

## OpenAFS Warning detection

There's been a concerted effort over the last few years, by many developers,
to reduce the number of warnings in the OpenAFS tree. In an attempt to
prevent warnings from creeping back in, we now have the ability to break the
build when new warnings appear.

This is only available for systems with gcc 4.2 or later or clang 3.2 or
later, and is disabled unless the `--enable-checking` option is supplied to
configure. Because we can't remove all of the warnings, we permit file by
file (and warning by warning) disabling of specific warnings. The
`--enable-checking=all` option prevents
this, and errors for any file containing a warning.  (At present, using
`--enable-checking=all` will error out quite early in the build, so this
is only useful for developers attempting to clean up compiler warnings.)

## Disabling warnings

If warnings are unavoidable in a particular part of the build, they may be
disabled in an number of ways.

You can disable a single warning type in a particular file by using GCC
pragmas. If a warning can be disabled with a pragma, then the switch to use
will be listed in the error message you receive from the compiler. Pragmas
should be wrapped in `IGNORE_SOME_GCC_WARNINGS`, so that they aren't used
with non-gcc compilers, and can be disabled if desired. For example:

    #ifdef IGNORE_SOME_GCC_WARNINGS
    # pragma GCC diagnostic warning "-Wold-style-definition"
    #endif

It would appear that when built with `-Werror`, the llvm clang compiler will
still upgrade warnings that are suppressed in this way to errors. In this case,
the fix is to mark that warning as ignored, but only for clang. For example:

    #ifdef IGNORE_SOME_GCC_WARNINGS
    # ifdef __clang__
    #  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    # else
    #  pragma GCC diagnostic warning "-Wdeprecated-declarations"
    # endif
    #endif

If the source cannot be changed to add a pragma, you might be able to use the
autoconf function `AX_APPEND_COMPILE_FLAGS` to create a new macro that disables
the warning and then use macro for the build options for that file. For an
example, see how the autoconf macro `CFLAGS_NOIMPLICIT_FALLTHROUGH` is defined and
used.

Finally, if there isn't a way to disable the specific warning, you will need to
disable all warnings for the file in question. You can do this by supplying
the autoconf macro `@CFLAGS_NOERROR@` in the build options for the file. For
example:

    lex.yy.o : lex.yy.c y.tab.c
           ${CC} -c ${CFLAGS} @CFLAGS_NOERROR@ lex.yy.c

## Inhibited warnings

If you add a new warning inhibition, also add it to the table below.

| Source file                               | Warning            | Comments                                       |
| ----------------------------------------- | ------------------ | ---------------------------------------------- |
| `uss/lex.i`                               | fallthrough        | clang fallthrough, flex generated code         |
| `comerr/et_lex.lex.l`                     | fallthrough        | See Note 1.                                    |
| `afs/afs_syscall.c`                       | old-style          |                                                |
| `afs/afs_syscall.c`                       | strict-proto       |                                                |
| `afs/afs_syscall.c`                       | all (ukernel)      | syscall pointer issues                         |
| `afsd/afsd_kernel.c`                      | deprecated         | `daemon()` marked as deprecated on Darwin      |
| `bozo/bosserver.c`                        | deprecated         | `daemon()` marked as deprecated on Darwin      |
| `bucoord/commands.c`                      | all                | signed vs unsigned for dates                   |
| `external/heimdal/hcrypto/validate.c`     | all                | statement with empty body                      |
| `external/heimdal/hcrypto/evp.c`          | cast-function-type | Linux kernel build uses `-Wcast-function-type` |
| `external/heimdal/hcrypto/evp-algs.c`     | cast-function-type | Linux kernel build uses `-Wcast-function-type` |
| `external/heimdal/krb5/crypto.c`          | use-after-free     | False postive on certain GCC compilers         |
| `libadmin/samples/rxstat_query_peer.c`    | all                | `util_RPCStatsStateGet` types                  |
| `libadmin/samples/rxstat_query_process.c` | all                | `util_RPCStatsStateGet` types                  |
| `libadmin/test/client.c`                  | all                | `util_RPCStatsStateGet` types                  |
| `volser/vol-dump.c`                       | format             | `afs_sfsize_t`                                 |
| `rxkad/ticket5.c`                         | format-truncation  | See Note 2.                                    |
| `rxkad/ticket5.c`                         | deprecated         | hcrypto single-DES                             |
| `lwp/process.c`                           | dangling-pointer   | See Note 3.                                    |
| `src/rxkad/test/stress_c.c`               | deprecated         | hcrypto single-DES                             |
| `auth/authcon.c`                          | deprecated         | hcrypto single-DES                             |
| `kauth/kaprocs.c`                         | deprecated         | hcrypto single-DES                             |
| `kauth/krb_udp.c`                         | deprecated         | hcrypto single-DES                             |
| `libafscp/afscp_util.c`                   | deprecated         | hcrypto single-DES                             |
| `tests/common/rxkad.c`                    | deprecated         | hcrypto single-DES                             |


Notes:

1. flex generated code pragma set to ignored where included in `error_table.y`
2. Inside included file `v5der.c` in the function `_heim_time2generalizedtime`,
   the two snprintf calls raise format-truncation warnings due to the
   arithmetic on `tm_year` and `tm_mon` fields
3. Ignore the legitimate use of saving the address of a stack variable.
