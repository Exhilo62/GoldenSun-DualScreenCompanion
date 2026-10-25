#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gs_ram.h"
#include "gs_state.h"

// Reads a UTF-16LE string of printable ASCII from memory; returns its length, or 0 if it holds anything else.
static int readU16(GsMemRead rd, void* ctx, uint32_t addr, char* out, int max) {
	int n = 0;
	for (; n < max - 1; ++n) {
		uint32_t c = rd(ctx, addr + 2 * (uint32_t) n, 2);
		if (c == 0) break;
		if (c < 32 || c > 126) { out[0] = 0; return 0; }
		out[n] = (char) c;
	}
	out[n] = 0;
	return n;
}

const GsRam gsRamGs1 = {GS_AREA_A, GS_CAM_X, GS_CAM_Y, GS_PLAYER_X, GS_PLAYER_Y, GS_COINS, GS_CHAR_BASE, GS_ENEMY_BASE, GS_MENU_FLAGS,
	GS_PARTY_MASK, GS_PARTY_ORDER, GS_BATTLE_SEL, GS_BATTLE_ACT, GS_BATTLE_DEPTH, GS_BATTLE_TOP, GS_BATTLE_CMD, GS_BATTLE_DESC, GS_BATTLE_TIDX, GS_BATTLE_CMDS, true, 7, 0x0807B6A8u, 0x02038D04u, 0x0807A830u, 0x0203396Cu, true, 0, 0, 0};
// The Lost Age (AGFE), verified from 5 saves + a new game; battle menu: depth 0x03007DD3 (3 hero bar, 0 list, 2 round menu/target), top 0x03007DB8 (1 on the hero bar),
// description 0x03007D34, target name 0x03007CF4, ally target index 0x03007CC0 (party order; depth 2 with the flag at 255), highlighted command text 0x02035708; see docs/LOST_AGE.md. Camera scroll is at 0x0300112C/2E (leads the picture by one frame; camLag); 0x020322F6/FE is a smoother 1 px/frame counter that does NOT match the picture. The RAM player
// coordinates (0x03001124/26, 0x02030ECA/CE) are not used, so playerX/playerY are 0 here and the reader derives the world position from camera + leader sprite.
const GsRam gsRamTla = {0x02000420u, 0x0300112Cu, 0x0300112Eu, 0, 0, 0x02000250u, 0x02000520u, 0x020308C8u, 0x02000060u,
	0, 0x0200001Cu, 0x03007E48u, 0, 0x03007DD3u, 0x03007DB8u, 0, 0x03007D34u, 0x03007CC0u, 0, true, 18, 0x080B2364u, 0x02039238u, 0x080B12C8u, 0x02031E40u, false, 0x02035708u, 0x03007CF4u, 1, 0x02000458u};
static const GsRam* ram = &gsRamGs1;
void gsSetRam(const GsRam* r) { ram = r; }
const GsRam* gsRam(void) { return ram; }

static const char* activeName = "Golden Sun";
static bool activeMapped = true;

void gsSetGame(const char* name, bool mapped) { activeName = name; activeMapped = mapped; }
const char* gsGameName(void) { return activeName; }
bool gsGameMapped(void) { return activeMapped; }

void gsReadState(GsState* out, GsMemRead rd, void* ctx) {
	memset(out, 0, sizeof(*out));
	out->mode = GS_MODE_FIELD;
	if (!activeMapped) return;   // unverified RAM map: show no game info rather than wrong numbers
	out->mode = (rd(ctx, GS_REG_DISPCNT, 2) & 7) == 1 ? GS_MODE_BATTLE : GS_MODE_FIELD;
	out->menuOpen = out->mode == GS_MODE_FIELD && (rd(ctx, ram->menuFlags, 1) & GS_MENU_BIT) != 0;
	out->areaGroup = (int) rd(ctx, ram->areaA, 2);   // u16 map id
	out->areaKey = (uint32_t) out->areaGroup;
	if (ram->hasMap) {
		out->camX = (int) rd(ctx, ram->camX, 2);
		out->camY = (int) rd(ctx, ram->camY, 2);
		if (ram->playerX) {
			out->playerX = (int) rd(ctx, ram->playerX, 2);
			out->playerY = (int) rd(ctx, ram->playerY, 2);
		}
	}
	// Leader sprite. Player RAM position, camera and screen position are related by a per-map constant
	// (screen = player + K - cam), so collect the 32x32 sprites and learn K by voting on frames where the
	// player moved (followers lag behind and vote inconsistently).
	typedef struct { int kx, ky, n; } Vote;
	static Vote votes[8];
	static uint32_t calMap = 0xFFFFFFFFu;
	static int lastPX, lastPY;
	if (out->areaKey != calMap || abs(out->playerX - lastPX) > 64 || abs(out->playerY - lastPY) > 64) {
		memset(votes, 0, sizeof(votes));
		calMap = out->areaKey;
	}
	bool moved = out->playerX != lastPX || out->playerY != lastPY;
	lastPX = out->playerX; lastPY = out->playerY;
	int handTile = 0x7FFF;
	int cand[16][2], nc = 0;
	for (int i = 0; i < 128 && nc < 16; ++i) {
		uint32_t a0 = rd(ctx, GS_OAM + 8 * i, 2), a1 = rd(ctx, GS_OAM + 8 * i + 2, 2);
		if (((a0 >> 8) & 3) == 2) continue;           // disabled
		{
			static const unsigned char dim[3][4][2] = {
				{{8, 8}, {16, 16}, {32, 32}, {64, 64}}, {{16, 8}, {32, 8}, {32, 16}, {64, 32}}, {{8, 16}, {8, 32}, {16, 32}, {32, 64}}};
			unsigned shape = (a0 >> 14) & 3, size = (a1 >> 14) & 3;
			if (shape == 0 && size == 1) {   // 16x16: the menu pointing hand uses tile 0x104 (lists) or 0x10C (Djinn page)
				unsigned tile = rd(ctx, GS_OAM + 8 * i + 4, 2) & 0x3FF;
				if (tile >= 0x104 && tile <= 0x10C && (!out->handFound || (unsigned) handTile > tile)) {
					int hx = a1 & 0x1FF, hy = a0 & 0xFF;
					if (hx > 255) hx -= 512;
					if (hy > 159) hy -= 256;
					out->handFound = true; out->handX = hx; out->handY = hy; handTile = (int) tile;
				}
			}
			if (shape < 3 && out->spriteCount < 40) {
				int sx = a1 & 0x1FF, sy = a0 & 0xFF;
				if (sx > 255) sx -= 512;
				if (sy > 159) sy -= 256;
				int sw = dim[shape][size][0], sh = dim[shape][size][1];
				if (((a0 >> 8) & 3) == 3) { sx -= sw / 2; sy -= sh / 2; sw *= 2; sh *= 2; }  // double-size affine
				out->sprites[out->spriteCount].x = (short) sx; out->sprites[out->spriteCount].y = (short) sy;
				out->sprites[out->spriteCount].w = (short) sw; out->sprites[out->spriteCount].h = (short) sh;
				++out->spriteCount;
			}
		}
		if (((a0 >> 14) & 3) != 0 || ((a1 >> 14) & 3) != 2) continue;  // square, 32x32 only
		int y = a0 & 0xFF, x = a1 & 0x1FF;
		if (x > 255) x -= 512;
		if (x < -16 || x > 240 || y > 160) continue;
		cand[nc][0] = x; cand[nc][1] = y; ++nc;
	}
	int bestVote = -1;
	for (int c = 0; moved && c < nc; ++c) {
		int kx = cand[c][0] + 16 - (out->playerX - out->camX), ky = cand[c][1] + 26 - (out->playerY - out->camY);
		int slot = -1, free = -1;
		for (int v = 0; v < 8; ++v) {
			if (votes[v].n && abs(votes[v].kx - kx) <= 1 && abs(votes[v].ky - ky) <= 1) { slot = v; break; }
			if (!votes[v].n && free < 0) free = v;
		}
		if (slot < 0 && free >= 0) { slot = free; votes[slot].kx = kx; votes[slot].ky = ky; }
		if (slot >= 0) ++votes[slot].n;
	}
	for (int v = 0; v < 8; ++v) if (votes[v].n >= 3 && (bestVote < 0 || votes[v].n > votes[bestVote].n)) bestVote = v;
	int ex, ey;  // expected screen position of the leader's sprite top-left
	if (bestVote >= 0) {
		ex = out->playerX + votes[bestVote].kx - out->camX - 16; ey = out->playerY + votes[bestVote].ky - out->camY - 26;
	} else if (out->areaGroup == 3 || out->areaGroup == 7 || out->areaGroup == 8) {
		ex = out->playerX + 963 - out->camX - 16; ey = out->playerY - 192 - out->camY - 28;  // Vale default
	} else {
		ex = 104; ey = 58;  // not calibrated yet: assume the camera is centred on the player
	}
	int best = 1 << 30;
	for (int c = 0; c < nc; ++c) {
		int dx = cand[c][0] - ex, dy = cand[c][1] - ey, d = dx * dx + dy * dy;
		if (d < best) { best = d; out->sprX = cand[c][0]; out->sprY = cand[c][1]; }
	}
	out->hasSprite = best < 40 * 40;
	if (ram->hasMap && !ram->playerX) {   // The Lost Age: world position = camera + the leader sprite's feet; 0,0 while the camera is parked (transitions)
		bool live = out->camX || out->camY;
		out->playerX = live ? out->camX + (out->hasSprite ? out->sprX + 16 : 120) : 0;
		out->playerY = live ? out->camY + (out->hasSprite ? out->sprY + 26 : 80) : 0;
	}
	int size = 0;
	int pre[GS_PARTY_MAX], pn = -1;   // games without a party mask: the leading distinct entries of the order array are the party
	if (ram->partyMask) {
		for (uint32_t mask = rd(ctx, ram->partyMask, 1); mask; mask >>= 1) size += mask & 1;
	} else {
		bool dup = false;
		pn = 0;
		for (int i = 0; i < GS_PARTY_MAX; ++i) {
			uint32_t r = rd(ctx, ram->partyOrder + (uint32_t) i, 1);
			if (r >= GS_CHAR_COUNT) break;
			for (int k = 0; k < pn; ++k) if (pre[k] == (int) r) dup = true;
			if (dup) break;
			pre[pn++] = (int) r;
		}
		// The Lost Age's opening (before the first real party exists) leaves the save-data array empty (zeros): the game then keeps its active party in a
		// second copy, whose first entry is the current leader (Jenna alone at the Venus Lighthouse).
		if (dup && ram->partyOrderAlt) {
			pn = 0;
			for (int i = 0; i < GS_PARTY_MAX; ++i) {
				uint32_t r = rd(ctx, ram->partyOrderAlt + (uint32_t) i, 1);
				bool again = r >= GS_CHAR_COUNT;
				for (int k = 0; k < pn; ++k) if (pre[k] == (int) r) again = true;
				if (again) break;
				pre[pn++] = (int) r;
			}
		}
		size = pn;
	}
	if (size < 1) size = 1;
	if (size > GS_PARTY_MAX) size = GS_PARTY_MAX;
	out->partySize = size;
	out->coins = rd(ctx, ram->coins, 4);
	// Party order array: record indices, 0xFF-terminated. Fall back to join order if it looks invalid.
	int order[GS_PARTY_MAX], slotOfRec[GS_CHAR_COUNT];
	int n = 0;
	if (pn >= 0) {
		for (; n < pn; ++n) order[n] = pre[n];
	} else {
		for (int i = 0; i < GS_PARTY_MAX; ++i) {
			uint32_t r = rd(ctx, ram->partyOrder + (uint32_t) i, 1);
			if (r >= GS_CHAR_COUNT) break;
			order[n++] = (int) r;
		}
	}
	if (n > size) n = size;   // entries after the last member can be stale (a replaced member leaves its old index behind): the mask gives the true size
	if (n == size && ram->partyMask) {   // a fresh join can leave the list unfilled (all zeros): it must name distinct members that the mask contains
		uint32_t mask = rd(ctx, ram->partyMask, 1), seen = 0;
		for (int i = 0; i < n; ++i) {
			if (!(mask >> order[i] & 1) || (seen >> order[i] & 1)) { n = -1; break; }
			seen |= 1u << order[i];
		}
	}
	if (n != size) {
		n = size;
		uint32_t mask = ram->partyMask ? rd(ctx, ram->partyMask, 1) : 0;
		int k = 0;
		for (int r = 0; r < GS_CHAR_COUNT && k < GS_PARTY_MAX; ++r) if (mask >> r & 1) order[k++] = r;   // members in join order
		if (!mask || k != size) for (int i = 0; i < GS_PARTY_MAX; ++i) order[i] = i;
	}
	for (int r = 0; r < GS_CHAR_COUNT; ++r) slotOfRec[r] = -1;
	for (int i = 0; i < n; ++i) slotOfRec[order[i]] = i;
	for (int i = n; i < GS_PARTY_MAX; ++i) order[i] = i;
	for (int i = 0; i < GS_PARTY_MAX; ++i) {
		uint32_t base = ram->charBase + (uint32_t) order[i] * GS_CHAR_STRIDE;
		GsMember* m = &out->party[i];
		m->present = i < size;
		m->rec = (uint8_t) order[i];
		for (int k = 0; k < 15; ++k) {
			char c = (char) rd(ctx, base + GS_OFF_NAME + k, 1);
			m->name[k] = (c >= 32 && c < 127) ? c : 0;
		}
		m->level = (uint8_t) rd(ctx, base + GS_OFF_LEVEL, 1);
		m->maxHp = (uint16_t) rd(ctx, base + GS_OFF_MAXHP, 2);
		m->maxPp = (uint16_t) rd(ctx, base + GS_OFF_MAXPP, 2);
		m->hp = (uint16_t) rd(ctx, base + GS_OFF_HP, 2);
		m->pp = (uint16_t) rd(ctx, base + GS_OFF_PP, 2);
		for (int k = 0; k < GS_ITEM_SLOTS; ++k) {
			uint32_t raw = rd(ctx, base + GS_OFF_ITEMS + 2 * k, 2);
			m->items[k].id = raw & 0x1FF;
			m->items[k].equipped = (raw & 0x200) != 0;
			m->items[k].qty = (uint8_t) ((raw >> 11) + 1);
		}
		m->abilityCount = 0;
		for (int k = 0; k < GS_ABILITY_MAX; ++k) {
			uint16_t a = (uint16_t) rd(ctx, base + GS_OFF_ABILITIES + 4 * k, 2);
			if (!a) break;
			m->abilities[m->abilityCount++] = a;
		}
		m->exp = rd(ctx, base + GS_OFF_EXP, 4);
		m->cursed = rd(ctx, base + GS_OFF_CURSE, 1) != 0;
		m->atk = (uint16_t) rd(ctx, base + GS_OFF_ATK, 2);
		m->def = (uint16_t) rd(ctx, base + GS_OFF_DEF, 2);
		m->agi = (uint16_t) rd(ctx, base + GS_OFF_AGI, 2);
		m->classId = (uint8_t) rd(ctx, base + GS_OFF_CLASS, 1);
		static const int pairOf[4] = {0, 2, 3, 1};   // our Venus, Mars, Jupiter, Mercury -> record pair (Venus, Mercury, Mars, Jupiter)
		for (int e = 0; e < 4; ++e) {
			m->elemPow[e] = (uint16_t) rd(ctx, base + GS_OFF_ELEM + 4u * (uint32_t) pairOf[e], 2);
			m->elemRes[e] = (uint16_t) rd(ctx, base + GS_OFF_ELEM + 4u * (uint32_t) pairOf[e] + 2, 2);
		}
		m->expLevelStart = m->expNext = 0;
		if (ram->expTable && order[i] < GS_CHAR_COUNT && m->level >= 1 && m->level < 99) {
			uint32_t tbl = ram->expTable + (uint32_t) order[i] * GS_EXP_STRIDE;
			uint32_t start = rd(ctx, tbl + 4u * (uint32_t) (m->level - 1), 4), need = rd(ctx, tbl + 4u * (uint32_t) m->level, 4);
			if (need > start && need < 20000000u) { m->expLevelStart = start; m->expNext = need > m->exp ? need - m->exp : 0; }
		}
	}
	static const int djinnOff[GS_ELEMENTS] = {GS_OFF_DJINN_VENUS, GS_OFF_DJINN_MARS, GS_OFF_DJINN_JUPITER, GS_OFF_DJINN_MERCURY};
	for (int e = 0; e < GS_ELEMENTS; ++e) {
		out->djinnOwned[e] = out->djinnSet[e] = 0;
		for (int i = 0; i < GS_CHAR_COUNT; ++i) {
			uint32_t base = ram->charBase + (uint32_t) i * GS_CHAR_STRIDE + (uint32_t) djinnOff[e];
			uint32_t djMask = ram->djinnCount >= 32 ? 0xFFFFFFFFu : (1u << ram->djinnCount) - 1;
			uint32_t own = rd(ctx, base, 4) & djMask, set = rd(ctx, base + GS_OFF_DJINN_SET_DELTA, 4) & djMask;
			out->djinnOwned[e] |= own;
			out->djinnSet[e] |= set;
			int slot = slotOfRec[i];
			if (slot >= 0) { out->party[slot].djinnOwned[e] = own; out->party[slot].djinnSet[e] = set & own; }
		}
		out->djinnSet[e] &= out->djinnOwned[e];
	}
	out->enemyCount = 0;
	out->battleSel = out->battleAct = -1;
	out->battleCmd = out->battleTarget = -1;
	out->battleTop = false;
	out->battleDepth = -1;
	out->battleHidden = 0;
	if (out->mode == GS_MODE_BATTLE && ram->battleCmdCount) {
		// The game's bar drops commands that do not apply: Summon when none is available, Djinn when no Djinn is owned (a new game's first fights),
		// and Psynergy when the hero has none. The bar holds only what is left, in the same order.
		unsigned n = rd(ctx, ram->battleCmdCount, 1);
		bool anyDjinn = false;
		for (int e = 0; e < GS_ELEMENTS; ++e) anyDjinn |= out->djinnOwned[e] != 0;
		if (n == 5) out->battleHidden = 1 << 3;
		else if (n == 4) out->battleHidden = (uint8_t) ((1 << 3) | (anyDjinn ? 1 << 1 : 1 << 2));
		else if (n == 3) out->battleHidden = (1 << 1) | (1 << 2) | (1 << 3);
	}
	out->battleNoSummon = (out->battleHidden >> 3) & 1;
	out->battlePhase = GS_PH_NONE;
	out->battleText[0] = 0;
	if (out->mode != GS_MODE_BATTLE) return;
	{
		uint32_t sel = rd(ctx, ram->battleSel, 1), act = ram->battleAct ? rd(ctx, ram->battleAct, 1) : 0xFF;
		// Both are character record indices, not party slots (verified with a reordered party).
		out->battleSel = sel < GS_CHAR_COUNT ? slotOfRec[sel] : -1;
		out->battleAct = act < GS_CHAR_COUNT ? slotOfRec[act] : -1;
	}
	for (int i = 0; i < GS_ENEMY_MAX; ++i) {
		uint32_t base = ram->enemyBase + (uint32_t) i * GS_CHAR_STRIDE;
		GsMember* m = &out->enemies[out->enemyCount];
		int len = 0;
		for (; len < 15; ++len) {
			char c = (char) rd(ctx, base + GS_OFF_NAME + len, 1);
			if (c == 0) break;
			if (c < 32 || c >= 127) { len = 0; break; }
			m->name[len] = c;
		}
		m->name[len] = 0;
		m->level = (uint8_t) rd(ctx, base + GS_OFF_LEVEL, 1);
		m->maxHp = (uint16_t) rd(ctx, base + GS_OFF_MAXHP, 2);
		m->hp = (uint16_t) rd(ctx, base + GS_OFF_HP, 2);
		m->atk = (uint16_t) rd(ctx, base + GS_OFF_ATK, 2);
		m->def = (uint16_t) rd(ctx, base + GS_OFF_DEF, 2);
		m->agi = (uint16_t) rd(ctx, base + GS_OFF_AGI, 2);
		if (len == 0 || m->level == 0 || m->level > 99 || m->maxHp == 0 || m->hp > m->maxHp) break;
		m->present = true;
		m->rec = 0xFF;
		out->enemyCount++;
	}
	// The Lost Age: the highlighted command is read from its on-screen text; depth/top tell bar, list and target/round menu apart.
	if (ram->battleLabel && ram->battleDepth) {
		uint32_t depth = rd(ctx, ram->battleDepth, 1);
		out->battleDepth = (int) depth;
		char lab[20], tname[20], desc[56];
		readU16(rd, ctx, ram->battleLabel, lab, sizeof(lab));
		int tl = readU16(rd, ctx, ram->battleTname, tname, sizeof(tname));
		int dl = readU16(rd, ctx, ram->battleDesc, desc, sizeof(desc));
		static const char* const barNames[6] = {"Attack", "Psynergy", "Djinn", "Summon", "Item", "Defend"};
		static const char* const roundNames[3] = {"Fight", "Switch", "Flee"};
		int barCmd = -1, roundCmd = -1;
		for (int k = 0; k < 6; ++k) if (!strcmp(lab, barNames[k])) barCmd = k;
		for (int k = 0; k < 3; ++k) if (!strcmp(lab, roundNames[k])) roundCmd = k;
		int enemyIdx = -1;
		for (int k = 0; tl >= 3 && k < out->enemyCount; ++k) if (!strcmp(tname, out->enemies[k].name)) { enemyIdx = k; break; }
		uint32_t top = ram->battleTop ? rd(ctx, ram->battleTop, 1) : 0;
		if (out->battleSel >= 0 && enemyIdx >= 0 && depth == 2) {
			out->battlePhase = GS_PH_TARGET;
			out->battleTarget = enemyIdx;
			snprintf(out->battleText, sizeof(out->battleText), "%s", out->enemies[enemyIdx].name);
		} else if (out->battleSel >= 0 && depth == 2 && enemyIdx < 0 && roundCmd < 0 && top == 255 && ram->battleTidx &&
		           rd(ctx, ram->battleTidx, 1) < (uint32_t) out->partySize) {   // choosing a party member (Herb, Cure...): index in party order
			uint32_t t = rd(ctx, ram->battleTidx, 1);
			out->battlePhase = GS_PH_TARGET;
			out->battleTarget = (int) t;
			out->battleTargetAlly = true;
			snprintf(out->battleText, sizeof(out->battleText), "%s", out->party[t].name);
		} else if (out->battleSel >= 0 && depth == 0) {
			out->battlePhase = GS_PH_LIST;
			if (dl >= 8 && desc[0] >= 'A' && desc[0] <= 'Z') memcpy(out->battleText, desc, sizeof(out->battleText));
		} else if (out->battleSel >= 0 && depth == 3 && top == 1 && barCmd >= 0) {
			out->battlePhase = GS_PH_COMMAND;
			out->battleCmd = barCmd;
		} else if (depth == 2 && top == 0 && roundCmd >= 0 && enemyIdx < 0) {
			out->battlePhase = GS_PH_COMMAND;
			out->battleTop = true;
			out->battleCmd = roundCmd;
		}
		return;
	}
	// Menu level from the depth variable; the highlighted command is only reliable on the command bar.
	if (out->battleSel >= 0 && ram->battleDepth) {
		uint32_t depth = rd(ctx, ram->battleDepth, 1);
		out->battleDepth = (int) depth;
		uint32_t cmd = rd(ctx, ram->battleCmd, 1);
		char desc[56];
		int dl = readU16(rd, ctx, ram->battleDesc, desc, sizeof(desc));
		if (depth == 3) {
			out->battlePhase = GS_PH_COMMAND;
			out->battleTop = rd(ctx, ram->battleTop, 1) == 0;
			out->battleCmd = cmd < (out->battleTop ? 3u : 6u) ? (int) cmd : -1;
			// With fewer than six icons the game's bar holds only the commands that apply: map back to the six-slot order.
			if (!out->battleTop && out->battleHidden && cmd < 6) {   // game index -> app index: skip the commands the game hides
				unsigned seen = 0;
				out->battleCmd = -1;
				for (int c = 0; c < 6; ++c) {
					if ((out->battleHidden >> c) & 1) continue;
					if (seen++ == cmd) { out->battleCmd = c; break; }
				}
			}
		} else if (depth == 0) {
			out->battlePhase = GS_PH_LIST;
			if (dl >= 8 && desc[0] >= 'A' && desc[0] <= 'Z') memcpy(out->battleText, desc, sizeof(out->battleText));
		} else if (depth == 2) {
			uint32_t t = rd(ctx, ram->battleTidx, 1);
			out->battlePhase = GS_PH_TARGET;
			// Enemy targets leave the enemy's name in a UTF-16 buffer near 0x03007CC0 (its exact spot shifts by menu).
			bool enemy = false;
			if (t < (uint32_t) out->enemyCount) {
				const char* nm = out->enemies[t].name;
				int len = (int) strlen(nm);
				for (uint32_t a = 0x03007C00u; !enemy && a < 0x03007D00u; a += 2) {
					int k = 0;
					while (k < len && rd(ctx, a + 2 * (uint32_t) k, 2) == (uint32_t) (unsigned char) nm[k]) ++k;
					if (k == len && len >= 3) enemy = true;
				}
			}
			if (enemy) {
				out->battleTarget = (int) t;
				snprintf(out->battleText, sizeof(out->battleText), "%s", out->enemies[t].name);
			} else if (t < (uint32_t) out->partySize) {
				out->battleTarget = (int) t;
				out->battleTargetAlly = true;
				snprintf(out->battleText, sizeof(out->battleText), "%s", out->party[t].name);
			}
		}
	}
}
