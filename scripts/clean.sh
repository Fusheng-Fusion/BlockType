#!/bin/bash

echo "Cleaning build directories..."

# 删除所有 build-* 目录
for dir in build-*; do
  if [ -d "$dir" ]; then
    echo "Removing $dir..."
    rm -rf "$dir"
  fi
done

# 删除 install 目录
if [ -d "install" ]; then
  echo "Removing install..."
  rm -rf install
fi

echo "Clean complete."