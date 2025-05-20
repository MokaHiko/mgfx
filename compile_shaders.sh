#!/bin/sh

# OUTPUT_DIR="./compiled_shaders"

# Create output directory if it doesn't exist
# mkdir -p "$OUTPUT_DIR"

slangc -entry vertexMain assets/shaders/unlit.slang -target spirv -o unlit.vert.spv
slangc -entry fragmentMain assets/shaders/unlit.slang -target spirv -o unlit.frag.spv

###slangc assets/shaders/unlit.slang -entry vertexMain -entry fragmentMain -target spirv -o unlit.spv
