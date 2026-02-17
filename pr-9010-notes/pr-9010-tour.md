# PR #9010 Feature Tour — Pip Dependency Handling Utilities

## How to use

1. Build Slicer from the `python-dependency-handling-improvements` branch
2. Launch Slicer
3. Open the Python console (View → Python Console)
4. Copy the entire code block below and paste it into the console
5. Follow the info dialogs — click OK to advance through each step

## Tour script

```python
import os
import tempfile

import qt
from packaging.requirements import Requirement

import slicer

# Record whether scikit-image was already installed before the tour,
# so we can restore the user's environment at the end.
try:
    import importlib.metadata as _md
    _skimage_version_before_tour = _md.version("scikit-image")
except _md.PackageNotFoundError:
    _skimage_version_before_tour = None


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _clear_cache():
    """Purge the pip download cache so installs show real download progress."""
    slicer.util._executePythonModule("pip", ["cache", "purge"])


def _uninstall_skimage():
    """Remove scikit-image if present. Silently ignores errors."""
    try:
        slicer.util.pip_uninstall("scikit-image")
    except Exception:
        pass


def _fresh_slate():
    """Uninstall scikit-image and clear the cache."""
    _uninstall_skimage()
    _clear_cache()


# ---------------------------------------------------------------------------
# Step 1 — Welcome
# ---------------------------------------------------------------------------

def step_welcome():
    slicer.util.infoDisplay(
        "Welcome to the Pip Dependency Handling Feature Tour!\n"
        "\n"
        "This guided walkthrough demonstrates the new Python package\n"
        "management utilities added to slicer.util (PR #9010).\n"
        "\n"
        "At each step an info dialog explains what is about to happen.\n"
        "Click OK to proceed to the next step.\n"
        "\n"
        "We will use scikit-image as a demo package throughout the tour.",
        windowTitle="Feature Tour [1/11] — Welcome",
    )
    step_load_requirements()


# ---------------------------------------------------------------------------
# Step 2 — load_requirements
# ---------------------------------------------------------------------------

def step_load_requirements():
    slicer.util.infoDisplay(
        "load_requirements(path)\n"
        "\n"
        "Parses a requirements.txt file into a list of\n"
        "packaging.requirements.Requirement objects.\n"
        "\n"
        "Comments, blank lines, and pip options (-r, -c, --index-url)\n"
        "are automatically skipped.\n"
        "\n"
        "We will create a temp requirements file and load it.",
        windowTitle="Feature Tour [2/11] — load_requirements",
    )

    with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
        f.write("# Example requirements file\n")
        f.write("numpy>=1.20\n")
        f.write("scikit-image>=0.20\n")
        f.write("nonexistent-package-xyz>=1.0\n")
        f.write("-c constraints.txt\n")
        f.write("\n")
        path = f.name

    reqs = slicer.util.load_requirements(path)
    os.unlink(path)

    lines = [f"  {r.name}  {r.specifier}" for r in reqs]
    slicer.util.infoDisplay(
        f"Loaded {len(reqs)} requirements (comments and options skipped):\n"
        "\n" + "\n".join(lines),
        windowTitle="Feature Tour [2/11] — load_requirements result",
    )
    step_load_pyproject()


# ---------------------------------------------------------------------------
# Step 3 — load_pyproject_dependencies
# ---------------------------------------------------------------------------

def step_load_pyproject():
    slicer.util.infoDisplay(
        "load_pyproject_dependencies(path)\n"
        "\n"
        "Alternative to load_requirements — reads the\n"
        "[project.dependencies] list from a pyproject.toml file\n"
        "(PEP 621) and returns the same Requirement objects.\n"
        "\n"
        "Only the dependencies list is read; other fields in\n"
        "[project] (name, version, etc.) are not required.\n"
        "\n"
        "We will create a temp pyproject.toml and load it.",
        windowTitle="Feature Tour [3/11] — load_pyproject_dependencies",
    )

    with tempfile.NamedTemporaryFile(mode="w", suffix=".toml", delete=False) as f:
        f.write("[project]\n")
        f.write("dependencies = [\n")
        f.write('    "numpy>=1.20",\n')
        f.write('    "scikit-image>=0.20",\n')
        f.write('    "nonexistent-package-xyz>=1.0",\n')
        f.write("]\n")
        path = f.name

    reqs = slicer.util.load_pyproject_dependencies(path)
    os.unlink(path)

    lines = [f"  {r.name}  {r.specifier}" for r in reqs]
    slicer.util.infoDisplay(
        f"Loaded {len(reqs)} dependencies from pyproject.toml:\n"
        "\n" + "\n".join(lines) + "\n"
        "\n"
        "Same result as load_requirements — both return\n"
        "Requirement objects that work with pip_check and pip_ensure.",
        windowTitle="Feature Tour [3/11] — load_pyproject_dependencies result",
    )
    step_pip_check()


# ---------------------------------------------------------------------------
# Step 4 — pip_check
# ---------------------------------------------------------------------------

def step_pip_check():
    slicer.util.infoDisplay(
        "pip_check(req)\n"
        "\n"
        "Checks whether requirement(s) are satisfied — pure Python,\n"
        "no subprocess call to pip. Handles version specifiers, extras,\n"
        "and environment markers.\n"
        "\n"
        "We will check several requirements against the current\n"
        "environment.",
        windowTitle="Feature Tour [4/11] — pip_check",
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
        ok = slicer.util.pip_check(req)
        mark = "SATISFIED" if ok else "NOT satisfied"
        results.append(f"  [{mark}]  {spec}\n      {desc}")

    slicer.util.infoDisplay(
        "pip_check results:\n"
        "\n" + "\n\n".join(results),
        windowTitle="Feature Tour [4/11] — pip_check results",
    )
    step_install_dialog()


# ---------------------------------------------------------------------------
# Step 5 — pip_install with modal progress dialog
# ---------------------------------------------------------------------------

def step_install_dialog():
    _fresh_slate()

    slicer.util.infoDisplay(
        "pip_install() — Modal Progress Dialog\n"
        "\n"
        "Default mode: blocking=True, show_progress=True\n"
        "\n"
        "A modal dialog appears with:\n"
        "  - A status message\n"
        "  - An indeterminate progress bar\n"
        "  - A collapsible Details section showing the pip log\n"
        "\n"
        "The dialog cannot be closed via Escape or the X button.\n"
        "\n"
        "Click OK to install scikit-image. Try expanding the\n"
        "Details section in the progress dialog that appears!",
        windowTitle="Feature Tour [5/11] — Progress Dialog",
    )

    slicer.util.pip_install("scikit-image", requester="Feature Tour")

    slicer.util.infoDisplay(
        "Installation complete!\n"
        "\n"
        "You just saw the modal progress dialog with real-time\n"
        "pip output in the Details section.",
        windowTitle="Feature Tour [5/11] — Progress Dialog done",
    )
    step_nonblocking()


# ---------------------------------------------------------------------------
# Step 6 — Non-blocking install with status bar
# ---------------------------------------------------------------------------

def step_nonblocking():
    _fresh_slate()

    slicer.util.infoDisplay(
        "pip_install() — Non-blocking with Status Bar\n"
        "\n"
        "Mode: blocking=False, show_progress=True\n"
        "\n"
        "The call returns immediately. Watch for:\n"
        "  - A busy cursor while installation runs\n"
        "  - Pip output appearing in the STATUS BAR at the bottom\n"
        "  - The UI remains fully interactive\n"
        "\n"
        "We also call isPipInstallInProgress() right after starting,\n"
        "which should return True.\n"
        "\n"
        "Click OK to begin the non-blocking install.",
        windowTitle="Feature Tour [6/11] — Status Bar Mode",
    )

    def on_complete(return_code):
        msg = (
            f"Non-blocking install finished (return code {return_code}).\n"
            "\n"
            "The status bar showed pip output as it happened and the\n"
            "UI remained responsive throughout."
        )
        # Schedule the dialog outside the callback context
        qt.QTimer.singleShot(0, lambda: _after_nonblocking(msg))

    slicer.util.pip_install(
        "scikit-image",
        blocking=False,
        show_progress=True,
        requester="Feature Tour",
        completedCallback=on_complete,
    )

    # Check the in-progress flag shortly after starting
    def check_flag():
        in_progress = slicer.util.isPipInstallInProgress()
        print(f"[Tour] isPipInstallInProgress() = {in_progress}")

    qt.QTimer.singleShot(500, check_flag)


def _after_nonblocking(msg):
    slicer.util.infoDisplay(msg, windowTitle="Feature Tour [6/11] — Status Bar done")
    step_pip_ensure()


# ---------------------------------------------------------------------------
# Step 7 — pip_ensure with confirmation dialog
# ---------------------------------------------------------------------------

def step_pip_ensure():
    _fresh_slate()

    slicer.util.infoDisplay(
        "pip_ensure() — The Recommended High-Level API\n"
        "\n"
        "This is the function extensions should call. It:\n"
        "  1. Checks which requirements are missing (pip_check)\n"
        "  2. Shows a confirmation dialog listing the packages\n"
        "  3. Installs missing packages with a progress dialog\n"
        "  4. Detects if updated packages were already imported\n"
        "     and offers to restart Slicer if needed\n"
        "\n"
        "You will see a confirmation dialog, then a progress dialog.\n"
        "(No restart prompt this time — scikit-image was not imported.)\n"
        "\n"
        "Click OK to begin.",
        windowTitle="Feature Tour [7/11] — pip_ensure",
    )

    reqs = [Requirement("scikit-image>=0.20")]
    slicer.util.pip_ensure(reqs, requester="Feature Tour")

    slicer.util.infoDisplay(
        "pip_ensure completed!\n"
        "\n"
        "The full workflow was:\n"
        "  1. pip_check found scikit-image was missing\n"
        "  2. Confirmation dialog listed the missing package\n"
        "  3. Progress dialog showed the installation\n"
        "  4. No restart prompt (scikit-image wasn't imported yet)\n"
        "\n"
        "Calling pip_ensure again now would be instant — pip_check\n"
        "sees that scikit-image is already installed and skips\n"
        "everything.",
        windowTitle="Feature Tour [7/11] — pip_ensure done",
    )
    step_restart_prompt()


# ---------------------------------------------------------------------------
# Step 8 — Restart prompt demonstration
# ---------------------------------------------------------------------------

def step_restart_prompt():
    slicer.util.infoDisplay(
        "pip_ensure() — Restart Prompt\n"
        "\n"
        "After installing, pip_ensure detects whether any updated\n"
        "packages were already imported in the current session.\n"
        "If so, a restart dialog appears (old versions remain in memory).\n"
        "\n"
        "To demonstrate: we will import scikit-image (it was just\n"
        "installed), then uninstall and re-install it via pip_ensure.\n"
        "Since skimage is now in sys.modules, the restart prompt\n"
        "should appear.\n"
        "\n"
        "When the restart dialog appears, click NO to continue\n"
        "the tour (do not actually restart).\n"
        "\n"
        "Click OK to begin.",
        windowTitle="Feature Tour [8/11] — Restart Prompt",
    )

    # Import scikit-image so it's in sys.modules
    import skimage  # noqa: F401
    print(f"[Tour] Imported skimage {skimage.__version__}")

    # Now uninstall and re-install — pip_ensure will detect skimage in sys.modules
    _uninstall_skimage()
    _clear_cache()

    reqs = [Requirement("scikit-image>=0.20")]
    slicer.util.pip_ensure(reqs, requester="Feature Tour (restart demo)")

    slicer.util.infoDisplay(
        "Restart prompt demonstrated!\n"
        "\n"
        "You saw a 'Restart Recommended' dialog because scikit-image\n"
        "was already imported before the reinstall. The Details section\n"
        "showed the package name and version transition.\n"
        "\n"
        "This feature helps extension users know when they need to\n"
        "restart Slicer after a package update.",
        windowTitle="Feature Tour [8/11] — Restart Prompt done",
    )
    step_constraints()


# ---------------------------------------------------------------------------
# Step 9 — Constraints file
# ---------------------------------------------------------------------------

def step_constraints():
    _fresh_slate()

    slicer.util.infoDisplay(
        "Constraints File Support\n"
        "\n"
        "pip_install() and pip_ensure() accept a constraints parameter.\n"
        "\n"
        "A constraints file limits which versions pip may install,\n"
        "without triggering installation by itself. It is passed to\n"
        "pip as '-c constraints.txt'.\n"
        "\n"
        "We will create a constraints file that pins scikit-image\n"
        "to < 0.25, then install it.\n"
        "\n"
        "Click OK to begin.",
        windowTitle="Feature Tour [9/11] — Constraints",
    )

    with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
        f.write("scikit-image>=0.20,<0.25\n")
        constraints_path = f.name

    slicer.util.pip_install(
        "scikit-image",
        constraints=constraints_path,
        requester="Feature Tour (constrained)",
    )
    os.unlink(constraints_path)

    import importlib.metadata

    installed_version = importlib.metadata.version("scikit-image")

    slicer.util.infoDisplay(
        f"Installed scikit-image {installed_version}\n"
        f"(constrained to >=0.20, <0.25).\n"
        "\n"
        "The constraints file was passed as '-c constraints.txt'\n"
        "to pip, limiting the installable version range.",
        windowTitle="Feature Tour [9/11] — Constraints result",
    )
    step_skip_packages()


# ---------------------------------------------------------------------------
# Step 10 — skip_packages
# ---------------------------------------------------------------------------

def step_skip_packages():
    _fresh_slate()

    slicer.util.infoDisplay(
        "skip_packages — Selective Dependency Installation\n"
        "\n"
        "pip_install() and pip_ensure() accept a skip_packages parameter.\n"
        "\n"
        "When provided, each package is installed individually with\n"
        "--no-deps, and its dependency tree is walked recursively,\n"
        "skipping any packages whose name matches the skip list.\n"
        "Package metadata is updated so pip doesn't later try to\n"
        "install the skipped packages.\n"
        "\n"
        "This replaces ~70-90 lines of boilerplate that extensions\n"
        "like SlicerNNUNet and SlicerTotalSegmentator duplicate today.\n"
        "\n"
        "We will install scikit-image while skipping 'imageio'\n"
        "(one of its dependencies).\n"
        "\n"
        "Click OK to begin.",
        windowTitle="Feature Tour [10/11] — skip_packages",
    )

    skipped = slicer.util.pip_install(
        "scikit-image",
        skip_packages=["imageio"],
        requester="Feature Tour (skip_packages)",
    )

    lines = [f"  {s}" for s in (skipped or [])]
    slicer.util.infoDisplay(
        f"Installation complete! {len(skipped or [])} package(s) skipped:\n"
        "\n" + "\n".join(lines) + "\n"
        "\n"
        "The skipped packages were not installed, and their\n"
        "Requires-Dist entries were removed from the installed\n"
        "package metadata.\n"
        "\n"
        "To prove the scrub worked, we will now try installing\n"
        "scikit-image again (without skip_packages). Since\n"
        "scikit-image is already installed and its metadata no\n"
        "longer lists imageio as a dependency, pip will say\n"
        "'already satisfied' and imageio will remain absent.\n"
        "\n"
        "Click OK to try it.",
        windowTitle="Feature Tour [10/11] — skip_packages result",
    )

    # Re-install scikit-image normally — should NOT pull in imageio
    slicer.util.pip_install("scikit-image", requester="Feature Tour (verify scrub)")
    imageio_installed = slicer.util.pip_check(Requirement("imageio"))

    slicer.util.infoDisplay(
        "Verification complete!\n"
        "\n"
        f"  imageio installed: {imageio_installed}\n"
        "\n"
        "This should be False. A normal pip_install of scikit-image\n"
        "did not bring in imageio, because the metadata scrub removed\n"
        "it from scikit-image's declared dependencies.",
        windowTitle="Feature Tour [10/11] — skip_packages verified",
    )
    step_cleanup()


# ---------------------------------------------------------------------------
# Step 11 — Cleanup
# ---------------------------------------------------------------------------

def step_cleanup():
    if _skimage_version_before_tour is not None:
        slicer.util.infoDisplay(
            "Cleanup\n"
            "\n"
            f"scikit-image {_skimage_version_before_tour} was installed before\n"
            "the tour. We will reinstall that exact version now.",
            windowTitle="Feature Tour [11/11] — Cleanup",
        )
        _uninstall_skimage()
        slicer.util.pip_install(
            f"scikit-image=={_skimage_version_before_tour}",
            requester="Feature Tour (restore)",
        )
    else:
        slicer.util.infoDisplay(
            "Cleanup\n"
            "\n"
            "scikit-image was not installed before the tour.\n"
            "We will uninstall it to leave your environment clean.",
            windowTitle="Feature Tour [11/11] — Cleanup",
        )
        _uninstall_skimage()

    slicer.util.infoDisplay(
        "Tour Complete!\n"
        "\n"
        "You have seen all the major features:\n"
        "\n"
        "  - load_requirements() — parse requirements.txt files\n"
        "  - load_pyproject_dependencies() — parse pyproject.toml\n"
        "  - pip_check() — fast requirement validation\n"
        "  - pip_install() — modal progress dialog\n"
        "  - pip_install() — non-blocking status bar mode\n"
        "  - isPipInstallInProgress() — concurrent install guard\n"
        "  - pip_ensure() — high-level check + prompt + install\n"
        "  - pip_ensure() — restart prompt for updated imports\n"
        "  - Constraints file support\n"
        "  - skip_packages — selective dependency installation\n"
        "\n"
        "For full details, try:  help(slicer.util.pip_ensure)\n"
        "\n"
        "Thank you for reviewing PR #9010!",
        windowTitle="Feature Tour [11/11] — Complete!",
    )


# ---------------------------------------------------------------------------
# Start the tour
# ---------------------------------------------------------------------------

step_welcome()
```
