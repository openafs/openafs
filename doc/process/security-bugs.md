# Security Bugs

If you think you have found a security bug, do **NOT** submit a bug report to
`openafs-bugs@openafs.org` or any public forum, and do not submit a fix to
Gerrit. Instead, please report it privately to the OpenAFS security team.

Any bug that may be potentially exploited by malicious users to intentionally
escalate privileges or disrupt services may be a security bug. Bugs with minor
security impact may not warrant special treatment; if you're unsure, report the
bug to the OpenAFS security team, and we will tell you if it can just be
treated as a normal bug.

## How to report a security bug

If you suspect you have discovered a security bug, please report it to the
OpenAFS security officer at <security@openafs.org>. Please encrypt sensitive
information with PGP (GPG) when sending it. Either use an email client that has
PGP capabilities or encrypt the contents before sending.

When encrypting a bug report, use the PGP key that has signed the most recent
security advisory on <https://www.openafs.org/security> (currently, that is
`D961 95E0 4D80 45FF 4160  FD17 28D9 A6F3 64EB 7512`).

A private ticket will be created with limited access when your mail is
received. The security team will verify the bug report and develop and release
a fix.  If you already have a fix, please include it with your report, as that
can speed up the process considerably.  It can also be helpful to provide an
example program illustrating the security issue.

## Security release

The security team creates special security releases to address reported
security issues.  A security advisory describing the issue and the patches are
published on the OpenAFS web site at the same time as the security release.
Information on previous security releases is published on
<https://www.openafs.org/security>.
