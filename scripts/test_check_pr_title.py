# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT
"""Tests for check_pr_title.py. Standard library only -- see .github/workflows/test-scripts.yml."""

from __future__ import annotations

import contextlib
import io
import unittest

from check_pr_title import check, is_conventional, main, override_header


class IsConventionalTests(unittest.TestCase):
    def test_accepts_every_supported_shape(self) -> None:
        for title in (
            "feat: add a 2D camera",
            "fix: stop leaking the swap chain",
            "feat!: run every update() before every render()",
            "feat(scene): add a 2D camera",
            "fix(rendering)!: drop the old backend",
            "chore(main): release 0.10.0",
            "ci: check PR titles",
            "deps: bump spdlog",
        ):
            with self.subTest(title=title):
                self.assertTrue(is_conventional(title))

    def test_rejects_titles_release_please_would_ignore(self) -> None:
        for title in (
            # A title that was once merged like this, which is why this check exists.
            "Scene add camera 2d with a tested worldscreen transform",
            "",
            "feat",
            "feat:",
            "feat: ",
            "feat:add a camera",
            "Feat: add a camera",
            "feature: add a camera",
            "wip: add a camera",
            "feat(): add a camera",
            "feat(a(b)): add a camera",
            "feat (scene): add a camera",
            "[TDD] feat: add a camera",
        ):
            with self.subTest(title=title):
                self.assertFalse(is_conventional(title))


class OverrideHeaderTests(unittest.TestCase):
    def test_no_override(self) -> None:
        self.assertIsNone(override_header(None))
        self.assertIsNone(override_header(""))
        self.assertIsNone(override_header("Just a description."))

    def test_first_non_blank_line_of_the_override(self) -> None:
        # The shape used to fix a badly titled merge after the fact.
        body = (
            "BEGIN_COMMIT_OVERRIDE\n"
            "\n"
            "feat!: add a fixed timestep updatable (#76)\n"
            "\n"
            "BREAKING CHANGE: ranged_value bounds are auto template parameters.\n"
            "END_COMMIT_OVERRIDE\n"
            "\n"
            "Rest of the description."
        )
        self.assertEqual(override_header(body), "feat!: add a fixed timestep updatable (#76)")

    def test_crlf_description(self) -> None:
        body = "BEGIN_COMMIT_OVERRIDE\r\nfeat: add a camera\r\nEND_COMMIT_OVERRIDE\r\n"
        self.assertEqual(override_header(body), "feat: add a camera")

    def test_empty_override(self) -> None:
        self.assertEqual(override_header("BEGIN_COMMIT_OVERRIDE\n\nEND_COMMIT_OVERRIDE"), "")


class CheckTests(unittest.TestCase):
    def test_valid_title_passes(self) -> None:
        self.assertIsNone(check("feat: add a camera", ""))

    def test_invalid_title_fails_with_a_fix(self) -> None:
        problem = check("Add a camera", "")
        self.assertIsNotNone(problem)
        self.assertIn("'Add a camera'", problem)
        self.assertIn("feat: add a 2D camera", problem)

    def test_valid_override_rescues_an_invalid_title(self) -> None:
        body = "BEGIN_COMMIT_OVERRIDE\nfeat: add a camera\nEND_COMMIT_OVERRIDE"
        self.assertIsNone(check("Add a camera", body))

    def test_invalid_override_fails_even_with_a_valid_title(self) -> None:
        # release-please reads the override, not the title, so a good title does not help.
        body = "BEGIN_COMMIT_OVERRIDE\nAdd a camera\nEND_COMMIT_OVERRIDE"
        problem = check("feat: add a camera", body)
        self.assertIsNotNone(problem)
        self.assertIn("BEGIN_COMMIT_OVERRIDE", problem)

    def test_empty_override_fails(self) -> None:
        self.assertIsNotNone(check("feat: add a camera", "BEGIN_COMMIT_OVERRIDE\nEND_COMMIT_OVERRIDE"))


class MainTests(unittest.TestCase):
    def run_main(self, *argv: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = main(list(argv))
        return code, out.getvalue()

    def test_pass(self) -> None:
        code, out = self.run_main("--title", "fix: something")
        self.assertEqual(code, 0)
        self.assertIn("OK", out)

    def test_fail_emits_an_error_annotation(self) -> None:
        code, out = self.run_main("--title", "Something", "--body", "")
        self.assertEqual(code, 1)
        self.assertTrue(out.startswith("::error "))


if __name__ == "__main__":
    unittest.main()
