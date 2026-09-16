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
    version = git("rev-parse", "--short", "HEAD")
    if git("status", "--porcelain"):
        version += "-dirty"
except Exception:
    version = "unknown"          # no git, or not a checkout — say so rather than guess

print(f"fw version: {version}")
env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])
