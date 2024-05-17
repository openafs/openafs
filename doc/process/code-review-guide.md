# Code Review Guidelines

OpenAFS's use of Gerrit imposes a requirement that all changes undergo
code review prior to being merged to any branch, whether it be master or
a stable release branch.  The following guidelines discuss what the
scope of that review is expected to cover.  All of the points need to be
addressed by some reviewer, though not necessarily by the same reviewer.
When doing a review that only addresses some of these topics, indicate
in the review comment which portions were/were not done.

## What does the change claim to do?

This is often taken for granted, but in order to review a change we have
to know what it is trying to do (so that we can tell if it successfully
does so).  In our Gerrit-based workflow, this should be in the commit
message itself, but a patch attached to a bug or email may not have that
additional context.  If the change is fixing a bug, the bug report can
be expected to give some description of the issue being fixed.  In
addition to the sense of "what" is expected to change, a reviewer also
expects to know the "why" behind the change.  This becomes important
when assessing if this change is the right thing to do (see below).

At this part of the review, it's enough to ensure that the what and why
are documented and seem plausible.

## Does the change do what it claims to do?

With the newfound knowledge of the goal of the change, it becomes possible
to assess whether it achieves that goal.  Follow through the "normal" or
"success" path of the new/changed code and scrutinize what it _actually_
does.  As you do this, strive to discern differences between the actual
behavior and the intended behavior -- does it live up to what was
advertised?

If the actual changes have more things than the stated goal, that may be
a sign that there are unrelated changes included -- it is often best to
ask to split these into a separate commit (but if not, they should be
acknowledged in the commit message as intended inclusions).  It is also
okay to ask for mechanical or similarly repetitive changes to be broken
out into dedicated commits (so that the reviewer would just
independently reproduce the mechanical change and compare the resulting
tree).

Having looked at the normal path, it's then time to look at the
not-normal path.  Error cases, edge cases, look for these wherever you
can and see if the code is going to do the right thing in those
scenarios.  Try to spot any copy/paste issues in this phase as well --
if we are doing conceptually the same thing in more than one instance,
check that each instance is actually touching the fields appropriate for
that instance.  Sometimes the difference is just a single character, and
it's easy to miss (sometimes the copy/paste has been cleaned up once on
a given line but needed to be cleaned up twice).  Is there a cleanup
handler that should be jumped to?  Locks or other state that need to be
released?  It is generally reasonable to assume that the adjacent error
cases/early returns are correct, and only examine what has changed
between that call site and the one you're looking at.  Check for edge
cases -- if there are conditionals against a specific constant, verify
the expected behavior when the input is that value/length, as well as
just above and just below.  This holds especially for string lengths
(e.g., in PRNAMEs the length bound need not account for a trailing NUL)
but checking for edge cases is always important.

## Does the change not do something it should be doing?

If the goal of the change is to be comprehensive about doing something
(within a given scope), check that it has actually changed all the
places that need to change.  It can take a bit of creativity to be able
to write an efficient check for this -- sometimes just searching on the
API name is sufficient, but maybe there is a specific constant (for port
number, AFS service number, a given data field's length, etc.) that
would apply,  This is probably going to require a lot of manual
inspection; `${PAGER} $(git grep -l ${PATTERN})` followed by a search in
each file to find the relevant instance(s) is a useful technique.

Do NOT just rely on the compiler to detect incompatible/missing APIs --
we have code that is conditionally compiled based on architecture and
environment, and we also want to ensure that we find relevant comments,
tests, and documentation that might be exercising the interface in
question.

Check for comments/documentation for the affected areas of the code.
Are there any comments that have become stale and need to be changed,
but are not changed?  Unless the goal of the change is to fix a bug and
restore the documented behavior, if the behavior is changing, we should
expect the documentation (if present) to be changing as well.  (If there
is no documentation, consider adding some.)

Are there related functions or logic that should be getting a similar
change to it?  See above about conditionally compiled code -- sometimes
we have multiple implementations of a given API depending on the
environment or architecture, and often (but not always) when we need to
change one, we need to change others.  A network API may have multiple
versions as it has evolved over time; learn about the intended
differences between generations of the API to determine which one(s)
should be getting a given change.  If fixing a bug (especially when
security-relevant), try to generalize or abstract the nature of the fix
to see if there are other places where an analogous fix would apply.
Returning uninitialized data to a network caller is the classic example
here, but this can arise in many ways.

## Does the change not do what it claims to not do?

If there is supposed to be no functional change, is that true?
If a given class of behavior/logic is supposed to be unmodified, trace
through the code and confirm that that's the case.

## Is that the right thing to do?

This question is a bit open-ended and sometimes hard to apply in
practice, but it's usually worth taking a step back and thinking about
what problem the change is trying to solve, whether it could be solved
some other way, and whether solving a different problem would be more
appropriate.  For example, rather than trying to optimize an existing
API at the cost of very convoluted logic, maybe there is a simpler API
or different data structure to achieve the overall goal that has a
different overall structure.  Contrariwise, though, changing the
behavior of a network API is pretty complicated to get right, and a
local adjustment may be more worthwhile.

## Style and Standard Compliance

This is sometimes done in passing during an earlier stage of the review,
but always make a point to consider whether the changes conform to the
project's style guidelines.  New code should conform to the project (or
prevailing) style, and generally we should fix the style when making
substantive changes to a given line.  Often we will fix style of
adjacent lines as well, if there are not too many, but it's a judgement
call -- if the focus of the change becomes more about style than
bugfixes, it's probably time for a separate cleanup commit.  It's also
important to think about whether the change conforms to the language
specification; we officially target C89 plus a handful of extensions
(though it may be simpler to reason about it as C99 minus a handful of
features).  The precise list of extensions over C89 that we use is a
work in progress, but includes (and will be updated as encountered):

- the long long int type
- designated initializers
- the `__func__` predefined macro

If we're using additional language extensions or more modern
features, they need to be scoped to a file/situation where we can
guarantee they are available.

We must always try to avoid undefined behavior (though there are
existing cases where we do potentially trigger it).

Our network and userspace/kernel APIs need to conform to any relevant
standards or specifications that apply.  We cannot unilaterally change
existing RPC-L (the .xg files processed by rxgen), and the scope for
adding new things is limited.  When we add new pioctls, they have to
come from the OpenAFS-specific range (i.e., be 'O' pioctls).  `Com_err`
codes in .et files are generally managed by the central.org registrar,
etc.

## Documentation

New APIs should get documented via at least doxygen comments attached to
their implementation.  Changes to CLI commands require man page changes.
New public APIs should also get man pages.  The reviewer should check
that these are being done.

If the change is modifying the behavior of existing functionality that
has documentation, update the documentation accordingly.  (Don't forget
the HISTORY section of man pages!)  If there is not existing
documentation, the code review is a good opportunity to ask if
documentation should be added, but the answer to that question will vary
on the specifics of the case in question.

## Test Coverage

The current test coverage in the tree is very sparse.  If adding to or
changing the behavior of an area of the code that does have some test
coverage, add tests for the new/changed behavior.  If fixing a bug, add
a regression test.  It is a good idea to confirm that the (new) test
fails with the code change reverted, though sometimes this is
complicated enough to be an unreasonable burden on the reviewer.

For areas of the tree that do not have existing code coverage, we should
consider adding some.  It's good for the reviewer to ask (and consider
themself) what level of effort would be involved in adding test coverage
for this part of the code and whether we should put that effort in at
this time.  The answer may be "not worth it right now", though.
