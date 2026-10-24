# Concept: banner cards on the game-pick screen

Not used in v0.1.0 (the text-title cards are). Kept so it can be brought back later.

What it looked like: each game card showed the fan-art game logo (Golden Sun / Golden Sun: The Lost Age) as a banner next to the portrait,
the party names stacked in a column at the right (with the LAST PLAYED tag above them on the card played last).

How to bring it back:
1. Everything is kept: the fan art in `assets/reference/card_logo_gs1.png`, `card_logo_gs2.webp`, `card_face_gs1.webp`, `card_face_gs2.webp`,
   the baker `tools/gencards.ps1` (writes `src/card_data.c`) and the declarations `artCardLogo()` / `artCardFace()` in `src/art.h`.
2. Add `src/card_data.c` to `SRCS` in the Makefile (and to the source lists in `tools/selftest.py` that contain `src/mapicon_data.c`).
3. Apply `docs/concepts/pick_banners_launcher.patch` to `src/launcher.c` (`git apply`), or `git show concept-pick-banners:src/launcher.c`.
   The git tag `concept-pick-banners` marks the commit with the finished concept (portraits original, banners next to them, names stacked).
