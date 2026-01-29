#!/bin/bash

# Build the Docker image
echo "Building Docker image..."
docker build -f "$(dirname "$0")/Dockerfile.claude" -t slicer-claude "$(dirname "$0")"

# Run the container with mounted directories
# Mount at same paths as host to avoid CMake path issues
# Run as host user to avoid permission issues with created files
# Mount only specific directories to maintain security isolation
echo "Starting container..."
docker run -it --rm \
  --user $(id -u):$(id -g) \
  -e HOME=$HOME \
  -v "$HOME/Slicer:$HOME/Slicer" \
  -v "$HOME/slicer-superbuild-v5.10:$HOME/slicer-superbuild-v5.10" \
  -v "$HOME/.cache:$HOME/.cache" \
  -w "$HOME/Slicer" \
  slicer-claude

# When you exit the container, it will be automatically removed (--rm flag)
