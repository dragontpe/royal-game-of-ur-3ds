ROYAL GAME OF UR - 3DS ASSETS / BABYLON ROOM REVISION

CHANGED
Added a 0.60 x 0.40 m wooden table, enclosed glazed-brick room, high lion and
white-rosette frieze, floor, table supports and two low oil lamps. Back wall is
Z=-0.42 m. Table top is exactly Y=0, supporting the original board and pieces.
Backdrop artwork is an original simplified redraw based on inspected real
Pergamon reconstruction photographs, not a photographic texture pasted on a wall.
Warm illumination and edge falloff are stored in the room base-color PNGs.
At the close gameplay angle the view is intentionally mostly quiet tabletop and
plain blue brick. See babylon_room_overview.png for the high frieze and full room.

Die-tip marks are approximately twice the old diameter and bright cream/white.
Only the die region (PNG rows 178-255) was changed. All counter texels above it,
board/counter geometry and transforms, board texture, standalone board/counter
GLBs, board_layout.json and dice_scoring.json are unchanged. Dice mesh geometry
and transforms are unchanged too: the same two vertices remain marked.

PREVIOUS FILES
Files replaced in this revision, including the old die and complete-set GLBs,
pieces PNG/T3X files, source blend, reports and older previews, were MOVED into
old/2026-10-26_before_backdrop_dice/ before installing replacements.

DELIVERABLES
  glb/ur_backdrop.glb             Backdrop only, identity transforms, world placed
  glb/ur_complete_set.glb         Original set + room + gameplay camera
  glb/ur_die.glb                  Original die geometry with enlarged marks
  glb/ur_board.glb                Unchanged
  glb/ur_counter_shell.glb        Unchanged
  glb/ur_counter_dark.glb         Unchanged
  textures/*_basecolor.png        Opaque baked RGB PNGs
  textures/t3x/*_rgb565.t3x        Ready for Tex3DS_TextureImport
  textures/t3x/*_etc1.t3x          High-quality ETC1 alternatives
  source/ur_assets.blend          Updated original Blender scene, packed images
  camera.json                    Perspective camera and orbit contract
  board_layout.json              Original tile centers and motifs, unchanged
  dice_scoring.json              Original marked-vertex directions, unchanged
  asset_report.json              Geometry, texture sizes, bounds and orbit checks
  validation_report.json         Actual binary GLB and preservation checks

BUDGETS
  Board 284 triangles; each counter 124; each die 20, all unchanged.
  Whole backdrop: 428 triangles, 25 mesh nodes, 3 materials / 3 textures.
  Complete scene: 2,528 triangles per eye, 5,056 for two eye views, 44 mesh nodes.
  Board + pieces textures: 512x512 and 256x256 (original sizes).
  Added wall 512x512, furnishings 512x512, floor 256x256.
  Complete scene: five materials and five textures (two original + three added).
  Total GPU base-level texture storage: RGB565 1,835,008 bytes (1.75 MiB);
  ETC1 458,752 bytes (448 KiB). Excludes buffers, headers and other allocations.
  No mipmaps. RGB565 and ETC1 conversions and decoded previews included.
  Runtime frame time and stereo comfort have NOT been tested on physical 3DS.

RENDERER CONTRACT
Plain glTF 2.0 GLB; meters, +Y up, board length X and width Z.
Triangle indices; float POSITION, NORMAL and TEXCOORD_0 only.
Opaque materials, one base-color texture each. No required extensions, normal
maps, vertex colors, skinning, morphs or tangents. Backdrop uses inward-facing
single-sided walls. Lighting is baked: sample base color without another PBR
or strong runtime diffuse pass. No alpha-blended lamp effects or flame billboards.
Backdrop object transforms are identity: no additional placement transform needed.
The complete GLB contains node transforms for the original game pieces; apply them.
Apply your existing glTF-to-citro3d UV convention exactly once.

CAMERA
Position [0.10, 0.48, 0.32] m; look-at [0, 0.006, 0]; +Y up.
Near 0.01 m, far 2.5 m, aspect 400/240. Radius 0.58058245 m.
Default yaw 17.354 degrees, pitch 54.728 degrees.
Orbit yaw is +/-30 degrees RELATIVE TO DEFAULT YAW; pitch is 35..70 degrees.
Yaw convention: atan2(camera.x-target.x, camera.z-target.z).
For yaw a and pitch p, at the fixed radius r:
  position = target + [r*cos(p)*sin(a), r*sin(p), r*cos(p)*cos(a)].
The earlier gameplay preview was orthographic, so it had no vertical FOV.
This revision uses perspective with the same native-preview position, target and
distance. Fixed vertical FOV is 35.2243 degrees, widened from the target-plane
framing match to keep ALL original pieces visible at every tested orbit angle.
This framing adjustment changes camera projection only, not board or counters.
The GLB camera matches camera.json. Read exact values from camera.json.

PREVIEWS
  gameplay_400x240.png            Actual reimported complete GLB / exported camera
  babylon_gameplay_large.png      Same view at 1200x720
  orbit_yaw_-30_pitch_35.png      Four corners of the allowed orbit envelope
  orbit_yaw_+30_pitch_35.png
  orbit_yaw_-30_pitch_70.png
  orbit_yaw_+30_pitch_70.png
  dice_marks_400x240.png          Actual dice positions on table, closer camera
  babylon_room_overview.png       Wider view of the complete room and high frieze
  room_front.png / room_left.png / room_right.png   Geometry inspection views
  *_rgb565.png / *_etc1.png       Decoded texture-conversion previews

VALIDATION
Actual front, left, right, three-quarter and four orbit corner renders inspected.
20 sampled yaw/pitch combinations passed 117 background ray checks each, plus
rays to all board tiles and piece tops; no void and no backdrop occlusion found.
Board/counter vertices, normals, UVs, indices and node transforms compared exactly
to the prior complete GLB. Counter-region pixels and scoring file compared exactly.
Updated complete GLB reimported into Blender: 44 meshes and 2,528 triangles.
GLB camera FOV/aspect/clipping values checked against camera.json.
Embedded PNGs checked against supplied external base-color images.
Normals, indices, triangle budgets, texture bounds, opacity and extensions checked.

INSPECTED PHOTO REFERENCES
Processional Way reconstruction, Pergamon Museum:
https://commons.wikimedia.org/wiki/File:Processional_Way_(Babylon).jpg
Lion relief close-up:
https://commons.wikimedia.org/wiki/File:Pergamon_Museum_Berlin_2007113.jpg
Museum reconstruction context:
https://www.smb.museum/en/museums-institutions/vorderasiatisches-museum/exhibitions/detail/from-fragment-to-monument/

Original board / counter / dice references:
https://www.britishmuseum.org/collection/object/W_1928-1009-378
https://www.britishmuseum.org/collection/object/W_1928-1009-379-h
https://www.britishmuseum.org/collection/object/W_1928-1009-385
