#!/bin/bash
set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build-${BUILD_TYPE,,}"

echo "Building blocktype in ${BUILD_TYPE} mode..."

# 检测 LLVM 路径
if [ -z "$LLVM_DIR" ]; then
  if [ -d "/opt/homebrew/opt/llvm@18/lib/cmake/llvm" ]; then
    # macOS Apple Silicon
    LLVM_DIR="/opt/homebrew/opt/llvm@18/lib/cmake/llvm"
  elif [ -d "/usr/local/opt/llvm@18/lib/cmake/llvm" ]; then
    # macOS Intel
    LLVM_DIR="/usr/local/opt/llvm@18/lib/cmake/llvm"
  elif [ -d "/usr/lib/llvm-18/cmake" ]; then
    # Linux
    LLVM_DIR="/usr/lib/llvm-18/cmake"
  else
    echo "Warning: LLVM_DIR not set and LLVM not found in default locations"
  fi
fi

cmake -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="$(pwd)/install" \
  ${LLVM_DIR:+-DLLVM_DIR="${LLVM_DIR}"}

# 检测 CPU 核心数
if command -v nproc &> /dev/null; then
  NPROC=$(nproc)
elif command -v sysctl &> /dev/null; then
  NPROC=$(sysctl -n hw.ncpu)
else
  NPROC=4
fi

cmake --build "${BUILD_DIR}" -j${NPROC}

echo ""
echo "Build complete. Binary at ${BUILD_DIR}/bin/blocktype"
