ROYAL GAME OF UR - OLD 3DS STEREO ASSET PACK

Museum-inspired game assets built through the configured Blender MCP.
The inlay artwork is an original simplified redraw, not a museum photo texture.

FILES
  glb/ur_board.glb                 Board, origin at bottom center
  glb/ur_counter_shell.glb         One light counter, bottom-centered origin
  glb/ur_counter_dark.glb          One dark counter, bottom-centered origin
  glb/ur_die.glb                   One tetrahedral die, origin at base centroid
  glb/ur_complete_set.glb          Board + 7 light + 7 dark counters + 4 dice
  textures/*_basecolor.png         Standalone baked, opaque RGB textures
  textures/t3x/*_rgb565.t3x        Ready for tex3ds/citro3d import
  textures/t3x/*_etc1.t3x          Smaller compressed GPU-format alternatives
  source/ur_assets.blend           Final editable, packed Blender source
  board_layout.json                Tile centers/motifs in glTF meter coordinates
  dice_scoring.json                Original marked-vertex directions for scoring
  previews/                       Four views, details, native 400x240 and conversion previews
  asset_report.json               Budgets, units, layout and counts
  validation_report.json          Checks on the actual exported GLB binary contents

COUNTS (ALL INCLUDE HIDDEN BOTTOM FACES)
  Board:       284 triangles; 416 exported vertices
  Counter:     124 triangles; 100 exported vertices, each
  Die:          20 triangles;  52 exported vertices, each
  Complete:  2,100 triangles per eye; 4,200 across two eye views
  19 independently transformable mesh nodes, four mesh definitions, two materials.
  Shared mesh definitions save memory; all visible copies still need rendering.
  Actual performance has NOT been measured on a 3DS.

TEXTURES
  Board 512x512; pieces atlas 256x256 (both counter styles and all die faces).
  Base-color PNGs include local AO and soft orientation-based lighting.
  No PBR shading is needed: use the base-color image directly in the renderer.
  Normals are included for your vertex format and any optional renderer use.
  No vertex colors, extra UVs, tangents, skinning, morph targets or normal maps.
  Do not multiply these images by a second strong diffuse lighting pass.
  No moving-counter contact shadows are baked into the board.
  No mipmaps were generated. Compare filtering at the actual gameplay camera.
  RGB565 total base-level GPU texture storage: 655,360 bytes (640 KiB).
  ETC1 total base-level GPU texture storage:   163,840 bytes (160 KiB).
  These figures exclude .t3x headers and other runtime allocations.
  Both formats were converted and their decoded previews inspected.
  RGB565 is the recommended initial fidelity baseline; ETC1 is available to test.
  Full-sheet .t3x textures: no sprite atlas trimming, repacking, rotation or border insertion.
  Apply your renderer's glTF-to-citro3d texture-coordinate convention exactly once.
  Compare orientation against top.png: the large end is at negative X and the center
  rosette is in column 3, row 1. The source PNG and GLB embedded images are identical.

GLTF CONTRACT
  Plain glTF 2.0 GLB, float POSITION/NORMAL/TEXCOORD_0 and triangle indices.
  No required extensions and no mesh/texture compression extension.
  Two ordinary opaque base-color materials; metallic=0, roughness=1.
  PNG base-color images are embedded in GLBs as well as supplied separately.
  Units: meters. glTF is +Y up. Board length along X; width along Z.
  Board bounding box: X +/-0.1505, Y 0..0.024, Z +/-0.055.
  Counter nominal diameter 0.0206, height 0.0033.
  Die originally 0.017 base side / 0.013 height, slightly reduced by tiny edge bevels.
  Individual asset nodes are at the origin, with identity rotation and unit scale.
  For the complete-set GLB, apply all node transforms (including die rotations).
  Counters placed on the board have base Y=0.024 and meet the top surface.
  The scene is a display arrangement, not a prescribed starting game position.
  Four dice are included as a reusable modern gameplay layout choice; use as many
  as your selected rules require. This does not claim four dice belonged to BM 120834.

HISTORICAL SCOPE
  Main board: British Museum 120834 (registration 1928,1009.378), 2600-2400 BCE.
  Twenty-square 4x3 + 2-square bridge + 2x3 shape; five rosettes and panel categories
  matched to the museum top-down photograph. Shell, blue, red and dark joinery palette.
  Shell counter based on 1928,1009.379.h: circular with five dark-blue inlaid dots;
  the dark counterpart uses contrasting light dots. Seven copies of each.
  Tetrahedral dice have TWO marked vertices, represented consistently on adjacent faces.
  The original board was hollow; this closed exterior asset represents that box form.
  Interior joinery, movable drawer hardware and individual inlay geometry are omitted.
  Side decoration is simplified for the 3DS screen, not an archaeological reconstruction.
  Clean readable surfaces are intentional; cracks and excavation damage are not reproduced.

REFERENCES
  Board record:
  https://www.britishmuseum.org/collection/object/W_1928-1009-378
  Counter record:
  https://www.britishmuseum.org/collection/object/W_1928-1009-379-h
  Dice record:
  https://www.britishmuseum.org/collection/object/W_1928-1009-385
  Board top photograph (viewed for reference, not embedded in model textures):
  https://media.britishmuseum.org/media/Repository/Documents/2014_11/12_20/d3d4b951_abe4_4872_8f24_a3e1015448dd/mid_00129984_001.jpg
  Penn Museum excavation illustration:
  https://www.penn.museum/sites/expedition/files/2018/05/citylife_8.jpg
  Irving Finkel, On the Rules for the Royal Game of Ur:
  https://genjam.org/wp-content/uploads/2021/09/onrules4gameofur.pdf

QA COMPLETED
  Actual front, left, right, three-quarter and top renders inspected.
  Close-ups checked and top/underside bake overlap corrected.
  Exported complete GLB reimported into Blender; 19 mesh nodes / 2,100 triangles confirmed.
  exported_assets_preview.png renders that REIMPORTED GLB, not the construction scene.
  Binary GLBs checked for budgets, dimensions, one UV set, unit normals, finite data,
  valid indices, nondegenerate triangles, opaque materials and lack of unwanted extensions.
  RGB565/ETC1 conversions and their decoded atlas previews inspected.
  Remaining integration check: stereo frame time and texture orientation on real hardware.
