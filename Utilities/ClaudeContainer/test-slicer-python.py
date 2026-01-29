#!/usr/bin/env python3
"""
Test script to verify Slicer Python environment works in container
"""

print("=" * 60)
print("Testing Slicer Python Environment")
print("=" * 60)

# Test basic Python
print("\n1. Basic Python: OK")
print(f"   Python version: {__import__('sys').version}")

# Test importing Slicer modules
try:
    import slicer
    print("\n2. Import slicer: OK")
    print(f"   Slicer version: {slicer.app.applicationVersion}")
except ImportError as e:
    print(f"\n2. Import slicer: FAILED - {e}")
    exit(1)

# Test importing MRML
try:
    import vtk, qt, ctk
    print("\n3. Import vtk, qt, ctk: OK")
except ImportError as e:
    print(f"\n3. Import vtk, qt, ctk: FAILED - {e}")
    exit(1)

# Create a simple MRML scene
try:
    scene = slicer.mrmlScene
    print(f"\n4. Access MRML scene: OK")
    print(f"   Number of nodes: {scene.GetNumberOfNodes()}")
except Exception as e:
    print(f"\n4. Access MRML scene: FAILED - {e}")
    exit(1)

# Test creating a node
try:
    node = slicer.vtkMRMLScalarVolumeNode()
    node.SetName("TestVolume")
    scene.AddNode(node)
    print(f"\n5. Create and add MRML node: OK")
    print(f"   Node name: {node.GetName()}")
    print(f"   Node ID: {node.GetID()}")
except Exception as e:
    print(f"\n5. Create and add MRML node: FAILED - {e}")
    exit(1)

print("\n" + "=" * 60)
print("✅ All tests passed! Slicer Python environment is working.")
print("=" * 60)

# Exit Slicer cleanly
import sys
sys.exit(0)
