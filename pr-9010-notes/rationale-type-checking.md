# Rationale: `from __future__ import annotations` + `TYPE_CHECKING`

This document explains the approach used for adding type annotations to
`Base/Python/slicer/util.py`, as part of the work toward #8226.

## The problem

For type annotations, we need types like `Path`, `Callable`, `Requirement`, and `Popen`
to appear in function signatures. But `util.py` has a convention of zero module-level
imports -- everything is imported lazily inside function bodies. This is deliberate:
`slicer.util` is imported early during Slicer startup, and lazy imports avoid loading
unnecessary modules and prevent circular import issues.

## Options considered

### 1. Import annotation types at module level

For example, `from pathlib import Path` at the top of the file. This would work --
stdlib imports like `Path`, `Callable`, `Popen` are cheap. But it breaks the lazy-import
convention. More importantly, `from packaging.requirements import Requirement` adds an
external package dependency to module load time, meaning `slicer.util` would fail to
import if `packaging` isn't installed.

### 2. `TYPE_CHECKING` with string annotations

Put annotation-only imports under a `TYPE_CHECKING` block (which is `False` at runtime,
so nothing is imported). Without `from __future__ import annotations`, Python evaluates
annotation expressions at runtime, so types from the `TYPE_CHECKING` block would raise
`NameError`. Every annotation using these types would need manual string-quoting:
`path: "str | Path"` instead of `path: str | Path`. This works but is verbose and
error-prone.

### 3. `from __future__ import annotations` + `TYPE_CHECKING` (chosen)

The `__future__` import is a compiler directive (not a real module import) with ~zero
runtime cost. It makes all annotations lazy strings -- they're never evaluated at runtime.
This means types under `TYPE_CHECKING` can be referenced without string-quoting, giving
us clean syntax like `path: str | Path` while fully preserving the lazy-import convention.

The only runtime import is `from typing import TYPE_CHECKING` itself, which is extremely
lightweight (it just reads a `False` constant from the `typing` module).

## Why this is safe

- `from __future__ import annotations` only affects how annotation expressions are
  compiled. It doesn't change any runtime behavior of functions or classes.
- `util.py` has no existing type annotations and no code that introspects
  `__annotations__` or calls `typing.get_type_hints()`, so there is zero risk of
  breaking existing behavior.
- The existing doctests (for `toBool`) are unaffected -- they don't involve annotations.

## Precedent

There is no existing use of `from __future__ import annotations` in the `slicer` package.
This is the first file being typed per #8226, so it naturally sets a precedent.

Note that Python 3.14 will make annotations lazy by default (PEP 649), at which point
the `__future__` import becomes unnecessary.
