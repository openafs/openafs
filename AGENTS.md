# Instructions for AI coding agents

This is the OpenAFS project. Read doc/process/ai-policy.md before making
changes. The human you are working with is the sole author and is responsible
for every line; they must be able to explain all of it in code review.

These instructions describe the requirements for changes submitted to
the OpenAFS project. If you are unsure whether a rule applies, or
following it conflicts with what the user is asking for (for example,
work that will not be submitted upstream), ask the user instead of
guessing. If these instructions and the documents in doc/process disagree,
or a document is unclear, point this out to the user and ask how to proceed.
The user may choose to override a rule; if they do, tell them
when the result could not be accepted upstream. The licensing, AI
disclosure, and security bug rules cannot be overridden for any work
that will be made public, including pushes to the OpenAFS Gerrit, a
public fork, or any other public repository or site.

At the start of a session, tell the human once that they must read
and are expected to follow the policy described in
doc/process/ai-policy.md.

OpenAFS has a small team of maintainers who are the only ones with
commit access to the official OpenAFS repository. Other community
members may also review changes, but review time is limited. All
changes will be reviewed by humans and must be understandable to the
reviewers. All changes must be maintainable by a human. Keep changes
small, focused, and easy to review, and follow the limit on the number
of commits submitted at one time in doc/process/submitting-changes.md.
Be respectful of reviewers' and maintainers' time
(doc/process/code-of-conduct.md).

Every change is merged by a maintainer after review in Gerrit. Do not
push to the OpenAFS Gerrit or any other remote without explicit
permission from the user. Pushing to the OpenAFS Gerrit submits a change for
review; it does not merge it.  GitHub pull requests are not accepted.  The
GitHub repository is only a mirror (doc/process/submitting-changes.md).

## Project guidelines

All guidelines are in doc/process/ (read doc/process/01-contributor-guide.md
for an index of the processes).

If a change fixes, or you discover, a possible security vulnerability: stop,
read doc/process/security-bugs.md, and tell the user to follow that process.
Do not prepare a public commit or describe the issue in a commit message.

Read the relevant process documents at each stage of your workflow:
- Always read first: doc/process/ai-policy.md
- Before editing code:
    - doc/process/code-style-guide.md
    - doc/process/compiler-warnings.md (do not introduce new warnings)
    - doc/process/code-status.md before modifying any component, to check
      whether it is maintained. If a component is legacy or deprecated,
      ask the user how to proceed.
    - doc/process/build-system.md when changing makefiles or files in src/cf
    - the "Kernel autoconf tests" section of doc/process/submitting-changes.md
      when adding or changing kernel API tests in src/cf
- Before building/testing:
    - the "Build requirements", "Getting the source code", and "Compiling
      the source code" sections of doc/process/howto-build-openafs.md, and
      doc/process/unit-tests.md (build and run tests before telling the
      user a change is ready)
    - Some testing may require setting up an entire AFS cell, or installing
      a kernel module on a particular platform.  If you cannot build or run the
      tests, tell the user what was not verified.
- Before creating a commit:
    - doc/process/commit-messages.md

Also read, when relevant:
- doc/process/platform-support.md when changing platform-specific code
- doc/process/standards-compliance.md when changing RPCs, pioctls, public
  library functions or symbols, on-disk formats, or dump tags
- doc/process/backporting-guide.md when backporting a change from the
  development branch (master) to a stable branch

## Licensing

- OpenAFS is currently distributed under the IBM Public License Version 1.0
  (IPL-1.0), with some individual files under permissive licenses (such as
  BSD or MIT).  Follow the guidelines listed in
  doc/process/submitting-changes.md for deciding which license to use.
- Never copy, adapt, or port code from GPL-only projects (including the
  Linux kernel, glibc, or Samba) or any other IPL-incompatible sources into
  OpenAFS. When working on Linux kernel compatibility, inspect kernel
  headers only to understand interfaces and write original implementation
  code.
- Do not change existing license headers or add SPDX tags unless explicitly
  instructed. When creating a new file other than a small file such as a
  makefile or .gitignore, the default license is the 2-clause BSD license
  (doc/process/submitting-changes.md). Ask the user for the copyright holder
  and to confirm the license header.

## Commit messages

- Every commit containing AI-generated or AI-modified content MUST contain a
  git trailer in exactly this format:

  ```
  Assisted-by: AGENT_NAME:MODEL_VERSION [TOOL1] [TOOL2] ...
  ```

  where `[TOOL1] [TOOL2] ...` are additional analysis tools used.

  Examples:

  ```
  Assisted-by: AGENT_NAME:MODEL_VERSION
  Assisted-by: AGENT_NAME:MODEL_VERSION coccinelle
  ```

- The trailer is not needed if you were only used to review code, explain
  errors or code, or find or flag mistakes that the human then fixes
  (doc/process/ai-policy.md). It is required if any of your output is
  used in the commit, including code, documentation, rewritten text, or
  commit message text. If it is unclear whether disclosure is needed, add
  `Assisted-by: AGENT_NAME:MODEL_VERSION`.
- Put the Assisted-by line in the final trailer block, with no blank lines
  between it and Change-Id or other trailers.
- List specialized analysis tools only (such as coccinelle, sparse, smatch,
  clang-tidy). Do NOT list common tools such as git, gcc, make, editors, lint,
  or indent.
- Do NOT add Co-authored-by, "Generated with", or any other AI attribution
  lines. AI tools are not authors of OpenAFS commits.
- Do NOT add a Signed-off-by line on the user's behalf.
- Follow the OpenAFS subject prefix convention
  `<subsystem>: <imperative summary>`, max ~50 chars, no trailing period.
  Begin the summary with a capital letter.
  Do NOT use "Conventional Commits" prefixes like feat:, fix:, or chore:.
  The `<subsystem>` is normally the component's directory name (e.g. `afs:`
  for src/afs). For a platform-specific change, use the platform instead
  (e.g. `LINUX:`, `macOS:`), even if the files changed are outside that
  platform's directories. If a change crosses several subsystems, omit the
  `<subsystem>`.
- Wrap commit message body lines at ~72 columns, with the exception when quoting
  output from commands (e.g. compiler or build errors, command examples, etc.)
- Leave the Change-Id: generation to the repository's commit-msg git hook
  (do not invent or hallucinate a Change-Id hash). When amending an existing
  commit, keep its Change-Id line unchanged so Gerrit records the result as
  a new patchset of the same change (doc/process/howto-use-gerrit.md).
- When backporting a commit to a stable branch, use `git cherry-pick -x`.  The
  original Change-Id must be removed and a new Change-Id must be created for
  Gerrit.

## Code style

Follow doc/process/code-style-guide.md. The GNU indent options listed in
that guide describe the general style only. Do not run indent or any other
automatic formatter on OpenAFS code; it does not preserve the preprocessor
indentation and other parts of the style, and its output needs fixing by hand.
Where the indent options differ from the prose in code-style-guide.md or the
rules below, the prose and the rules below take precedence.

- The examples in doc/process/code-style-guide.md show each tab character as
  `>-------`. Write a real tab character in code, never the marker.

Key rules agents commonly get wrong:

- Code is C89 plus the extensions listed in the guide. Declare all
  variables at the start of a block, never in the middle of a block or
  inside a `for` statement.
- Do not use `//` comments.
- Use `static_inline`, not `static inline`.
- In function definitions, put the return type on its own line, above the
  function name.
- For C and C-like code, indent 4 columns per level. Build leading whitespace
  from one tab for each full 8 columns, then spaces for the remaining 1-7
  columns: level 1 = 4 spaces, level 2 = 1 tab, level 3 = 1 tab + 4 spaces,
  level 4 = 2 tabs. Continuation lines align with the opening parenthesis
  using the same tab-then-spaces rule. Never use 8 or more consecutive
  spaces of indentation, and never put a space before a tab.
- Use braces around all conditional and loop bodies, including single
  statements and empty bodies:

```c
if (x != 0) {
    y = 1;
}

while (do_something()) {
    /* empty body */
}
```

- Compare integers and pointers explicitly: `if (count != 0)` and
  `if (ptr == NULL)`, not `if (count)` or `if (!ptr)`. Only an int that
  represents a boolean may be tested directly.
- Test flag bits explicitly: `if ((flags & FLAG_X) != 0)`.
- Mark intentional switch fall-through with `AFS_FALLTHROUGH;`, not a
  comment.
- Indent goto labels by one space.
- Preprocessor directives always begin in column 1, with one additional
  space (not a tab) between the `#` and the keyword for each level of
  nesting. A header file's include guard does not count as a level:

```c
#if defined(foo)
# ifdef bar
#  include <xyzzy.h>
# endif
#endif
```

- Verify whitespace in diffs: Many AI file-editing tools silently convert tabs
  to spaces. After applying edits to C and C-like files, check the diff with
  the `indent-with-non-tab` rule enabled:
  `git -c core.whitespace=indent-with-non-tab diff --check`, adding `--cached`
  for staged changes, or `show --check` in place of `diff --check` after
  committing. For other files, a warning from this check does not mean the
  indentation is wrong; keep the file's existing indentation. See the Tabs
  section of doc/process/code-style-guide.md.

## Style in existing code

Much existing code does not follow the style guide.

- New files and new functions must follow the style guide fully.
- When changing a line of existing code that does not follow the style
  guide, update that line's style as part of the change
  (doc/process/submitting-changes.md). Whether to also fix nearby lines is
  a judgement call; if the user has not said which, ask before making the
  change.
- When adding new lines of code to an existing function, follow the style
  guide unless the surrounding code is largely different; in that case,
  follow the style of the surrounding code. Always follow the rules for
  braces, explicit integer and pointer comparisons, boolean tests,
  `AFS_FALLTHROUGH`, declarations at the start of a block, and no `//`
  comments.
- Never mix large style changes with functional changes. Do not reformat
  lines you are not otherwise changing.
- If a larger style cleanup seems warranted, do not make it on your own.
  Describe the cleanup to the user and ask whether to do it, and if so,
  whether it goes in its own commit before or after the functional change.
- When moving code for refactoring, put the move in its own commit and
  leave the moved code exactly as it was, even if it does not follow the
  style guide. Make any style or logic changes in separate commits.

## Scope

- Keep each commit to one logical change. Follow the commit organization
  guidelines in doc/process/submitting-changes.md, in particular:
    - Each commit must build with `./configure --enable-checking`.
    - Do not add code that is not used (dead code), except new functions or
      RPCs used by later commits in the same series. Code called only by
      unit tests still counts as dead code.
    - Add tests and documentation in the same commit as the behavior they
      test or document. Changes to command behavior need man page updates
      in doc/man-pages/pod*.
    - Do not change the output of existing commands unless the change
      requires it; scripts and tools parse the output (see "Prohibited
      Changes" in doc/process/submitting-changes.md).
- Before committing, tell the user to review and test every change
  themselves.

## Architecture and Idioms

- Memory and locking: Use OpenAFS allocation and locking wrappers appropriate to
  the subsystem (e.g., osi_Alloc/afs_osi_Alloc in kernel code,
  opr_mutex_enter/AFS_GLOCK, etc.) rather than raw standard library or
  OS-specific calls unless the surrounding file explicitly uses them.
- Dual-compiled code: Remember that many files are compiled for both
  userspace (pthreaded or LWP) and kernel space across multiple OSes. Do not
  introduce OS-specific headers or libc calls into shared tree paths without
  checking `#ifdef KERNEL` / `#ifdef UKERNEL` guards.
- Generated files: Never directly edit files generated by rxgen (.xdr.c, .cs.c,
  .ss.c, .h from .xg files) or compile_et (.c/.h from .et files). Edit the
  source .xg or .et file instead, subject to
  doc/process/standards-compliance.md: existing RPC definitions must not be
  changed (except in the few example and test .xg files it describes), and
  the user must register any new RPCs or error codes per
  standards-compliance.md.
