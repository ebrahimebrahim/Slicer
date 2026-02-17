---
name: pr-feature-tour
description: >
  Create an interactive feature tour script for a Slicer PR. Use when the user
  wants a guided demo that reviewers can paste into Slicer's Python console to
  try out new or changed APIs. Produces a menu-driven Python script embedded in
  a markdown file.
---

# Create a PR Feature Tour

Generate an interactive Python script that PR reviewers paste into Slicer's Python console (View → Python Console). The script presents a menu of demos, each showcasing a feature from the PR.

## Output Format

A markdown file with:

1. A short header (title, 2-3 line setup instructions)
2. A single `python` code block containing the entire tour script

The script is self-contained — no external files needed. Reviewers copy-paste the whole block.

## Script Structure

Follow this skeleton:

```python
import importlib.metadata as _md
import os
import tempfile

import qt
from packaging.requirements import Requirement  # if needed

import slicer

# ---------------------------------------------------------------------------
# Configuration — change these to try different settings
# ---------------------------------------------------------------------------

SOME_CONSTANT = "value"        # explain what this controls

# ---------------------------------------------------------------------------
# State
# ---------------------------------------------------------------------------

# Capture pre-tour state so cleanup can restore it
state_before_tour = ...

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def some_helper():
    """Short helper functions for setup/teardown between demos."""
    pass

# ---------------------------------------------------------------------------
# Demo N — Feature Name
# ---------------------------------------------------------------------------

def demo_feature_name():
    """One-line description of what API calls this demonstrates"""

    # Setup (if needed)

    slicer.util.infoDisplay(
        "function_name() — what it does in one line.\n"
        "\n"
        "What to watch for.",
        windowTitle="Demo N: Feature Name",
    )

    # The actual API calls being demonstrated
    result = slicer.util.some_function(...)

    slicer.util.infoDisplay(
        "Brief result summary.\n"
        "Key takeaway in one line.",
        windowTitle="Demo N: Feature Name — done",
    )

# ... more demo functions ...

# ---------------------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------------------

def do_cleanup():
    """Restore the user's environment. Always ask before acting."""
    if slicer.util.confirmYesNoDisplay(
        "Clean up changes made during the tour?",
        windowTitle="Feature Tour — Cleanup",
    ):
        # Restore pre-tour state
        pass

    slicer.util.infoDisplay(
        "Tour complete!",
        windowTitle="Feature Tour — Done",
    )

# ---------------------------------------------------------------------------
# Menu and main loop
# ---------------------------------------------------------------------------

MENU_ITEMS = [
    "1. First Feature",
    "2. Second Feature",
    # ...
    "---",
    "Run All",
    "Exit Tour",
]

DEMO_FUNCS = {
    MENU_ITEMS[0]: demo_first_feature,
    MENU_ITEMS[1]: demo_second_feature,
    # ...
}

DEMO_ORDER = MENU_ITEMS[:N]  # just the numbered items


def run_tour():
    slicer.util.infoDisplay(
        "Welcome to the PR #NNNN Feature Tour!\n"
        "\n"
        "Pick demos from the menu.\n"
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


run_tour()
```

## Design Rules

### Menu

Use an instance-based `qt.QInputDialog` for the menu — ~10 lines, no custom Qt widgets needed. Include a `---` separator, `Run All`, and `Exit Tour` entries. Cancel (closing the dialog) also exits.

**PythonQt note:** Do NOT use the static `QInputDialog.getItem()` — PythonQt wraps its return value unreliably (the `ok` boolean may not work and indexing the result may give single characters instead of the full string). Instead, create an instance, call `exec_()`, and read `textValue()`:

```python
dialog = qt.QInputDialog(slicer.util.mainWindow())
dialog.setWindowTitle("Title")
dialog.setLabelText("Prompt:")
dialog.setComboBoxItems(items_list)
dialog.setComboBoxEditable(False)
if dialog.exec_() != qt.QDialog.Accepted:
    break  # user cancelled
choice = dialog.textValue()
```

### Configurable constants

Put user-changeable values (package names, version ranges, paths, etc.) as uppercase constants at the top of the script with inline comments. Reviewers should be able to change a constant and re-run to test with different inputs.

### Dialog text

**3-5 lines per dialog.** One-liner API description + what to watch for (pre-dialog) or result + takeaway (post-dialog). Do NOT write walls of text explaining parameters, return types, or design rationale — that belongs in the PR description or docstrings, not in popup dialogs.

### Demo functions

- Each demo is a **standalone function** that can be called independently from the menu or from "Run All."
- Name them `demo_<feature>()` so the mapping from menu item to code is obvious.
- Demos do NOT chain to the next demo. They simply return.
- Group related features into one demo when they are closely related (e.g., two loaders that return the same type). Aim for 3-7 demos total.

### Non-blocking / async operations

If a demo starts an async operation and needs to wait for it, use `qt.QEventLoop`:

```python
loop = qt.QEventLoop()
def on_done(result):
    qt.QTimer.singleShot(0, loop.quit)
start_async_operation(callback=on_done)
loop.exec_()  # blocks here until callback fires
```

The `QTimer.singleShot(0, loop.quit)` ensures `quit()` runs on the next event loop tick, avoiding the edge case where the callback fires before `exec_()` starts.

### State and cleanup

- Capture pre-tour state at script load time (before any demos run).
- `do_cleanup()` runs when the user exits the menu. Always **ask** before modifying the environment (use `confirmYesNoDisplay`).
- Offer to restore the original state if something was present before the tour.

### Console output

Use `print(f"[Tour] ...")` for diagnostic output that goes to the Python console (not a dialog). Useful for showing data structures, flag values, etc.

### Naming

Don't use `_` prefixes on functions or module-level variables — there's no module boundary in a pasted script, so there's nothing to hide from. The naming itself (`demo_*` for demos, everything else for infrastructure) is sufficient.

## Deciding What to Demo

Read the PR's changed files and description to identify the user-facing API surface. Group into demos by workflow, not by individual function. A good demo shows the function in context — calling it with realistic inputs and showing the result — not just that it exists. Skip internal/private functions. Focus on what an extension developer would call.
