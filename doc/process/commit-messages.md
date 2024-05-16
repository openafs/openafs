# Commit Messages

Each commit must have a commit message describing the commit. Commit messages
should be written in plain, easily understandable English.

Correct spelling and grammar is generally expected, but is not as important as
overall clarity, and keep in mind not all contributors are native English
speakers. Correct spelling of technical terms (such as function names or
filenames) is much more important, since they may be searched for.

The format for a proper commit message is:

    <scope>: <summary>
    
    <body lines>
    
    <footer lines>

## Subject

The first line of the commit message is the subject line. The subject line
should be no more than 50 characters; slightly longer subject lines may be
acceptable, but don't go over 72 characters.

The subject line consists of a scope prefix and a change summary.

The scope prefix indicates which component is changed when the commit is
applied. This should normally be the directory name of the component. For
example, the `afs:` component is the cache manager under `src/afs`. If the
change effects multiple components, the scope prefix should be omitted.

Alternatively, the scope prefix can also be a platform, if the change is a
platform-specific change. For example, the scope prefix can be `LINUX:` for a
change that only affects Linux platforms, even if the commit is changing code
outside of LINUX directories.

The summary should be a brief summary of the change. Begin the summary with a
capital letter as you would at the start of a sentence. Do not end the summary
with a period.

Write the summary in _imperative mood_. Imperative mood just means spoken or
written as if giving a command or instruction. For example:

* Update the Quick Start Guide
* Add -x option to foobar
* Remove unused variable baz

Avoid using _indicative mood_. Some examples of summaries to avoid:

* Fixed bug in X
* Changing behavior of Y
* More fixes for broken stuff
* Amazing new feature Z

A well formed summary should be able to complete the following sentence:

    If applied, this commit will <your summary here>

For example,

    If applied, this commit will _Update the Quick Start Guide_

When possible, try to describe what the commit actually does and be precise;
don't describe the bug it may be fixing, or what the code is _not_ doing. For
example, try to avoid summaries like these:

* Fix segfault in foo
* Don't give a NULL quux to Y
* Foo crashes when quux is NULL

Even though sometimes these are using the imperative mood described above, they
are not describing the change itself, but are focused on the related bug, or
what the code used to do. The summary should instead directly describe what the
commit is actually doing, what change it is making. Sometimes describing a
commit this way is difficult or requires awkward phrasing, so this isn't always
required, but is preferred when practical.

## Body

The body of the commit message should contain an explanation in easy-to-read
text on what changed and why this change was made. Lines should be wrapped at
72 characters, but can exceed this limit when providing examples or quoted
text.

The explanation will be committed to the permanent source changelog, so should
make sense to a competent reader who has long since forgotten the immediate
details and context for the commit. Try to use your best judgement on the level
of detail required, but try to focus especially _why_ this specific change was
made.

Keep in mind someone may be reading this message years or even decades
later! The current OpenAFS public source history dates back to the year 2000,
and is derived from code originally written in the 1980s.

Try to consider both the user-visible effects of a commit as well as the
internal changes. For example, user-visible effects may include a certain
command now behaves differently, or no longer crashes. This information is very
useful when deciding what changes are to be included in stable releases, or if
someone is searching for a fix for a particular issue. Including symptoms of
the failure which the commit addresses (e.g. exact error messages) are
especially useful.

Describing and justifying internal changes may include mentioning how code was
moved to a different file or function, or why code changes were made a certain
way. This can be very useful to code reviewers and makes it faster and easier
to review your submission. This is also helpful to future readers who may be
trying to understand how the code got into its current state, or who might be
considering reverting your change because of a bug in it discovered later.

Do include:

* A brief and clear explanation of what is changed to the code tree when
  this commit is applied.

* A brief and clear explanation of why this change was made.

* If relevant, the current behavior or state of the code before this change was
  made, and the intended behavior after the change is made. This helps provide
  context if the commit is read years later.

* If relevant, exact error messages or build errors, if this commit fixes such
  errors.

* References to bug reports or mail list discussions. But don't assume
  referenced information will always be available; the commit message should be
  understandable on its own.

* References to previous commits by SHA-1 and commit subject if this commit
  fixes a regression or issue from the previous commit. Please provide at least
  the first 12 characters of the SHA-1 ID.

* Specific mentions of any new commands or public interfaces.

## Footer

After the main body of the commit is the footer, which contains some metadata
and references for the commit. Most of the footer is written as a series of
trailers that can be used with `git interpret-trailers`, which look similar to
RFC 822 email headers.

The only part of the footer that is always required is the `Change-Id` trailer,
which is used for gerrit. This should be generated by a `commit-msg` git hook
before you submit the commit to gerrit.

Gerrit itself will add its own trailers after a commit is merged, to show the
gerrit URL for a commit as well as some review information. Don't add any
gerrit-provided trailers yourself; other trailers are fine to add if you want
(`Signed-off-by`, `Cc`, etc), but are not required and aren't common.

OpenAFS also has a long-existing convention of referencing RT tickets on
<https://rt.central.org/> with a line that looks like this:

    FIXES 1275

Which would be included in a commit that fixes (or at least references) ticket
1275. This isn't a real trailer, but is usually included between the main body
of the commit message and the actual git trailers.

Commits that are backported from another branch also have a "cherry picked
from" line added by `git cherry-pick -x` as the very last line before
backport-specific information is included. See
[backporting-guide](backporting-guide.md).
