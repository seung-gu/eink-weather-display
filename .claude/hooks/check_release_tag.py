"""PreToolUse hook on Bash: lets a release tag through only when it is vMAJOR.MINOR.PATCH, above
every earlier release, and has an entry in CHANGELOG.md. A v* tag publishes firmware that every
board with auto update on installs, so the release skill (.claude/skills/release) has to run first.
Exit 2 blocks the command and hands the message back to Claude.

It reads `git [-C dir] tag ... <name>` in the command, after an optional `cd dir`. A tag made
another way (gh release create, the GitHub web page) is not seen.
"""
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
VERSION = re.compile(r"v(\d+)\.(\d+)\.(\d+)")
SHELL_OPERATORS = set("();<>|&")
# git tag options that take the next word as their value, and the ones that list, delete or verify
# tags instead of creating one
VALUED_LONG = {"--message", "--file", "--local-user", "--cleanup", "--trailer"}
VALUED_SHORT = "mFu"
NOT_CREATING_LONG = {"--delete", "--list", "--verify", "--contains", "--no-contains", "--points-at",
                     "--merged", "--no-merged"}
NOT_CREATING_SHORT = "dlvn"


def git(repo, *args):
    return subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True).stdout.strip()


def tag_name(args):
    """The tag a `git tag` with these arguments creates, or None if it lists, deletes or verifies."""
    names, value_next = [], False
    for a in args:
        if value_next:
            value_next = False
        elif a.startswith("--"):
            if a.split("=")[0] in NOT_CREATING_LONG:
                return None
            value_next = a in VALUED_LONG
        elif a.startswith("-"):
            for k, c in enumerate(a[1:], 1):
                if c in NOT_CREATING_SHORT:
                    return None
                if c in VALUED_SHORT:       # -m msg, -am msg, or -mmsg with the value attached
                    value_next = k == len(a) - 1
                    break
        else:
            names.append(a)
    return names[0] if names else None


def created_tags(command, cwd):
    """(directory, tag name) for every `git tag` in the command that creates a tag."""
    lexer = shlex.shlex(command, posix=True, punctuation_chars=True)
    lexer.whitespace_split = True
    try:
        words = list(lexer)
    except ValueError:              # unbalanced quotes: git would refuse the command too
        return []
    segments = [[]]
    for w in words:
        if set(w) <= SHELL_OPERATORS:
            segments.append([])
        else:
            segments[-1].append(w)
    found, here = [], Path(cwd)
    for seg in segments:
        if seg[:1] == ["cd"] and len(seg) > 1:
            here = here / os.path.expanduser(seg[1])
            continue
        if seg[:1] != ["git"]:
            continue
        i, repo = 1, here
        while i < len(seg) and seg[i].startswith("-"):     # git's own options, before the command
            if seg[i] == "-C" and i + 1 < len(seg):
                repo = repo / os.path.expanduser(seg[i + 1])
            i += 2 if seg[i] in ("-C", "-c") else 1
        if seg[i:i + 1] == ["tag"] and (name := tag_name(seg[i + 1:])):
            found.append((repo, name))
    return found


def problem(repo, name):
    """Why this tag must not be made, or None."""
    if not name.startswith("v") or Path(git(repo, "rev-parse", "--show-toplevel") or "/") != REPO:
        return None
    if not (m := VERSION.fullmatch(name)):
        return f"{name}: release tags are vMAJOR.MINOR.PATCH, such as v1.2.0."
    released = [tuple(map(int, v.groups())) for t in git(repo, "tag", "-l", "v*").split()
                if (v := VERSION.fullmatch(t))]
    if released and tuple(map(int, m.groups())) <= max(released):
        return f"{name} is not above the last release, v%d.%d.%d." % max(released)
    changelog = REPO / "CHANGELOG.md"
    text = changelog.read_text(encoding="utf-8") if changelog.exists() else ""
    if not re.search(rf"^## {re.escape(name)}\b", text, re.M):
        return f"CHANGELOG.md has no '## {name}' entry."
    return None


def main():
    try:
        event = json.load(sys.stdin)
    except ValueError:
        return 0
    command = event.get("tool_input", {}).get("command", "")
    if "tag" not in command:
        return 0
    problems = [p for repo, name in created_tags(command, event.get("cwd") or ".")
                if (p := problem(repo, name))]
    if not problems:
        return 0
    print("\n".join(problems), file=sys.stderr)
    print("Pick the version and write the entry with the release skill (.claude/skills/release/SKILL.md)"
          " first.", file=sys.stderr)
    return 2


sys.exit(main())
