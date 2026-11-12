# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT
"""Check that a pull request will produce a commit message release-please understands.

PRs here are squash-merged, so the PR title becomes the commit message on ``main``, and that
message is what release-please reads to decide whether -- and how -- to cut a release. A title
without a Conventional Commit type (``Scene add camera 2d ...``, as one PR was once merged) is silently
ignored: nothing fails, the release just never happens.

release-please also honours a ``BEGIN_COMMIT_OVERRIDE`` ... ``END_COMMIT_OVERRIDE`` block in the
PR description, reading it in place of the squashed commit message. A PR carrying a valid one
passes too, whatever its title says.

Run by ``.github/workflows/pr-title.yml``. See ``docs/project/releasing.md``.
"""

from __future__ import annotations

import argparse
import re
import sys

# The types release-please's default changelog configuration recognises. Anything else parses as
# a Conventional Commit but is dropped, so it would fail in exactly the way this check exists for.
TYPES = (
    "feat",
    "fix",
    "perf",
    "revert",
    "docs",
    "style",
    "chore",
    "refactor",
    "test",
    "build",
    "ci",
    "deps",
)

# `type(scope)!: description`. The scope is optional and may not be empty or nest parentheses;
# the description must start right after a single space.
HEADER_RE = re.compile(rf"^(?:{'|'.join(TYPES)})(?:\([^()\s][^()]*\))?!?: \S")

OVERRIDE_RE = re.compile(r"BEGIN_COMMIT_OVERRIDE(.*?)END_COMMIT_OVERRIDE", re.DOTALL)


def is_conventional(header: str) -> bool:
    """Whether ``header`` is a Conventional Commit header with a type release-please acts on."""
    return HEADER_RE.match(header.strip()) is not None


def override_header(body: str | None) -> str | None:
    """The first line of the description's commit override, or None when there is no override.

    An override that is present but empty returns ``""``, so it fails rather than falling back to
    the title: release-please would not fall back either.
    """
    match = OVERRIDE_RE.search(body or "")
    if match is None:
        return None
    lines = [line.strip() for line in match.group(1).splitlines() if line.strip()]
    return lines[0] if lines else ""


def check(title: str, body: str | None) -> str | None:
    """Return None when the PR is fine, or a message explaining what to fix."""
    header = override_header(body)
    if header is not None:
        if is_conventional(header):
            return None
        return (
            f"The BEGIN_COMMIT_OVERRIDE block in the PR description starts with {header!r}, which "
            "is not a Conventional Commit release-please understands. release-please reads the "
            "override instead of the title, so fix the override (or remove it)."
        )

    if is_conventional(title):
        return None
    return (
        f"The PR title {title!r} is not a Conventional Commit release-please understands. PRs are "
        "squash-merged, so the title becomes the commit message release-please reads, and without "
        "a type it will not cut a release. Retitle it as `<type>[(scope)][!]: <description>`, "
        f"where <type> is one of: {', '.join(TYPES)}. For example: `feat: add a 2D camera`."
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--title", required=True, help="the pull request title")
    parser.add_argument("--body", default="", help="the pull request description")
    args = parser.parse_args(argv)

    problem = check(args.title, args.body)
    if problem is None:
        print(f"OK: {args.title!r}")
        return 0

    # `::error::` turns the message into an annotation on the run summary, so it is readable
    # straight from the PR's checks list without opening the log.
    print(f"::error title=PR title is not a Conventional Commit::{problem}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
