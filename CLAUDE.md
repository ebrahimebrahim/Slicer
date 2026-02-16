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

## Testing

```bash
cd Slicer-build/Slicer-build
ctest -j$(nproc)                    # run all tests
ctest -R TestName                   # run specific test by regex
ctest -R TestName --verbose         # verbose output
ctest -L ModuleName                 # run tests by label
```

Tests are CTest-based. Python module tests are also invoked through CTest. Nightly results go to [slicer.cdash.org](https://slicer.cdash.org).

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

## Code Formatting

Enforced via pre-commit hooks (`.pre-commit-config.yaml`):
- **C++**: clang-format (Mozilla-based style, see `.clang-format`). Files: `.cpp`, `.cxx`, `.h`, `.hpp`, `.hxx`, `.txx`.
- **Python**: ruff for linting (`.ruff.toml`, targets Python 3.12), pyupgrade for modernization (`--py312-plus`).
- **YAML**: prettier.

Run `pre-commit run --all-files` to check formatting locally.

## Developer Setup

Run `Utilities/SetupForDevelopment.sh` after cloning to configure git hooks and settings.

## Slicer Python Environment

The Slicer superbuild bundles its own Python interpreter, separate from the system Python. The executable is located at `<superbuild-path>/python-install/bin/PythonSlicer`. Use this interpreter (not system `python3` or `pip`) when testing Slicer Python APIs or checking installed packages.
