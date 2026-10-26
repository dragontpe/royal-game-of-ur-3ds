#!/bin/sh
# Regenerate every generated asset, then `make`:
#   romfs/scene.bin + textures (from Frank's Blender pack), 2D art and fonts,
#   decals, sounds and music.
set -e
cd "$(dirname "$0")/.."
tools/import_assets.sh "$@"
python3 tools/art/build_all.py
python3 tools/audio/make_sfx.py
