# Investigation: pyproject.toml vs requirements.txt for Slicer Extension Dependencies

## Context

PR #9010 uses `requirements.txt` as the on-disk format for extension dependency
declarations. Shoudl we move to `pyproject.toml` instead, since
that it is where the Python community is heading and since it offers richer metadata?

This document captures the technical investigation into both formats for this
specific use case: a Slicer extension declaring which Python packages should be
installed into the host application's environment at runtime.

---

## How load_requirements works today

`load_requirements` (in `slicer.util`) reads a file line-by-line, skips
comments (`#`), empty lines, and pip options (`-r`, `-c`, `--index-url`, etc.),
and parses each remaining line into a `packaging.requirements.Requirement`
object.

```python
def load_requirements(path):
    reqs = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#") and not line.startswith("-"):
                reqs.append(Requirement(line))
    return reqs
```

The downstream API (`pip_check`, `pip_ensure`, `pip_install`) operates entirely
on `Requirement` objects. The file format is only relevant at this entry point.

### requirements.txt features actually used

- Package names with version specifiers: `numpy>=1.20`, `scipy>=1.0,<2.0`
- Extras: `requests[socks]>=2.0`
- Environment markers: `package; sys_platform == "win32"`
- Comments and blank lines for readability

### requirements.txt features explicitly not used

- `-r` (recursive includes) -- extensions load their own files
- `-c` (constraints) -- passed separately via `constraints` parameter
- `--index-url`, `--find-links`, `--hash` -- not supported
- `--no-deps` -- handled by `no_deps_requirements` parameter at the API level

---

## How pyproject.toml specifies dependencies

Dependencies are PEP 508 strings in a TOML table (PEP 621):

```toml
[project]
name = "my-extension"
version = "1.0.0"
dependencies = [
    "numpy>=1.20,<2.0",
    "scipy>=1.7",
    'SimpleITK>=2.0; python_version >= "3.9"',
]

[project.optional-dependencies]
gpu = ["cupy>=10.0"]
viz = ["matplotlib>=3.5", "plotly>=5.0"]
```

PEP 735 (accepted, pip support since 25.1) adds `[dependency-groups]` for
non-distributable dependency sets:

```toml
[dependency-groups]
runtime = ["numpy>=1.20", "scipy>=1.7"]
test = ["pytest>=7", {include-group = "runtime"}]
```

The individual dependency strings are identical to what goes in
requirements.txt -- PEP 508 specifiers.

---

## Programmatic parsing comparison

### requirements.txt

Ad-hoc text parsing. Must handle comments, blank lines, pip directives, line
continuations. No stdlib parser exists.

```python
# Slicer's current approach (simplified)
reqs = []
for line in open(path):
    line = line.strip()
    if line and not line.startswith("#") and not line.startswith("-"):
        reqs.append(Requirement(line))
```

### pyproject.toml

Structured parsing with stdlib `tomllib` (Python 3.11+) and `packaging`:

```python
import tomllib
from packaging.requirements import Requirement

with open("pyproject.toml", "rb") as f:
    data = tomllib.load(f)
reqs = [Requirement(s) for s in data["project"]["dependencies"]]
```

No heuristics, no ambiguity. Both `tomllib` and `packaging` are available in
Slicer's Python 3.12 environment.

For `[dependency-groups]`, the PyPA-maintained `dependency-groups` library
(MIT, zero runtime deps beyond `packaging`) provides resolution with
`include-group` support.

---

## How pip and uv handle pyproject.toml

### pip

- `pip install .` -- builds and installs the package *itself* plus its
  `[project.dependencies]`. Requires a `[build-system]` table.
- `pip install -c constraints.txt .` -- same, with version constraints applied.
- `pip install --group dev` (pip 25.1+) -- installs a named dependency group
  from the current directory's pyproject.toml.

Key issue: `pip install .` installs the package, not just its dependencies.
Extensions aren't distributable Python packages, so this requires a build
backend and dummy metadata (`name`, `version`).

### uv

- `uv pip install -r pyproject.toml` -- installs only the *dependencies*
  without building/installing the package. This is exactly the "install these
  deps into the host environment" use case.
- `uv pip install -r pyproject.toml -c constraints.txt` -- with constraints.
- `uv pip install -r pyproject.toml --extra gpu` -- optional extras.
- `uv pip install --group dev` -- dependency groups.

The `uv pip install -r pyproject.toml` behavior is ideal for Slicer's use
case, but Slicer currently uses pip, not uv.

---

## Feature comparison

| Feature | requirements.txt | pyproject.toml |
|---|---|---|
| Standardized format | No (pip implementation detail) | Yes (PEP 621, PEP 735) |
| Programmatic parsing | Ad-hoc text parsing | Trivial (`tomllib` + `packaging`) |
| Package metadata (name, version, license) | No | Yes (`[project]` table) |
| Optional dependency groups | Multiple files | `[project.optional-dependencies]` |
| Non-distributable dependency groups | Multiple files | `[dependency-groups]` (PEP 735) |
| Tool configuration (ruff, pytest, etc.) | No | Yes (`[tool.*]` tables) |
| Entry points / scripts | No | Yes |
| Inline constraints (`-c`) | Yes | No |
| Inline pip options (`--no-deps`, etc.) | No (not per-line) | No |
| pip support | Native | `pip install .` (builds package) |
| uv support | Native | `uv pip install -r` (deps only) |
| Requires dummy metadata | No | Yes (`name`, `version` required) |

---

## Constraints file handling

requirements.txt can embed `-c constraints.txt` inline (though Slicer's
`load_requirements` skips this and handles constraints separately).

pyproject.toml has no way to reference a constraints file. Constraints are
strictly a pip/uv CLI concept (`-c constraints.txt`). They must always be
passed as a separate argument.

Slicer already passes constraints as a separate `constraints` parameter to
`pip_ensure`/`pip_install`, so this difference is neutral in practice.

uv has a `[tool.uv]` table with `constraint-dependencies`, but this is
uv-specific and not standardized.

---

## --no-deps handling

Neither format supports per-package `--no-deps`. This is always a global pip
CLI flag. Slicer's `no_deps_requirements` parameter handles this by splitting
installation into two pip invocations:

1. `pip install --no-deps <problematic_packages>`
2. `pip install <regular_packages>`

The pip 25.1 release notes explicitly state: "pip-specific options are NOT
supported for Dependency Groups."

This situation is identical regardless of file format.

---

## Community consensus (2025-2026)

- **pyproject.toml is the modern standard** for declaring package metadata and
  dependencies. All major tools support it.

- **requirements.txt is NOT deprecated** and has no deprecation planned. Brett
  Cannon (Python core dev) distinguished between "dependencies for packages"
  (pyproject.toml) and "dependencies for deployment" (requirements.txt). The
  two serve different purposes.

- **PEP 735 dependency-groups** was explicitly designed as "the standardized
  replacement for requirements.txt" for dev workflows. Accepted in 2024, pip
  support arrived in April 2025.

- The practical distinction: `[project.dependencies]` declares *abstract*
  dependencies of a package. requirements.txt declares *concrete* dependencies
  for a deployment. Slicer extensions fall somewhere in between.

---

## What a pyproject.toml-based extension would look like

```toml
# Extension's pyproject.toml
[project]
name = "SlicerMyExtension"    # required but not used for packaging
version = "0.0.0"             # required but not used for packaging
dependencies = [
    "numpy>=1.20,<2.0",
    "scikit-image>=0.20",
]

[project.optional-dependencies]
gpu = ["cupy>=10.0"]
```

Extension code:

```python
reqs = slicer.util.load_pyproject_dependencies("pyproject.toml")
slicer.util.pip_ensure(reqs, requester="MyExtension")
```

The `name` and `version` fields are mandatory in PEP 621 but would be
meaningless here -- the extension is not a Python package. This is the main
ergonomic downside.

With `[dependency-groups]` (PEP 735), you could avoid the `[project]` table
entirely:

```toml
[dependency-groups]
runtime = [
    "numpy>=1.20,<2.0",
    "scikit-image>=0.20",
]
gpu = [
    {include-group = "runtime"},
    "cupy>=10.0",
]
```

This is arguably the cleanest fit for the Slicer use case, since it doesn't
pretend the extension is a Python package.
