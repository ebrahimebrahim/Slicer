# Feature Proposal: `skip_packages` parameter for `pip_install`

## Motivation

Multiple Slicer extensions need to install a Python package *and all of its transitive dependencies* while excluding a specific set of packages from the entire dependency tree. This is necessary because:

1. **Slicer bundles modified packages.** Slicer's SimpleITK has a custom IO class. If pip overwrites it with the upstream SimpleITK, Slicer's image loading breaks. This affects any extension whose dependencies transitively depend on SimpleITK.

2. **Some packages require special installation.** PyTorch must be installed via the SlicerPyTorch extension, which selects the correct wheel for the user's CUDA/CPU/MPS hardware. Letting pip install torch from PyPI would get the wrong build.

3. **Some replacements are unnecessary/disruptive.** For example, `requests` is already bundled with Slicer; replacing it forces an unnecessary restart.

4. **Some packages are unwanted or broken.** SlicerTotalSegmentator skips `rt_utils` (depends on opencv-python, which is hard to build and not needed) and `dicom2nifti` (broken on Python 3.9). Extensions need the flexibility to exclude arbitrary packages for arbitrary reasons.

### Extensions implementing this pattern today

At least two Slicer extensions independently implement a recursive selective-install mechanism:

- **SlicerTotalSegmentator** (`TotalSegmentator.py:727-792`) — `pipInstallSelective()` skips SimpleITK, torch, nnunetv2, requests, rt_utils, dicom2nifti.
- **SlicerNNUNet** (`InstallLogic.py:288-320`) — `pipInstallSelective()` skips SimpleITK, torch, requests (and acvl-utils on Python 3.9).

Both implementations are nearly identical: ~70-90 lines of code doing `--no-deps` install, METADATA file scrubbing, recursive dependency walking with a skip list, and requirement string normalization. This is complex, error-prone boilerplate that belongs in the platform.

SlicerMONAIAuto3DSeg takes a different approach (using pip extras to select specific sub-packages of MONAI), but would also benefit from `skip_packages` if its dependencies ever collided with Slicer-bundled packages.

## What `skip_packages` should do

Add a `skip_packages` parameter to `pip_install` (and by extension, to `pip_ensure`). When provided, it changes pip installation from a single `pip install <package>` into a controlled recursive process that installs the package and all its transitive dependencies *except* the named packages.

### User-facing API

```python
# On pip_install:
slicer.util.pip_install(
    "nnunetv2>=2.3",
    skip_packages=["SimpleITK", "torch", "requests"],
)

# On pip_ensure:
reqs = [Requirement("nnunetv2>=2.3")]
slicer.util.pip_ensure(
    reqs,
    skip_packages=["SimpleITK", "torch", "requests"],
    requester="SlicerNNUNet",
)
```

### What it should guarantee

1. **None of the named packages are installed or upgraded.** Even if they appear as transitive dependencies at any depth in the tree.

2. **All *other* dependencies are installed.** The user gets a working nnunetv2 (or whatever package), minus the excluded packages. In practice, the excluded packages are already present in Slicer's environment (just not via pip), so things work.

3. **Future pip operations don't pull in skipped packages.** After installation, running `pip check` or `pip install --upgrade <something>` should not see the skipped packages as "missing" and try to install them. This requires modifying the installed package's METADATA file.

4. **The return value (or a callback) reports which packages were skipped and what version specs they declared.** This is important because the caller may need to act on it — for example, SlicerNNUNet captures the skipped torch version requirement and passes it to `PyTorchUtilsLogic.installTorch()`.

## How it should work internally

### Algorithm

```
pip_install_with_skips(requirement, skip_packages):
    1. pip install <requirement> --no-deps
    2. Read the installed package's declared dependencies via importlib.metadata.requires()
    3. For each dependency:
       a. Evaluate PEP 508 environment markers; skip if they don't apply
       b. If the package name matches skip_packages: record it as skipped, continue
       c. If the package is already installed at a compatible version: skip
       d. Otherwise: recursively call pip_install_with_skips(dependency, skip_packages)
    4. Scrub the installed package's METADATA file to remove Requires-Dist lines
       for all skipped packages
    5. Return the list of skipped requirement strings
```

### Step-by-step detail

#### Step 1: Install the top-level package with `--no-deps`

```python
_executePythonModule("pip", ["install", requirement, "--no-deps"], blocking=True)
```

This gets the package's code and metadata installed without touching any dependencies. The METADATA file is now on disk, which is needed for step 2.

#### Step 2: Read declared dependencies

```python
import importlib.metadata
importlib.invalidate_caches()  # ensure we see the just-installed metadata
deps = importlib.metadata.requires(package_name) or []
```

This returns strings like:
- `"numpy>=1.20"`
- `"SimpleITK>=2.0"`
- `"torch>=2.0"`
- `"ruff ; extra == \"dev\""`
- `"scipy>=1.0 ; python_version >= \"3.10\""`

#### Step 3: Filter and recurse

For each dependency string:

1. Parse it into a `packaging.requirements.Requirement`.
2. **Evaluate markers.** If the requirement has an environment marker (like `python_version < "3.9"`) that doesn't apply to the current environment, skip it. Also skip `extra == "..."` markers (those are optional dependencies, not required).
3. **Check the skip list.** Compare `req.name` (canonicalized) against `skip_packages`. If it matches, add the full requirement string to the skipped list and continue.
4. **Check if already satisfied.** Use `pip_check(req)` (already available in the PR). If the package is installed at a compatible version, skip.
5. **Recurse.** Call the same function for this dependency, passing along the same `skip_packages` list.

The recursion is key: if nnunetv2 depends on `dynamic-network-architectures`, which depends on `torch`, the torch skip must apply at that depth too.

Existing code in both SlicerTotalSegmentator and SlicerNNUNet handles this recursion. SlicerNNUNet's version is cleaner and includes proper marker evaluation and a `cleanPyPiRequirement` helper to normalize the requirement strings from `importlib.metadata`.

#### Step 4: Scrub METADATA

After installing a package with `--no-deps`, its `METADATA` file still declares all original dependencies including the skipped ones. If left as-is, a future `pip check` or `pip install --upgrade` will see them as missing and try to install them.

The fix is to remove the `Requires-Dist:` lines for skipped packages from the METADATA file:

```python
import importlib.metadata

meta_path = next(
    p.locate() for p in importlib.metadata.files(package_name)
    if p.name == "METADATA"
)

# Latin-1 because some packages have non-UTF-8 metadata
with open(meta_path, "r+", encoding="latin-1") as f:
    lines = f.readlines()
    f.seek(0)
    for line in lines:
        if line.startswith("Requires-Dist: "):
            req_str = line[len("Requires-Dist: "):].strip()
            req = Requirement(req_str)
            if _canonicalize(req.name) in canonicalized_skip_set:
                continue  # drop this line
        f.write(line)
    f.truncate()
```

This must happen for every package installed during the recursive walk, not just the top-level one. (In practice, it's rare for sub-dependencies to also depend on the skipped packages, but torch is a common transitive dependency.)

#### Step 5: Return skipped requirements

Return a list of the original requirement strings that were skipped, e.g.:

```python
["torch>=2.0", "SimpleITK>=2.0.2", "requests"]
```

The caller can then act on these. The most common action is finding the torch entry and passing its version spec to `PyTorchUtilsLogic.installTorch()`.

### Integration with existing `pip_install` modes

`skip_packages` changes the installation from a single pip subprocess into a multi-step Python-driven process. This means:

- **Blocking modes** work naturally — the recursive walk runs synchronously.
- **Non-blocking modes** are more complex. The simplest approach: if `skip_packages` is provided, always run the recursive walk in a background thread (or treat it as blocking internally within the progress dialog, which already uses a responsive event loop). The progress dialog and log callbacks still work — each sub-install produces log output.
- **The `constraints` parameter** should be passed through to every recursive pip call, so version constraints are respected at all levels.
- **The `no_deps_requirements` parameter** is orthogonal to `skip_packages` and both could be provided, but in practice `skip_packages` subsumes the `no_deps_requirements` use case. When `skip_packages` is used, the top-level package is already installed with `--no-deps` internally.

### Package name canonicalization

Package names must be compared in canonicalized form. pip and PyPI treat `SimpleITK`, `simpleitk`, `simple-itk`, and `simple_itk` as equivalent. Use `packaging.utils.canonicalize_name()` for comparison:

```python
from packaging.utils import canonicalize_name
skip_set = {canonicalize_name(name) for name in skip_packages}
# Then: canonicalize_name(req.name) in skip_set
```

Both existing extensions do prefix matching (`requirement.startswith(packageToSkip)`) which is fragile — it would match `SimpleITKUtilities` if someone named a package that. Canonicalized exact matching on the parsed requirement name is more correct.

### Signature

```python
def pip_install(
    requirements: str | list[str],
    constraints: str | Path | None = None,
    no_deps_requirements: str | list[str] | None = None,
    skip_packages: list[str] | None = None,
    blocking: bool = True,
    show_progress: bool = True,
    requester: str | None = None,
    parent: qt.QWidget | None = None,
    logCallback: Callable[[str], None] | None = None,
    completedCallback: Callable[[int], None] | None = None,
) -> list[str] | None:
    """...
    :param skip_packages: Package names to exclude from installation.
        When provided, the requirements are installed with --no-deps, and their
        dependency trees are walked recursively, installing each dependency
        individually while skipping any that match this list. Skipped packages
        are also removed from installed METADATA files to prevent future pip
        operations from flagging them as missing.
        Returns a list of the skipped requirement strings (with version specs).
        Package names are compared in canonicalized form (case-insensitive,
        hyphens/underscores normalized).
    ...
    """
```

For `pip_ensure`, the parameter passes through to `pip_install`:

```python
def pip_ensure(
    requirements: list[Requirement],
    constraints: str | Path | None = None,
    skip_packages: list[str] | None = None,  # new
    prompt_install: bool = True,
    prompt_restart: bool = True,
    requester: str | None = None,
    skip_in_testing: bool = True,
    show_progress: bool = True,
) -> list[str] | None:
    """...
    :param skip_packages: Forwarded to pip_install. See pip_install documentation.
    ...
    """
```

### What about `pip_check` and `skip_packages`?

`pip_check` currently checks if requirements are satisfied. When `skip_packages` is in play, a package can be "satisfied" even though its METADATA has been scrubbed (because the scrubbing removes the `Requires-Dist` lines, so `importlib.metadata.requires()` no longer lists the skipped packages). So `pip_check` should work correctly without modification, as long as METADATA scrubbing is done.

However, it could be useful to add a `skip_packages` parameter to `pip_check` as well, for the case where someone wants to check if requirements *would be* satisfied after a hypothetical `pip_install` with `skip_packages`:

```python
slicer.util.pip_check(
    Requirement("nnunetv2>=2.3"),
    skip_packages=["SimpleITK", "torch"],
)
```

This is a nice-to-have, not a must-have.

## Edge cases and considerations

### Circular dependencies

The recursive walk should track visited packages to avoid infinite loops. A `seen` set of canonicalized package names handles this.

### Packages already installed at incompatible versions

If a sub-dependency requires `numpy>=2.0` but `numpy==1.26` is installed, the current algorithm would call `pip install numpy>=2.0 --no-deps`. This upgrades numpy. This is correct behavior — only packages in `skip_packages` are protected.

### Extras

If a dependency string is `package[extra1,extra2]>=1.0`, the extras activate additional optional dependencies. The recursive walk should install the package with `--no-deps`, then read its `requires` metadata and evaluate which extras-gated dependencies are activated (by evaluating `extra == "extra1"` markers). `pip_check` already handles extras recursively, so the same logic can be reused.

However, the current implementations in SlicerNNUNet and SlicerTotalSegmentator skip extras entirely (they filter out `extra ==` markers as optional). This is probably fine in practice — the top-level extension knows which extras it needs and can list them in its own requirements.

### Error handling

If a sub-dependency fails to install, the recursive walk should report the error but ideally continue installing other dependencies (best-effort). This matches the current behavior of both SlicerNNUNet and SlicerTotalSegmentator, which log errors and attempt to continue.

### Performance

The recursive walk makes one pip subprocess call per package (each with `--no-deps`). This is slower than a single `pip install` that resolves everything at once. In practice the overhead is acceptable because:
- Most sub-dependencies are already installed (Slicer bundles numpy, scipy, etc.)
- The `pip_check` / `needsToInstallRequirement` check skips already-satisfied packages without spawning pip
- The number of packages actually needing installation is typically small (5-15)

## Summary

| Aspect | Current state (extension-level) | With `skip_packages` |
|---|---|---|
| Where the logic lives | Duplicated in each extension (~70-90 lines each) | In `slicer.util` (single implementation) |
| Package name matching | Prefix string matching (fragile) | Canonicalized exact name matching |
| Marker evaluation | Regex-based (SlicerTotalSegmentator) or via packaging (SlicerNNUNet) | Via `packaging.requirements` (correct) |
| METADATA scrubbing | Each extension does it separately | Handled automatically |
| Requirement string normalization | Each extension has its own regex/helper | Handled internally via `packaging.requirements.Requirement` |
| Progress/UI feedback | Custom per-extension | Integrated with `pip_install`'s progress dialog |
| Return value (skipped reqs) | List of raw strings | List of parsed requirement strings |
| Testing | Minimal (SlicerNNUNet has some unit tests) | Centralized tests in Slicer's test suite |
