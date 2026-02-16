# Python Dependency Handling Improvements

## Summary

This PR adds new utility functions to `slicer.util` for managing Python dependencies in Slicer extensions. The goal is to provide a standardized, user-friendly way to check, prompt for, and install Python packages.

**New/updated functions:**

| Function | Purpose |
|----------|---------|
| `load_requirements(path)` | Load a `requirements.txt` file into `Requirement` objects |
| `pip_check(req)` | Check if requirements are satisfied (pure Python, no subprocess) |
| `pip_install(...)` | Extended with modal progress dialog, non-blocking mode, status bar feedback, and `--no-deps` support |
| `pip_ensure(reqs, requester="...")` | High-level: checks, prompts, installs with progress, and offers restart if updated packages were already imported |

All install functions support optional `constraints` (constraints file) and `no_deps_requirements` (packages to install with `--no-deps`) parameters.

**Typical usage in an extension:**

```python
reqs = slicer.util.load_requirements(self.resourcePath("requirements.txt"))
slicer.util.pip_ensure(reqs, requester="MyExtension")
import my_dependency  # Now safe
```

**Behavior of `pip_install`:**

Four operating modes:

- `show_progress=True, blocking=True` : Modal progress dialog (new default behavior)
- `show_progress=True, blocking=False`: Status bar messages
- `show_progress=False, blocking=True`: Busy cursor only (the previous default behavior, except with a busy cursor now added)
- `show_progress=False, blocking=False`: No visual indication that anything is happening (without looking at python console); specify callbacks to create more reasonable custom behaviors.

## Changes

- **Base/Python/slicer/util.py** — New functions and non-blocking infrastructure
- **Base/Python/slicer/tests/test_slicer_util_pip.py** — Unit tests
- **Docs/** — Updated `python_faq.md` and `script_repository/gui.md`

---

<details>
<summary><strong>Testing Snippets (click to expand)</strong></summary>

Convenient snippets to quickly try things:

### load_requirements

```python
import tempfile, os
with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
    f.write("numpy>=1.20\npandas>=2.0\nscipy\n")
    path = f.name
reqs = slicer.util.load_requirements(path)
print([f"{r.name}: {r.specifier}" for r in reqs])
os.unlink(path)
```

### pip_check

```python
from packaging.requirements import Requirement

# Check bundled package
slicer.util.pip_check(Requirement("numpy>=1.0"))  # True

# Unsatisfiable version
slicer.util.pip_check(Requirement("numpy>=99999.0"))  # False

# Package not installed
slicer.util.pip_check(Requirement("nonexistent-package-xyz"))  # False

# Marker that doesn't apply (always satisfied)
slicer.util.pip_check(Requirement("foo; sys_platform == 'nonexistent'"))  # True
```

### pip_install with progress dialog (default)

```python
# Default: shows modal progress dialog, blocks until complete
# (If already installed, completes quickly with "already satisfied")
slicer.util.pip_install("scikit-image", requester="ReviewTest")
```

To see the full installation flow, uninstall first:
```python
slicer.util.pip_uninstall("scikit-image")
slicer.util.pip_install("scikit-image", requester="ReviewTest")
```

If it's still too fast, try putting "torch" as the package :)

### pip_install without progress dialog

```python
# Busy cursor only, no dialog
slicer.util.pip_install("scikit-image", show_progress=False)
```

### Non-blocking pip_install with status bar messages

```python
slicer.util.pip_install("scikit-image", blocking=False, requester="ReviewTest")
```

### Non-blocking pip_install with custom callbacks

```python
def onLog(line):
    print(f"[pip] {line}")

def onComplete(code):
    print(f"Done! Return code: {code}")

# Using --help to see callbacks firing with lots of output
slicer.util.pip_install("--help", blocking=False, show_progress=False, logCallback=onLog, completedCallback=onComplete)
```

### Check if pip install is in progress

```python
slicer.util.pip_install("scikit-image", blocking=False, requester="ReviewTest")
print(slicer.util.isPipInstallInProgress())  # probably True
```

versus just this:

```python
print(slicer.util.isPipInstallInProgress())  # False
```

### pip_ensure

```python
from packaging.requirements import Requirement

# With prompt dialog
reqs = [Requirement("charset-normalizer>=3.0")]
slicer.util.pip_ensure(reqs, requester="ReviewTest") # Does nothing if already installed, try uninstalling first to see it do an install: slicer.util.pip_uninstall("charset-normalizer")


# Without install prompt (doesn't ask for confirmation whether you want to install)
slicer.util.pip_ensure(reqs, prompt_install=False, requester="ReviewTest") # Does nothing if already installed, try uninstalling first to see it do an install: slicer.util.pip_uninstall("charset-normalizer")
```

### pip_ensure restart prompt

After installation, `pip_ensure` checks if any updated packages were already imported in the current session. If so, it shows a "Restart Recommended" dialog with details (old → new versions). The user can restart immediately or continue.

```python
from packaging.requirements import Requirement
import numpy  # Ensure numpy is in sys.modules

# Reinstall numpy (already imported) — should trigger restart prompt
slicer.util.pip_uninstall("numpy")
reqs = [Requirement("numpy>=1.0")]
slicer.util.pip_ensure(reqs, requester="ReviewTest")
# A "Restart Recommended" dialog should appear because numpy was already imported
```

To disable the restart prompt: `slicer.util.pip_ensure(reqs, prompt_restart=False)`

### With constraints file

```python
import tempfile, os

# Create a constraints file that pins charset-normalizer
with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
    f.write("charset-normalizer==3.3.2\n")
    constraints_path = f.name

# Install httpx (which depends on charset-normalizer) with the constraint
slicer.util.pip_install("httpx", constraints=constraints_path)
os.unlink(constraints_path)
```

### PythonSlicer (command-line) usage

Run this from a terminal using Slicer's PythonSlicer executable (not the Slicer Python console):

```bash
/path/to/PythonSlicer -c "import slicer.util; slicer.util.pip_install('charset-normalizer')"
```

This verifies that `pip_install` works in the PythonSlicer environment where `slicer.app` and Qt are not available. The function automatically falls back to simple blocking mode.

</details>

---

<details>
<summary><strong>Design Rationale (click to expand)</strong></summary>

### Why requirements.txt instead of pyproject.toml?

- **Semantics:** `pyproject.toml` defines a distributable Python package. Slicer extensions aren't Python packages—they just need "install these things into this environment," which is exactly what `requirements.txt` is for.
- **Directness:** `requirements.txt` _is_ pip's native input format. So no translation layer needed this way.
- **Constraints support:** `pip install -c constraints.txt` handles dependency conflicts across extensions. Even with `pyproject.toml` you'd need a separate constraints file.
- **uv is still okay:** There is some interest in incorporating `uv` in the future. It is good to know that `uv` supports `requirements.txt` natively (`uv pip compile requirements.txt`).

### Why pure-Python pip_check instead of pip --dry-run?

Installing is not done frequently, but _checking_ may be called frequently. A pure-Python implementation using `importlib.metadata` avoids the overhead of a subprocess. If we used `pip --dry-run` it would have to be a subprocess.

### Why explicit pip_ensure instead of lazy import magic?

We considered [LazyImportGroup](https://github.com/Slicer/Slicer/issues/7707) which intercepts first use of imports to trigger installation. It is elegant, but it reduces transparency and makes debugging harder for extension developers. For now we get this pattern which has more boilerplate but is more transparent and simple to debug:

```python
slicer.util.pip_ensure(reqs, requester="MyExtension")
import my_dependency  # Explicit, debuggable
```

IDE support works via `TYPE_CHECKING`:
```python
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    import my_dependency  # For type hints only
```

We can still consider the lazy import ideas in the future -- the lower level tools here would still be useful.

### Why `pip_install` now defaults to showing a progress dialog

Before this PR, `pip_install` was always blocking with no progress feedback. The new defaults (`blocking=True`, `show_progress=True`) mean existing code that calls `pip_install` will now get a modal progress dialog "for free" without any code changes. This improves UX for many extensions immediately, while still allowing opt-out via `show_progress=False`.

Hopefully it doesnt' break too many extensions...

### Prevention of multiple non-blocking pip installs

When `pip_install` runs with `blocking=False`, it returns immediately while pip runs in the background. If another non-blocking `pip_install` is started before the first completes then we get chaos.

The solution adopted here is a module-level `_pip_install_in_progress` flag that raises `RuntimeError` if a second non-blocking install is attempted. Developers can check `isPipInstallInProgress()` first if they want to guard against this themselves.

### Why `no_deps_requirements` parameter

Some Python packages declare overly strict dependency requirements that conflict with other packages in Slicer's environment. The standard workaround requires two separate pip calls:

```python
pip_install("--no-deps problematic-package==1.0")  # Ignore its deps
pip_install("numpy scipy")  # Install known-good deps manually
```

The `no_deps_requirements` parameter handles this two-step process internally, making the intent self-documenting:

```python
pip_install(requirements="numpy scipy", no_deps_requirements="problematic-pkg==1.0")
```

This also correctly handles non-blocking mode by chaining the two pip calls internally.

### Why `pip_` function naming

The new functions follow the existing naming convention established by `slicer.util.pip_install` and `slicer.util.pip_uninstall`. This provides a consistent, discoverable API where all pip-related utilities share the `pip_` prefix.

### Why `pip_uninstall` was also modified

While the focus of this work is on dependency installation, `pip_uninstall` was updated with the same non-blocking parameters (`blocking`, `logCallback`, `completedCallback`) for API consistency. Since both functions share the same underlying infrastructure (`launchConsoleProcess` and `_executePythonModule`), extending non-blocking support to `pip_uninstall` required minimal additional code and ensures users have a symmetric API for both operations.

### Non-blocking implementation

The non-blocking mode uses a QTimer-based polling approach inspired by [SlicerMONAIAuto3DSeg](https://github.com/lassoan/SlicerMONAIAuto3DSeg). A background thread reads process output into a queue while QTimer polls from the main thread, keeping the Qt event loop responsive.

### How the restart prompt works

After `pip_ensure` installs packages, it snapshots all installed package versions (before and after) using `importlib.metadata`. For any packages whose version changed, it checks whether their top-level import names appear in `sys.modules` (meaning they were already imported in the current session). The distribution-to-import-name mapping uses `importlib.metadata.packages_distributions()` (Python 3.11+). If any already-imported packages were updated, a "Restart Recommended" dialog shows the affected packages with their old → new versions. The `prompt_restart=False` parameter disables this check.

</details>

---

<details>
<summary><strong>References (click to expand)</strong></summary>

- This work is part of the [44th Slicer project week](https://projectweek.na-mic.org/PW44_2026_GranCanaria/Projects/PythonDependenciesInExtensions/)!
- [#7171 — Improving Support for Python Package Dependencies in Slicer Extensions](https://github.com/Slicer/Slicer/issues/7171) — This PR implements the "runtime installation" approach described in the issue: extensions declare dependencies via `requirements.txt`, and enhanced `slicer.util` functions handle checking and installation with optional constraints file support for coordinating versions across extensions.
- [#7707 — Allow scripted modules to declare and lazily install pip requirements](https://github.com/Slicer/Slicer/issues/7707) — Proposes a `LazyImportGroup` context manager that intercepts imports and triggers installation on first attribute access. This PR takes a simpler, more explicit approach: developers call `pip_ensure()` at the point dependencies are needed, then import normally. The explicit pattern trades some elegance for transparency and easier debugging. The `LazyImportGroup` approach could potentially be built on top of the primitives provided here (`load_requirements`, `pip_check`, `pip_install` with callbacks) if desired in the future.

</details>
