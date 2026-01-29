---
name: slicer-python-test
description: Run a Python script in the Slicer environment. Use this when testing Python code that uses Slicer modules (slicer, vtk, qt, ctk, MRML). Essential in containers or headless environments where xvfb is needed.
---

# Run Python Script in Slicer Environment

Use this skill to test Python code that depends on Slicer modules.

## When to Use

- Testing Python snippets that import `slicer`, `vtk`, `qt`, `ctk`, or MRML classes
- Running in a container or headless environment (no display)
- Verifying Slicer scripted module logic

## Finding the Slicer Executable

The Slicer executable is in the **inner build** directory (not the superbuild root):

```bash
# Pattern: <superbuild-dir>/Slicer-build/Slicer
# Example: ~/Slicer-SuperBuild-Release/Slicer-build/Slicer

# Find it with:
find ~ -name "Slicer" -path "*/Slicer-build/*" -type f -executable 2>/dev/null | head -1
```

## Command Pattern

### In a container or headless environment (use xvfb):

```bash
xvfb-run --auto-servernum --server-args="-screen 0 1024x768x24" \
  <path-to-Slicer-build>/Slicer \
    --no-splash \
    --no-main-window \
    --python-script <path-to-script.py>
```

### On a system with display:

```bash
<path-to-Slicer-build>/Slicer \
  --no-splash \
  --no-main-window \
  --python-script <path-to-script.py>
```

## Important Notes

1. **Script must exit explicitly** - Add `sys.exit(0)` at the end of your script, otherwise Slicer will keep running
2. **xvfb is required in containers** - The offscreen Qt platform (`QT_QPA_PLATFORM=offscreen`) doesn't work well with Slicer; use xvfb instead
3. **SSL warnings are normal** - You may see `QSslSocket: cannot resolve...` warnings; these don't affect functionality

## Example Test Script

```python
#!/usr/bin/env python3
"""Test script for Slicer Python environment"""

import slicer
import vtk

# Your test code here
node = slicer.vtkMRMLScalarVolumeNode()
node.SetName("TestNode")
slicer.mrmlScene.AddNode(node)

print(f"Created node: {node.GetID()}")

# Always exit explicitly in headless mode
import sys
sys.exit(0)
```

## Detecting Headless Environment

```python
import os
headless = "DISPLAY" not in os.environ or os.environ.get("DISPLAY") == ""
```
