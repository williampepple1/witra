#!/usr/bin/env bash
# =============================================================================
#  macdeploy.sh
#
#  macOS packaging helper for Witra.
#
#  Usage: packaging/macdeploy.sh <build-dir>
#
#  Runs macdeployqt on the app bundle, then creates a disk image at
#  <build-dir>/dmg/Witra-<version>-macOS.dmg.
#
#  Override the version with $WITRA_VERSION (CI sets this from a v* tag).
# =============================================================================
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"
VERSION="${WITRA_VERSION:-1.1.1}"
OUT="${BUILD}/dmg"
STAGE="${OUT}/stage"
DMG="${OUT}/Witra-${VERSION}-macOS.dmg"

echo "Witra macOS Packaging"
echo "Version: ${VERSION}"
echo "Build dir: ${BUILD}"

# Clean and create output directories
rm -rf "${OUT}"
mkdir -p "${STAGE}"

# Find the app bundle
BUNDLE=""
for candidate in \
    "${BUILD}/witra.app" \
    "${BUILD}/Witra.app" \
    "${BUILD}/bin/witra.app" \
    "${BUILD}/bin/Witra.app"; do
    if [[ -d "${candidate}" ]]; then
        BUNDLE="${candidate}"
        break
    fi
done

# If not found, search recursively
if [[ -z "${BUNDLE}" ]]; then
    BUNDLE=$(find "${BUILD}" -name "witra.app" -o -name "Witra.app" 2>/dev/null | head -1)
fi

if [[ -z "${BUNDLE}" || ! -d "${BUNDLE}" ]]; then
    echo "Error: Could not find witra.app bundle in ${BUILD}" >&2
    echo "Contents of ${BUILD}:" >&2
    ls -la "${BUILD}" >&2
    exit 1
fi

echo "Found app bundle: ${BUNDLE}"

# Run macdeployqt
echo "Running macdeployqt..."
macdeployqt "${BUNDLE}"

# Copy to staging area with nice name
cp -R "${BUNDLE}" "${STAGE}/Witra.app"

# Create Applications symlink for drag-to-install
ln -s /Applications "${STAGE}/Applications"

# Create the disk image
echo "Creating disk image: ${DMG}"
hdiutil create \
    -volname "Witra" \
    -srcfolder "${STAGE}" \
    -ov \
    -format UDZO \
    "${DMG}"

echo ""
echo "=== macOS Packaging Complete ==="
echo "Disk image: ${DMG}"
du -h "${DMG}"
