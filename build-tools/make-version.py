#!/usr/bin/env python3
"""make-version.py - Bump the OpenAFS release version strings.

SYNOPSIS

    build-tools/make-version.py [--commit] VERSION

DESCRIPTION

Bump the version string numbers embedded in the source code to make a new
OpenAFS release.

This script handles for following types of releases:

  * final releases:   X.Y.Z      example: 1.8.17
  * pre-release:      X.Y.ZpreN  example: 1.8.17pre1
  * point release:    X.Y.Z.N    example: 1.8.16.1

These version numbers are converted to version strings that are
compatible for Apple macOS and Mircosoft Windows packaging.

For macOS, the following conversions are done:

  * final releases:     X.Y.Z     -> X.Y.Z
  * pre-releases:       X.Y.ZpreN -> X.Y.ZfcN
  * point releases:     X.Y.Z.N   -> X.Y.(Z+1)dN

Note: The MACOS_VERSION point release continues the next minor's dev cycle.

For Microsoft Windows, the version information is set in the
AFSPRODUCT_VER_MAJOR, AFSPRODUCT_VER_MINOR, and AFSPRODUCT_VER_PATCH macros.

  * final releases:     X.Y.Z     -> MAJOR=X, MINOR=Y, PATCH=ZZBB
  * pre-releases:       X.Y.ZpreN -> MAJOR=X, MINOR=Y, PATCH=ZZBB
  * point releases:     X.Y.Z.N   -> MAJOR=X, MINOR=Y, PATCH=ZZBB

Note: MAJOR and MINOR are just set to X and Y. BB is a build counter that is
not derived from the version being made. Instead, this script reads the current
AFSPRODUCT_VER_PATCH value already in the NTMakefile files to find the previous
MAJOR.MINOR.ZZ.BB, then: if the new MAJOR.MINOR.ZZ matches that previous one,
BB is the previous BB plus one; otherwise BB resets to 00.

EXAMPLES

macOS (MACOS_VERSION in configure.ac and configure-libafs.ac):

    make-version.py 1.8.17      ->  MACOS_VERSION=1.8.17
    make-version.py 1.8.17pre1  ->  MACOS_VERSION=1.8.17fc1
    make-version.py 1.8.16.1    ->  MACOS_VERSION=1.8.17d1

Windows (AFSPRODUCT_VER_MAJOR/MINOR/PATCH in the NTMakefile files), showing
how the build counter (BB) behaves across successive runs. Starting from
MAJOR=1 MINOR=8 PATCH=1600 (i.e. 1.8.16 build 00):

    make-version.py 1.8.17pre1  ->  MAJOR=1 MINOR=8 PATCH=1700
                                    (new ZZ=17, so BB resets to 00)
    make-version.py 1.8.17pre2  ->  MAJOR=1 MINOR=8 PATCH=1701
                                    (still ZZ=17, so BB bumps to 01)
    make-version.py 1.8.17      ->  MAJOR=1 MINOR=8 PATCH=1702
                                    (still ZZ=17, so BB bumps to 02)
    make-version.py 1.8.17.1    ->  MAJOR=1 MINOR=8 PATCH=1703
                                    (still ZZ=17, so BB bumps to 03)

"""

import argparse
import re
import subprocess
import sys
from collections import namedtuple
from pathlib import Path

VERSION_RE = re.compile(
    r'^(?P<major>\d+)\.(?P<minor>\d+)\.(?P<patch>\d+)'
    r'(?:pre(?P<pre>\d+)|\.(?P<point>\d+))?$'
)

# The OpenAFS release version being made: kind is 'pre', 'point', or
# 'final'; n is the pre-release or point-release number, or None.
Version = namedtuple('Version', ['major', 'minor', 'patch', 'kind', 'n'])

MACOS_FILES = ['configure.ac', 'configure-libafs.ac']
WINNT_FILES = [
    'src/config/NTMakefile.amd64_w2k',
    'src/config/NTMakefile.i386_nt40',
    'src/config/NTMakefile.i386_w2k',
]

MACOS_VERSION_RE = re.compile(r'^MACOS_VERSION=.*$', re.MULTILINE)
WIN_MAJOR_RE = re.compile(r'^AFSPRODUCT_VER_MAJOR=(\d+)$', re.MULTILINE)
WIN_MINOR_RE = re.compile(r'^AFSPRODUCT_VER_MINOR=(\d+)$', re.MULTILINE)
WIN_PATCH_RE = re.compile(r'^AFSPRODUCT_VER_PATCH=(\d+)$', re.MULTILINE)


class VersionError(Exception):
    """A malformed version string, or inconsistent NTMakefile state."""


def parse_version(text):
    """Parse a release version string into a Version tuple."""
    m = VERSION_RE.match(text)
    if not m:
        raise VersionError(
            f"can't parse version {text!r}; expected X.Y.Z, X.Y.ZpreN, "
            "or X.Y.Z.N")
    major = int(m.group('major'))
    minor = int(m.group('minor'))
    patch = int(m.group('patch'))
    if m.group('pre') is not None:
        return Version(major, minor, patch, 'pre', int(m.group('pre')))
    if m.group('point') is not None:
        return Version(major, minor, patch, 'point', int(m.group('point')))
    return Version(major, minor, patch, 'final', None)


def git(root, *args, check=True):
    """Run a git command in the given repo."""
    return subprocess.run(['git', *args], cwd=root, check=check)


def repo_root():
    """Return the path to the top of the git repo."""
    out = subprocess.run(
        ['git', 'rev-parse', '--show-toplevel'],
        capture_output=True, text=True, check=True)
    return Path(out.stdout.strip())


def change_macos_version(root, version):
    """Update MACOS_VERSION in the macOS build files for the release."""

    # Determine the macOS version string, which depends on the kind of version.
    if version.kind == 'pre':
        macos_version = f"{version.major}.{version.minor}.{version.patch}fc{version.n}"
    elif version.kind == 'final':
        macos_version = f"{version.major}.{version.minor}.{version.patch}"
    elif version.kind == 'point':
        macos_version = f"{version.major}.{version.minor}.{version.patch + 1}d{version.n}"
    else:
        raise AssertionError(version.kind)

    # Write the new version string.
    for file in MACOS_FILES:
        path = root / file
        text = path.read_text()
        text, count = MACOS_VERSION_RE.subn(f'MACOS_VERSION={macos_version}', text, count=1)
        if count != 1:
            raise VersionError(f"no MACOS_VERSION line found in {file}")
        path.write_text(text)


def change_win_version(root, version):
    """Bump the AFSPRODUCT_VER_MAJOR/MINOR/PATCH macros in the NTMakefile
    files for the release, carrying forward the build counter (BB)."""

    # Read the existing version info. This is duplicated in different files,
    # so bail if a mismatch is found.
    old_versions = {}
    for file in WINNT_FILES:
        text = (root / file).read_text()
        m = WIN_MAJOR_RE.search(text)
        if not m:
            raise VersionError(f"no AFSPRODUCT_VER_MAJOR line found in {file}")
        m2 = WIN_MINOR_RE.search(text)
        if not m2:
            raise VersionError(f"no AFSPRODUCT_VER_MINOR line found in {file}")
        m3 = WIN_PATCH_RE.search(text)
        if not m3:
            raise VersionError(f"no AFSPRODUCT_VER_PATCH line found in {file}")
        old_versions[file] = (int(m.group(1)), int(m2.group(1)), int(m3.group(1)))
    if len(set(old_versions.values())) != 1:
        raise VersionError(
            f"AFSPRODUCT_VER_MAJOR/MINOR/PATCH disagrees across NTMakefile files: {old_versions}")
    old_version = list(old_versions.values())[0]

    # Determine the new AFSPRODUCT_VER_PATCH.  The AFSPRODUCT_VER_PATCH is
    # encoded as ZZBB: ZZ is the release's patch number (X.Y.Z), and BB is a
    # build counter which just increments each time we make another release
    # on the same major.minor.ZZ train (pre-release, point release, or
    # final).
    #
    # Split the old ZZBB apart and then if old major.minor.ZZ matches the
    # major.minor.Z we are making now, bump BB by one to record another build
    # of it; otherwise a new ZZ is starting, so reset BB to 00. Recombine into
    # the new ZZBB.
    old_major, old_minor, old_patch = old_version
    old_zz = old_patch // 100
    old_bb = old_patch % 100
    if (old_major, old_minor, old_zz) == (version.major, version.minor, version.patch):
        new_bb = old_bb + 1
    else:
        new_bb = 0
    if version.patch > 99 or new_bb > 99:
        raise VersionError(
            f"AFSPRODUCT_VER_PATCH component out of range: "
            f"{version.patch:02d}{new_bb:02d} (need two digits each)")
    new_patch = version.patch * 100 + new_bb

    # Write the new version info.
    for file in WINNT_FILES:
        path = root / file
        text = path.read_text()
        text, count = WIN_MAJOR_RE.subn(f'AFSPRODUCT_VER_MAJOR={version.major}', text, count=1)
        if count != 1:
            raise VersionError(f"no AFSPRODUCT_VER_MAJOR line found in {file}")
        text, count = WIN_MINOR_RE.subn(f'AFSPRODUCT_VER_MINOR={version.minor}', text, count=1)
        if count != 1:
            raise VersionError(f"no AFSPRODUCT_VER_MINOR line found in {file}")
        text, count = WIN_PATCH_RE.subn(f'AFSPRODUCT_VER_PATCH={new_patch:04d}', text, count=1)
        if count != 1:
            raise VersionError(f"no AFSPRODUCT_VER_PATCH line found in {file}")
        path.write_text(text)


def commit(root, version):
    """Commit the staged version file changes, if any."""
    all_files = MACOS_FILES + WINNT_FILES
    git(root, 'add', *all_files)
    nothing_staged = git(root, 'diff', '--cached', '--quiet', '--', *all_files,
        check=False).returncode == 0
    if nothing_staged:
        print("Skipping commit; nothing staged.")
    else:
        subject = f"Make OpenAFS {version}"
        body = f"Update version strings for the {version} release."
        git(root, 'commit', '-m', f"{subject}\n\n{body}")


def main():
    """Parse the command line and bump the release version strings."""
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        'version',
        help="OpenAFS version, X.Y.Z (final), X.Y.ZpreN (prerelease), or "
             "X.Y.Z.N (point release)")
    ap.add_argument(
        '--commit', action='store_true',
        help="Create git commit with a 'Make OpenAFS <version>' commit message")
    args = ap.parse_args()

    try:
        version = parse_version(args.version)
        root = repo_root()
        change_macos_version(root, version)
        change_win_version(root, version)

        if args.commit:
            commit(root, args.version)
    except VersionError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
