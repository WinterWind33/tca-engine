# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT
"""Tests for promote_changelog.py. Standard library only -- see .github/workflows/test-scripts.yml."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from promote_changelog import (
    ALREADY_PRESENT,
    EMPTY,
    PROMOTED,
    ChangelogError,
    main,
    normalise_version,
    promote,
    unreleased_body,
)

HEADER = """# Changelog

All notable changes to this project will be documented in this file.

"""

POPULATED = (
    HEADER
    + """## [Unreleased]

### Fixed

- Something that was broken.

## [0.2.4](https://example.invalid/compare/v0.2.3...v0.2.4) (2026-08-26)

### Bug Fixes

* an earlier fix
"""
)

EMPTY_UNRELEASED = (
    HEADER
    + """## [Unreleased]

## [0.2.4] - 2026-08-26

### Fixed

- an earlier fix
"""
)


class PromoteTests(unittest.TestCase):
    def test_promotes_populated_section(self) -> None:
        updated, status = promote(POPULATED, "0.2.5", "2026-08-26")

        self.assertEqual(status, PROMOTED)
        self.assertIn("## [Unreleased]\n\n## [0.2.5] - 2026-08-26\n", updated)

    def test_entries_move_under_the_new_version(self) -> None:
        updated, _ = promote(POPULATED, "0.2.5", "2026-08-26")

        body_after = unreleased_body(updated)
        self.assertEqual(body_after.strip(), "")
        self.assertIn(
            "## [0.2.5] - 2026-08-26\n\n### Fixed\n\n- Something that was broken.", updated
        )

    def test_leaves_earlier_sections_untouched(self) -> None:
        updated, _ = promote(POPULATED, "0.2.5", "2026-08-26")

        self.assertIn("## [0.2.4](https://example.invalid/compare/v0.2.3...v0.2.4)", updated)
        self.assertIn("* an earlier fix", updated)

    def test_empty_unreleased_is_a_no_op(self) -> None:
        updated, status = promote(EMPTY_UNRELEASED, "0.2.5", "2026-08-26")

        self.assertEqual(status, EMPTY)
        self.assertEqual(updated, EMPTY_UNRELEASED)

    def test_is_idempotent(self) -> None:
        once, _ = promote(POPULATED, "0.2.5", "2026-08-26")
        twice, status = promote(once, "0.2.5", "2026-08-27")

        self.assertEqual(status, ALREADY_PRESENT)
        self.assertEqual(twice, once)

    def test_promotes_only_the_first_unreleased_heading(self) -> None:
        stray = POPULATED + "\n## [Unreleased]\n\n- a stray older section\n"

        updated, _ = promote(stray, "0.2.5", "2026-08-26")

        self.assertEqual(updated.count("## [Unreleased]"), 2)
        self.assertEqual(updated.count("## [0.2.5] - 2026-08-26"), 1)

    def test_missing_unreleased_section_is_an_error(self) -> None:
        with self.assertRaises(ChangelogError):
            promote(HEADER + "## [0.2.4] - 2026-08-26\n", "0.2.5", "2026-08-26")

    def test_indented_heading_is_not_a_section(self) -> None:
        with self.assertRaises(ChangelogError):
            promote(HEADER + "  ## [Unreleased]\n\n- indented\n", "0.2.5", "2026-08-26")


class VersionTests(unittest.TestCase):
    def test_accepts_plain_and_prefixed_versions(self) -> None:
        self.assertEqual(normalise_version("0.2.5"), "0.2.5")
        self.assertEqual(normalise_version("v0.2.5"), "0.2.5")

    def test_accepts_prerelease_versions(self) -> None:
        self.assertEqual(normalise_version("1.0.0-alpha.2"), "1.0.0-alpha.2")

    def test_rejects_malformed_versions(self) -> None:
        for bad in ("", "1.0", "1.0.0.0", "01.0.0", "junk", "1.0.0-"):
            with self.subTest(version=bad), self.assertRaises(ChangelogError):
                normalise_version(bad)


class MainTests(unittest.TestCase):
    def _write(self, content: str, newline: str = "\n") -> Path:
        directory = tempfile.mkdtemp()
        path = Path(directory) / "CHANGELOG.md"
        with open(path, "w", encoding="utf-8", newline="") as handle:
            handle.write(content.replace("\n", newline))
        return path

    def test_writes_the_promoted_file(self) -> None:
        path = self._write(POPULATED)

        exit_code = main(["--version", "0.2.5", "--changelog", str(path), "--date", "2026-08-26"])

        self.assertEqual(exit_code, 0)
        self.assertIn("## [0.2.5] - 2026-08-26", path.read_text(encoding="utf-8"))

    def test_preserves_crlf_line_endings(self) -> None:
        path = self._write(POPULATED, newline="\r\n")

        main(["--version", "0.2.5", "--changelog", str(path), "--date", "2026-08-26"])

        with open(path, "r", encoding="utf-8", newline="") as handle:
            raw = handle.read()
        self.assertIn("\r\n", raw)
        self.assertNotIn("\n\n\n", raw.replace("\r\n", "\n").replace("\n\n\n", ""))
        self.assertEqual(raw.count("\n"), raw.count("\r\n"))

    def test_reports_failure_for_a_bad_version(self) -> None:
        path = self._write(POPULATED)

        self.assertEqual(main(["--version", "nope", "--changelog", str(path)]), 1)

    def test_reports_failure_for_a_missing_file(self) -> None:
        self.assertEqual(main(["--version", "0.2.5", "--changelog", "does-not-exist.md"]), 1)


if __name__ == "__main__":
    unittest.main()
