# Summary: pyproject.toml vs requirements.txt for PR #9010

## The discussion

The PR uses `requirements.txt` as the format for extension dependency files.
A question was raised: should we be using `pyproject.toml`? Considering the direction of the Python
community, richer metadata, uv support, and potential for static analysis and
rollback.

## Points in favor of the argument

**Programmatic parseability.** `load_requirements` does ad-hoc text parsing
(skip `#`, skip `-`, strip whitespace). A pyproject.toml equivalent uses
`tomllib` (stdlib since 3.11) with no heuristics or ambiguity.

**Optional dependency groups.** Extensions with GPU-optional or test
dependencies end up with multiple requirements files. pyproject.toml handles
this in one file via `[project.optional-dependencies]` or PEP 735
`[dependency-groups]`.

**Community direction.** pyproject.toml is the standardized format (PEP 621).
requirements.txt has no formal spec.

## Points where the argument is weaker

**"pip understands pyproject.toml"** -- yes, but `pip install .` *builds and
installs the package itself*, requiring a `[build-system]` table and dummy
`name`/`version` fields. Extensions aren't Python packages. The cleaner path
(`uv pip install -r pyproject.toml`) is uv-specific and Slicer uses pip.

**"static analysis of dependencies"** -- pyproject.toml enables this for
package registries, but Slicer's `pip_check` already does pure-Python
dependency resolution via `importlib.metadata`. The benefit is less clear for
runtime extension deps installed into a host app.

**"roll back changes"** -- this is a uv/lockfile feature, orthogonal to the
file format choice.

**`--no-deps` per package** -- neither format supports this. Slicer's
`no_deps_requirements` parameter solves it at the API level regardless. The
pip 25.1 docs explicitly say pip-specific options aren't supported in
dependency groups.

**Constraints files** -- requirements.txt can embed `-c constraints.txt`
inline. pyproject.toml cannot. Slicer already passes constraints as a
separate parameter, so this is neutral, but it's not a pyproject.toml
advantage.

## My take

For what `load_requirements` + `pip_ensure` actually do today, the format
doesn't matter much. Both boil down to a list of PEP 508 strings. The
downstream API (`pip_check`, `pip_ensure`, `pip_install`) operates on
`Requirement` objects regardless of source.

The real question is whether Slicer wants extensions to eventually be "more
like Python packages" (with metadata, optional groups, entry points) or to
remain "just a list of things to install."

The PR description's rationale is largely correct but understates the parsing
advantage of structured TOML. The pro-TOML argument is largely correct but
overstates the practical benefit for this specific use case.

## Suggested path forward

A pragmatic middle path: `load_requirements` could gain a sibling (e.g.
`load_pyproject_dependencies`) or could auto-detect the format, so extensions
can use either. The downstream API doesn't care -- it operates on `Requirement`
objects.

This would let early adopters use pyproject.toml without forcing a format
change on everyone, and without any changes to the core install machinery.

The suggestion to adopt uv is a separate (and much larger) decision from the
file format question. Those shouldn't be conflated.

## If pyproject.toml is adopted

PEP 735 `[dependency-groups]` is the cleanest fit for the Slicer use case,
since it doesn't pretend the extension is a Python package:

```toml
[dependency-groups]
runtime = [
    "numpy>=1.20,<2.0",
    "scikit-image>=0.20",
]
```

No dummy `name`/`version` required. pip 25.1+ supports this natively. The
PyPA-maintained `dependency-groups` library provides programmatic resolution.
