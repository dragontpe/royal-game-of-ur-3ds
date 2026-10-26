#!/bin/sh
# Build ur-3ds.cia (installable with FBI) from the normal build.
# makerom and bannertool are the copies built for Gammon-X (~/gammon-x/vendor/tools).
set -e
cd "$(dirname "$0")/.."
export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
export PATH=$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH
MAKEROM=$HOME/gammon-x/vendor/tools/Project_CTR/makerom/bin/makerom
BANNERTOOL=$HOME/gammon-x/vendor/tools/bannertool/output/manual/bannertool
VERSION_MAJOR=0 VERSION_MINOR=10 VERSION_MICRO=0

make >/dev/null
mkdir -p build/cia
$BANNERTOOL makesmdh -s "Royal Game of Ur" -l "The Royal Game of Ur" -p "No Banners Software" \
    -i tools/cia/icon.png -o build/cia/icon.icn >/dev/null
$BANNERTOOL makebanner -i tools/cia/banner.png -a tools/cia/banner.wav -o build/cia/banner.bnr >/dev/null
$MAKEROM -f cia -o ur-3ds.cia -elf ur-3ds.elf -rsf tools/cia/ur.rsf -target t -exefslogo \
    -icon build/cia/icon.icn -banner build/cia/banner.bnr \
    -major $VERSION_MAJOR -minor $VERSION_MINOR -micro $VERSION_MICRO \
    -DAPP_TITLE="Royal Game of Ur" -DAPP_PRODUCT_CODE="CTR-H-URGM" -DAPP_UNIQUE_ID="0xF7A11" \
    -DAPP_SYSTEM_MODE="64MB" -DAPP_SYSTEM_MODE_EXT="Legacy" -DAPP_CATEGORY="Application" \
    -DAPP_USE_ON_SD="true" -DAPP_MEMORY_TYPE="Application" -DAPP_CPU_SPEED="268MHz" \
    -DAPP_ENABLE_L2_CACHE="false" -DAPP_VERSION_MAJOR="$VERSION_MAJOR" -DAPP_ROMFS="romfs" \
    -DAPP_ENCRYPTED=false
ls -la ur-3ds.cia
