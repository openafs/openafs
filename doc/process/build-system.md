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
