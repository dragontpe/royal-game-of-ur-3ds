#!/bin/sh
# Copy Frank's Blender asset pack into assets/ and build romfs/scene.bin + textures.
# Usage: tools/import_assets.sh ["/path/to/Royal Game of Ur - 3DS Assets"]
set -e
cd "$(dirname "$0")/.."
SRC=${1:-"$HOME/Desktop/Royal Game of Ur - 3DS Assets"}
mkdir -p assets/glb assets/t3x romfs/tex
cp -X "$SRC"/*.json "$SRC"/README.txt assets/
cp -X "$SRC"/glb/*.glb assets/glb/
cp -X "$SRC"/textures/t3x/*.t3x assets/t3x/
cp -X assets/t3x/*.t3x romfs/tex/
GLBS=assets/glb/ur_complete_set.glb
python3 tools/glb2scene.py romfs/scene.bin $GLBS
