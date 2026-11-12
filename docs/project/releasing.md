# Releasing

Two things happen when a change lands on `main`: a version number gets decided, and a changelog
entry gets filed. They are owned by different halves of the process, and keeping that split
straight is the whole point of this document.

| Artefact | Owner | Source of truth |
|---|---|---|
| Version number, git tag, GitHub release | release-please | Conventional Commit messages on `main` |
| `project(VERSION ...)` in `CMakeLists.txt` | release-please (`extra-files`) | `.release-please-manifest.json` |
| `CHANGELOG.md` entries | **you**, by hand | the pull request that makes the change |
| Promoting `## [Unreleased]` to `## [X.Y.Z]` | `scripts/promote_changelog.py`, in CI | the version release-please decided |

release-please runs with `"skip-changelog": true` in `release-please-config.json`, so it never
writes `CHANGELOG.md`. That is deliberate. Generated changelogs read like commit logs; this
project would rather have prose written for whoever consumes the engine. The cost is that the
entries have to be written by a human, which means you.

## When you open a pull request

Two things, both small:

1. **Add your entry to `CHANGELOG.md`** under `## [Unreleased]`, using the Keep a Changelog
   headings (`Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`). Write it for
   someone who uses the engine and did not read the diff. Internal churn — refactors, CI
   tweaks, test-only changes — does not get an entry.

2. **Title the PR as a [Conventional Commit](https://www.conventionalcommits.org/)**. PRs are
   squash-merged, so the title becomes the commit message on `main`, and that is what
   release-please reads to decide the next version. While the project is pre-1.0, `fix:` bumps
   the patch, `feat:` bumps the minor, and a breaking change (`feat!:` or a `BREAKING CHANGE:`
   footer) also bumps the minor — the major stays `0`.

   `.github/workflows/pr-title.yml` checks this: a PR whose title has no type release-please
   acts on gets a failing **PR title** check. It re-runs as soon as you edit the title, so fix
   the title and the check turns green. A `BEGIN_COMMIT_OVERRIDE` ... `END_COMMIT_OVERRIDE`
   block in the description also satisfies it, because release-please reads that block instead
   of the title. The check does not block the merge button. That needs branch protection,
   which a private personal repository only gets with GitHub Pro. It also cannot see a commit
   message you edit in the squash-merge dialog, so leave that message alone.

   All of this relies on the repository setting **Settings → General → Pull Requests → Allow
   squash merging → Default commit message** being **Pull request title and commit details**
   (`squash_merge_commit_title: PR_TITLE` in the API). GitHub's own default, **Default
   message**, uses the *commit's* message instead of the PR title for a PR with a single commit.
   That would let a well-titled PR merge as `WIP`, with the check green.

`## [Unreleased]` must stay at the very top of the file, directly under the header. Nothing
enforces this, and the promotion script only ever touches the first such heading it finds.

## What happens on merge

Merging to `main` sets off `.github/workflows/release-please.yml`, which runs **twice** for a
single release. That is not a quirk to work around; it is how the release gets made.

**First run** — triggered by your merge commit:

1. release-please works out the next version from the commits since the last tag, and opens a
   release pull request that bumps `.release-please-manifest.json` and `CMakeLists.txt`.
2. The *Promote the changelog's Unreleased section* step checks out that release branch, reads
   the new version out of the manifest, and runs `scripts/promote_changelog.py`. The result is
   committed onto the release branch.
3. The *Auto-merge release PR* step merges it immediately. There is no human approval gate. It
   waits for GitHub to report the PR mergeable before asking for the merge: the push in step 2
   invalidates GitHub's cached mergeability, and merging inside that window fails outright.

**Second run** — triggered by the release PR's own merge commit:

4. release-please sees the version bump, creates the git tag and publishes the GitHub release.

Steps 2 and 3 key off whether an open PR labelled `autorelease: pending` exists on a
`release-please--*` branch, not off whether *this* run created it. That is deliberate: if either
step fails, re-running the workflow finds the same PR and carries on from there. Both steps are
safe to run again on a release that is already past them. The branch name is checked as well as
the label, because those two steps end in an unattended squash merge into `main` and the label
alone is something any repository writer can apply.

The workflow also runs under a `concurrency` group, so two merges to `main` in quick succession
queue rather than race each other for the same release PR. Runs are never cancelled in flight: a
cancelled run can leave a release half-made.

The promotion happens in step 2, *before* the merge, so the changelog section lands in the same
commit that step 4 tags. A release therefore always contains the changelog entry describing
itself. Promoting after the tag instead would leave every release tagged one commit before its
own changelog — which is exactly the kind of off-by-one that nobody notices for six months.

### Why a PAT and not `GITHUB_TOKEN`

Both the action and the checkout use `secrets.RELEASE_PLEASE_TOKEN`. Pushes authenticated with
the default `GITHUB_TOKEN` do not trigger further workflow runs — GitHub suppresses them to stop
runaway recursion. Since this release depends on a second run being triggered by the first run's
own push, `GITHUB_TOKEN` would silently break it: the release PR would merge and nothing would
ever tag it.

## The promotion script

`scripts/promote_changelog.py` rewrites `## [Unreleased]` into `## [Unreleased]` followed by
`## [X.Y.Z] - YYYY-MM-DD`, leaving the entries beneath the new heading.

```bash
python3 scripts/promote_changelog.py --version 0.2.5
python3 scripts/promote_changelog.py --version v0.2.5 --date 2026-08-26 --changelog CHANGELOG.md
```

It is safe to re-run. Three outcomes, all exit `0`:

| Situation | What it does |
|---|---|
| `## [Unreleased]` has entries | promotes them under the new version heading |
| a `## [X.Y.Z]` section already exists | nothing — makes the step idempotent on a re-run |
| `## [Unreleased]` is empty | nothing — a release with no hand-written entries gets no section, rather than an empty one |

It fails, with exit `1`, when `## [Unreleased]` is missing entirely or the version is not valid
semver. Both mean something upstream has gone wrong, and failing the release is the right
response.

CRLF and LF files are both handled, and whichever the file already uses is preserved.

## Tests

`scripts/test_promote_changelog.py` covers the promotion script and `scripts/test_check_pr_title.py`
the PR title check; standard library only, no dependencies.

```bash
cd scripts && python -m unittest discover -p "test_*.py" --verbose
```

`.github/workflows/test-scripts.yml` runs them on every branch, but only when something under
`scripts/` changes.

## If a release goes wrong

- **The release PR has no changelog commit.** Either `## [Unreleased]` was empty (legitimate — a
  dependency bump or CI fix often is) or the promotion step failed. Check its log.
- **`## [Unreleased]` ended up mid-file.** It belongs directly under the header. Move it back;
  this is the failure mode the old setup produced constantly, when release-please still generated
  into the same file.
- **The release PR merged but nothing got tagged, and later runs log `There are untagged, merged
  release PRs outstanding - aborting`.** release-please identifies its own release PRs by title
  and by the `autorelease: pending` label. Renaming the PR — prefixing the title, say — makes it
  unrecognisable, so it never gets tagged and it blocks every release after it. **Do not edit the
  release PR's title.** (There is no need to prefix it to skip CI: the build workflows already
  ignore `release-please--**` branches.) To recover: tag and publish the release by hand, then relabel the
  merged PR `autorelease: tagged` so the next run stops aborting.
- **A release got tagged with no changelog section.** Add the section by hand in a follow-up PR,
  titled `docs:` so it does not itself trigger a release.
