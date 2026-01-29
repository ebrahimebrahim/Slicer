#!/bin/bash

# Build the Docker image
echo "Building Docker image..."
docker build -f "$(dirname "$0")/Dockerfile.claude" -t slicer-claude "$(dirname "$0")"

# Run the container and test building from the inner build directory
echo ""
echo "Starting container and testing build from inner build directory..."
echo "Using: $HOME/slicer-superbuild-v5.10/Slicer-build"
echo ""

docker run --rm \
  --user $(id -u):$(id -g) \
  -v "$HOME/Slicer:$HOME/Slicer" \
  -v "$HOME/slicer-superbuild-v5.10:$HOME/slicer-superbuild-v5.10" \
  -w "$HOME/slicer-superbuild-v5.10/Slicer-build" \
  slicer-claude \
  bash -c '
    set -e  # Exit on any error

    echo "=== Building Slicer (inner build) with make -j 8 ==="
    echo "Working directory: $(pwd)"
    echo ""
    make -j 8

    echo ""
    echo "=== Build completed successfully! ==="
  '

echo ""
echo "Container exited and was removed."
