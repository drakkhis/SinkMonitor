#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SML_ROOT="/mnt/Workspace/Antigravity/SatisfactorySpaceElevatorMOD/tools/SatisfactoryModLoader"
UPROJECT="${SML_ROOT}/FactoryGame.uproject"
RUN_UAT="/mnt/Workspace/UnrealEngineCSS/Engine/Build/BatchFiles/RunUAT.sh"
DIST_DIR="${REPO_ROOT}/dist"
ARCHIVE_SRC="${SML_ROOT}/Saved/ArchivedPlugins/SinkMonitor"

echo "=== [1/3] Running C++20 standalone rate tracker unit tests ==="
mkdir -p "${REPO_ROOT}/build"
g++ -std=c++20 -Wall -Wextra -Werror \
  -I "${REPO_ROOT}/SinkMonitor/Source/SinkMonitor/Public" \
  "${REPO_ROOT}/SinkMonitor/Tests/test_rate_tracker.cpp" \
  -o "${REPO_ROOT}/build/test_rate_tracker"
"${REPO_ROOT}/build/test_rate_tracker"

echo "=== [2/3] Packaging SinkMonitor for Windows, WindowsServer, LinuxServer ==="
UE_WINE_MSVC="${UE_WINE_MSVC:-/home/drakkhis/.local/share/msvc-wine}" \
WINE_MSVC_SHORT_PREFIX="${SML_ROOT}" \
WINE_MSVC_SHORT_DRIVE="s" \
"${RUN_UAT}" \
  -ScriptsForProject="${UPROJECT}" \
  PackagePlugin \
  -project="${UPROJECT}" \
  -clientconfig=Shipping \
  -serverconfig=Shipping \
  -utf8output \
  -DLCName=SinkMonitor \
  -build \
  -platform=Win64 \
  -server \
  -serverplatform=Win64+Linux \
  -nocompileeditor \
  -installed \
  -merge

echo "=== [3/3] Staging release archives to ${DIST_DIR} ==="
mkdir -p "${DIST_DIR}"
rm -f "${DIST_DIR}"/SinkMonitor*.zip
cp -v "${ARCHIVE_SRC}"/SinkMonitor*.zip "${DIST_DIR}/"
echo "Done. Archives staged in ${DIST_DIR}."
