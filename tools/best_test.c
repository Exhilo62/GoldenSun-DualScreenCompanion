// Unit test for the bestiary: counting once per battle, sorting, save/load round trip, damaged files. Prints "ok name" / "FAIL name".
#include <stdio.h>
#include <string.h>

#include "../src/bestiary.h"

static int fails;
static void check(const char* name, int cond) { printf("%s %s\n", cond ? "ok" : "FAIL", name); if (!cond) ++fails; }

static void enemy(GsState* st, int i, const char* name, int lv, int hp, int atk, int def, int agi) {
	GsMember* m = &st->enemies[i];
	memset(m, 0, sizeof(*m));
	m->present = true; snprintf(m->name, sizeof(m->name), "%s", name);
	m->level = (uint8_t) lv; m->maxHp = (uint16_t) hp; m->hp = (uint16_t) hp; m->atk = (uint16_t) atk; m->def = (uint16_t) def; m->agi = (uint16_t) agi;
	st->enemyCount = i + 1;
}

int main(void) {
	static GsState st;
	bestReset();
	memset(&st, 0, sizeof(st));
	st.mode = GS_MODE_BATTLE;
	enemy(&st, 0, "Wild Wolf", 8, 90, 40, 20, 30);
	enemy(&st, 1, "Bat", 3, 20, 10, 5, 50);
	for (int f = 0; f < 10; ++f) bestObserve(&st);   // ten frames of one battle
	check("two enemies recorded", bestCount() == 2);
	check("sorted by name (Bat first)", !strcmp(bestAt(0)->name, "Bat") && !strcmp(bestAt(1)->name, "Wild Wolf"));
	check("counted once per battle", bestAt(0)->seen == 1 && bestAt(1)->seen == 1);
	check("no save while the battle runs", !bestTakeSave());
	st.mode = GS_MODE_FIELD; st.enemyCount = 0;
	bestObserve(&st);
	check("a battle that added something asks for a save", bestTakeSave());
	check("asks only once", !bestTakeSave());
	st.mode = GS_MODE_BATTLE;
	enemy(&st, 0, "Bat", 3, 20, 10, 5, 50);
	bestObserve(&st); bestObserve(&st);
	check("second battle counts again", bestAt(0)->seen == 2 && bestAt(1)->seen == 1);
	enemy(&st, 0, "Bat", 5, 44, 12, 7, 55);
	bestObserve(&st);
	check("stats follow the latest sighting", bestAt(0)->level == 5 && bestAt(0)->maxHp == 44 && bestAt(0)->agi == 55);
	check("short names ignored", (enemy(&st, 0, "X", 1, 1, 1, 1, 1), bestObserve(&st), bestCount() == 2));
	check("save works", bestSave("best_test.tmp"));
	bestReset();
	check("reset empties", bestCount() == 0);
	check("load works", bestLoad("best_test.tmp") && bestCount() == 2);
	check("round trip keeps data", !strcmp(bestAt(0)->name, "Bat") && bestAt(0)->seen == 2 && bestAt(0)->level == 5 && bestAt(1)->atk == 40);
	FILE* f = fopen("best_test.tmp", "r+b");
	fputc('Z', f);   // damage the magic number
	fclose(f);
	check("damaged file is rejected and leaves an empty list", !bestLoad("best_test.tmp") && bestCount() == 0);
	check("missing file is not an error for the list", !bestLoad("no_such_file.bin") && bestCount() == 0);
	remove("best_test.tmp");
	return fails ? 1 : 0;
}
