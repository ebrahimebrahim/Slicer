# PR #9010 Feature Tour — Pip Dependency Handling Utilities

## How to use

1. Build Slicer from the `python-dependency-handling-improvements` branch
2. Launch Slicer, open the Python console (View → Python Console)
3. Paste the code block below — a menu will appear to pick demos

## Tour script

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
        "pip_ensure done. Calling it again would be instant — pip_check\n"
        "sees the package is installed and skips everything.\n"
        "\n"
        "Now: restart prompt demo. We will import the package, uninstall\n"
        "it, then pip_ensure again. Since it is in memory, a restart\n"
        "dialog will appear. Click NO to continue (do not restart).",
        windowTitle="Demo 4a: pip_ensure — done",
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
        f"while skipping '{DEMO_SKIP_DEP}' (one of its dependencies).",
        windowTitle="Demo 5a: Constraints — done",
    )

    # --- Part 2: skip_packages ---
    fresh_slate()

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
