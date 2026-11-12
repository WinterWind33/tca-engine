# Copyright (c) 2026 Andrea Ballestrazzi
# SPDX-License-Identifier: MIT
"""Tests for the shell steps in .github/workflows/release-please.yml.

Standard library only -- see .github/workflows/test-scripts.yml.

The release workflow has broken three times now, each time in a way that only shows up against
the real GitHub API during an actual release: 0.3.0 merged into a window where the PR was not yet
mergeable, and 0.5.0 first failed to find its own release PR (the label lookup went through
GitHub's eventually-consistent search index) and then merged against a stale mergeability verdict.
None of that is reachable from a unit test of a Python script, because the logic lives in the
workflow's `run:` blocks.

So this drives those blocks directly, with a stub `gh` on PATH that replays each of those
failures. The steps are read out of the workflow file itself rather than copied here, so a change
to the workflow is a change to what is under test.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

WORKFLOW = Path(__file__).resolve().parent.parent / ".github" / "workflows" / "release-please.yml"

# The stub replays a scenario picked by $GH_CASE, counting its own invocations in $GH_STATE so a
# case can answer differently on the first calls than on later ones -- which is what "GitHub has
# not caught up yet" looks like from inside the step.
GH_STUB = r"""#!/usr/bin/env bash
n=$(( $(cat "$GH_STATE" 2>/dev/null || echo 0) + 1 )); echo "$n" > "$GH_STATE"

if [ "$1" = "api" ]; then
  case "$GH_CASE" in
    found)    echo '[{"number":35,"headRefName":"release-please--branches--main","labels":["autorelease: pending"]}]' ;;
    late)     if [ "$n" -ge 3 ]; then
                echo '[{"number":35,"headRefName":"release-please--branches--main","labels":["autorelease: pending"]}]'
              else
                echo '[]'
              fi ;;
    none)     echo '[]' ;;
    imposter) echo '[{"number":99,"headRefName":"attacker/pwn","labels":["autorelease: pending"]},{"number":35,"headRefName":"release-please--branches--main","labels":[]}]' ;;
  esac
  exit 0
fi

if [ "$1" = "pr" ] && [ "$2" = "view" ]; then
  case "$GH_CASE" in
    stale_head) if [ "$n" -ge 3 ]; then echo "MERGEABLE CLEAN NEWSHA"; else echo "MERGEABLE CLEAN OLDSHA"; fi ;;
    unknown)    if [ "$n" -ge 3 ]; then echo "MERGEABLE CLEAN NEWSHA"; else echo "UNKNOWN UNKNOWN NEWSHA"; fi ;;
    conflict)   echo "CONFLICTING DIRTY NEWSHA" ;;
    apifail)    if [ "$n" -ge 2 ]; then echo "MERGEABLE CLEAN NEWSHA"; else exit 1; fi ;;
    *)          echo "MERGEABLE CLEAN NEWSHA" ;;
  esac
  exit 0
fi

if [ "$1" = "pr" ] && [ "$2" = "merge" ]; then
  case "$GH_MERGE" in
    flaky) if [ -f "$GH_MERGED_ONCE" ]; then
             echo merged; exit 0
           fi
           touch "$GH_MERGED_ONCE"; echo "GraphQL: Base branch was modified." >&2; exit 1 ;;
    never) echo "GraphQL: Base branch was modified." >&2; exit 1 ;;
    *)     echo merged; exit 0 ;;
  esac
fi
"""


def extract_run_block(workflow: str, step_name: str) -> str:
    """Return the body of the `run: |` block belonging to the named step.

    A targeted reader rather than a YAML parse, because these tests are standard-library only and
    the shape being read is fixed: `- name: <step>`, then that step's keys, then `run: |`
    followed by a more-indented literal block.
    """
    lines = workflow.splitlines()
    try:
        start = next(i for i, line in enumerate(lines) if line.strip() == f"- name: {step_name}")
    except StopIteration:
        raise AssertionError(f"no step named {step_name!r} in {WORKFLOW}") from None

    # Keys of this step are indented one level past the "- " that introduces it.
    key_indent = len(lines[start]) - len(lines[start].lstrip()) + 2

    for i in range(start + 1, len(lines)):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            continue
        indent = len(line) - len(line.lstrip())
        if indent < key_indent:
            break  # ran past the end of this step without finding a run block
        if indent == key_indent and stripped == "run: |":
            body: list[str] = []
            for follow in lines[i + 1:]:
                if follow.strip() and len(follow) - len(follow.lstrip()) <= key_indent:
                    break
                body.append(follow)
            return textwrap.dedent("\n".join(body)) + "\n"

    raise AssertionError(f"step {step_name!r} has no `run: |` block")


def _tools_available() -> bool:
    return bool(shutil.which("bash")) and bool(shutil.which("jq"))


@unittest.skipUnless(_tools_available(), "needs bash and jq, which the CI runner has")
class ReleaseWorkflowStepTestCase(unittest.TestCase):
    """Base fixture: one workflow step on disk, with a stub `gh` ahead of the real one on PATH."""

    step_name: str = ""

    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)

        stub_dir = root / "bin"
        stub_dir.mkdir()
        gh = stub_dir / "gh"
        gh.write_text(GH_STUB, newline="\n")
        gh.chmod(0o755)

        # The waits are what these steps are *for*; they are not what is being tested, and paying
        # them a dozen times per case would make this suite minutes long.
        script = extract_run_block(WORKFLOW.read_text(encoding="utf-8"), self.step_name)
        script = script.replace("sleep 5", "sleep 0").replace("sleep 10", "sleep 0")
        self.script = root / "step.sh"
        self.script.write_text(script, newline="\n")

        self.github_output = root / "github_output"
        self.github_output.touch()
        self.env = {
            **os.environ,
            "PATH": f"{stub_dir}{os.pathsep}{os.environ['PATH']}",
            "GH_STATE": str(root / "calls"),
            "GH_MERGED_ONCE": str(root / "merged_once"),
            "GITHUB_OUTPUT": str(self.github_output),
            "GH_TOKEN": "stub",
            "REPO": "owner/repo",
            "PR_NUMBER": "35",
        }

    def run_step(self, **env: str) -> tuple[int, str, dict[str, str]]:
        result = subprocess.run(
            ["bash", str(self.script)],
            env={**self.env, **env},
            capture_output=True,
            text=True,
        )
        outputs = dict(
            line.split("=", 1)
            for line in self.github_output.read_text().splitlines()
            if "=" in line
        )
        return result.returncode, result.stdout + result.stderr, outputs


class FindTheOpenReleasePRTests(ReleaseWorkflowStepTestCase):
    step_name = "Find the open release PR"

    def test_finds_the_release_pr(self) -> None:
        code, _, outputs = self.run_step(GH_CASE="found", PRS_CREATED="true")
        self.assertEqual(code, 0)
        self.assertEqual(outputs["number"], "35")
        self.assertEqual(outputs["branch"], "release-please--branches--main")

    def test_retries_until_the_pr_is_labelled(self) -> None:
        """The 0.5.0 failure: the PR existed, but was not yet visible as labelled."""
        code, _, outputs = self.run_step(GH_CASE="late", PRS_CREATED="true")
        self.assertEqual(code, 0)
        self.assertEqual(outputs["number"], "35")

    def test_no_release_pr_is_not_a_failure(self) -> None:
        code, stdout, outputs = self.run_step(GH_CASE="none", PRS_CREATED="false")
        self.assertEqual(code, 0)
        self.assertEqual(outputs["number"], "")
        self.assertIn("nothing to promote or merge", stdout)

    def test_does_not_wait_when_no_pr_was_created(self) -> None:
        """The run that finishes a release finds nothing pending; that is the answer, not a wait."""
        _, stdout, _ = self.run_step(GH_CASE="none", PRS_CREATED="false")
        self.assertNotIn("waiting for release-please", stdout)

    def test_waits_when_a_pr_was_just_created(self) -> None:
        _, stdout, _ = self.run_step(GH_CASE="none", PRS_CREATED="true")
        self.assertIn("(11/12)", stdout)

    def test_ignores_a_mislabelled_branch(self) -> None:
        """A pending label on a non-release branch must not buy an unattended merge into main."""
        code, stdout, outputs = self.run_step(GH_CASE="imposter", PRS_CREATED="false")
        self.assertEqual(code, 0)
        self.assertEqual(outputs["number"], "")
        self.assertIn("::warning::PR #99", stdout)


class AutoMergeReleasePRTests(ReleaseWorkflowStepTestCase):
    step_name = "Auto-merge release PR"

    def test_merges_when_every_signal_agrees(self) -> None:
        code, stdout, _ = self.run_step(GH_CASE="clean", GH_MERGE="ok", EXPECTED_HEAD="NEWSHA")
        self.assertEqual(code, 0)
        self.assertIn("merged", stdout)

    def test_waits_for_the_promoted_commit_to_become_the_head(self) -> None:
        """The 0.5.0 failure: `mergeable` still described the commit before the promotion."""
        code, stdout, _ = self.run_step(GH_CASE="stale_head", GH_MERGE="ok", EXPECTED_HEAD="NEWSHA")
        self.assertEqual(code, 0)
        self.assertIn("waiting for GitHub to catch up", stdout)

    def test_waits_out_an_unknown_merge_state(self) -> None:
        """The 0.3.0 failure: merging while GitHub was still computing mergeability."""
        code, stdout, _ = self.run_step(GH_CASE="unknown", GH_MERGE="ok", EXPECTED_HEAD="NEWSHA")
        self.assertEqual(code, 0)
        self.assertIn("waiting for GitHub to compute", stdout)

    def test_a_real_conflict_fails_loudly(self) -> None:
        code, stdout, _ = self.run_step(GH_CASE="conflict", GH_MERGE="ok", EXPECTED_HEAD="NEWSHA")
        self.assertEqual(code, 1)
        self.assertIn("conflicts with main", stdout)

    def test_survives_a_transient_api_failure(self) -> None:
        code, stdout, _ = self.run_step(GH_CASE="apifail", GH_MERGE="ok", EXPECTED_HEAD="NEWSHA")
        self.assertEqual(code, 0)
        self.assertIn("merged", stdout)

    def test_retries_a_rejected_merge(self) -> None:
        code, stdout, _ = self.run_step(GH_CASE="clean", GH_MERGE="flaky", EXPECTED_HEAD="NEWSHA")
        self.assertEqual(code, 0)
        self.assertIn("Retrying (1/5)", stdout)

    def test_gives_up_after_five_rejected_merges(self) -> None:
        code, stdout, _ = self.run_step(GH_CASE="clean", GH_MERGE="never", EXPECTED_HEAD="NEWSHA")
        self.assertEqual(code, 1)
        self.assertIn("Could not merge release PR", stdout)

    def test_skips_the_head_check_when_nothing_was_promoted(self) -> None:
        code, stdout, _ = self.run_step(GH_CASE="clean", GH_MERGE="ok", EXPECTED_HEAD="")
        self.assertEqual(code, 0)
        self.assertIn("merged", stdout)


if __name__ == "__main__":
    unittest.main()
