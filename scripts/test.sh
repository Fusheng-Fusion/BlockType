#!/bin/bash
set -e

BUILD_DIR=${1:-build-release}

echo "Running tests..."

if [ ! -d "${BUILD_DIR}" ]; then
  echo "Build directory ${BUILD_DIR} not found. Run ./scripts/build.sh first."
  exit 1
fi

cd "${BUILD_DIR}"

# 检测 CPU 核心数
if command -v nproc &> /dev/null; then
  NPROC=$(nproc)
elif command -v sysctl &> /dev/null; then
  NPROC=$(sysctl -n hw.ncpu)
else
  NPROC=4
fi

ctest --output-on-failure -j${NPROC}

echo ""
echo "Tests completed."
