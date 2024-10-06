#!/bin/bash

BUILD_DIR="cmake-build-release"

if [ -d "$BUILD_DIR" ]; then
  echo "Removing existing build directory..."
  rm -rf "$BUILD_DIR"
fi

echo "Creating build directory..."
mkdir "$BUILD_DIR"

cd "$BUILD_DIR" || exit

echo "Running CMake..."
cmake -DCMAKE_BUILD_TYPE=Release ..

echo "Building the project..."
make -j 8

echo "Build complete."
