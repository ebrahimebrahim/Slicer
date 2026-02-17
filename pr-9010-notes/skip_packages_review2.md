# Review of skip_packages commits (a9cad69 + c498991)

## 1. Top-level string requirements with markers could be mangled by shlex.split

**Severity: Low**

In `_pip_install_with_skips` (util.py:5162):

```python
if isinstance(requirements, str):
    req_strings = shlex.split(requirements)
```

If someone calls `_pip_install_with_skips('scipy>=1.0; sys_platform == "linux"', ...)`,
`shlex.split` will split on whitespace within the marker *before* parsing, so
`Requirement(req_str)` would receive a fragment like `scipy>=1.0;` and fail.

Mitigated in practice because `pip_ensure` always passes a list, not a raw string.
But it's a latent bug if anyone calls `pip_install(..., skip_packages=...)` with a
single string containing markers. Consider documenting that the string path doesn't
support markers, or parsing before splitting.

## 2. `_scrub_metadata` does unnecessary I/O when `skip_set` is empty

**Severity: Low**

At line 5227, `_scrub_metadata(canonical, skip_set)` is called for every installed
package. When `skip_packages=[]` (empty list, empty `skip_set`), this opens and
rewrites METADATA files for every package in the tree, accomplishing nothing.

Fix:

```python
if skip_set:
    _scrub_metadata(canonical, skip_set)
```

## 3. `_log` always calls `processEvents()` even with no callback

**Severity: Trivial**

The `_log` helper (line 5171) always calls `slicer.app.processEvents()` even when
`log_fn is None` and no message was produced. Harmless but wasteful. Consider
returning early when `log_fn is None`, or separating the `processEvents` call.

## 4. Failed installs are silently swallowed

**Severity: Medium**

At lines 5213-5216, if `_executePythonModule` raises `CalledProcessError`, the failure
is logged as a warning and the walk continues. The return value only reports *skipped*
packages, not *failed* ones. A caller relying on all non-skipped packages being present
will hit import errors later with no clear link back to the install failure.

Worth considering whether failures should be collected and returned (or raised).

## 5. Top-level requirements with extras lose the extras in the pip call

**Severity: Low**

At line 5209, the marker-stripping fix constructs `f"{req.name}{req.specifier}"`,
which drops `req.extras`. If a top-level requirement is `package[gpu]>=1.0`, the
`[gpu]` extra would be silently lost from the pip call.

For sub-dependencies this is fine (extras-gated deps are filtered at line 5237). But
for top-level requirements passed by the user, extras would be dropped.

## 6. Second commit (c498991) looks clean

- Strips markers before passing to `_build_pip_args` (correct fix for the shlex bug)
- Removes the redundant `except` clause (`finally` already handles both paths)
- Adds a well-structured test

No issues found.
