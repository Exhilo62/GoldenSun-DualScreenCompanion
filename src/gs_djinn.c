// Generated from the game's own text with tools/textdec.py (Golden Sun groups 4 and 6).
#include <string.h>
#include <stddef.h>
#include "gs_djinn.h"
#include "gs_ram.h"

// Djinn names and descriptions (text groups 4 and 6; element order Venus, Mars, Jupiter, Mercury).
static const char* const djinnNames[4][7] = {
	{"Flint", "Granite", "Quartz", "Vine", "Sap", "Ground", "Bane"},
	{"Forge", "Fever", "Corona", "Scorch", "Ember", "Flash", "Torch"},
	{"Gust", "Breeze", "Zephyr", "Smog", "Kite", "Squall", "Luff"},
	{"Fizz", "Sleet", "Mist", "Spritz", "Hail", "Tonic", "Dew"}
};
static const char* const djinnDescs[4][7] = {
	{"Strike a blow that can cleave stone.", "Create a mighty earthen barrier.", "Revive a downed ally.", "Tangle foes to drop Agility.", "Attack a foe and steal HP.", "Use gravity to hold a foe.", "Attack with nature's venom."},
	{"Boost party Attack with flame's fury.", "Wrap a foe in feverish delusion.", "Boost party Defense with a heat aura.", "Stun a foe with a blast attack.", "Restore party PP with passion's flames.", "Block damage to party with a firewall.", "Penetrate defense with a melting blast."},
	{"Attack with mighty wind gusts.", "Boost party Resistance.", "Boost party Agility with swift wind.", "Veil a foe's vision in smoke.", "Attack twice next round.", "Paralyze a foe with a storm.", "Seal a foe's Psynergy."},
	{"Restore HP with calming water.", "Drench a foe to drop its Attack.", "Lull a foe into deep sleep.", "Restore party HP with soothing mist.", "Freeze a foe to drop its Defense.", "Heal all party ailments.", "Revive a downed ally."}
};

// BEGIN TLA DJINN
static const char* const djinnNamesTla[4][18] = {   // The Lost Age (18 per element)
	{"Flint", "Granite", "Quartz", "Vine", "Sap", "Ground", "Bane", "Echo", "Iron", "Steel", "Mud", "Flower", "Meld", "Petra", "Salt", "Geode", "Mold", "Crystal"},
	{"Forge", "Fever", "Corona", "Scorch", "Ember", "Flash", "Torch", "Cannon", "Spark", "Kindle", "Char", "Coal", "Reflux", "Core", "Tinder", "Shine", "Fury", "Fugue"},
	{"Gust", "Breeze", "Zephyr", "Smog", "Kite", "Squall", "Luff", "Breath", "Blitz", "Ether", "Waft", "Haze", "Wheeze", "Aroma", "Whorl", "Gasp", "Lull", "Gale"},
	{"Fizz", "Sleet", "Mist", "Spritz", "Hail", "Tonic", "Dew", "Fog", "Sour", "Spring", "Shade", "Chill", "Steam", "Rime", "Gel", "Eddy", "Balm", "Serac"},
};
static const char* const djinnDescsTla[4][18] = {
	{"Strike a blow that can cleave stone.", "Create a mighty earthen barrier.", "Revive a downed ally.", "Tangle foes to drop Agility.", "Attack a foe and steal HP.", "Use gravity to hold a foe.", "Attack with nature's venom.", "Attack with a double strike.", "Bolster the party's Defense.", "Siphon a foe's HP with a kiss.", "Slow a foe with sticky mud.", "Refresh allies and restore HP.", "Launch a powerful team strike.", "Turn a foe to stone.", "Restore allies' status to normal.", "Strike with a clod of earth.", "Strike a foe.", "Restore HP to all allies."},
	{"Boost party Attack with flame's fury.", "Wrap a foe in feverish delusion.", "Boost party Defense with a heat aura.", "Stun a foe with a blast attack.", "Restore party PP with passion's flames.", "Block damage to party with a firewall.", "Penetrate defense with a melting blast.", "Strike with the power of Mars.", "Revive an ally with cheers of support.", "Increase all allies' Attack.", "Paralyze foes with a strong blow.", "Rally your allies to boost Agility.", "Counter an enemy's attack.", "Strike through an enemy's Defense.", "Revive a downed ally.", "Dazzle foes and strike decisively.", "Call forth wandering souls to attack.", "Fatigue your foes to drop their PP."},
	{"Attack with mighty wind gusts.", "Boost party Resistance.", "Boost party Agility with swift wind.", "Veil a foe's vision in smoke.", "Attack twice next round.", "Paralyze a foe with a storm.", "Seal a foe's Psynergy.", "Restore HP quickly.", "Numb a foe with a lightning strike.", "Focus will to restore PP.", "Calm a foe with soothing scents.", "Hide away to avoid damage.", "Poison a foe as you strike.", "Restore everyone's PP.", "Take a deep breath, and strike!", "Call the Grim Reaper on your foes.", "Negotiate a temporary cease-fire.", "Blast enemies with a wind strike."},
	{"Restore HP with calming water.", "Drench a foe to drop its Attack.", "Lull a foe into deep sleep.", "Restore party HP with soothing mist.", "Freeze a foe to drop its Defense.", "Heal all party ailments.", "Revive a downed ally.", "Blind an enemy with fog.", "Reduce a foe's resistance.", "Restore HP with healing herbs.", "Create a watery shield.", "Strike to reduce a foe's defense.", "Increase all allies' Elemental strength.", "Seal a foe's Psynergy.", "Weaken a foe's Attack.", "Speed up Djinn recovery time.", "Revive all downed allies.", "Strike a chilling finishing blow."},
};
// END TLA DJINN
const char* gsDjinnName(int element, int index) {
	if (gsRam() == &gsRamTla) return element < 0 || element > 3 || index < 0 || index > 17 ? "?" : djinnNamesTla[element][index];
	if (element < 0 || element > 3 || index < 0 || index > 6) return "?";
	return djinnNames[element][index];
}

const char* gsDjinnDescription(int element, int index) {
	if (element < 0 || element > 3 || index < 0) return "";
	if (gsRam() == &gsRamTla) return index > 17 ? "" : djinnDescsTla[element][index];
	return index > 6 ? "" : djinnDescs[element][index];
}

const char* gsDjinnByDescription(const char* text) {
	const char* found = NULL;
	bool tla = gsRam() == &gsRamTla;
	for (int e = 0; e < 4; ++e)
		for (int i = 0; i < (tla ? 18 : 7); ++i) {
			const char* dsc = tla ? djinnDescsTla[e][i] : djinnDescs[e][i];
			if (!strcmp(dsc, text)) {
				if (found) return NULL;   // shared text: do not guess
				found = tla ? djinnNamesTla[e][i] : djinnNames[e][i];
			}
		}
	return found;
}
