"""Stamps the build with the commit it came from, so a report can be traced back to code.

Written by hand the constant goes stale the first time someone flashes without remembering to
bump it, and a version that lies is worse than none. git already knows the answer.

The dirty suffix is the point. A bare SHA says "this is commit abc1234" even when the working
tree has moved on, which is the same lie in a different costume. git status covers both a
modified file and a new one that was never added — git describe --dirty misses the second,
because it diffs against HEAD and a file HEAD never had is nothing to diff. Ignored files do
not count, which is what .gitignore is for.
"""
Import("env")
import subprocess


def git(*args):
    return subprocess.check_output(["git", *args], text=True,
                                   stderr=subprocess.DEVNULL).strip()


try:
    commit = git("rev-parse", "--short", "HEAD")
    dirty = bool(git("status", "--porcelain"))
    if dirty:
        commit += "-dirty"
except Exception:
    commit, dirty = "unknown", True   # no git, or not a checkout — say so rather than guess

# The name people read on the dashboard: the release tag (v1, v2, ...) when the build is exactly a
# tagged commit, so a released board says which release it runs. Anything else is a build no
# release matches, and keeps the commit. FW_COMMIT is the commit either way; updates compare by it.
version = commit
if not dirty:
    try:
        version = git("describe", "--tags", "--exact-match", "--match", "v*", "HEAD")
    except Exception:
        pass                          # not tagged

print(f"fw version: {version} ({commit}, {env['PIOENV']})")
env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version)),
                       ("FW_COMMIT", env.StringifyMacro(commit)),
                       # the env name says which board the build is for; updates pick the file by it
                       ("FW_BOARD", env.StringifyMacro(env["PIOENV"]))])
