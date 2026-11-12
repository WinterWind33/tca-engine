# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT
"""Promote the changelog's ``## [Unreleased]`` section into a numbered release section.

``CHANGELOG.md`` is hand-written here: contributors add entries under ``## [Unreleased]`` in the
pull request that makes the change, and release-please is configured with ``skip-changelog`` so it
never rewrites the file. Something still has to turn ``## [Unreleased]`` into
``## [X.Y.Z] - YYYY-MM-DD`` when a release is cut -- this script is that step.

It is run by ``.github/workflows/release-please.yml`` against release-please's own release branch,
so the promotion lands inside the release pull request, alongside the version bump. That ordering
matters: it means the tagged commit contains the changelog section describing itself.

See ``docs/project/releasing.md`` for the whole flow.
"""

from __future__ import annotations

import argparse
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

# Semantic Versioning 2.0.0, without build metadata: release-please never produces a `+build` tag.
SEMVER_PATTERN = (
    r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)"
    r"(?:-(?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*)"
    r"(?:\.(?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*))*)?"
)
SEMVER_RE = re.compile(rf"^{SEMVER_PATTERN}$")

# The heading must be exactly "## [Unreleased]". Trailing whitespace is tolerated, leading is not:
# an indented heading is not a heading as far as Keep a Changelog is concerned.
UNRELEASED_RE = re.compile(r"^## \[Unreleased\][ \t]*$", re.MULTILINE)

# Any level-2 heading, used to find where the Unreleased section stops.
SECTION_RE = re.compile(r"^## ", re.MULTILINE)

PROMOTED = "promoted"
ALREADY_PRESENT = "already-present"
EMPTY = "empty"


class ChangelogError(RuntimeError):
    """Raised when the changelog is not in a shape this script can safely edit."""


def normalise_version(version: str) -> str:
    """Strip a leading ``v`` and validate the result as a semantic version."""
    candidate = version[1:] if version.startswith("v") else version
    if not SEMVER_RE.match(candidate):
        raise ChangelogError(f"'{version}' is not a valid semantic version")
    return candidate


def unreleased_body(content: str) -> str:
    """Return the text between the ``## [Unreleased]`` heading and the next level-2 heading."""
    match = UNRELEASED_RE.search(content)
    if match is None:
        raise ChangelogError("no '## [Unreleased]' section found")

    rest = content[match.end() :]
    following = SECTION_RE.search(rest)
    return rest if following is None else rest[: following.start()]


def promote(content: str, version: str, date: str) -> tuple[str, str]:
    """Promote the Unreleased section, returning the new content and what was done.

    The status is one of ``PROMOTED``, ``ALREADY_PRESENT`` (a section for this version already
    exists) or ``EMPTY`` (nothing has been written under Unreleased). Both of the latter leave the
    content untouched, which makes the script safe to re-run.
    """
    version = normalise_version(version)

    if re.search(rf"^## \[{re.escape(version)}\]", content, re.MULTILINE):
        return content, ALREADY_PRESENT

    body = unreleased_body(content)
    if not body.strip():
        # Deliberately not an error, and deliberately not an empty section: a release with no
        # hand-written entries is legitimate (a dependency bump, a CI fix), and a version heading
        # with nothing under it is worse than no heading at all.
        return content, EMPTY

    promoted = UNRELEASED_RE.sub(
        f"## [Unreleased]\n\n## [{version}] - {date}", content, count=1
    )
    return promoted, PROMOTED


def read_changelog(path: Path) -> tuple[str, str]:
    """Plain open(newline="") rather than Path.read_text(newline=...), which is 3.13+.

    Returns the changelog text with ``\n`` endings plus the endings it really uses."""
    with open(path, "r", encoding="utf-8", newline="") as handle:
        raw = handle.read()
    newline = "\r\n" if "\r\n" in raw else "\n"
    return raw.replace("\r\n", "\n"), newline


def write_changelog(path: Path, content: str, newline: str) -> None:
    if newline != "\n":
        content = content.replace("\n", newline)
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(content)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--version",
        required=True,
        help="version being released, with or without a leading 'v' (e.g. 0.2.5)",
    )
    parser.add_argument(
        "--changelog",
        type=Path,
        default=Path("CHANGELOG.md"),
        help="path to the changelog (default: CHANGELOG.md in the working directory)",
    )
    parser.add_argument(
        "--date",
        default=None,
        help="release date as YYYY-MM-DD (default: today, UTC)",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    date = args.date or datetime.now(timezone.utc).strftime("%Y-%m-%d")

    try:
        content, newline = read_changelog(args.changelog)
        updated, status = promote(content, args.version, date)
    except ChangelogError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except OSError as error:
        print(f"error: could not read '{args.changelog}': {error}", file=sys.stderr)
        return 1

    if status == PROMOTED:
        write_changelog(args.changelog, updated, newline)
        print(f"Promoted [Unreleased] to [{normalise_version(args.version)}] - {date}")
    elif status == ALREADY_PRESENT:
        print(f"Nothing to do: a section for {args.version} already exists")
    else:
        print("Nothing to do: [Unreleased] is empty")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
