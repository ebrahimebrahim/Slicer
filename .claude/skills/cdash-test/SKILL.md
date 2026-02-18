---
name: cdash-test
description: >
  Fetch and analyze CDash test results. Use when given a CDash test URL
  (e.g., slicer.cdash.org/tests/12345) or when investigating test failures
  from CDash. Do NOT use WebFetch on CDash URLs — they are JavaScript SPAs
  that return empty HTML. Use the helper script instead.
---

# Fetch CDash Test Results

CDash test URLs (`/tests/BUILDTESTID`) are JavaScript SPAs that cannot be fetched directly. Use the helper script which calls the JSON API.

## Command

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

# Raw JSON for programmatic processing
python3 /home/ebrahim/Slicer/.claude/skills/cdash-test/fetch_cdash_test.py 32245887 --json
```

## Key Fields in the JSON Response

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
