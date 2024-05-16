# Submitting Patches

## Introduction

This is a general guide for contributing changes to OpenAFS. This document
gives guidance on how to organize and document your changes.

OpenAFS maintainers and developers use the **Gerrit Code Review** system for
code review and collaboration.  Your patches should be submitted to the
[OpenAFS Gerrit](https://gerrit.openafs.org/) code review system to be
reviewed. It is not unusual for changes to be requested by reviewers before the
patches are accepted.  After reading this document, see
[howto-use-gerrit](howto-use-gerrit.md) for information on how to use the
Gerrit Code Review system.

A git mirror of the OpenAFS git repository is provided on GitHub at
<https://github.com/openafs/openafs>. However, please note that GitHub pull
requests are not accepted for code changes to OpenAFS. Instead, use the Gerrit
code review system to submit your patches and collaborate with our community of
developers.

You can obtain the OpenAFS source tree from the upstream repository:

    $ git clone git://git.openafs.org/openafs.git

Development proceeds on the `master` branch, so new changes should generally be
based on the current `master` branch.

See [howto-build-openafs](howto-build-openafs.md) for instructions on how to build the binaries
from source code.

See [backporting-guide](backporting-guide.md) for information on how to create
changes for **stable** releases.

## Commit Guidelines

OpenAFS has evolved various styles and guidelines for our commits over time.
Not just for how code is formatted, but also how commits are organized, what
can and cannot be changed, and generally our process for making changes. There
are many exceptions to all of these; they aren't strict rules, but general
guidelines and suggestions.

Don't worry too much about strictly following everything here -- it's the job
of the reviewers to check your submission -- but if you follow these guidelines
as best you can, it makes things easier for everyone and helps your submitted
changes get accepted more quickly.

### Code Style

Try to follow the official style guide in [code-style-guide](code-style-guide)
for new code.  Sometimes old existing code doesn't follow all of the styling
rules, and adding strictly-conforming code right next to old code looks odd or
confusing. In these cases, you can try to update the old code and fix the
styling, or just adhere to the prevailing surrounding style.

If you are changing a line of code that has incorrect styling, update the style
at the same time. Whether to fix incorrect styling on nearby lines is a
judgement call depending on the situation, but fixing styling on an
already-changing line should almost always be done.

### Commit Messages

See [commit-messages](commit-messages.md) for guidance on writing commit
messages. Commit messages are important, and commits will not be accepted if
the commit message isn't good enough. We can help you write it, though, and
make suggestions.

### Whitespace

Whitespace errors can be conveniently detected by git (`git show --check`), and
are highlighted in gerrit. We use one additional whitespace check that is not
enabled by default in git:

    $ git config core.whitespace indent-with-non-tab

Otherwise, the defaults should suffice. Please use `git show --check` or
similar commands before you push a commit to gerrit.

### Prohibited Changes

In general, do not break things for existing users. That broadly means not
altering existing behavior, except of course to fix bugs or add features.

The output of most commands cannot easily be altered; changing the output must
be done very carefully to avoid breaking existing tools that parse the output
of commands. This is currently rather important, because most commands don't
print data in common machine readable formats like JSON or YAML, and so parsing
our human-readable output is the only option automated tools have.

Many components of OpenAFS involve network protocols, userspace-to-kernel
protocols, and public library interfaces. Existing definitions and interfaces
for these generally cannot be changed; we can only add new behavior by adding
new RPCs and interfaces. See [standards-compliance](standards-compliance.md)
for details on what specifically is restricted, and how to make changes in
these areas.

### Commit Organization

Changes to the code often need to be broken up into several commits. Changes
can be submitted as a branch of several commits together, but we accept
individual commits one at a time, not entire branches. So each commit must
stand on its own to some degree.

Deciding how to split up a change into commits isn't always easy, but here are
some general guidelines:

* Make only one logical change per commit: fix one problem, or add one new
  feature, etc. Don't fix unrelated issues in a commit just because they are
  nearby in the code.

* For example, if your changes include both bug fixes and code cleanup,
  separate those changes into two or more commits.

* On the other hand, if you make a single logical change to numerous files,
  group those changes into a single commit.  Thus a single logical change is
  contained within a single commit.

* Each commit should make an easily understood change that can be verified by
  reviewers.  Each commit should be justifiable on its own merits. This point
  can be subjective, so try to use your best judgement.

* Do not make breaking changes in a commit, even if the breaking changes are
  fixed in a subsequent commit. You should be able to build OpenAFS on all
  supported platforms with `./configure --enable-checking` after applying each
  change.

* Do not add code that is not used ("dead code"). An exception is
  when you are adding new functions or RPCs. In this situation, dead code is
  allowed as long as commits later in the branch use the new code. A commit
  adding dead code will not be accepted until the later commit using the new
  code is ready to be accepted. Unit tests do not count as using code; a
  function that is only called by unit tests is still dead code.

* For branches involving many commits, only submit up to about 15 or so commits
  at the same time to gerrit. If your change involves more commits, you can
  make them available in a branch in your own repository to provide context for
  the gerrit-submitted commits.

If you want more guidance on splitting a change up into individual commits, it
can be helpful to try to think of commits as existing in one of a few
categories. Most commits fall into just one; if a commit seems to be in more
than one category, then that is a sign that it could be split it into separate
commits.

Some categories or types of commits are discussed below. These aren't formally
defined and aren't mentioned in the commits themselves in any way, it's just a
helpful way to think about them.

#### Bugfix Commit

A bugfix commit fixes a bug, or generally intentionally changes the behavior of
something.

The commit usually should contain the smallest reasonable change to fix the
problematic behavior. If the relevant code needs to be significantly moved
around or refactored in order to fix the bug, that refactoring should be done
as a separate commit first (a refactoring commit), and then the bugfix commit
can be very small and easy to follow.

If the bugfix can be done without refactoring, but it's clear that refactoring
would be helpful, do the bugfix first, and then the refactoring afterwards.
This makes it easier to pull the bugfix into other branches, such as the
OpenAFS stable branch, or for downstream packagers to pull in the fix.

The commit should fix all instances of the bug in question, if the fixes are
similar. If the fixes are very different (for example, a different platform
requires a different approach), that may warrant splitting into separate
commits.

Sometimes you need to decide whether to fix a bug with a smaller targeted
"bandaid" fix, or a larger more invasive fix to address the underlying problem.
If you're not sure which to do, you can do both: implement a smaller fix first,
and then try to do a larger change in the next commit. This makes it easy to
accept just the smaller commit (dropping the larger one), just the larger fix
(squash the two commits together), or both (the smaller fix goes in a stable
branch, and both go in the master branch).

If a unit test is practical, add a test for the bug in the `tests/` directory,
before the bug is fixed, where the buggy behavior results in a successful test.
Then change the test in the same commit as the bugfix commit, showing the
change in behavior.

Make sure to mention what bug you're fixing in the commit message (see
[commit-messages](commit-messages.md) for details on commit messages).

#### Refactoring Commit

A refactoring commit makes changes to code, but causes no change (or very
little change) in behavior. This includes commits that move around or refactor
code, remove dead code, edit comments, change whitespace, or reindent C
preprocessor directives.

If a refactoring is very large, it should be split up into smaller commits just
to make reviewing easier. You can split up large refactorings into commits that
handle a specific file, a specific subsystem, or a specific type or variant of
the refactoring you're doing.

If the commit was done in an automated or semi-automated fashion, say what
specific commands you used in the commit message.

If it's possible to verify that the commit doesn't make behavior changes, say
how to do so in the commit message. For example, `git show -w` is useful to see
that a commit doesn't have any non-whitespace changes.

#### New-code Commit

This type of commit adds a new feature or function. The commit message should
mention why the new feature is useful. New platform support can be considered
this type of commit.

Larger commits can be broken up by adding new subsystems/modules in different
commits.

Tests and documentation should be added in the same commit that implements the
behavior being tested/documented.

New code should be platform-agnostic where possible. If platform-specific logic
is required, it should be designed such that there is a higher-level function
that calls platform-specific functions, which helps avoid #ifdef's littering
common code paths. This abstraction should be done even if you're only
implementing one platform at the time.

New code should always be built when possible. Do not add new `./configure`
options to enable building new code; instead, always build the code, and add a
runtime option to decide whether the new code is used. If new code must be a
build-time decision, try to always build as much code as possible, and let the
build-time switch determine if the new code is actually used. This can result
in code that is built but not used, but that's good; it makes sure the code is
always buildable.

New features often should be disabled by default, or should offer some graceful
detection to be automatically turned on.

#### Doc/test Commit

This type of commit just adds missing documentation or tests for existing code.
These commits don't need justification in their commit messages; the benefits
of tests or documentation are obvious.

#### Revert Commit

Rarely, commits are reverted because they contain a mistake or we have decided
to take a different approach. Usually it's better to just write a bugfix commit
to fix the error, but sometimes it's easier to see what is happening if the
entire commit is reverted, and an alternate approach is implemented.

Where possible, reverts should be done using `git revert`. A justification for
the revert should be added to the commit message.

#### Import Commit

We sometimes import code from other projects in `src/external`. Importing
external code is done as its own commit via the
`src/external/import-external-git.pl` script, with the author set to the author
of that project. All importing of external code should be done using that
mechanism where possible.

### Tests

Commits should include unit tests verifying the change in the commit, when
possible. See [unit-tests](unit-tests.md) for details.

### Documentation

If you are writing a commit that introduces new behavior, or alters existing
behavior, it likely needs to be documented. Most new documentation is done in
the form of man pages, written in POD. These are located in the
`doc/man-pages/pod*/` directories, and are processed by the
`doc/man-pages/generate-man` script, usually called by `regen.sh`.

### License

New source files must have a block at the top of the file with the
copyright notice and license information, except for small/trivial files such
as makefiles, `.gitignore` files, etc.

Much of the existing OpenAFS code is licensed under the IBM Public License
(IPL), but new source files should use the 2-clause BSD license where possible.
While other similar licenses may be effectively equivalent, it is helpful to
use 2-clause BSD specifically so we don't accumulate a mess of different
licenses.

Since the IPL is incompatible with all versions of the GNU General Public
License (GPL), and most other copyleft licenses, we cannot accept GPL-licensed
code unless it has an appropriate exception. This may change some day, but
remains the case for now.

### Kernel autoconf tests

Writing code for the kernel on various platforms is a bit special for a variety
of reasons, such as managing the relevant APIs. Kernel APIs tend to be somewhat
unstable, adding or removing arguments to functions over time. When this
happens, often it helps keep the OpenAFS code clearer by making a small wrapper
for the function, to choose which variant of the function to call. This is
helpful even if there are only two variants of the function; callers get
cluttered with `#if` spaghetti very quickly.

For some platforms, it's enough to use existing preprocessor symbols to check
what kernel version we're building for to see what API variant is available
(for example, `__FreeBSD_version`). Sometimes this is not specific enough, and
instead we must create an autoconf test. This is a test run by the
`./configure` script that tries to build a kernel module using the API in
question to see which variant is available.

Relying on existing symbols is almost never good enough on Linux, due to the
notoriously unstable API, and where version numbers are unreliable due to
downstream kernels frequently including changes from later kernels. So on
Linux, any interface we use that differs in different kernel versions should
use an autoconf test to detect the variant; only check `LINUX_VERSION_CODE` as
an extra safety check, or if an autoconf test is not practical.

When adding an autoconf test for a kernel function, you must document in what
kernel version the relevant API changed, so we know when we can remove the
test. Kernel versions earlier than the minimum supported platform in `INSTALL`
do not need to be considered, and so sufficiently old tests can be removed when
the minimum kernel version is raised.

These autoconf tests should also be written such that the "success" case
happens for the newer kernel version. We do this in case newer kernels break
our configure test in unrelated ways (for example, a header file moves); if the
test is supposed to fail for newer kernels anyway, we may not notice for a
while that the test is broken.

Usually this is intuitive; just try to call the newer version of the function.
But sometimes this is counterintuitive: if we need to check that a function has
been removed, we cannot just try to call it (then newer kernels would fail,
because the function is removed). Instead, one approach is to try to define our
own function with the same name but a completely different signature; that will
succeed on newer kernels where the function doesn't exist, but will fail for
older kernels where the function does exist.

There are many examples of existing kernel tests, most of which are for Linux.
See `src/cf/linux-*.m4` for Linux examples, and add new Linux tests to those
files when needed.
