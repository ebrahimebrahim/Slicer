#!/bin/bash

# Build the Docker image
echo "Building Docker image..."
docker build -f "$(dirname "$0")/Dockerfile.claude" -t slicer-claude "$(dirname "$0")"

echo ""
echo "Running Slicer Python test in container..."
echo ""

docker run --rm \
  --user $(id -u):$(id -g) \
  -e HOME=$HOME \
  -v "$HOME/Slicer:$HOME/Slicer" \
  -v "$HOME/slicer-superbuild-v5.10:$HOME/slicer-superbuild-v5.10" \
  -v "$HOME/.cache:$HOME/.cache" \
  -w "$HOME/Slicer" \
  slicer-claude \
  bash -c "
    set -e
    echo '=== Running Slicer with Python test script (using xvfb) ==='
    xvfb-run --auto-servernum --server-args=\"-screen 0 1024x768x24\" \
      $HOME/slicer-superbuild-v5.10/Slicer-build/Slicer \
        --no-splash \
        --no-main-window \
        --python-script $HOME/Slicer/test-slicer-python.py
  "

echo ""
echo "Test completed."
