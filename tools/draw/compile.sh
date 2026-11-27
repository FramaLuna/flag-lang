#!/bin/sh
# Compiles the shaders of draw for Vulkan into the SPIR-V that lib/draw.flg embeds.
#   tools/draw/compile.sh path/to/glslang
cd "$(dirname "$0")"
"$1" -V --quiet shader.vert -o ../../lib/draw_vertex.spv && "$1" -V --quiet shader.frag -o ../../lib/draw_fragment.spv
