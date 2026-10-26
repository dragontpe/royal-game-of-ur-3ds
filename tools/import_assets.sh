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
rm -f romfs/tex/*_etc1.t3x
cp -X assets/t3x/*_rgb565.t3x romfs/tex/   # the game loads RGB565; ETC1 stays in assets/ for a memory squeeze
GLBS=assets/glb/ur_complete_set.glb
python3 tools/glb2scene.py romfs/scene.bin $GLBS
