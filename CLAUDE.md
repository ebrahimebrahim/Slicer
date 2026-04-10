# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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

### Reference implementations

- **Scripted modules**: `Modules/Scripted/SampleData/` and `Modules/Scripted/SegmentStatistics/`
  for the standard Python module pattern.
- **Loadable modules**: `Modules/Loadable/Volumes/` and `Modules/Loadable/Markups/` for the
  C++ module pattern with MRML nodes, logic, and Qt widgets.
- **CLI modules**: `Modules/CLI/AddScalarVolumes/` for the minimal CLI pattern with XML
  descriptor and SlicerExecutionModel.
- **Segment Editor effects**: `Modules/Loadable/Segmentations/EditorEffects/Python/SegmentEditorEffects/`
  for the effect API and `AbstractScriptedSegmentEditorEffect.py` base class.

### Key patterns

- **Node/Logic/Widget triad**: Loadable modules typically define `vtkMRMLXxxNode` (data), `vtkSlicerXxxLogic` (business logic observing MRML), and `qSlicerXxxModuleWidget` (Qt GUI).
- **MRML Observer pattern**: Logic classes observe MRML node modifications via VTK's event system (`vtkCommand`/`AddObserver`).
- **Subject Hierarchy**: Unified tree organizing all data nodes (`Modules/Loadable/SubjectHierarchy/`).

### Key Python API files

- `Base/Python/slicer/util.py` — The most important file: data loading/saving, node access,
  `arrayFromVolume()`, `loadVolume()`, `getNode()`, and UI utilities.
- `Base/Python/slicer/ScriptedLoadableModule.py` — Base classes for scripted modules
  (`ScriptedLoadableModule`, `ScriptedLoadableModuleWidget`, `ScriptedLoadableModuleLogic`,
  `ScriptedLoadableModuleTest`).
- `Base/Python/slicer/parameterNodeWrapper/` — Declarative parameter node system for module parameters.
- `Base/Python/slicer/__init__.py` — Top-level namespace (`slicer.mrmlScene`, `slicer.app`,
  `slicer.modules`).

### Applications

`Applications/SlicerApp/` — Main application entry point, default modules, and main window setup.

### Extensions

`Extensions/` — CMake infrastructure for building out-of-tree extension modules. Extensions follow the same CLI/Loadable/Scripted patterns.

## Script Repository

The developer guide includes a script repository with working Python recipes for common tasks.
The main entry point is `Docs/developer_guide/script_repository.md`, which includes per-topic files:

| File                                    | Topics                                        |
| --------------------------------------- | --------------------------------------------- |
| `script_repository/gui.md`              | Layouts, views, widget access, shortcuts       |
| `script_repository/volumes.md`          | Loading volumes, NumPy access, scalar/vector   |
| `script_repository/segmentations.md`    | Segment Editor, effects, import/export         |
| `script_repository/transforms.md`       | Linear and non-linear transforms               |
| `script_repository/markups.md`          | Fiducials, curves, planes, ROIs                |
| `script_repository/models.md`           | Surface meshes, polydata, model display        |
| `script_repository/dicom.md`            | DICOM loading, exporting, database             |
| `script_repository/plots.md`            | Chart views and plot series                    |
| `script_repository/sequences.md`        | Time sequences, browsing, replay               |
| `script_repository/subjecthierarchy.md` | Subject hierarchy tree operations              |

When writing Slicer Python code, search the script repository first for an existing example
before writing code from scratch. These snippets are the closest equivalent to official cookbook
recipes and are more idiomatic than ad-hoc code generation.

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

Additionally, use the `CLAUDE:` prefix for commits that won't be submitted upstream — Claude-specific artifacts, scratchwork notes, planning documents, `pr-*-notes/` files, CLAUDE.md updates, etc. Only commits with standard prefixes (ENH, BUG, etc.) get cherry-picked to branches destined for merging to main. Keep implementation changes and non-submission changes in separate commits to make cherry-picking clean.

## Code Formatting

Enforced via pre-commit hooks (`.pre-commit-config.yaml`):
- **C++**: clang-format (Mozilla-based style, see `.clang-format`). Files: `.cpp`, `.cxx`, `.h`, `.hpp`, `.hxx`, `.txx`.
- **Python**: ruff for linting (`.ruff.toml`, targets Python 3.12), pyupgrade for modernization (`--py312-plus`).
- **YAML**: prettier.

Run `pre-commit run --all-files` to check formatting locally.

## Coding Conventions

- **ASCII only in source code.** Unicode support has improved but may still cause errors.
- **Internationalization.** All user-facing strings must be translatable. In Python scripted
  modules, import `from slicer.i18n import tr as _` and wrap strings with `_("...")`.
  For shared context strings (e.g., module categories), use
  `translate("qSlicerAbstractCoreModule", "Quantification")` instead. Format placeholders
  inside translated strings must not be translated (use `_("Delete {count} files").format(count=n)`,
  not variable names in the target language). Developer-only text and log messages can remain
  untranslated. See the
  [SlicerLanguagePacks developer manual](https://github.com/SoniaPujolLab/SlicerLanguagePacks/blob/main/DevelopersManual.md).
- **Python naming in modules.** Scripted modules use camelCase for methods (`onApplyButton`,
  `setParameterNode`) and follow the `logic`/`widget`/`test` class separation pattern.
- **C++ naming.** Follows VTK conventions: `vtkNew`, `vtkSmartPointer`, `SetX()`/`GetX()`
  accessors, `PrintSelf`/`Modified()` pattern.

## Common Pitfalls

- **`arrayFromVolume` returns a view, not a copy.** After modifying the array in-place,
  call `slicer.util.arrayFromVolumeModified(volumeNode)` to notify the display pipeline.
  Forgetting this results in the view not updating.
- **MRML node names are not unique identifiers.** Multiple nodes can share the same name.
  Use `node.GetID()` for reliable identification, not `node.GetName()`.
- **The Python console runs on the main Qt thread.** Long-running operations block the UI.
  Use `slicer.app.processEvents()` in loops or run work in a background thread with
  `qt.QTimer.singleShot()` callbacks.
- **RAS vs LPS coordinate systems.** Slicer uses RAS (Right-Anterior-Superior) internally,
  while many file formats and tools use LPS (Left-Posterior-Superior). Transforms between
  RAS and LPS are a common source of sign-flip bugs.
- **Volume axis ordering.** `slicer.util.arrayFromVolume()` returns arrays in KJI order
  (slice, row, column), not IJK. This is the reverse of what many users expect.
- **`slicer.util.pip_install()` for runtime dependencies.** Slicer bundles its own Python.
  Extensions should install additional packages via `slicer.util.pip_install("package")`,
  not via system pip.

## Prefer Existing APIs

Before writing custom math, geometry, or image processing code, search for existing
implementations in this order:

1. **`slicer.util` and the script repository** — many common operations are one-liners.
2. **VTK filters** — smoothing, decimation, boolean ops, distance fields, coordinate
   transforms, interpolation.
3. **ITK filters** — registration, segmentation, morphology, statistics.
4. **CLI modules** (`Modules/CLI/`) — resampling, registration, model generation,
   invokable from Python via `slicer.cli.run()`.

## Building Documentation

Build docs locally from the source tree root using `uv`:

```bash
uvx --from sphinx --with-requirements requirements-docs.txt sphinx-build -b html Docs Docs/_build/html
```

Set `EXCLUDE_API_REFERENCE=True` to skip CLI API reference generation for faster iteration.

## Developer Setup

Run `Utilities/SetupForDevelopment.sh` after cloning to configure git hooks and settings.

## Slicer Python Environment

The Slicer superbuild bundles its own Python interpreter, separate from the system Python. The executable is located at `<superbuild-path>/python-install/bin/PythonSlicer`. Use this interpreter (not system `python3` or `pip`) when testing Slicer Python APIs or checking installed packages.
