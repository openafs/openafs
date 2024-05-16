# Gerrit Guide for OpenAFS

A Gerrit Code Review service is hosted by MIT for the OpenAFS project. Patches
for OpenAFS are reviewed and accepted using this online system.  This is a
guide for developers submitting patches for OpenAFS using the Gerrit Code
Review system.

This guide assumes you are comfortable using Git on the command line and you
have Git installed on your local machine.  If you are new to Git, take some
time to read the Git documentation and learn the basics.  You should be
familiar with the common Git commands like `clone`, `fetch`, `checkout`, and
`push`.

## About Gerrit

Gerrit is an open-source, web-based code review tool, to streamline the process
of reviewing and accepting changes.  This collaborative platform enables teams
to work together on software development projects by supporting peer review and
approval of patches before they are merged into the main codebase.

* **Change submission**: A developer creates one or more a commits in their
  local Git repository.
* **Patch upload**: The developer uploads the commits to Gerrit for review
  using `git push`.
* **Review request**: The commits are assigned a unique identifier (Change-Id),
  and a review request is sent to the designated reviewers.
* **Code review**: Reviewers examine the code, providing feedback in the form
  of comments, questions, or approval votes.
* **Changes and replies**: Authors can address reviewer concerns by making
  changes to the patch or responding with explanations.
* **Approval**: Once all reviewers have approved the patch (or the author has
  addressed their concerns), the patch is considered "approved" and can be
  merged into the codebase by the OpenAFS maintainers.

Gerrit features include:

* **Web-based interface**: Reviewers and authors interact through a web
  interface, making it easy to collaborate remotely.
* **Self-service sign-on**: Sign-on with OpenID and push changes with
  your public ssh key.
* **Change tracking**: Gerrit tracks changes made during the review process,
  allowing for easy comparison of different patch versions.
* **Discussion forums**: Reviewers and authors can engage in threaded
  discussions about specific parts of the code.
* **Customizable workflows**: Teams can define their own review processes and
  approval rules.
* **Integration with Git**: Gerrit is tightly integrated with Git, making it
  simple to create, manage, and merge patches.

## Gerrit workflow

The Gerrit workflow is patch-oriented.  Patches are logical changes that are
tracked across revisions. This makes using Gerrit different than other popular
Git based change management systems, which are branch-oriented. In order to
track changes to patches, Gerrit assigns a unique identifier called a
`Change-Id` to each change, independent of Git commit SHA1 hashes.

* Individual patches are submitted instead of **pull requests**.
* A unique `Change-Id` is associated with each patch.
* A shorter, human readable **Gerrit Number** is also assigned to each patch.
* A patchset number is assigned to track revisions of the patch. The patchset
  number of the patch starts at 1 and increments each time the patch is
  updated.
* Patches are updated with a regular `git push`, not `git push --force`.
* Multiple patches in a stack can be submitted in one `git push`.
* An optional **topic** name can be assigned by the submitter when submitting
  patches, which can be helpful to track related patches.

## Getting Started with Gerrit

### Create a local git repository

Start by cloning the OpenAFS git repository:

    $ git clone git://git.openafs.org/openafs.git
    $ cd openafs

Gerrit requires a user name and email for registration, so be sure your
`user.name` and `user.email` in your git configuration Gerrit uses the `user.name` and
`user.email` in your git configuration

Verify your name and email are in the git configuration. Run the following in
your local OpenAFS git repository:

    $ git config --get user.name
    $ git config --get user.email

Add a remote for submitting changes for code review. Note this is a different
URL than the one used to clone the repository (called "origin" by default).

    $ git remote add gerrit ssh://gerrit.openafs.org/openafs.git

## Create a Gerrit account

You will need a Gerrit account to submit and review patches.

Gerrit uses OpenID for authentication, so the first step is to create an OpenID
account if you do not already have one.  The [Launchpad](https://launchpad.net)
OpenID provider works well.

Once you have an OpenID account, navigate to <https://gerrit.openafs.org> and
select **Sign In**. You will be redirected to a page where you can select your
OpenID account.  Choose your OpenID provider, e.g., **Launchpad ID**.

After signing in, access the **Settings** menu and then click on **Contract
Information**. If the email associated with your Gerrit account does not match
the email in your Git configuration, select **Register New Email...** to update
your settings.

## Upload your SSH key

You will need a SSH key pair to upload code changes to Gerrit.  It is
recommended you create password protected SSH key pair specifically for use
with Gerrit.

To create a new ssh key with the default name:

    $ ssh-keygen -t rsa -f ~/.ssh/id_rsa

The public key will be created as `~/.ssh/id_rsa.pub`.

Upload your public SSH key:

1. Sign-in to [Gerrit](https://gerrit.openafs.org/)
2. Select **Settings**, then **SSH Keys**.
3. Paste your public key value into the **Add SSH Public Key** text area box.
4. Click **Add** to add your public key.
5. Select **Profile** and note the listed `Username`.

## Update your SSH configuration

Add a stanza to your SSH configuration to make it easier to access Gerrit with
SSH.

Add the following to your `~/.ssh/config` file:

    Host gerrit.openafs.org
      User <username>
      IdentityFile <path>
      Port 29418
      HostKeyAlgorithms +ssh-rsa
      PubkeyAcceptedAlgorithms +ssh-rsa

Where:

1. `<username>` is the username from the Gerrit **Profile** page.
2. `<path>` is the path to your SSH private key on your local system (e.g.,
   `~/.ssh/id_rsa` if you used the default key name.)

## Setup Gerrit Change-Id git hook

Gerrit needs to identify commits that belong to the same logical change. For
instance, when a change needs to be modified, a second commit can be uploaded
to address issues reported by code reviewers. Gerrit allows attaching different
commits to the same logical change, and relies upon a `Change-Id` trailer at
the bottom of a commit message. With this `Change-Id`, Gerrit can automatically
associate a new version of a change back to its original review, even across
cherry-picks and rebases.

The `Change-Id` value is generated by the local `commit-msg` git hook when you
create new commits.  This hook generates a `Change-Id` and adds it to the
footer of the commit message when you run `git` commands which create commit
objects (e.g.  `git commit`, `git commit --amend`, `git rebase`, `git
cherry-pick`).

Download the Gerrit `commit-msg` commit hook to your `.git/hooks` directory in
your OpenAFS git repository. Run this command in the top level directory of
your Git repository to download the hook:

    $ scp -p -P 29418 gerrit.openafs.org:hooks/commit-msg .git/hooks/

## Submitting commits

See [submitting-changes](submitting-changes.md) for general guidelines on
creating commits to be submitted to OpenAFS.

This section describes how to to submit changes to the Git `master` branch.
Changes must normally be accepted on the `master` branch before being
backported to the release branches.

See [backporting-guide](backporting-guide.md) for information on how to create
changes for the release branches.

Be sure your change is currently checked out and is based on the OpenAFS
`master` branch.  Use `git log` to check.

    $ git log -p origin/master..HEAD

Review your change locally before pushing to Gerrit with `git show`.  Be sure
the commit messages contains the Gerrit `Change-Id` in the Git commit message.

    $ git show

After you have reviewed and tested your change locally, use Git to submit the
change to Gerrit.  Specify the remote ref as `refs/for/master` to push your
change for the `master` branch.

    $ git push gerrit HEAD:refs/for/master

If the current `HEAD` contains multiple commits since the `master` branch, each
commit will be submitted to Gerrit as a separate Gerrit change.

It is recommended to add other developers as **Code Reviewers** to your changes.
This can be done in the Gerrit web interface for the change.

## Gerrit Topics

Gerrit allows developers to add a label to changes called a **topic**. Setting
a topic can be helpful to identify related commits, and search for commits
based on a topic name.

It is recommended to set the same topic name on a set of related changes
especially on each change in a stack of commits.

The topic name can be automatically assigned to changes by appending a name to
the remote ref when pushing to Gerrit.

    $ git push gerrit HEAD:refs/for/master/<my-topic-name>

You can also change the topic name on your changes using the Gerrit web
interface.

## Updating commits

You may receive requests for changes before your commit can be merged to the
OpenAFS codebase. In this case you will need to revise your changes and
resubmit them to Gerrit.  This should be done by modifying your change with
`git commit --amend`, not by submitting a new change or doing a forced push.
The revised change will have a new SHA1, but should contain the same
`Change-Id` in the commit message.

Be sure you have the existing change checked out in your local repository. You
can fetch the existing change with git.  See the **Download** tab for various
options on how to checkout the existing change.  For example:

    $ git fetch https://gerrit.openafs.org/openafs refs/changes/<nn>/<number>/<patchset>
    $ git checkout FETCH_HEAD

Now that you have the commits checked out, you can make your changes locally. If
the change is a single commit, edit the files, and then amend the commit.

    $ git add -p  # verify each change
    $ git commit --amend

Update the commit message if necessary, but **do not** change or remove the
`Change-Id` line at the bottom of the commit message.

When you have completed your local change, push the new patchset to Gerrit,
just like you did originally.

    $ git push gerrit HEAD:refs/for/master

If your change consist of multiple commits, then use `git rebase -i` to
interactively edit each commit that needs to be updated.

You may also need to rebase (or cherry-pick) your changes to an updated master
branch, if the master branch has changed since your last push to Gerrit. In
this case, fetch the current master branch and rebase your changes on the updated
branch. Usually this can be done with a single `git pull --rebase`.
