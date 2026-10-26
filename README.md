# The Royal Game of Ur (3DS)

Homebrew for the Nintendo 3DS by No Banners Software. Finkel rules, three
computer opponents (Beginner, Medium, Expert) and a two-player mode.

Two boards: the Ur board (Finkel rules, 7 pieces a side) and the later
Egyptian "Twenty Squares" board (RoyalUr's aseb preset: 16-step track,
rosettes every fourth step, 5 pieces a side, as on the 177 BC tablet).
`tools/xcheck/run.sh` checks both against RoyalUr's Python library.

- Top screen: Frank's Blender board in stereo 3D. Straight top-down during
  play; the lamp-lit Babylon room in the menus.
- Bottom screen: menus and a touch map of the board.
- Boot splash: painted key art (`design/keyart/keyart_v1.png`, GPT Image 2)
  with "© 2026 No Banners Software" at the bottom.

## Controls

- Stylus: tap Roll, then tap a ringed piece or its outlined square. Tap your
  name row to bring in a new piece, the Home box to bear off.
- Buttons: A rolls, the D-pad picks a move, A plays it. START pauses.
- Debug: SELECT saves both screens to `sdmc:/3ds/ur-3ds/shots/`,
  SELECT+R shows frame times.

Saves live in `sdmc:/3ds/ur-3ds/` (settings, records, the game in progress,
saved at the start of every turn).

## Build

devkitPro at `/opt/devkitpro`:

    export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM
    export PATH=$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH
    tools/build_assets.sh       # scene, textures, 2D art, fonts, sounds (needs Python 3 + Pillow + numpy)
    make                        # ur-3ds.3dsx
    tools/make_cia.sh           # ur-3ds.cia (makerom/bannertool from ~/gammon-x/vendor/tools)

`tools/import_assets.sh [pack]` re-imports the Blender asset pack (default
`~/Desktop/Royal Game of Ur - 3DS Assets`). Static meshes are merged per
texture, so the room costs four draw calls.

## Layout

| Path | What |
|---|---|
| `game/` | Rules (`ur_rules`) and AI (`ur_ai`), portable C99, no heap. See `game/README.md`. |
| `host/` | Mac tests: `make -C host test` (unit + fuzz + AI), `make -C host ladder` |
| `tools/xcheck/` | Cross-check against RoyalUr's MIT Python library (`tools/xcheck/run.sh N SEED`) |
| `source/` | 3DS front end: `main.c` (screens, cameras), `play.c` (turn flow, bottom UI), `world.c` (3D pieces, dice, decals), `scene.c` (citro3d), `draw.c` (2D + text), `sfx.c`, `save.c`, `pages.c` (rules and history) |
| `tools/art/` | 2D art and font bakers (theme in `common.py`), comps, splash key art |
| `tools/audio/make_sfx.py` | Synthesised sounds and the lyre music loop |
| `tools/az/azrun.sh` | Silent Azahar run in an isolated HOME, kills only its own PID |
| `design/` | Comps, key art, robot screenshots |

## Testing without the console

- `make ROBOT=1` builds a robot that plays through the real UI (taps and
  buttons), tours every page, tests pause, quit and Continue, and plays one
  game per AI level plus a two-player game at 4x speed. It logs to
  `sdmc:/3ds/ur-3ds/robot.txt` and saves screenshots. Run it with
  `tools/az/azrun.sh`, then `python3 tools/shots.py`.
- Azahar speed says nothing about the Old 3DS. Real frame times and AI think
  times still need a run on the hardware.
