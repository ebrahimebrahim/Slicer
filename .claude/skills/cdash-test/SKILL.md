---
name: cdash-test
description: >
  Fetch and analyze CDash test results. Use when given a CDash test URL
  (e.g., slicer.cdash.org/tests/12345) or when investigating test failures
  from CDash. Do NOT use WebFetch on CDash URLs — they are JavaScript SPAs
  that return empty HTML. Use the helper scripts instead.
---

# Fetch CDash Test Results

CDash test URLs (`/tests/BUILDTESTID`) are JavaScript SPAs that cannot be fetched directly. Use the helper scripts which call the JSON API.

## Investigating a test failure — typical workflow

1. **Fetch the failing test** to understand what failed and read the output.
2. **Check test history** to determine if it's a one-off flake or a persistent regression.
3. Read the relevant source code (the test command and file paths are in the output).

## Script 1: Fetch a single test result

```bash
python3 /home/ebrahim/Slicer/.claude/skills/cdash-test/fetch_cdash_test.py <URL_OR_ID>
```

### Options

| Flag | Description |
|---|---|
| (none) | Full formatted summary with test output |
| `--output-only` | Print only the test output (stdout/stderr) |
| `--json` | Print raw JSON response from CDash API |

### Examples

```bash
# Full summary from a test URL
python3 /home/ebrahim/Slicer/.claude/skills/cdash-test/fetch_cdash_test.py https://slicer.cdash.org/tests/32245887

# Just the test output
python3 /home/ebrahim/Slicer/.claude/skills/cdash-test/fetch_cdash_test.py 32245887 --output-only
```

## Script 2: Query test history

Check recent pass/fail history for a test across all sites and builds. Use this to determine if a failure is a one-off flake or a regression.

```bash
python3 /home/ebrahim/Slicer/.claude/skills/cdash-test/query_cdash_tests.py <TEST_NAME> [options]
```

### Options

| Flag | Description |
|---|---|
| `--days N` | Look back N days (default: 7) |
| `--site SITE` | Filter to a specific build machine |
| `--failed-only` | Show only failures |
| `--project PROJECT` | CDash project name (auto-detected if omitted) |
| `--json` | Print raw JSON |

### Examples

```bash
# Recent 7-day history across all sites
python3 /home/ebrahim/Slicer/.claude/skills/cdash-test/query_cdash_tests.py qSlicerSslTest

# 30-day failure history
python3 /home/ebrahim/Slicer/.claude/skills/cdash-test/query_cdash_tests.py qSlicerSslTest --failed-only --days 30

# History for a specific build machine
python3 /home/ebrahim/Slicer/.claude/skills/cdash-test/query_cdash_tests.py qSlicerSslTest --site metroplex.kitware
```

## Key fields in the single-test JSON response

| Field | Description |
|---|---|
| `test.test` | Test name |
| `test.status` | "Passed" or "Failed" |
| `test.build` | Build configuration (OS, compiler, Qt) |
| `test.site` | Build machine name |
| `test.time` | Execution time |
| `test.command` | Exact command used to run the test |
| `test.output` | Full stdout/stderr |
| `test.update.revision` | Git commit SHA |
| `test.measurements` | Array of `{name, value}` (e.g., Exit Value) |

## Notes

- The CDash project for Slicer nightly/preview builds is `SlicerPreview`, not `Slicer`. The query script auto-detects this.
- CDash filter comparison codes: `61` = exact match, `83` = is after (for dates).
