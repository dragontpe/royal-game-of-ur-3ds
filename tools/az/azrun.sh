#!/bin/zsh
# azrun.sh <rom> <seconds> <out.png> [stereo]
# Muted background Azahar run in an isolated HOME (tools/az/home), Old 3DS mode.
# Kills only the Azahar it started (other sessions may run their own).
# stereo: 3D slider at 100% and side-by-side output, so both eyes land in the shot.
AZ=${0:A:h}; FH=$AZ/home; LOG=$AZ/azahar.log
Q="$FH/Library/Application Support/Azahar/config/qt-config.ini"
sed -i '' -e 's/^is_new_3ds=.*/is_new_3ds=false/' -e 's/^is_new_3ds\\default=.*/is_new_3ds\\default=false/' "$Q"
if [ "$4" = stereo ]; then F=100; R=1; else F=0; R=0; fi
sed -i '' -e "s/^factor_3d=.*/factor_3d=$F/" -e 's/^factor_3d\\default=.*/factor_3d\\default=false/' \
          -e "s/^render_3d=.*/render_3d=$R/" -e 's/^render_3d\\default=.*/render_3d\\default=false/' "$Q"
FRONT=$(osascript -e 'tell application "System Events" to get name of first application process whose frontmost is true' 2>/dev/null)
BEFORE=$(pgrep -f "Azahar.app/Contents/MacOS/azahar" | sort)
defaults write org.azahar-emu.azahar NSAppSleepDisabled -bool YES
open -g -n -a /Applications/Azahar.app --env HOME=$FH --stdout $LOG --stderr $LOG --args "$1"
sleep 1; [ -n "$FRONT" ] && osascript -e "tell application \"$FRONT\" to activate" 2>/dev/null
AFTER=$(pgrep -f "Azahar.app/Contents/MacOS/azahar" | sort)
MINE=$(comm -13 <(echo "$BEFORE") <(echo "$AFTER"))
sleep $2
W=$($AZ/winid 2>/dev/null | awk -v p="$MINE" '$2==p && $3>h {h=$3; w=$1} END {print w}')
[ -n "$W" ] && screencapture -x -o -l $W $3
for p in ${(f)MINE}; do kill -9 $p 2>/dev/null; done
defaults delete org.azahar-emu.azahar NSAppSleepDisabled 2>/dev/null
echo "pid $MINE  window $W"
grep -i -E "fatal|exception|abort|crash|data abort|unmapped|invalid" $LOG | grep -v mvk | tail -5
