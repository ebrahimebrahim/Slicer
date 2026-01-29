# Claude Code Development Container for Slicer

A Docker container setup for running Claude Code with `--dangerously-skip-permissions` while maintaining security isolation from sensitive files (SSH keys, credentials, etc.).

## Quick Start

```bash
# Run the interactive container
./run-claude-container.sh

# Inside the container:
claude --dangerously-skip-permissions
```

## What's Included

| File | Purpose |
|------|---------|
| `Dockerfile.claude` | Ubuntu 22.04 with Slicer build deps, Claude Code, ccache, xvfb |
| `run-claude-container.sh` | Interactive container for development |
| `test-building-slicer-in-container.sh` | Verify C++ builds work |
| `test-git-in-container.sh` | Verify git operations work |
| `test-slicer-python-in-container.sh` | Verify Slicer Python environment works |
| `test-slicer-python.py` | Sample Python test script |

## Security Isolation

The container only mounts specific directories you configure. By default:
- Slicer source directory (read/write)
- Slicer build tree (read/write)
- `~/.cache` for Slicer cache files

**Not accessible** from container:
- `~/.ssh` (SSH keys)
- `~/.aws` (AWS credentials)
- `~/.config` (application configs, tokens)
- `~/.gnupg` (GPG keys)

## Customization

Before using, update these files for your setup:

### In `Dockerfile.claude`:
- Git user name/email
- `safe.directory` path (your Slicer source path)
- ccache symlink paths (if your build uses ccache)
- WORKDIR

### In the shell scripts:
- Source directory path
- Build directory path

## Key Learnings

### Git "Dubious Ownership"
Mounted volumes appear to have different ownership inside Docker. Fix with:
```dockerfile
git config --global --add safe.directory /path/to/mounted/repo
```

### Headless Slicer Execution
Use `xvfb` (X Virtual Framebuffer), not `QT_QPA_PLATFORM=offscreen`:
```bash
xvfb-run --auto-servernum --server-args="-screen 0 1024x768x24" \
  /path/to/Slicer-build/Slicer --no-splash --no-main-window --python-script script.py
```

### User Permissions
Run container as host user to avoid root-owned files:
```bash
docker run --user $(id -u):$(id -g) ...
```

### Path Matching
Mount directories at the same paths as on host to reuse existing CMake build trees:
```bash
-v "$HOME/Slicer:$HOME/Slicer"  # Not -v "$HOME/Slicer:/workspace/Slicer"
```

### HOME Environment Variable
Set HOME so Slicer can find its cache directories:
```bash
docker run -e HOME=$HOME ...
```
