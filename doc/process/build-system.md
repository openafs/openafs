# OpenAFS Build System

This document describes the build system for OpenAFS for unix-like systems and
how to help maintain it.

## About the build system

The OpenAFS build system is based on GNU Autoconf, and GNU Libtool, and POSIX
make.

A `Makefile.in` file is provided for each component.  Automake is not used at
this time. Makefile.in files are manually edited and checked into the Git repo.
Each component makefile should have the standard targets:

* all
* install
* dest (legacy installation paths)
* clean

A top-level Makefile provides the targets to build the entire tree, or just
selected components.  This top-level makefile should build components in the
correct order based on component inter-dependencies.

Parallel builds are supported when building with a make that supports parallel
target evaluation (e.g., GNU make -j option). Unfortunately, `configure` checks
are not parallelized at this time.

Operating system specific parameters are maintained in `src/config`, with one
file for each operating system version supported in the build.

## Autoconf guidelines

Put autoconf M4 macros in the `src/cf` directory.

## Makefile guidelines

Do not add GNU Make specific features.  We attempt support `make`
implementations which comply with the POSIX make specification.

Do not use `$<` for non-pattern rules in any cross-platform dir as it
requires a reasonable make that is not available on all systems.

Do not have build rules that build multiple targets. Make doesn't seem able to
handle this, and it interferes with parallel (`-j`) builds.  In particular,
build the rxgen targets individually; don't use the flags for building all the
files in one shot.

Try to test builds using `gmake -j # MAKE="gmake -j #"`, it seems like a good
way to find missing or order-dependent dependency rules.

## Using autoconf results in Makefiles

Simple `AC_SUBST` vars from autoconf are usually used in Makefiles like so:

    LDFLAGS_foo = @LDFLAGS_foo@
    foo-command: $(OBJS)
	$(LT_LDRULE_static) $(OBJS) $(LDFLAGS_foo)

Assign a Makefile variable with `VAR = @VAR@`, so we get expansion behavior
that's consistent with other Makefile vars; the `@` references are literal
string substitution.

For Makefile logic that needs to be conditional on certain autoconf flags,
consider using the convention of a `@IF_FOO@` subst var. For example:

    # in src/cf/foo.cf
    AS_IF([test x"$enable_foo" = xyes],
	  [AC_SUBST([IF_FOO], [])],
	  [AC_SUBST([IF_FOO], ['#'])])

    # in src/baz/Makefile.in
    @IF_FOO@FOO_LIBS = $(top_builddir)/src/foo/libfoo.la
    baz-server: $(FOO_LIBS)

In this example, when the feature is disabled, Makefile lines starting with
`@IF_FOO@` will be ignored, since they will appear commented-out with `#`,
and so `$(FOO_LIBS)` will be empty. When the feature is enabled, all of the
`@IF_FOO@` lines will appear as normal lines, and so `FOO_LIBS` will be set
to `$(top_builddir)/src/foo/libfoo.la`.

Some code uses `AC_SUBST` vars directly for this purpose. For example, we could
instead do this:

    # in src/cf/foo.cf
    AS_IF([test x"$enable_foo" = xyes],
	  [AC_SUBST([FOO_LIBS], ["\$(top_builddir)/src/foo/libfoo.la"])],
	  [AC_SUBST([FOO_LIBS], [])])

    # in src/baz/Makefile.in
    FOO_LIBS = @FOO_LIBS@
    baz-server: $(FOO_LIBS)

But this moves the actual definition of `FOO_LIBS` far away from where it is
used and can involve awkward string escaping, making the Makefile logic harder
to read. This also becomes difficult to use if there are many different vars
that need to be set; every single Makefile var needs its own `AC_SUBST` and
Makefile assignment.

Using the `@IF_FOO@` approach is also flexible, since any Makefile line can
be commented out; not just variable assignments, but commands or entire
recipes. For example:

    install:
	$(LT_INSTALL_PROGRAM) baz-server ${DESTDIR}${afssrvlibexecdir}/baz-server
	@IF_FOO@$(LT_INSTALL_PROGRAM) baz-server.foo ${DESTDIR}${afssrvlibexecdir}/baz-server.foo

Some existing code uses shell logic instead, which tends to be more cumbersome.
For example:

    install:
	$(LT_INSTALL_PROGRAM) baz-server ${DESTDIR}${afssrvlibexecdir}/baz-server
	if [ x"@ENABLE_FOO@" = xyes ] ; then \
	    $(LT_INSTALL_PROGRAM) baz-server.foo ${DESTDIR}${afssrvlibexecdir}/baz-server.foo; \
	fi

There are no strict rules for whether to use either of these approaches; it's a
judgement call based on the specific situation.
