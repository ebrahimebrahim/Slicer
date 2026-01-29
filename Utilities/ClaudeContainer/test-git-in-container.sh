#!/bin/bash

# Build the Docker image
echo "Building Docker image..."
docker build -f "$(dirname "$0")/Dockerfile.claude" -t slicer-claude "$(dirname "$0")"

echo ""
echo "Testing git functionality in container..."
echo ""

docker run --rm \
  --user $(id -u):$(id -g) \
  -v "$HOME/Slicer:$HOME/Slicer" \
  -w "$HOME/Slicer" \
  slicer-claude \
  bash -c '
    set -e

    echo "=== Git version ==="
    git --version

    echo ""
    echo "=== Git config ==="
    git config user.name
    git config user.email

    echo ""
    echo "=== Git status ==="
    git status

    echo ""
    echo "=== Current branch ==="
    git branch --show-current

    echo ""
    echo "=== Recent commits ==="
    git log --oneline -5

    echo ""
    echo "=== Test creating a commit ==="
    echo "# Test file created by container" > container-git-test.txt
    git add container-git-test.txt
    git commit -m "TEST: Container git test commit"

    echo ""
    echo "=== Verify the commit was created ==="
    git log --oneline -1

    echo ""
    echo "=== Clean up test commit ==="
    git reset --soft HEAD~1
    git restore --staged container-git-test.txt
    rm container-git-test.txt

    echo ""
    echo "✅ Git is working correctly in the container!"
  '

echo ""
echo "Test completed."
