#!/bin/bash

# Set the default build type to Debug
BUILD_TYPE=${1:-Debug}

# Check if valid build type is passed
if [[ "$BUILD_TYPE" != "Debug" && "$BUILD_TYPE" != "Release" ]]; then
    echo "Invalid build type. Please specify 'Debug' or 'Release'."
    exit 1
fi

# Create a build directory if it doesn't exist
if [ ! -d "build" ]; then
    mkdir build
fi

# Run CMake to configure the build
echo "Building with CMake in $BUILD_TYPE mode..."
cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE

# Build the project
cmake --build build --config $BUILD_TYPE

echo "Build completed in $BUILD_TYPE mode."
