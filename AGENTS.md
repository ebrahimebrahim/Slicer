# AGENTS.md

This file provides guidance to LLM-based coding agents, including Codex, when working with code in this repository.

## Project Overview

3D Slicer is a multi-platform medical image informatics and visualization application. It is a C++17/Python hybrid codebase using Qt (5.15 or 6.8) for the GUI, with VTK, ITK, and CTK as core frameworks. The build system is CMake with a SuperBuild pattern that downloads and builds ~40 external dependencies.

## Build System

Slicer uses an out-of-source CMake SuperBuild. You do **not** build inside the source tree:

```bash
mkdir Slicer-build && cd Slicer-build
cmake -DSlicer_USE_SYSTEM_QT:BOOL=ON ../Slicer
make -j$(nproc)  # or ninja
```

The SuperBuild first builds all dependencies (VTK, ITK, CTK, Python, etc.) then builds Slicer itself as an "inner build" inside `Slicer-build/Slicer-build/`.

Key CMake options: `Slicer_USE_SYSTEM_QT`, `Slicer_BUILD_CLI`, `Slicer_USE_SimpleITK`, `Slicer_USE_PYTHONQT`, `CMAKE_BUILD_TYPE`.

### Python-only changes

When only Python files are modified (no C++ recompilation needed), use this target to copy them into the build tree:

```bash
cd Slicer-build/Slicer-build
cmake --build . --target CopySlicerPythonScriptFiles
```

This is much faster than a full rebuild and sufficient for iterating on Python code.

## Testing

```bash
cd Slicer-build/Slicer-build
ctest -j$(nproc)                    # run all tests
ctest -R TestName                   # run specific test by regex
ctest -R TestName --verbose         # verbose output
ctest -L ModuleName                 # run tests by label
```

Tests are CTest-based. Python module tests are also invoked through CTest. Nightly results go to [slicer.cdash.org](https://slicer.cdash.org).

### Python test naming in CTest

Python tests are registered via `slicer_add_python_unittest()` in `CMake/SlicerMacroPythonTesting.cmake`. The CTest name is: `py_${TESTNAME_PREFIX}${script_name_without_extension}`. For example, `Base/Python/slicer/tests/test_slicer_util_pip.py` registered with `TESTNAME_PREFIX nomainwindow_` becomes `py_nomainwindow_test_slicer_util_pip`. Use `ctest -N | grep <keyword>` to find the actual test name before running.

### TDD for bug fixes

When fixing an identified bug, use TDD: write the test first (or revert the fix to confirm the test fails), then apply the fix and confirm the test passes. This ensures the test actually catches the bug rather than passing vacuously. Also do this for tests with heavy mocking or complex setup, where it's easy for a test to pass vacuously because the mocks aren't wired correctly. Not every test needs this treatment, but bug-fix tests and mock-heavy tests definitely do.

### Headless Slicer smoke tests

When running a temporary Slicer Python smoke test with `Slicer --no-main-window --no-splash --python-script /tmp/test.py`, explicitly exit the application from the script. Otherwise the script can print the expected success output but leave the Slicer process running indefinitely.

For a simple success-only smoke test, end the script with:

```python
slicer.app.exit(0)
```

For assertion-heavy scripts, use a wrapper that exits with a non-zero status on failure:

```python
import traceback

try:
  # test setup and assertions here
  slicer.app.exit(0)
except Exception:
  traceback.print_exc()
  slicer.app.exit(1)
```

## Code Architecture

### Core layers (bottom-up)

- **Libs/MRML/** — Medical Reality Markup Language: the data model. Scene graph (`vtkMRMLScene`), nodes for volumes, transforms, markups, segmentations, etc. Everything revolves around MRML nodes.
- **Libs/vtkITK/** — Bridge between VTK and ITK image processing pipelines.
- **Libs/vtkSegmentationCore/** — Segmentation data structures and conversion infrastructure.
- **Base/** — Application framework built on MRML:
  - `Base/QTCore/` — Non-GUI application logic (`qSlicerCoreApplication`, module loading)
  - `Base/QTGUI/` — GUI framework (`qSlicerApplication`, layout manager, widget base classes)
  - `Base/QTCLI/` — Infrastructure for running CLI modules as separate processes
  - `Base/Logic/` — `vtkSlicerApplicationLogic`, task scheduling, data I/O
  - `Base/Python/slicer/` — Python `slicer` package providing the scripting API

### Module system

Modules live in `Modules/` and come in three types:
- **CLI** (`Modules/CLI/`) — Standalone executables communicating via XML parameter descriptors. Run as separate processes.
- **Loadable** (`Modules/Loadable/`) — C++ plugins (e.g., Volumes, Segmentations, Markups, Transforms, VolumeRendering). Each has Logic, MRML nodes, and Qt widgets.
- **Scripted** (`Modules/Scripted/`) — Pure Python modules using the same widget/logic pattern.

### Key patterns

- **Node/Logic/Widget triad**: Loadable modules typically define `vtkMRMLXxxNode` (data), `vtkSlicerXxxLogic` (business logic observing MRML), and `qSlicerXxxModuleWidget` (Qt GUI).
- **MRML Observer pattern**: Logic classes observe MRML node modifications via VTK's event system (`vtkCommand`/`AddObserver`).
- **Subject Hierarchy**: Unified tree organizing all data nodes (`Modules/Loadable/SubjectHierarchy/`).

### Applications

`Applications/SlicerApp/` — Main application entry point, default modules, and main window setup.

### Extensions

`Extensions/` — CMake infrastructure for building out-of-tree extension modules. Extensions follow the same CLI/Loadable/Scripted patterns.

## Commit Message Convention

Prefix every commit message with one of:
- `BUG:` — Fix for runtime crash or incorrect result
- `COMP:` — Compiler error or warning fix
- `DOC:` — Documentation change
- `ENH:` — New functionality
- `PERF:` — Performance improvement
- `STYLE:` — No logic impact (indentation, comments)
- `WIP:` — Work in progress

Subject line: imperative mood, <72 chars, capitalized, no trailing period.

Use the `LOCAL:` prefix for commits that won't be submitted upstream — agent-specific artifacts, scratchwork notes, planning documents, `pr-*-notes/` files, `AGENTS.md` updates, etc. Only commits with standard prefixes (ENH, BUG, etc.) get cherry-picked to branches destined for merging to main. Keep implementation changes and non-submission changes in separate commits to make cherry-picking clean.

## Code Formatting

Enforced via pre-commit hooks (`.pre-commit-config.yaml`):
- **C++**: clang-format (Mozilla-based style, see `.clang-format`). Files: `.cpp`, `.cxx`, `.h`, `.hpp`, `.hxx`, `.txx`.
- **Python**: ruff for linting (`.ruff.toml`, targets Python 3.12), pyupgrade for modernization (`--py312-plus`).
- **YAML**: prettier.

Run `pre-commit run --all-files` to check formatting locally.

## Building Documentation

Build docs locally from the source tree root using `uv`:

```bash
uvx --from sphinx --with-requirements requirements-docs.txt sphinx-build -b html Docs Docs/_build/html
```

Set `EXCLUDE_API_REFERENCE=True` to skip CLI API reference generation for faster iteration.

### Script repository

The Slicer script repository is available locally in `Docs/developer_guide/script_repository/`. Super helpful for Slicer idioms, and for python snippets for many common tasks.

- `Docs/developer_guide/script_repository.md` — top-level script repository index
- `Docs/developer_guide/script_repository/` — contains lots of specific docs that can provide very helpful recipes and python snippets for common slicer tasks

## Developer Setup

Run `Utilities/SetupForDevelopment.sh` after cloning to configure git hooks and settings.

## Slicer Python Environment

The Slicer superbuild bundles its own Python interpreter, separate from the system Python. The executable is located at `<superbuild-path>/python-install/bin/PythonSlicer`. Use this interpreter (not system `python3` or `pip`) when testing Slicer Python APIs or checking installed packages.
