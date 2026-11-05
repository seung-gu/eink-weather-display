---
name: release
description: Release the weather firmware. Picks the next vMAJOR.MINOR.PATCH from the commits since the last release, writes the CHANGELOG.md entry, then tags and pushes. Use whenever the user asks for a release, a new version or a tag (릴리즈, 릴리스, 버전, 태그).
---

# Release

A `v*` tag builds the firmware and publishes it as a GitHub release (`.github/workflows/release.yml`), and every board with auto update on installs it on its next wake.

## Picking the version

Semantic Versioning, judged by what someone using the board notices rather than by how much code changed. Read every commit since the last release and the firmware diff (`src/`, `platformio.ini`, `scripts/`, the partition table), and take the highest level any of them reaches.

| Level | When | Past releases |
|---|---|---|
| major (x.0.0) | The user has to do something: flash over USB again, register again, change the board or the panel. Also a change to what the board and the server send each other that leaves an older server or older firmware unable to work with the new one | v1.0.0: first release; boards flashed before it needed one USB upload |
| minor (0.x.0) | The board does something new, or the screen or a wake behaves differently, and the user has nothing to do | v1.1.0: partial refresh |
| patch (0.0.x) | The same behavior, fixed or faster. A refactor with no visible change is a patch however large | v1.0.1: weather drawn before an update; v1.0.2: RSA certificate, shorter wakes |
| no release | Only docs, images, the README, the workflow or `.claude/` changed. A tag would make every board reinstall the same firmware | the old v1.02 |

Raising a level sets the ones below it to 0: 1.1.3 → 1.2.0, 1.2.0 → 2.0.0. A published version never changes; a fix goes out under a new number.

## Steps

1. Check that `main` is clean and matches `origin/main`. The last release is the highest three-part tag: `git tag -l 'v*.*.*' --sort=-v:refname | head -1`.
2. Read `git log <last>..HEAD` and the firmware diff. If the firmware did not change, tell the user no release is needed and stop.
3. Tell the user the version, its level, a one-line reason, and the CHANGELOG entry you would add. Wait for their go-ahead before writing anything.
4. Add the entry to CHANGELOG.md above the newest one, and commit it on its own ("Add vX.Y.Z to the changelog").
5. `git tag -a vX.Y.Z -m "<the entry's first bullet, shortened>"`, then `git push origin main vX.Y.Z`.
6. `gh run watch` the release run. It turns green only once the emcp server holds the release; tell the user when it does.

`.claude/hooks/check_release_tag.py` blocks `git tag v…` when the tag is not three-part, is not above the last release, or has no CHANGELOG entry.

## The CHANGELOG entry

A heading, then one bullet per change a user would notice, each saying what is different from the version before. No commit hashes or file names. The date is the day of the tag.

```
## v1.2.0 · minor · 2026-11-20

- The screen shows tomorrow's forecast below today's, instead of today's only.
```
