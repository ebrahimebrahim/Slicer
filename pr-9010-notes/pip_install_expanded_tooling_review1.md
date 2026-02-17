# Review: pip dependency handling changes (upstream/main..HEAD)

Reviewed commits (excluding CLAUDE:-prefixed):

- c498991 BUG: Fix shlex.split mangling requirements with environment markers
- a9cad69 ENH: Add skip_packages parameter to pip_install and pip_ensure
- c0a72cb ENH: Add load_pyproject_dependencies for pyproject.toml support
- 0410a72 ENH: Add restart prompt to pip_ensure when updated packages are already imported
- e1e2598 ENH: Add type annotations to pip dependency handling functions
- 445fba4 ENH: Simplify pip progress dialog status message
- 0446cae ENH: Prevent concurrent non-blocking pip_install operations
- 1d0053a ENH: Merge pip_install_with_progress into pip_install with extended API
- d6906a9 ENH: Add constraints parameter to pip install functions
- 6a44052 DOC: Document new pip dependency handling functions
- 8a4cba4 ENH: Add unit tests for pip-related functions in slicer.util
- 52a2721 ENH: Add pip_ensure function to slicer.util
- 935eae9 ENH: Add pip_install_with_progress function to slicer.util
- d4ce30a ENH: Add non-blocking mode to pip_install and pip_uninstall
- b501ddb ENH: Add pip_check function to slicer.util
- f583a6a ENH: Add load_requirements function to slicer.util

## Overall

Well-structured, well-documented, and thorough test coverage. The public
API surface (`load_requirements`, `load_pyproject_dependencies`, `pip_check`,
`pip_ensure`, `pip_install`) forms a clean layered design. Docstrings with
examples are a nice touch. A few items below.

---

## Bugs / actionable

### 1. `_pip_install_with_skips` silently swallows top-level install failures

`util.py:5211-5216` — When any package (including the top-level requirement
the user explicitly asked for) fails to install, the function logs a warning
and continues:

```python
except CalledProcessError:
    logging.warning("Failed to install %s, ...", req.name)
    return
```

This is reasonable for sub-dependencies (keep going, install what you can),
but surprising when the *top-level* requirement itself fails. A caller doing
`pip_ensure([Requirement("mypackage>=1.0")], ...)` would get no exception —
just a log warning.

**Suggestion:** Distinguish top-level vs. recursive calls (e.g. a `_depth`
parameter or a wrapper) and let top-level failures propagate.

### 2. `_isSlicerAppAvailable` doesn't actually verify `app` is set

`util.py:4627-4635`:

```python
try:
    from slicer import app
    return True
except ImportError:
    return False
```

Since we're *inside* the `slicer` package, `from slicer import app` will
succeed as long as the `app` attribute exists — even if it's `None` (which
it could be during early startup). The `ImportError` path would only trigger
if the `slicer` package itself isn't importable, which can't happen here.

**Suggestion:** Change to:

```python
try:
    from slicer import app
    return app is not None
except (ImportError, AttributeError):
    return False
```

### 3. shlex.split marker-mangling is only partially fixed

Commit c498991 fixed the issue in `_pip_install_with_skips` by stripping
markers before passing to `_build_pip_args`. But `_build_pip_args` itself
(`util.py:5115-5117`) still calls `shlex.split(requirements)` on string
input. A direct call like:

```python
pip_install("package>=1.0 ; python_version >= '3.9'")
```

would still be mangled. This is a pre-existing issue (the old `pip_install`
had the same `shlex.split` call), so not a regression. But since the commit
message claims to fix the marker-mangling problem, worth noting it's only
fixed for the `skip_packages` path.

---

## Minor / style

### 4. Extras-gated dependency check is fragile

`util.py:5236-5238`:

```python
if dep_req.marker is not None and "extra" in str(dep_req.marker):
    continue
```

This does a substring match on the stringified marker. A contrived package
name containing "extra" in a marker context could false-positive. The
`packaging` library doesn't expose marker variables directly, so this
pragmatic approach is probably fine in practice — just a fragility to note.

### 5. `type(x) == str` vs `isinstance`

`_build_pip_args` (`util.py:5115, 5118`) uses `type(requirements) == str`
rather than `isinstance(requirements, str)`. This is carried over from the
original `pip_install`, but since `_build_pip_args` is a new function, it
could use the more Pythonic `isinstance()` form.

### 6. `_pip_install_in_progress` flag not resilient to exceptions

If an exception is thrown between setting `_pip_install_in_progress = True`
(`util.py:5018`) and the `wrappedCompletedCallback` that clears it, the flag
stays `True` permanently, blocking all future non-blocking installs for the
session. A `try/finally` in the sync path or a more robust cleanup mechanism
would help.

---

## Tests

Test coverage is thorough. The mock-based tests for `_pip_install_with_skips`
are well-designed with the `_mock_dep_tree` helper. The
`GetInstalledVersionsSubprocessTest` that actually installs/uninstalls
`pip-install-test` from PyPI is a nice integration test.

No issues found in the test logic.
