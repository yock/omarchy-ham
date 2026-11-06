#!/usr/bin/env python3
"""Validate the station schema, its examples, and the ADR sequence."""

from __future__ import annotations

import json
import pathlib
import sys

from jsonschema import Draft202012Validator

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCHEMA_PATH = ROOT / "schema" / "station.schema.json"
EXAMPLES_DIR = ROOT / "examples"
ADR_DIR = ROOT / "docs" / "adr"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)


def main() -> int:
    failures = 0

    schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
    try:
        Draft202012Validator.check_schema(schema)
    except Exception as error:  # noqa: BLE001 - report the schema error verbatim
        fail(f"{SCHEMA_PATH}: {error}")
        return 1
    print(f"{SCHEMA_PATH.relative_to(ROOT)}: valid Draft 2020-12 schema")

    validator = Draft202012Validator(schema)
    examples = sorted(EXAMPLES_DIR.glob("*.json"))
    if not examples:
        fail(f"{EXAMPLES_DIR.relative_to(ROOT)}: no example profiles")
        failures += 1

    for path in examples:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            fail(f"{path.relative_to(ROOT)}: {error}")
            failures += 1
            continue

        errors = sorted(validator.iter_errors(data), key=lambda error: list(error.path))
        if errors:
            failures += 1
            fail(f"{path.relative_to(ROOT)}: invalid")
            for error in errors:
                where = ".".join(str(part) for part in error.path) or "<root>"
                print(f"  {where}: {error.message}", file=sys.stderr)
        else:
            print(f"{path.relative_to(ROOT)}: valid")

    adrs = sorted(ADR_DIR.glob("*.md"))
    if not adrs:
        fail(f"{ADR_DIR.relative_to(ROOT)}: no ADRs")
        failures += 1
    for index, path in enumerate(adrs, start=1):
        prefix = f"{index:04d}-"
        if not path.name.startswith(prefix):
            failures += 1
            fail(f"{path.relative_to(ROOT)}: expected filename prefix {prefix}")

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
