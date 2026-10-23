#include <string.h>

#include <stdbool.h>
#include <stdio.h>
#include "dialoglog.h"

static char entries[DLOG_MAX][DLOG_LEN];
static int count;
static unsigned version;
static uint32_t seenNow[16], seenPrev[16];   // hashes of the messages found in this / the previous poll
static int nowN, prevN;

static uint32_t hashStr(const char* s) {
	uint32_t h = 2166136261u;
	for (; *s; ++s) h = (h ^ (unsigned char) *s) * 16777619u;
	return h;
}

static bool wasSeen(const uint32_t* set, int n, uint32_t h) {
	for (int i = 0; i < n; ++i) if (set[i] == h) return true;
	return false;
}

static void push(const char* text) {
	if (count == DLOG_MAX) {
		memmove(entries[0], entries[1], sizeof(entries[0]) * (DLOG_MAX - 1));
		--count;
	}
	snprintf(entries[count++], DLOG_LEN, "%s", text);
	++version;
}

// An older identical message moves to the end (the line was said again) instead of being listed twice.
static void touch(const char* text) {
	for (int i = 0; i < count; ++i) {
		if (strcmp(entries[i], text)) continue;
		if (i == count - 1) return;
		char tmp[DLOG_LEN];
		snprintf(tmp, sizeof(tmp), "%s", entries[i]);
		memmove(entries[i], entries[i + 1], sizeof(entries[0]) * (count - 1 - i));
		snprintf(entries[count - 1], DLOG_LEN, "%s", tmp);
		++version;
		return;
	}
	push(text);
}

void dlogReset(void) {
	count = 0; nowN = prevN = 0;
	++version;
}

// True if `small` is a proper substring of `big`: a leftover of an older line that the text engine partly overwrote.
static bool isFragment(const char* small, const char* big) {
	size_t ls = strlen(small), lb = strlen(big);
	return ls < lb && strstr(big, small) != NULL;
}

#define CAND_MAX 24

void dlogPoll(const uint16_t* ring, int words) {
	nowN = 0;
	static char cand[CAND_MAX][DLOG_LEN];
	int candN = 0;
	char buf[DLOG_LEN];
	int n = 0;
	bool open = false;
	for (int i = 0; i <= words; ++i) {
		uint16_t c = i < words ? ring[i] : 0;
		bool text = c >= 32 && c < 127;
		if (text || c == 3) {   // 3 = new line inside a message box
			if (n < DLOG_LEN - 1) buf[n++] = text ? (char) c : ' ';
			open = true;
			continue;
		}
		if (!open) continue;
		open = false;
		while (n && buf[n - 1] == ' ') --n;
		buf[n] = 0;
		n = 0;
		if (strlen(buf) < 10) continue;   // short strings are menu labels and leftovers
		bool letters = false;
		for (const char* p = buf; *p; ++p) if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) { letters = true; break; }
		if (!letters) continue;
		if (candN < CAND_MAX) snprintf(cand[candN++], DLOG_LEN, "%s", buf);
	}
	for (int k = 0; k < candN; ++k) {
		const char* text = cand[k];
		// When the game writes a new line over an older one, the part it did not reach stays in the ring: "I'm not sure he has, K" or
		// "ge the subject, Alex!". Such a leftover is a piece of a longer line, either in this ring or already in the log: skip it.
		bool fragment = false;
		for (int j = 0; j < candN && !fragment; ++j) if (j != k && isFragment(text, cand[j])) fragment = true;
		for (int j = 0; j < count && !fragment; ++j) if (isFragment(text, entries[j])) fragment = true;
		if (fragment) continue;
		uint32_t h = hashStr(text);
		if (wasSeen(seenNow, nowN, h)) continue;   // the game keeps two copies of a message in the ring
		if (nowN < 16) seenNow[nowN++] = h;
		// Text that is still sitting in the ring from before (stale) is not "said again": only a message that was absent from the
		// previous poll counts as new, or is moved to the end when it is already in the log.
		if (wasSeen(seenPrev, prevN, h)) continue;
		touch(text);
	}
	memcpy(seenPrev, seenNow, sizeof(seenNow));
	prevN = nowN;
}

int dlogCount(void) { return count; }
const char* dlogText(int i) { return i >= 0 && i < count ? entries[i] : ""; }
unsigned dlogVersion(void) { return version; }
