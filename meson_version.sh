#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
#
# meson_version.sh
# Project version from git, or from the version data of a source archive.
#
#   meson_version.sh                   print the version (project() in meson.build)
#   meson_version.sh --dist <version>  write <version> into the `meson dist` tree
#
# Sources, first match wins:
#  1. git, when the source dir is the top of its own work tree (not an
#     archive unpacked inside some other repository).
#  2. .tarball-version, written by --dist into `meson dist` tarballs.
#  3. .archive-version, filled in by `git archive` through export-subst
#     (also GitHub "Source code" downloads).
# Anything else is an error, never a guess.
#
# Versions ("+" part is SemVer build metadata):
#   HEAD tagged vX.Y.Z         -> X.Y.Z
#   N commits after vX.Y.Z     -> X.Y.Z+N.g<hash>
#   no vX.Y.Z tag reachable    -> 0.0.0+N.g<hash>  (git archive: 0.0.0+g<hash>,
#                                 the commit count is not available there)
#   no commit yet              -> 0.0.0+unborn
#   uncommitted changes        -> <any of the above> + ".dirty" metadata

set -eu

src=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
tarball_file=.tarball-version
archive_file=.archive-version

# $1: X.Y.Z, $2: commits past the tag ("" if unknown), $3: hash,
# $4: "dirty" or "".
compose() {
    meta=
    [ "$2" = 0 ] || meta=${2:+$2.}g$3
    [ -z "$4" ] || meta=${meta:+$meta.}dirty
    echo "$1${meta:++$meta}"
}

# $1: "dirty" to flag uncommitted changes, "clean" to ignore them.
git_version() {
    command -v git >/dev/null 2>&1 || return 1
    prefix=$(git -C "$src" rev-parse --show-prefix 2>/dev/null) || return 1
    [ -z "$prefix" ] || return 1

    if ! git -C "$src" rev-parse -q --verify HEAD >/dev/null; then
        echo "0.0.0+unborn"
        return 0
    fi

    hash=$(git -C "$src" rev-parse --short=12 HEAD) || return 1
    if desc=$(git -C "$src" describe --tags --long --match 'v[0-9]*' HEAD 2>/dev/null); then
        desc=${desc%-g*}        # vX.Y.Z-N
        count=${desc##*-}
        base=${desc%-*}
        base=${base#v}
    else
        count=$(git -C "$src" rev-list --count HEAD) || return 1
        base=0.0.0
    fi

    dirty=
    if [ "$1" = dirty ] &&
        [ -n "$(git -C "$src" --no-optional-locks status --porcelain --untracked-files=no)" ]; then
        dirty=dirty
    fi
    compose "$base" "$count" "$hash" "$dirty"
}

# Placeholders left unexpanded (plain copy of a checkout) or a git too old
# for %(describe) mean there is no usable version here.
archive_version() {
    [ -f "$src/$archive_file" ] || return 1
    commit=$(sed -n 's/^commit: //p' "$src/$archive_file")
    desc=$(sed -n 's/^describe: //p' "$src/$archive_file")
    case $commit in
    *[!0-9a-f]* | '') return 1 ;;
    esac
    hash=$(echo "$commit" | cut -c1-12)
    case $desc in
    '')
        compose 0.0.0 '' "$hash" '' ;;
    v[0-9]*-[0-9]*-g[0-9a-f]*)
        desc=${desc%-g*}        # vX.Y.Z-N
        base=${desc%-*}
        compose "${base#v}" "${desc##*-}" "$hash" '' ;;
    v[0-9]*)
        compose "${desc#v}" 0 "$hash" '' ;;
    *)
        return 1 ;;
    esac
}

case ${1:-} in
'')
    if version=$(git_version dirty); then
        :
    elif [ -f "$src/$tarball_file" ]; then
        version=$(cat "$src/$tarball_file")
    elif version=$(archive_version); then
        :
    else
        echo "meson_version.sh: no version for $src: not a git checkout, a" \
             "'meson dist' tarball ($tarball_file) or a git archive" \
             "($archive_file)" >&2
        exit 1
    fi
    echo "$version"
    ;;
--dist)
    # `meson dist` archives HEAD and names the tarball after the configured
    # version. Both must match the embedded one: a build dir configured at
    # another commit, or with uncommitted changes, is refused.
    configured=${2:?usage: meson_version.sh --dist <configured version>}
    dist_root=${MESON_PROJECT_DIST_ROOT:-${MESON_DIST_ROOT:?not run by meson dist}}
    version=$(git_version clean) || {
        echo "meson_version.sh: --dist needs the git work tree at $src" >&2
        exit 1
    }
    if [ "$version" != "$configured" ]; then
        echo "meson_version.sh: build dir configured as $configured but HEAD is" \
             "$version; commit or stash, then 'meson setup --reconfigure'" >&2
        exit 1
    fi
    echo "$version" > "$dist_root/$tarball_file"
    ;;
*)
    echo "usage: meson_version.sh [--dist <configured version>]" >&2
    exit 2
    ;;
esac
