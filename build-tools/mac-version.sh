#!/bin/sh
#
# Convert the OpenAFS version to the macOS packaging version (MACOS_VERSION).
#
# usage: mac-version.sh <version>
#
# <version> is an OpenAFS version string, as printed by git-version.
#
# For example, to print the macOS version of the current source tree:
#
#   ./build-tools/mac-version.sh `./build-tools/git-version .`
#
# macOS requires versions in the form N.N.N[{d|a|b|fc}N], so the OpenAFS
# version is converted as follows:
#
#   pre-release:      X.Y.ZpreN  ->  X.Y.ZfcN      sorts before X.Y.Z
#   regular release:  X.Y.Z      ->  X.Y.Z
#   point release:    X.Y.Z.N    ->  X.Y.(Z+1)dN   sorts after X.Y.Z and
#                                                  before X.Y.(Z+1)
#
# Builds made between release tags (X.Y.Z-N-gHASH) and from modified trees
# (X.Y.Z-N-gHASH-dirty) are given the version of the most recent release tag;
# that is, the -N-gHASH[-dirty] suffix is ignored.
#

if [ $# -ne 1 ]; then
    echo "usage: $0 <version>" >&2
    exit 1
fi
version=$1

# Strip the git describe suffix when building between releases.
v=`echo "$version" | sed -e 's/-.*//'`

# Accepted version patterns.
digits='[0-9][0-9]*'
x_y_z="${digits}\.${digits}\.${digits}"
x_y_z_pre_n="${x_y_z}pre${digits}"
x_y_z_n="${x_y_z}\.${digits}"

# Convert the OpenAFS version to the macOS version string.
if echo "$v" | grep -q "^${x_y_z}\$"; then
    # Regular release: X.Y.Z -> X.Y.Z
    macos_version="$v"
elif echo "$v" | grep -q "^${x_y_z_pre_n}\$"; then
    # Pre-release: X.Y.ZpreN -> X.Y.ZfcN
    macos_version=`echo "$v" | sed -e 's/pre/fc/'`
elif echo "$v" | grep -q "^${x_y_z_n}\$"; then
    # Point release: X.Y.Z.N -> X.Y.(Z+1)dN
    x=`echo "$v" | cut -f1 -d.`
    y=`echo "$v" | cut -f2 -d.`
    z=`echo "$v" | cut -f3 -d.`
    n=`echo "$v" | cut -f4 -d.`
    z_next=`expr $z + 1`
    macos_version="${x}.${y}.${z_next}d${n}"
else
    echo "$0: Cannot convert version '$version' to a macOS version;" \
	 "expected X.Y.Z, X.Y.ZpreN, or X.Y.Z.N" >&2
    macos_version="UNKNOWN"
fi

echo "$macos_version" | tr -d '\n'
