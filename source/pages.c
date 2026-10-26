/* How to play and The story: short pages with small illustrations.
 * Facts come from ~/Documents/research-reports/2026-10-26-royal-game-of-ur.md. */
#include "pages.h"

#include "draw.h"

typedef enum { ILL_NONE, ILL_PATH, ILL_DICE, ILL_MOVE, ILL_SHARED, ILL_ROSETTE, ILL_CONTROLS, ILL_FORTUNE, ILL_TWENTY } Illus;

typedef struct {
	const char *title, *body;
	Illus ill;
} Page;

static const Page rules[] = {
	{ "The race", "Each player has seven pieces. Race all of them along your track and off the board before the other player does.", ILL_PATH },
	{ "The dice", "Throw four pyramid dice. Each one has two marked tips. Count the marked tips pointing up: that is your move, from 0 to 4. A 0 means no move. 2 comes up most often.", ILL_DICE },
	{ "Moving", "Move one piece exactly that many squares, either a new piece or one already on the board. You can't land on your own piece, and leaving the board takes the exact number.", ILL_MOVE },
	{ "Captures", "The middle row is shared. Land on an enemy piece there and it goes back to the start. Your first four squares and your last two are safe.", ILL_SHARED },
	{ "Rosettes", "Land on a rosette and you throw again. The rosette in the middle is safe: a piece there can't be captured, and the other player can't land on it.", ILL_ROSETTE },
	{ "Twenty Squares", "Same five rosettes, but a 16-square track: four of your own, then the whole long arm, which both players share. Every fourth square is a rosette; the three on the arm are safe.", ILL_TWENTY },
	{ "Controls", "Stylus: tap Roll, then tap a ringed piece or its outlined square.\nButtons: A rolls, the D-pad picks a move, A plays it.\nSTART pauses. The game saves every turn.", ILL_CONTROLS },
};

static const Page story[] = {
	{ "Ur, 2600 BC", "Between 1922 and 1934 Leonard Woolley dug five boards like this one out of the Royal Cemetery at Ur, in today's Iraq. They are about 4,500 years old. The board in this game copies the one in the British Museum.", ILL_ROSETTE },
	{ "Across the ancient world", "The game travelled from Mesopotamia to the Levant, Egypt, Cyprus and Crete. Tutankhamun was buried with four boards. Someone even scratched one into a stone gateway figure at the Assyrian palace of Khorsabad.", ILL_NONE },
	{ "The tablet", "In Babylon, on 3 November 177 BC, a scribe named Itti-Marduk-balatu copied the rules onto a clay tablet. It is the only rule book that survives. Today it is in the British Museum.", ILL_NONE },
	{ "Pack of Dogs", "The tablet's version used five named pieces: Swallow, Storm-bird, Raven, Rooster and Eagle. Landing on a rosette promised good fortune: food, fine beer, or love in the tavern. Another copy calls the game Pack of Dogs.", ILL_NONE },
	{ "Fortunes", "The tablet also lists twelve fortunes beside the signs of the zodiac. In this game you hear them when you land on a rosette.", ILL_FORTUNE },
	{ "Twenty Squares", "Later the board changed shape: the bridge and the small block became one long arm off the middle row. Boards like this turn up in Egypt, the Levant, Cyprus and Crete, and Tutankhamun was buried with game boxes that carry it. This game's version is built in that Egyptian style.", ILL_NONE },
	{ "Played again", "Irving Finkel of the British Museum read the tablet and rebuilt the rules most people play today, the ones used here. The game never quite died: Jewish families in Kochi, India, played a version of it into the 1950s.", ILL_NONE },
};

static const Page *book;
static int npages, cur;

void pages_open(int which)
{
	book = which == PAGES_RULES ? rules : story;
	npages = which == PAGES_RULES ? (int)(sizeof(rules) / sizeof(rules[0])) : (int)(sizeof(story) / sizeof(story[0]));
	cur = 0;
}

int pages_update(u32 down, bool touch, int tx, int ty)
{
	int turn = 0;
	if (down & (KEY_DRIGHT | KEY_R | KEY_A))
		turn = 1;
	if (down & (KEY_DLEFT | KEY_L))
		turn = -1;
	if (down & KEY_B)
		return 1;
	if (touch) {
		if (in_rect(tx, ty, 248, 196, 56, 36))
			turn = 1;
		else if (in_rect(tx, ty, 188, 196, 56, 36))
			turn = -1;
		else if (in_rect(tx, ty, 16, 196, 80, 36))
			return 1;
	}
	if (turn > 0 && cur == npages - 1 && (down & KEY_A))
		return 1;
	if (turn) {
		int n = cur + turn;
		if (n >= 0 && n < npages)
			cur = n;
	}
	return 0;
}

/* Small board for illustrations: 24 px squares, centred under the text. */
#define MX 61
#define MY 116
#define SC 24
static float cx(int col) { return MX + 3 + col * SC + SC / 2.0f; }
static float cy(int row) { return MY + 3 + row * SC + SC / 2.0f; }

static void piece_at(int player, int row, int col)
{
	spr(player == 0 ? art_pc_l_xs_idx : art_pc_d_xs_idx, cx(col) - 10, cy(row) - 10);
}

static void target_at(int row, int col)
{
	spr(art_tgt_xs_idx, MX + 3 + col * SC + 1.5f, MY + 3 + row * SC + 1.5f);
}

static void illustrate(Illus ill)
{
	switch (ill) {
	case ILL_PATH:
		spr(art_map_s_idx, MX, MY);
		spr(art_path_l_s_idx, MX, MY);
		break;
	case ILL_DICE: {
		static const bool m[4] = { true, false, true, false };
		for (int i = 0; i < 4; i++)
			spr(m[i] ? art_die_m_l_idx : art_die_u_l_idx, 76 + i * 38, 140);
		panel(P_BOX3, 238, 136, 34, 38);
		text(TS_BIG, 255, 155, A_CENTER, "2");
		break;
	}
	case ILL_MOVE:
		/* a 3: the piece on its second square goes to the first shared one */
		spr(art_map_s_idx, MX, MY);
		piece_at(0, 2, 2);
		spr(art_ring_xs_idx, cx(2) - 12, cy(2) - 12);
		target_at(1, 0);
		panel(P_BOX3, 270, 136, 30, 34);
		text(TS_BIG, 285, 153, A_CENTER, "3");
		break;
	case ILL_SHARED:
		spr(art_map_s_idx, MX, MY);
		for (int c = 0; c < 8; c++)
			if (c != 3)
				target_at(1, c);
		piece_at(1, 1, 5);
		piece_at(0, 1, 2);
		break;
	case ILL_ROSETTE:
		spr(art_rosette_big_idx, 160 - 24, 132);
		break;
	case ILL_TWENTY:
		spr(art_map_t_s_idx, 13, MY);
		spr(art_path_t_s_idx, 13, MY);
		break;
	case ILL_FORTUNE:
		text(TS_QUOTE, 160, 128, A_CENTER, "\"You will find a friend.\"");
		text(TS_QUOTE, 160, 148, A_CENTER, "\"You will be powerful like a lion.\"");
		text(TS_QUOTE, 160, 168, A_CENTER, "\"You will draw fine beer.\"");
		break;
	default:
		break;
	}
}

void pages_draw(void)
{
	const Page *p = &book[cur];
	draw_rect(0, 0, 320, 240, COL_GROUND);
	text(TS_TITLE, 160, 20, A_CENTER, "%s", p->title);
	text_wrap(TS_TEXT, 20, 46, 280, 16, p->body);
	illustrate(p->ill);

	panel_btn(P_TILE3, 16, 200, 80, 30);
	text(TS_TILE, 56, 215, A_CENTER, "Back");
	text(TS_BODY, 160, 215, A_CENTER, "%d / %d", cur + 1, npages);
	if (cur > 0) {
		panel_btn(P_TILE3, 192, 200, 52, 30);
		spr(art_chev_l_idx, 212, 207);
	}
	if (cur < npages - 1) {
		panel_btn(P_TILE3, 252, 200, 52, 30);
		spr(art_chev_r_idx, 272, 207);
	}
}
