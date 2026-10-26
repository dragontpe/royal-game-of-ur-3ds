ROYAL GAME OF UR 3DS: SECOND BOARD BRIEF (the later "Twenty Squares" board)

Same rules as the asset pack you already delivered ("Royal Game of Ur - 3DS Assets"): Old 3DS XL, citro3d, stereoscopic, plain glTF 2.0 GLB, meters, +Y up, board length along X, lighting and AO baked into the base colour only, opaque, no PBR/normal maps/vertex colours/extensions, RGB565 and ETC1 .t3x versions of every texture. Work in the same .blend. Do NOT change the existing Ur board, counters, dice, table or room. This is an extra board the game swaps in.

WHAT IT IS
The later form of the game, the "Game of Twenty Squares" (Egyptian "aseb"), played across Egypt, the Levant, Cyprus and Crete for well over a thousand years after Ur. Tutankhamun's game boxes carry it, and the Babylonian rules tablet of 177 BC describes this layout. Same 20 squares and 5 rosettes as the Ur board, but the bridge and the small 2x3 block are replaced by one long arm: the middle row runs on for 8 more squares.

LAYOUT (exact; the game's rules depend on it)
Rows 0, 1, 2 run along X; row 0 is the far side (-Z, the dark player), row 2 the near side (+Z, the light player), row 1 the middle (Z = 0). Columns 0 to 11 run along +X. R = rosette, # = square, blank = no square.

col:     0  1  2  3  4  5  6  7  8  9 10 11
row 0:   R  #  #  #
row 1:   #  #  #  R  #  #  #  R  #  #  #  R
row 2:   R  #  #  #

- 20 squares: a 3x4 block (columns 0-3) plus an 8-square arm continuing the middle row (columns 4-11).
- Exactly 5 rosettes: (0,0), (2,0), (1,3), (1,7), (1,11). The last one is the very end of the arm.
- Square centres in metres: x = -0.2069375 + col * 0.037625, z = (row - 1) * 0.0366667, y = 0.024 (top surface). That puts the whole board, 12 squares long (about 0.46 m with its border), centred on the table at X = 0.
- The empty table areas above and below the arm (rows 0 and 2, columns 4-11) are where waiting pieces will sit, so keep them clear.

BUILD
1. A box board like the Ur board: same square size (cell pitch 0.037625 m, so the existing counters fit), same board height (top surface at Y = 0.024, bottom at Y = 0), origin at the bottom centre of the board's bounding box, the long axis along X.
2. Make it look clearly EGYPTIAN, not like the Ur board. It should read as a different board from a different culture at first glance. Direction: Tutankhamun's twenty-square game box (Egyptian Museum, Cairo JE 62058; also Met Museum 16.10.475 and the British Museum Enkomi casket 1897,0401.996 for box proportions).
   - Box: polished ebony, near-black brown, with thin gold-leaf trim along the top edge and the corners. The sides carry a repeating band of lotus and papyrus in ivory, gold and Egyptian blue between gold borders. No hieroglyphs or anything that looks like writing.
   - Playing surface: each square a plain warm-ivory inlay (more yellow than the Ur board's shell), separated by thin ebony strips. Real Egyptian boards leave most squares plain, so keep the 15 non-rosette squares clean, at most a fine engraved border.
   - Rosettes: bold eight-petal rosettes in Egyptian blue faience (a bright turquoise blue, clearly different from the Ur board's deep lapis), with gold centres and a thin gold ring around each. They must be the loudest thing on the board.
   - Palette: ebony (about 42,29,20), ivory (about 232,220,190), gold leaf (about 201,160,67), Egyptian blue (about 31,111,168), a touch of carnelian red only in the side bands.
   - Keep the same counters and dice as the Ur board. Don't model new pieces.
3. Seen straight from above, the whole board must fit the 400 px top screen, so a square is only about 32 px. The rosettes and motifs must read at that size: bold shapes, no fine detail.
4. The board must sit on the existing table without hanging off it and must not intersect the lamps.

BUDGETS
- Up to 400 triangles (hidden bottom faces included).
- One material, one base-colour texture, power of two, up to 1024x512.
- Opaque only.

DELIVERABLES (into the same asset folder, nothing existing overwritten)
- glb/ur_board_twenty.glb (this board only, origin as above, identity transform).
- textures/ur_board_twenty_basecolor.png, plus textures/t3x/ur_board_twenty_rgb565.t3x and _etc1.t3x.
- board_layout_twenty.json in the same format as board_layout.json: every tile with id, row, column, motif, rosette true/false and center_m in glTF metres.
- previews/twenty_top.png, twenty_three_quarter.png, and twenty_gameplay_400x240.png rendered straight top-down with the whole board plus a margin filling the width.
- A validation report like before (triangles, bounds, one UV set, unit normals, texture sizes) and a short README note.
