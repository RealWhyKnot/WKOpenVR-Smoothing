# Release workflow

Releases are tag-driven and fully automated. Pushing a `v*` tag triggers
[release.yml](workflows/release.yml), which builds the distribution,
publishes a GitHub release, and verifies the published body matches the
input. The release body is generated from `git log` between the previous
tag and the current tag plus the templated evergreen sections in
[release-template/](release-template/) -- there is no hand-written
narrative path. If a release needs content that the auto-generator can't
produce, the only supported path is the [extras file](#extras-file).

## Tag shapes

| Form | When | Example |
|---|---|---|
| `vYYYY.M.D.N` | Release. `.N` is the release iteration for that calendar day, starting at 0. | `v2026.5.6.0` |
| `vYYYY.M.D.N-XXXX` | Dev. `.N` is local build count; `XXXX` is a 4-hex UID. Rare on the release stream. | `v2026.5.6.0-A1B2` |

[build.ps1](../build.ps1) validates the shape via the regex
`^\d{4}\.\d+\.\d+\.\d+(-[A-Fa-f0-9]{4,8})?$` and fails fast on malformed
tags.

## Body composition

The release body is the verbatim output of
[Generate-ReleaseNotes.ps1](scripts/Generate-ReleaseNotes.ps1). Layout:

```
# OpenVR-Smoothing <tag>

## What's Changed

### Features
- feat(...): subject by @author in <sha>

### Bug Fixes
- fix(...): subject by @author in <sha>

**Full Changelog**: <compare-url>

## File integrity
<table from build.ps1's manifest.tsv>

## More
<links.md template>

## Install (fresh)
<install.md template>

## Uninstall
<uninstall.md template>

## What you need to do
<what-you-need-to-do.md template>

[--- Additional notes (extras file, if present) ---]
```

CHANGELOG.md keeps accumulating in-repo for browsing; only the public
release body shrinks to just-this-version content.

## Conventional-commit policy

Commits in the tag range get bucketed by the prefix on their subject line:

| Prefix | Section |
|---|---|
| `feat(...)?:` | Features |
| `fix(...)?:` | Bug Fixes |
| `perf(...)?:` | Performance |
| `refactor(...)?:` | Refactors |
| `revert(...)?:` | Reverts |
| `docs(...)?:` | Documentation |
| `style(...)?:` | Style |
| `test(...)?:` | Tests |
| `ci(...)?:` | CI |
| `build(...)?:` | Build |
| `chore(...)?:` | Chores |
| anything else | Other Changes |

Subjects starting with `<prefix>!:` (the breaking-change marker) match
the same section. Trailing version-stamps like ` (2026.5.6.0-A1B2)` are
stripped before grouping. Commits with `[skip changelog]` in the subject
are excluded entirely; merge commits are excluded by `--no-merges`.

The generator emits a workflow warning for each non-conforming subject so
the operator can amend if desired. Non-conforming subjects ship under
`Other Changes` -- no fail.

## Author handle remap

The generator emits `by @<author>` from the commit's `%an` field. GitHub
@-mentions only resolve when the handle matches an actual login. The
local git config uses the brand "WhyKnot"; the GitHub login is
"RealWhyKnot". The generator carries an `$AuthorHandleMap` hashtable
that remaps known git authors to the right login before emit. If a new
author lands on the repo, add a mapping or expect their @-mention to
render as plain text.

## Scrub gates

After the body is composed, three gates run before the workflow proceeds:

1. **ASCII normalisation.** A fixed table of common typographic patterns
   (em-dash, en-dash, ellipsis, smart quotes, NBSP, bullet, multiplication
   sign, arrows, section sign, pilcrow) is substituted to ASCII
   equivalents. The substitution is one-way and silent.

2. **Non-ASCII fail.** Anything left outside the printable-ASCII range
   (0x20-0x7E plus tab) after normalisation fails the script with the
   offending line, column, and Unicode code point.

3. **Voice + internal-only-vocabulary check.** Pattern groups match
   case-insensitively against the composed body: marketing puffery,
   internal-only vocabulary (process-tracking nouns that don't belong
   in public release notes), future-tense rhetoric, unverified
   time-of-effort claims. Any match fails the script with the offending
   pattern and match position. Fix by amending the offending commit
   subject (or extras file) to use plainer language, OR mark the commit
   `[skip changelog]` if the term is unavoidable in-repo.

The scrub list lives in
[Generate-ReleaseNotes.ps1](scripts/Generate-ReleaseNotes.ps1). Add
patterns there if a new tell shows up in the wild.

## Empty-slice guard

If the tag range yields zero qualifying commits, the script throws and
the workflow fails. Escape hatch: `-AllowEmpty` for the very first
release on a fresh repo where the range trick produces nothing
meaningful.

## Extras file

For content that the auto-generator can't capture -- coordination notes
for paired releases with sibling consumer overlays, migration
instructions, links to a long-form wiki page, etc. -- create a markdown
file at `.github/release-extras/<tag>.md` BEFORE pushing the tag. The
file's contents get appended verbatim below the auto section with a
`---` separator and an `## Additional notes` heading.

```
.github/release-extras/v2026.5.6.0.md   <- created before tag push
```

The same scrub gates run on the composed body, so an em-dash or
"comprehensive solution" in an extras file fails the workflow just like
it would in a commit subject.

The file is optional. Most releases don't need one. If you find yourself
reaching for an extras file every release, consider whether the content
should be a commit subject instead.

## Post-publish verification

After `gh release create` succeeds, the workflow re-fetches the
published body via `gh release view --json body` and compares it
SHA256-to-SHA256 against the input. The verify step uses an exponential
backoff retry loop (2+4+8+16+32 = 62s budget) because GitHub's
release-body read-after-write isn't strictly consistent -- right after a
write the next read can return a stub for several seconds before the
API settles.

If the body still differs after the retry budget:

1. Workflow logs a warning and runs `gh release edit --notes-file <input>`
   to auto-correct.
2. Re-runs the same retry loop after the edit.
3. If they still differ, fails the workflow loud with both files
   persisted to the runner temp dir for inspection.

This catches GitHub-side normalisation surprises (which are rare but
real) without letting a malformed body sit on the release indefinitely.

## Promote-back to main

After the release publishes, the workflow pushes the promoted
`CHANGELOG.md` and `wiki/Changelog.md` back to `main` via the GraphQL
`createCommitOnBranch` mutation. Mutation-authored commits are signed
server-side with GitHub's bot key, so the resulting commit lands as
verified=true. This satisfies the project-wide "all new commits must be
verified" rule and any future protected-branch rule on `main`.

A pre-check via `cmp` skips the mutation entirely if `main` already
carries the promoted bytes (e.g. a re-run of the same tag).

## Failure modes + remediations

| Symptom | Fix |
|---|---|
| `No commits found in range` | Check the tag's parent reachability. Either the prev-tag detection failed (push the actual prev tag) or every commit is `[skip changelog]` (push a real change before tagging). |
| `Non-ASCII characters in release body after normalisation` | Find the offending commit subject, amend it to use ASCII, force-push the tag at the new SHA. Or add the char to `$asciiSubs` in the generator. |
| `Voice or internal-only-vocabulary patterns in release body` | Amend the offending commit subject. Or `[skip changelog]` it if the term is genuinely unavoidable. |
| `Generate-ReleaseNotes.ps1 returned empty output` | The script failed silently or the slice was empty. Check the workflow log for warnings. |
| `Release body still differs after auto-correct + retries` | A GitHub-side issue. Compare the input file in the runner artifacts against what `gh release view` returns. Often a trailing-whitespace or unicode-form difference. |
| `createCommitOnBranch returned GraphQL errors` | Usually a stale `expectedHeadOid` (someone pushed to main between fetch and mutation). The release itself is fine; re-run just the promote step or re-run the workflow. |

## Updating the workflow

The workflow + scripts are versioned alongside the code. Changes go
through the same direct-to-main flow as anything else. After landing a
workflow change, the next genuine release exercises it. If the workflow
breaks mid-release, the tag is already pushed and gh's partial state may
need cleanup -- in extreme cases, `gh release delete <tag> --cleanup-tag`
and re-tag at the same SHA after the fix.

Workflow plumbing fixes MUST ride along with a genuine customer release;
never tag a release purely to test workflow plumbing. Use
`workflow_dispatch` against a draft release for plumbing tests instead.
