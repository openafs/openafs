Backport policy
---------------
All patches should land on master first, unless the patch fixes a bug
that only exists in the stable branch.

Once a patch has been accepted into master, anyone can propose
backports to stable branches.  It is preferred to check with the
Stable Release Manager before doing so, though, as that will give
an opportunity to check where in the stack of pending changes any
new addition should go.

When cherry-picking a commit from another branch, please append a
"cherry picked from" section in your commit message. You'll also need
a separate Change-ID for Gerrit to recognize this as a separate
change. One workflow to do this:

1) Use "git cherry-pick -ex" to pick your commits onto another branch.
   The -x option will append the appropriate "cherry picked from"
   message, and the -e option will open your editor for you to edit
   the commit message.
2) In your editor, delete the existing Change-ID line. Save and quit.
3) Run "git commit --amend", saving and quitting again. Git will run
   the commit hook and generate a new Change-ID for Gerrit.
