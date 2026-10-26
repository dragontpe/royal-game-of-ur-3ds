TWENTY SQUARES - EGYPTIAN SWAP BOARD

Added as a separate asset. No existing asset, texture, layout, preview, report or
source file is replaced. The original board, counters, dice, table and room remain
unchanged. There are no new counters or dice.

FILES
  glb/ur_board_twenty.glb
  textures/ur_board_twenty_basecolor.png
  textures/t3x/ur_board_twenty_rgb565.t3x
  textures/t3x/ur_board_twenty_etc1.t3x
  board_layout_twenty.json
  asset_report_twenty.json
  validation_report_twenty.json
  source/ur_assets_twenty.blend
  previews/twenty_top.png
  previews/twenty_three_quarter.png
  previews/twenty_gameplay_400x240.png
  previews/twenty_front.png / twenty_left.png / twenty_right.png
  previews/twenty_rgb565.png / twenty_etc1.png (decoded conversion previews)
  SHA256_twenty.json

SOURCE
ur_assets_twenty.blend is an additive revision of the existing room .blend.
All 45 pre-existing objects retain their geometry, transforms and visibility.
The added board is in collection SWAP_TwentySquares, hidden in the default room
view to avoid overlapping the original Ur board. To inspect the swap in Blender,
hide UR_Board and enable the new board's viewport eye and render camera icon.
The original source/ur_assets.blend is not overwritten.

RUNTIME CONTRACT
Plain glTF 2.0 GLB, meters, +Y up, length along X, one mesh/material/base-color image.
Identity node transform, origin at bottom center of the bounding box. Position at
[0,0,0] when swapping it for UR_Board. Do not load both boards on top of one another.
Use the existing counter and die assets and arrange them using the new layout.
POSITION / NORMAL / TEXCOORD_0 only. Opaque, single-sided, outward normals.
No extensions, vertex colors, tangents, skinning, morphs, PBR maps or normal maps.
The standard glTF material carries only the baked base-color image; sample that
image directly in the custom renderer without an additional PBR lighting pass.

GEOMETRY AND LAYOUT
92 triangles including hidden bottom faces, 192 exported vertices. Ceiling: 400.
Bounding box: X [-0.22875,0.22875], Y [0,0.024], Z [-0.05800005,0.05800005] m.
Overall size 0.4575 x 0.024 x 0.1160001 m. Borders are included in those dimensions.
Exactly 20 tiles: three rows of four at columns 0-3, then columns 4-11 in row 1 only.
Exactly five eight-petal rosettes: (0,0), (2,0), (1,3), (1,7), (1,11).
Other fifteen tiles are plain ivory with a quiet fine border.
JSON is the same schema as board_layout.json. Tile centers follow the brief exactly:
  x = -0.2069375 + col * 0.037625
  y = 0.024
  z = (row - 1) * 0.0366667
Row 0 is the far -Z side; row 2 is the near +Z side. Columns advance along +X.
Both waiting bays alongside the arm have open table space. Counter centers at
rows 0/2, columns 4-11 have at least 5.03 mm clearance between the edge of an
existing 20.6 mm counter and the board. No shelf or slab fills these bays.
Table edge clearance is 71.25 mm along X and about 142 mm along Z; no lamp overlap.

ART AND TEXTURE
Ebony boxwork, gold top/corner trim, warm ivory inlays, turquoise/Egyptian-blue
rosettes with gold centers/rings, lotus/papyrus side band and small carnelian accents.
No hieroglyphs or writing. This is a stylized game asset following the requested
T footprint and markers, rather than a literal replica of one museum artifact.
Original atlas artwork; actual local AO and soft orientation shading baked in
Blender using the same .72+.28*normal.Z lighting balance as the existing Ur board.
One opaque RGB 1024x512 image. RGB565 storage: 1,048,576 bytes (1 MiB).
ETC1 storage: 262,144 bytes (256 KiB). Sizes exclude headers and mipmaps.
tex3ds conversions use no trimming, repacking, rotation, borders or mipmaps.
Apply the renderer's existing glTF-to-citro3d UV convention exactly once.

PREVIEWS AND CHECKS
The native preview is a true straight top-down view, +X to the right and -Z up,
with the board plus margin filling the width: about 31 pixels per square at 400x240.
Top and native previews were rendered from the exported, reimported GLB.
Front, left, right, three-quarter and top views were visually inspected.
Layout JSON centers and all 5 rosette positions checked exactly. Every center
was checked against actual exported UVs and baked pixel color (gold/ivory).
All tile centers meet Y=.024; all 16 waiting positions ray-tested clear.
GLB budgets, bounds, opacity, one UV set, unit normals, outward winding,
nondegenerate triangles, identity transform and embedded texture checked.
RGB565 and ETC1 decoded atlas previews inspected. Hardware performance unmeasured.

INSPECTED REFERENCES
Tutankhamun game-box photograph, Egyptian Museum material direction:
https://www.ancient-egypt.co.uk/cairo%20museum/cm,%20tutankhamun,%20furniture/images/games%201.jpg
Tutankhamun JE 62058 / Carter 345 record:
https://more.bham.ac.uk/birminghamegyptology/wp-content/uploads/sites/54/2013/10/345.pdf
Met Museum 16.10.475a (box and inlaid playing-field construction):
https://www.metmuseum.org/art/collection/search/553268
British Museum Enkomi casket 1897,0401.996 (box proportions):
https://artsandculture.google.com/asset/ivory-game-box/VwES62VZj2q-QQ
