#!/usr/bin/env bash
set -euo pipefail

if [ -z "${1-}" ]; then
  echo "Usage: $0 X.Y.Z"
  exit 1
fi

VER="$1"
TAG="v$VER"

echo "This helper will show the commands to create and publish release $TAG."
echo
echo "1) Create annotated tag locally and push:"
echo "   git tag -a $TAG -m \"Release $TAG\""
echo "   git push origin $TAG"
echo
echo "2) Create and publish GitHub release (publishes and triggers CI):"
echo "   gh release create $TAG --title \"$TAG\" --notes-file RELEASE_DRAFT.md --repo OWNER/REPO"
echo
echo "After publishing, GitHub Actions will build Windows artifacts and upload them to the release."
