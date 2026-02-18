# Python Dependency Handling Improvements

## Summary

This PR introduces a new `slicer.pydeps` module for managing Python dependencies in Slicer extensions. The goal is to provide a standardized, user-friendly way to check, prompt for, and install Python packages. The existing `slicer.util.pip_install` and `slicer.util.pip_uninstall` functions are retained as backward-compatible wrappers that delegate to `slicer.pydeps`.

**New/updated functions:**

| Function | Purpose |
|----------|---------|
| `slicer.pydeps.load_requirements(path)` | Load a `requirements.txt` file into `Requirement` objects |
| `slicer.pydeps.load_pyproject_dependencies(path)` | Load `[project.dependencies]` from a `pyproject.toml` file into `Requirement` objects |
| `slicer.pydeps.pip_check(req)` | Check if requirements are satisfied (pure Python, no subprocess) |
| `slicer.pydeps.pip_install(...)` | Canonical home; extended with modal progress dialog, non-blocking mode, status bar feedback, `--no-deps` support, and `skip_packages` for selective dependency installation. Also available as `slicer.util.pip_install` (backward-compatible wrapper) |
| `slicer.pydeps.pip_ensure(reqs, requester="...")` | High-level: checks, prompts, installs with progress, and offers restart if updated packages were already imported |

All install functions support optional `constraints` (constraints file), `no_deps_requirements` (packages to install with `--no-deps`), and `skip_packages` (packages to exclude from the transitive dependency tree) parameters.

**Typical usage in an extension:**

```python
import slicer.pydeps

reqs = slicer.pydeps.load_requirements(self.resourcePath("requirements.txt"))
slicer.pydeps.pip_ensure(reqs, requester="MyExtension")
import my_dependency  # Now safe
```

Or, with `pyproject.toml`:

```python
import slicer.pydeps

reqs = slicer.pydeps.load_pyproject_dependencies(self.resourcePath("pyproject.toml"))
slicer.pydeps.pip_ensure(reqs, requester="MyExtension")
import my_dependency  # Now safe
```

With `skip_packages` (for extensions that need to exclude certain transitive dependencies):

```python
import slicer.pydeps

reqs = [Requirement("nnunetv2>=2.3")]
skipped = slicer.pydeps.pip_ensure(
    reqs,
    skip_packages=["SimpleITK", "torch", "requests"],
    requester="SlicerNNUNet",
)
# skipped contains the requirement strings that were excluded
```

**Behavior of `pip_install`:**

Four operating modes:

- `show_progress=True, blocking=True` : Modal progress dialog (new default behavior)
- `show_progress=True, blocking=False`: Status bar messages
- `show_progress=False, blocking=True`: Busy cursor only (the previous default behavior, except with a busy cursor now added)
- `show_progress=False, blocking=False`: No visual indication that anything is happening (without looking at python console); specify callbacks to create more reasonable custom behaviors.

## Changes

- **Base/Python/slicer/pydeps.py** — New module: `load_requirements`, `load_pyproject_dependencies`, `pip_check`, `pip_ensure`, `pip_install`, `pip_uninstall`, and non-blocking infrastructure
- **Base/Python/slicer/util.py** — Backward-compatible wrappers (`pip_install`, `pip_uninstall`) that delegate to `slicer.pydeps`
- **Base/Python/slicer/tests/test_slicer_pydeps.py** — Unit tests
- **Docs/** — Updated `python_faq.md` and `script_repository/gui.md`

---

<details>
<summary><strong>Testing Snippets (click to expand)</strong></summary>

Convenient snippets to quickly try things. First, run this once in the Python console:

```python
import slicer.pydeps
```

### load_requirements

```python
import tempfile, os
with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
    f.write("numpy>=1.20\npandas>=2.0\nscipy\n")
    path = f.name
reqs = slicer.pydeps.load_requirements(path)
print([f"{r.name}: {r.specifier}" for r in reqs])
os.unlink(path)
```

### load_pyproject_dependencies

```python
import tempfile, os
with tempfile.NamedTemporaryFile(mode='w', suffix='.toml', delete=False) as f:
    f.write("[project]\ndependencies = [\n")
    f.write('    "numpy>=1.20",\n    "pandas>=2.0",\n    "scipy",\n]\n')
    path = f.name
reqs = slicer.pydeps.load_pyproject_dependencies(path)
print([f"{r.name}: {r.specifier}" for r in reqs])
os.unlink(path)
```

### pip_check

```python
from packaging.requirements import Requirement

# Check bundled package
slicer.pydeps.pip_check(Requirement("numpy>=1.0"))  # True

# Unsatisfiable version
slicer.pydeps.pip_check(Requirement("numpy>=99999.0"))  # False

# Package not installed
slicer.pydeps.pip_check(Requirement("nonexistent-package-xyz"))  # False

# Marker that doesn't apply (always satisfied)
slicer.pydeps.pip_check(Requirement("foo; sys_platform == 'nonexistent'"))  # True
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
print(slicer.pydeps.isPipInstallInProgress())  # probably True
```

versus just this:

```python
print(slicer.pydeps.isPipInstallInProgress())  # False
```

### pip_ensure

```python
from packaging.requirements import Requirement

# With prompt dialog
reqs = [Requirement("charset-normalizer>=3.0")]
slicer.pydeps.pip_ensure(reqs, requester="ReviewTest") # Does nothing if already installed, try uninstalling first to see it do an install: slicer.util.pip_uninstall("charset-normalizer")


# Without install prompt (doesn't ask for confirmation whether you want to install)
slicer.pydeps.pip_ensure(reqs, prompt_install=False, requester="ReviewTest") # Does nothing if already installed, try uninstalling first to see it do an install: slicer.util.pip_uninstall("charset-normalizer")
```

### pip_ensure restart prompt

After installation, `pip_ensure` checks if any updated packages were already imported in the current session. If so, it shows a "Restart Recommended" dialog with details (old → new versions). The user can restart immediately or continue.

```python
from packaging.requirements import Requirement
import numpy  # Ensure numpy is in sys.modules

# Reinstall numpy (already imported) — should trigger restart prompt
slicer.util.pip_uninstall("numpy")
reqs = [Requirement("numpy>=1.0")]
slicer.pydeps.pip_ensure(reqs, requester="ReviewTest")
# A "Restart Recommended" dialog should appear because numpy was already imported
```

To disable the restart prompt: `slicer.pydeps.pip_ensure(reqs, prompt_restart=False)`

### With constraints file

```python
import tempfile, os

# Create a constraints file that pins charset-normalizer
with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
    f.write("charset-normalizer==3.3.2\n")
    constraints_path = f.name

# Install requests (which depends on charset-normalizer) with the constraint
slicer.util.pip_install("requests", constraints=constraints_path)
os.unlink(constraints_path)
```

### With skip_packages (selective dependency installation)

```python
# Install scikit-image but skip imageio (one of its dependencies)
skipped = slicer.util.pip_install(
    "scikit-image",
    skip_packages=["imageio"],
    requester="ReviewTest",
)
print(f"Skipped: {skipped}")

# Or via pip_ensure:
from packaging.requirements import Requirement
reqs = [Requirement("scikit-image>=0.20")]
skipped = slicer.pydeps.pip_ensure(
    reqs,
    skip_packages=["imageio"],
    requester="ReviewTest",
)
```

### PythonSlicer (command-line) usage

Run this from a terminal using Slicer's PythonSlicer executable (not the Slicer Python console):

```bash
/path/to/PythonSlicer -c "import slicer.pydeps; slicer.pydeps.pip_install('charset-normalizer')"
```

This verifies that `pip_install` works in the PythonSlicer environment where `slicer.app` and Qt are not available. The function automatically falls back to simple blocking mode.

</details>

---

<details>
<summary><strong>Design Rationale (click to expand)</strong></summary>

### Why requirements.txt? And why also pyproject.toml?

`requirements.txt` is the primary recommended format. Slicer extensions aren't Python packages — they just need "install these things into this environment," which is exactly what `requirements.txt` is for. It's pip's native input format, every Python developer knows it, and it requires no boilerplate beyond the dependency list itself.

That said, `pyproject.toml` is the modern standard in the Python ecosystem (PEP 621). It offers structured parsing via `tomllib` (stdlib) with no ad-hoc text handling, and extensions that already have a `pyproject.toml` for other tooling (ruff, pytest, etc.) can keep dependencies in one file. So we provide `load_pyproject_dependencies` as an alternative for extensions that prefer it.

Both formats boil down to PEP 508 dependency strings. Both loader functions return the same `list[Requirement]` type, so the downstream API (`pip_check`, `pip_ensure`, `pip_install`) works identically regardless of which one you use.

`load_pyproject_dependencies` reads only the `[project.dependencies]` list. Other fields in the `[project]` table (`name`, `version`, etc.) are not read or validated — extensions aren't Python packages and shouldn't need to provide them.

### Why pure-Python pip_check instead of pip --dry-run?

Installing is not done frequently, but _checking_ may be called frequently. A pure-Python implementation using `importlib.metadata` avoids the overhead of a subprocess. If we used `pip --dry-run` it would have to be a subprocess.

### Why explicit pip_ensure instead of lazy import magic?

We considered [LazyImportGroup](https://github.com/Slicer/Slicer/issues/7707) which intercepts first use of imports to trigger installation. It is elegant, but it reduces transparency and makes debugging harder for extension developers. For now we get this pattern which has more boilerplate but is more transparent and simple to debug:

```python
slicer.pydeps.pip_ensure(reqs, requester="MyExtension")
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

### Why `skip_packages` parameter

Multiple Slicer extensions (SlicerTotalSegmentator, SlicerNNUNet) independently implement ~70-90 lines of recursive selective-install code to install packages while excluding certain transitive dependencies. Common examples: SimpleITK (Slicer bundles a custom version), torch (must be installed via SlicerPyTorch for the correct CUDA/CPU build), and requests (already bundled, replacing it forces an unnecessary restart).

The `skip_packages` parameter centralizes this logic. When provided, each package is installed with `--no-deps`, its dependency tree is walked recursively, and any package matching the skip list is excluded. Package METADATA is updated after installation so that `pip check` doesn't flag skipped packages as missing.

### `skip_packages` vs `no_deps_requirements`

Both are available; they serve different purposes:

- **`no_deps_requirements`**: "Install these packages without any of their deps." You provide the correct deps yourself. Fast (2 pip calls), doesn't modify METADATA. Use when a specific package has broken dependency declarations.
- **`skip_packages`**: "Install everything except these specific packages, anywhere in the dependency tree." Automatic recursive walk with METADATA scrubbing. Slower (one pip call per package). Use when you want most of a package's deps but need to exclude specific ones that Slicer provides differently.

They are mutually exclusive — providing both raises `ValueError`.

### Why `pip_install` doesn't accept `list[Requirement]`

It would feel natural to write `pip_install(load_requirements("requirements.txt"))`. The fact that you can't is a little unfortunate — it looks like it *should* work. But `pip_install` is extremely widely used in the wild (via `slicer.util.pip_install`), and we don't want to make breaking changes to its signature. It's also a low-level function that mirrors the pip CLI, taking the same kind of string arguments you'd pass on the command line, so it makes sense to leave it working the way it does.

The new structured `list[Requirement]` input type goes into the new `slicer.pydeps` functions instead: `pip_check` and `pip_ensure`. In practice, `slicer.pydeps.pip_ensure(load_requirements("requirements.txt"))` is the call you want anyway -- it checks what's already installed, prompts the user, installs only what's missing, and detects whether a restart is needed. So accepting `Requirement` objects at the `pip_ensure` level rather than the `pip_install` level steers developers toward the safer, idempotent workflow.

### Why `pip_` function naming

The new functions follow the existing naming convention established by `slicer.util.pip_install` and `slicer.util.pip_uninstall`. All pip-related functions now live together in `slicer.pydeps` with the `pip_` prefix, providing a consistent, discoverable API.

### Why `slicer.pydeps` (module naming)

The new module needed a name that (1) clearly conveys its purpose, (2) doesn't collide with existing packages in the Python ecosystem, and (3) works within Slicer's Python environment.

**`slicer.packaging` was the initial choice** — it's descriptive and mirrors the well-known third-party `packaging` library. However, Slicer's Python environment adds the `slicer/` directory itself to `sys.path` (i.e., `<build>/bin/Python/slicer/` is a path entry). This means a file named `slicer/packaging.py` shadows the third-party `packaging` library for any top-level `from packaging import ...` statement, breaking imports of `packaging.requirements`, `packaging.markers`, etc.

**`slicer.dependencies`** was considered, but in a codebase where half the stack is C++ with its own dependency tree (VTK, ITK, Qt, CTK), the unqualified "dependencies" is ambiguous.

**`slicer.pydeps`** was chosen because:
- The `py` prefix immediately scopes it to *Python* dependencies, which is what this module manages
- It's short (6 characters, comparable to `util`) and easy to type
- No collision with any stdlib module or common PyPI package
- Reads naturally: `slicer.pydeps.pip_ensure(...)`, `slicer.pydeps.load_requirements(...)`

### Why `import slicer.pydeps` is explicit

`slicer.util` is auto-imported at startup (via `from slicer.util import *` in `slicerqt.py`), so `slicer.util.pip_install(...)` works without an explicit import. We intentionally did *not* do the same for `slicer.pydeps`.

`slicer.util` gets away with auto-import because its top-level imports are all stdlib — the heavy dependencies (`qt`, `ctk`, `slicer.app`) are lazy imports inside function bodies, so the startup cost is essentially zero. `slicer.pydeps` is different: it uses proper top-level imports for `packaging.markers`, `packaging.requirements`, `importlib.metadata`, etc. These are lightweight individually, but there's no reason for every Slicer session to pay that cost when most sessions never install a Python package.

The tradeoff is worth it. Top-level imports in `slicer.pydeps` make the module easier to read, ensure import errors surface immediately rather than hiding inside deeply nested call stacks, and follow standard Python conventions. The cost is one `import slicer.pydeps` line in extension code that uses the new API — a trivial ask. And for the common case of `slicer.util.pip_install(...)`, no explicit import is needed because the wrapper does its own lazy import internally.

### Why `pip_uninstall` was also modified

While the focus of this work is on dependency installation, `pip_uninstall` was updated with the same non-blocking parameters (`blocking`, `logCallback`, `completedCallback`) for API consistency. Since both functions share the same underlying infrastructure (`launchConsoleProcess` and `_executePythonModule`) in `slicer.pydeps`, extending non-blocking support to `pip_uninstall` required minimal additional code and ensures users have a symmetric API for both operations.

### Non-blocking implementation

The non-blocking mode uses a QTimer-based polling approach inspired by [SlicerMONAIAuto3DSeg](https://github.com/lassoan/SlicerMONAIAuto3DSeg). A background thread reads process output into a queue while QTimer polls from the main thread, keeping the Qt event loop responsive.

### How the restart prompt works

After `pip_ensure` installs packages, it snapshots all installed package versions (before and after) using `importlib.metadata`. For any packages whose version changed, it checks whether their top-level import names appear in `sys.modules` (meaning they were already imported in the current session). The distribution-to-import-name mapping uses `importlib.metadata.packages_distributions()` (Python 3.11+). If any already-imported packages were updated, a "Restart Recommended" dialog shows the affected packages with their old → new versions. The `prompt_restart=False` parameter disables this check.

</details>

---

<details>
<summary><strong>Interactive Feature Tour (click to expand)</strong></summary>

Build Slicer from the `python-dependency-handling-improvements` branch, launch it, open the Python console, and paste the script below. A menu lets you pick which demos to run — or select "Run All" for the full walkthrough.

```python
import importlib.metadata as _md
import os
import tempfile

import qt
from packaging.requirements import Requirement

import slicer
import slicer.pydeps

# ---------------------------------------------------------------------------
# Configuration — change these to try a different demo package
# ---------------------------------------------------------------------------

DEMO_PACKAGE = "scikit-image"        # pip install name
DEMO_IMPORT = "skimage"              # Python import name
DEMO_CONSTRAINT = ">=0.20,<0.25"     # version range for constraints demo
DEMO_SKIP_DEP = "imageio"            # dependency to skip in skip_packages demo

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------

try:
    pkg_version_before_tour = _md.version(DEMO_PACKAGE)
except _md.PackageNotFoundError:
    pkg_version_before_tour = None


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def clear_cache():
    """Purge the pip download cache so installs show real download progress."""
    slicer.pydeps._executePythonModule("pip", ["cache", "purge"])


def uninstall_pkg():
    """Remove the demo package if present."""
    try:
        slicer.util.pip_uninstall(DEMO_PACKAGE)
    except Exception:
        pass


def fresh_slate():
    """Uninstall the demo package and clear the cache."""
    uninstall_pkg()
    clear_cache()


# ---------------------------------------------------------------------------
# Demo 1 — Loading Dependencies
# ---------------------------------------------------------------------------

def demo_loading_deps():
    """load_requirements() and load_pyproject_dependencies()"""

    # --- requirements.txt ---
    slicer.util.infoDisplay(
        "load_requirements(path) parses a requirements.txt file into\n"
        "Requirement objects, skipping comments, blanks, and pip options.\n"
        "\n"
        "Watch the Python console for parsed results.",
        windowTitle="Demo 1: Loading Dependencies — requirements.txt",
    )

    with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
        f.write("# Example requirements file\n")
        f.write("numpy>=1.20\n")
        f.write(f"{DEMO_PACKAGE}>=0.20\n")
        f.write("nonexistent-package-xyz>=1.0\n")
        f.write("-c constraints.txt\n")
        f.write("\n")
        path = f.name

    reqs = slicer.pydeps.load_requirements(path)
    os.unlink(path)

    lines = [f"  {r.name}  {r.specifier}" for r in reqs]
    print(f"[Tour] load_requirements -> {len(reqs)} requirements:\n" + "\n".join(lines))

    # --- pyproject.toml ---
    with tempfile.NamedTemporaryFile(mode="w", suffix=".toml", delete=False) as f:
        f.write("[project]\n")
        f.write("dependencies = [\n")
        f.write('    "numpy>=1.20",\n')
        f.write(f'    "{DEMO_PACKAGE}>=0.20",\n')
        f.write('    "nonexistent-package-xyz>=1.0",\n')
        f.write("]\n")
        path = f.name

    reqs2 = slicer.pydeps.load_pyproject_dependencies(path)
    os.unlink(path)

    lines2 = [f"  {r.name}  {r.specifier}" for r in reqs2]
    print(f"[Tour] load_pyproject_dependencies -> {len(reqs2)} requirements:\n" + "\n".join(lines2))

    slicer.util.infoDisplay(
        f"Loaded {len(reqs)} requirements from requirements.txt\n"
        f"and {len(reqs2)} from pyproject.toml.\n"
        "\n"
        "Both return the same Requirement objects — the downstream\n"
        "API (pip_check, pip_ensure) works identically with either.",
        windowTitle="Demo 1: Loading Dependencies — result",
    )


# ---------------------------------------------------------------------------
# Demo 2 — Checking Requirements
# ---------------------------------------------------------------------------

def demo_checking_reqs():
    """pip_check() — pure-Python requirement validation"""

    slicer.util.infoDisplay(
        "pip_check(req) checks if a requirement is satisfied.\n"
        "Pure Python, no subprocess — fast enough to call frequently.\n"
        "\n"
        "We will test several cases against the current environment.",
        windowTitle="Demo 2: Checking Requirements",
    )

    checks = [
        ("numpy>=1.0", "Bundled with Slicer — should be satisfied"),
        ("numpy>=99999.0", "Impossibly high version — should fail"),
        ("nonexistent-xyz>=1.0", "Package not installed"),
        ('foo; sys_platform == "nonexistent"', "Marker does not apply — treated as satisfied"),
    ]

    results = []
    for spec, desc in checks:
        req = Requirement(spec)
        ok = slicer.pydeps.pip_check(req)
        mark = "SATISFIED" if ok else "NOT satisfied"
        results.append(f"  [{mark}]  {spec}\n      {desc}")

    slicer.util.infoDisplay(
        "pip_check results:\n"
        "\n" + "\n\n".join(results),
        windowTitle="Demo 2: Checking Requirements — results",
    )


# ---------------------------------------------------------------------------
# Demo 3 — Installing with Progress
# ---------------------------------------------------------------------------

def demo_install_progress():
    """pip_install() — modal dialog and non-blocking status bar"""

    # --- Part 1: Modal (blocking) ---
    fresh_slate()

    slicer.util.infoDisplay(
        "pip_install() — modal progress dialog (the new default).\n"
        "\n"
        "Try expanding the Details section!",
        windowTitle="Demo 3a: Modal Progress Dialog",
    )

    slicer.util.pip_install(DEMO_PACKAGE, requester="Feature Tour")

    slicer.util.infoDisplay(
        "Modal install done.\n"
        "\n"
        "Now switching to non-blocking mode: the call returns\n"
        "immediately and pip output appears in the status bar.",
        windowTitle="Demo 3a: Modal — done",
    )

    # --- Part 2: Non-blocking ---
    fresh_slate()

    slicer.util.infoDisplay(
        "pip_install() — non-blocking with status bar.\n"
        "\n"
        "Watch the STATUS BAR at the bottom. The UI stays interactive.\n"
        "isPipInstallInProgress() will be printed to the console.",
        windowTitle="Demo 3b: Status Bar Mode",
    )

    loop = qt.QEventLoop()
    _result = [None]

    def on_complete(return_code):
        _result[0] = return_code
        qt.QTimer.singleShot(0, loop.quit)

    slicer.util.pip_install(
        DEMO_PACKAGE,
        blocking=False,
        show_progress=True,
        requester="Feature Tour",
        completedCallback=on_complete,
    )

    # Check in-progress flag shortly after starting
    qt.QTimer.singleShot(500, lambda: print(
        f"[Tour] isPipInstallInProgress() = {slicer.pydeps.isPipInstallInProgress()}"
    ))

    loop.exec_()

    slicer.util.infoDisplay(
        f"Non-blocking install finished (return code {_result[0]}).\n"
        "The status bar showed pip output while the UI stayed responsive.",
        windowTitle="Demo 3b: Status Bar — done",
    )


# ---------------------------------------------------------------------------
# Demo 4 — Smart Install Workflow
# ---------------------------------------------------------------------------

def demo_smart_install():
    """pip_ensure() — check, prompt, install, restart detection"""

    # --- Part 1: Normal pip_ensure ---
    fresh_slate()

    slicer.util.infoDisplay(
        "pip_ensure() — the recommended high-level API for extensions.\n"
        "Checks requirements, shows confirmation, installs with progress.\n"
        "\n"
        "You will see a confirmation dialog, then a progress dialog.",
        windowTitle="Demo 4a: pip_ensure",
    )

    reqs = [Requirement(f"{DEMO_PACKAGE}>=0.20")]
    slicer.pydeps.pip_ensure(reqs, requester="Feature Tour")

    slicer.util.infoDisplay(
        "pip_ensure done. Now we will call it again immediately.\n"
        "\n"
        "pip_check sees the package is already installed, so\n"
        "pip_ensure skips everything — no dialogs, instant return.",
        windowTitle="Demo 4a: pip_ensure — done",
    )

    slicer.pydeps.pip_ensure(reqs, requester="Feature Tour (no-op)")

    slicer.util.infoDisplay(
        "pip_ensure returned instantly — nothing to install.\n"
        "\n"
        "Now: restart prompt demo. We will import the package, uninstall\n"
        "it, then pip_ensure again. Since it is in memory, a restart\n"
        "dialog will appear. (Click NO when asked to restart to continue the tour.)",
        windowTitle="Demo 4a: pip_ensure — no-op verified",
    )

    # --- Part 2: Restart prompt ---
    import importlib
    mod = importlib.import_module(DEMO_IMPORT)
    print(f"[Tour] Imported {DEMO_IMPORT} {mod.__version__}")

    uninstall_pkg()
    clear_cache()

    slicer.pydeps.pip_ensure(reqs, requester="Feature Tour (restart demo)")

    slicer.util.infoDisplay(
        "Restart prompt demonstrated.\n"
        "\n"
        "This helps users know when a restart is needed after\n"
        "updating packages that were already imported.",
        windowTitle="Demo 4b: Restart Prompt — done",
    )


# ---------------------------------------------------------------------------
# Demo 5 — Advanced Options
# ---------------------------------------------------------------------------

def demo_advanced_options():
    """Constraints file and skip_packages"""

    # --- Part 1: Constraints ---
    fresh_slate()

    slicer.util.infoDisplay(
        "Constraints file: limits which versions pip may install.\n"
        f"\n"
        f"Installing {DEMO_PACKAGE} constrained to {DEMO_CONSTRAINT}.",
        windowTitle="Demo 5a: Constraints",
    )

    with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
        f.write(f"{DEMO_PACKAGE}{DEMO_CONSTRAINT}\n")
        constraints_path = f.name

    slicer.util.pip_install(
        DEMO_PACKAGE,
        constraints=constraints_path,
        requester="Feature Tour (constrained)",
    )
    os.unlink(constraints_path)

    ver = _md.version(DEMO_PACKAGE)
    slicer.util.infoDisplay(
        f"Installed {DEMO_PACKAGE} {ver} (constrained to {DEMO_CONSTRAINT}).\n"
        "\n"
        f"Now: skip_packages demo — installing {DEMO_PACKAGE}\n"
        f"while skipping '{DEMO_SKIP_DEP}' (one of its dependencies).\n"
        f"\n"
        f"First we uninstall {DEMO_SKIP_DEP} (if present) so we can\n"
        f"verify at the end that skip_packages actually prevented it.",
        windowTitle="Demo 5a: Constraints — done",
    )

    # --- Part 2: skip_packages ---
    fresh_slate()
    try:
        slicer.util.pip_uninstall(DEMO_SKIP_DEP)
    except Exception:
        pass

    skipped = slicer.util.pip_install(
        DEMO_PACKAGE,
        skip_packages=[DEMO_SKIP_DEP],
        requester="Feature Tour (skip_packages)",
    )

    lines = [f"  {s}" for s in (skipped or [])]
    print(f"[Tour] Skipped packages:\n" + "\n".join(lines))

    # Verify the metadata scrub: re-install normally, check the skipped dep
    slicer.util.pip_install(DEMO_PACKAGE, requester="Feature Tour (verify scrub)")
    dep_installed = slicer.pydeps.pip_check(Requirement(DEMO_SKIP_DEP))

    slicer.util.infoDisplay(
        f"Skipped {len(skipped or [])} package(s).\n"
        f"{DEMO_SKIP_DEP} installed after normal re-install: {dep_installed}\n"
        "\n"
        "Re-installing normally did NOT pull in the skipped dependency\n"
        "because the metadata scrub removed it.",
        windowTitle="Demo 5b: skip_packages — verified",
    )


# ---------------------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------------------

def do_cleanup():
    """Restore the user's environment."""
    if pkg_version_before_tour is not None:
        msg = (
            f"{DEMO_PACKAGE} {pkg_version_before_tour} was installed before\n"
            "the tour. Restore it now?"
        )
    else:
        msg = f"Uninstall {DEMO_PACKAGE} to leave your environment clean?"

    if slicer.util.confirmYesNoDisplay(msg, windowTitle="Feature Tour — Cleanup"):
        if pkg_version_before_tour is not None:
            uninstall_pkg()
            slicer.util.pip_install(
                f"{DEMO_PACKAGE}=={pkg_version_before_tour}",
                requester="Feature Tour (restore)",
            )
        else:
            uninstall_pkg()

    slicer.util.infoDisplay(
        "Tour complete! For full API docs:\n"
        "  help(slicer.pydeps.pip_ensure)",
        windowTitle="Feature Tour — Done",
    )


# ---------------------------------------------------------------------------
# Menu and main loop
# ---------------------------------------------------------------------------

MENU_ITEMS = [
    "1. Loading Dependencies",
    "2. Checking Requirements",
    "3. Installing with Progress",
    "4. Smart Install Workflow",
    "5. Advanced Options",
    "---",
    "Run All",
    "Exit Tour",
]

DEMO_FUNCS = {
    MENU_ITEMS[0]: demo_loading_deps,
    MENU_ITEMS[1]: demo_checking_reqs,
    MENU_ITEMS[2]: demo_install_progress,
    MENU_ITEMS[3]: demo_smart_install,
    MENU_ITEMS[4]: demo_advanced_options,
}

DEMO_ORDER = MENU_ITEMS[:5]


def run_tour():
    slicer.util.infoDisplay(
        "Welcome to the PR #9010 Feature Tour!\n"
        "\n"
        "Pick demos from the menu. Uses scikit-image as a demo package\n"
        "(configurable via DEMO_PACKAGE at the top of the script).\n"
        'Select "Run All" for the full experience.',
        windowTitle="Feature Tour — Welcome",
    )

    while True:
        dialog = qt.QInputDialog(slicer.util.mainWindow())
        dialog.setWindowTitle("Feature Tour")
        dialog.setLabelText("Choose a demo:")
        dialog.setComboBoxItems(MENU_ITEMS)
        dialog.setComboBoxEditable(False)

        if dialog.exec_() != qt.QDialog.Accepted:
            break

        choice = dialog.textValue()

        if choice == "Exit Tour" or choice == "---":
            break

        if choice == "Run All":
            for key in DEMO_ORDER:
                DEMO_FUNCS[key]()
        else:
            func = DEMO_FUNCS.get(choice)
            if func:
                func()

    do_cleanup()


# ---------------------------------------------------------------------------
# Start the tour
# ---------------------------------------------------------------------------

run_tour()
```

</details>

---

<details>
<summary><strong>References (click to expand)</strong></summary>

- This work is part of the [44th Slicer project week](https://projectweek.na-mic.org/PW44_2026_GranCanaria/Projects/PythonDependenciesInExtensions/)!
- [#7171 — Improving Support for Python Package Dependencies in Slicer Extensions](https://github.com/Slicer/Slicer/issues/7171) — This PR implements the "runtime installation" approach described in the issue: extensions declare dependencies via `requirements.txt`, and the new `slicer.pydeps` module handles checking and installation with optional constraints file support for coordinating versions across extensions.
- [#7707 — Allow scripted modules to declare and lazily install pip requirements](https://github.com/Slicer/Slicer/issues/7707) — Proposes a `LazyImportGroup` context manager that intercepts imports and triggers installation on first attribute access. This PR takes a simpler, more explicit approach: developers call `pip_ensure()` at the point dependencies are needed, then import normally. The explicit pattern trades some elegance for transparency and easier debugging. The `LazyImportGroup` approach could potentially be built on top of the primitives provided by `slicer.pydeps` (`load_requirements`, `pip_check`, `pip_install` with callbacks) if desired in the future. See #8181.
- At https://github.com/KitwareMedical/SlicerNNUnet/pull/21 is a demo of how some of the features here can simplify python dependency handling in the NNUnet extension.

</details>
