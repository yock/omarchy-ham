# 0014 — Version 1 validation is hand-written in C++

## Status

Accepted.

## Context

Qt has no JSON Schema validator. The alternatives were to add a validator library
(`nlohmann/json-schema-validator`, `valijson`), generate C++ from the schema, or
hand-write the checks for the frozen version 1 schema.

The version 1 profile is small and frozen by ADR 0011. Its rules are the ones a
hand-written validator can express directly: required keys, unknown keys, types,
ranges, enums, regular expressions, and one conditional object (`tnc.transport`).
A validator dependency would add a packaging surface to the CLI and the library
for rules that are not going to move within the version.

## Decision

The C++ library validates version 1 with a hand-written validator. The Python
`jsonschema` package validates the examples and the published schema in CI, as an
independent cross-check. Unit tests cover every invalid case the validator
knows.

Any change to the schema requires a new profile version, a migration, and a
matching validator change. The Python check and the C++ validator must both be
updated in the same change.

## Consequences

- No runtime dependency beyond Qt 6 Core.
- The validator and the schema can drift; CI cross-checks them on the examples,
  and tests cover the rules, but the two implementations are not generated from
  one source.
- The validator can give a precise field path and message, which a generic
  validator would still have to be wrapped to produce.
- Version 2 is an opportunity to revisit this if the rule set grows.

## Rejected

- **A JSON Schema library.** Adds a dependency and packaging work for a small,
  frozen rule set.
- **Generated validator code.** More build machinery than the schema justifies,
  and the generated code would still need error-path wrapping.
- **No validation in the library.** The writer would accept typos and the CLI
  would write profiles that consumers cannot read.
