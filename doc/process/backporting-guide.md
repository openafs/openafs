# How to backport changes

All patches should land on master first, unless the patch fixes a bug that only
exists in the stable branch.

Once a patch has been accepted into master, anyone can propose backports to
stable branches.  It is preferred to check with the Stable Release Manager
before doing so, though, as that will give an opportunity to check where in the
stack of pending changes any new addition should go.

When cherry-picking a commit from another branch, please append a "cherry
picked from" section in your commit message. You'll also need a separate
`Change-Id` for Gerrit to recognize this as a separate change. One workflow to
do this:

1. Use `git cherry-pick -ex` to pick your commits onto another branch.
   The `-x` option will append the appropriate "cherry picked from"
   message, and the `-e` option will open your editor for you to edit
   the commit message.
2. In your editor, delete the existing `Change-Id` line. Save and quit.
3. Run `git commit --amend`, saving and quitting again. Git will run
   the commit hook and generate a new `Change-Id` for Gerrit.

A cherry-pick to a stable branch should be done with as few changes as possible
from the original master commit. Ideally, the commit isn't changed at all, so
the stable cherry-pick commit is identical to the original master commit. Since
the branches are not identical, though, a cherry-picked commit will often need
to be changed.

Minor edits to a commit are to be expected, and don't need to be mentioned in a
commit message. Significant changes, though, should be explained in the commit
message, in a block under the "cherry picked from" line following the original
commit message. For example:

    baz: Remove redundant foo checks

    The baz utility already checks for foo earlier during the common
    preamble; remove redundant checks for foo in per-command code for
    commands x, y, and z.

    Reviewed-on: https://gerrit.openafs.org/x
    Reviewed-by: Example User <example@example.org>
    Tested-by: BuildBot <buildbot@rampaginggeek.com>
    (cherry picked from commit deadb33fdeadb33fdeadb33fdeadb33fdeadb33f)

    [1.8.x note: The subcommand z doesn't exist at all in the 1.8
    branch.]

    Change-Id: Ideadb33fdeadb33fdeadb33fdeadb33fdeadb33f
