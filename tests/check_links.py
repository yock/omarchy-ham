#!/usr/bin/env python3
"""Check that relative Markdown links point at files that exist."""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
LINK = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
FENCE = re.compile(r"```.*?```", re.DOTALL)


def markdown_files() -> list[pathlib.Path]:
    files = [ROOT / "README.md", ROOT / "SPEC.md", ROOT / "GLOSSARY.md", ROOT / "AGENTS.md"]
    files.extend(sorted((ROOT / "docs").rglob("*.md")))
    return [path for path in files if path.exists()]


def main() -> int:
    failures = 0

    for path in markdown_files():
        text = FENCE.sub("", path.read_text(encoding="utf-8"))
        for target in LINK.findall(text):
            target = target.strip()
            if target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            target = target.split("#", 1)[0]
            if not target:
                continue
            resolved = (path.parent / target).resolve()
            if not resolved.exists():
                failures += 1
                print(f"{path.relative_to(ROOT)}: missing link target {target}", file=sys.stderr)

    if failures:
        return 1
    print(f"checked {len(markdown_files())} Markdown files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
