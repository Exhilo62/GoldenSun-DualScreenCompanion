#include <stdlib.h>

#include "menunav.h"
#include "ui.h"

#define HAND_DX 28   // the hand sits ~17px left of an entry and a tap lands ~11px into its text
#define HAND_DY 0    // hand top-left y equals the text row centre (measured on the Djinn page)
#define TOL_X 18     // under half a column on the Djinn page (57px apart); the undo step handles the rest
#define TOL_Y 4      // lists are 8px per row

bool navTap(MenuNav* n, const GsState* st, int fx, int fy) {
	n->tx = fx - HAND_DX;
	n->ty = fy - HAND_DY;
	n->active = false;
	n->dir = 0;
	n->finishing = false;
	n->pressFrames = n->wait = n->stuck = 0;
	if (!st->handFound) return false;
	if (abs(n->tx - st->handX) <= TOL_X && abs(n->ty - st->handY) <= TOL_Y) return true;
	n->active = true;
	n->prevX = st->handX; n->prevY = st->handY;
	n->lastErr = 0;
	return false;
}

static unsigned opposite(unsigned d) {
	return d == UK_UP ? UK_DOWN : d == UK_DOWN ? UK_UP : d == UK_LEFT ? UK_RIGHT : UK_LEFT;
}

unsigned navStep(MenuNav* n, const GsState* st) {
	if (!n->active) return 0;
	if (!st->handFound) { n->active = false; return 0; }
	if (n->pressFrames > 0) { --n->pressFrames; return n->dir; }
	if (n->wait > 0) { --n->wait; return 0; }
	if (n->finishing) { n->active = false; n->dir = 0; return 0; }

	int dx = n->tx - st->handX, dy = n->ty - st->handY;
	if (n->dir) {
		bool horiz = n->dir == UK_LEFT || n->dir == UK_RIGHT;
		int err = abs(horiz ? dx : dy);
		bool moved = abs(st->handX - n->prevX) + abs(st->handY - n->prevY) > 3;   // the hand bobs by a pixel or two
		n->stuck = moved ? 0 : n->stuck + 1;
		if (n->stuck >= 2) { n->active = false; n->dir = 0; return 0; }          // at the edge of the menu
		if (moved && err > n->lastErr + 3) {                                       // went past the target (or wrapped around): step back once
			n->dir = opposite(n->dir);
			n->pressFrames = 2; n->wait = 10; n->finishing = true;
			return n->dir;
		}
	}
	n->prevX = st->handX; n->prevY = st->handY;
	if (abs(dx) > TOL_X) {
		n->dir = dx > 0 ? UK_RIGHT : UK_LEFT;
		n->lastErr = abs(dx);
	} else if (abs(dy) > TOL_Y) {
		n->dir = dy > 0 ? UK_DOWN : UK_UP;
		n->lastErr = abs(dy);
	} else {
		n->active = false; n->dir = 0;
		return 0;
	}
	n->pressFrames = 2;
	n->wait = 8;
	return n->dir;
}
