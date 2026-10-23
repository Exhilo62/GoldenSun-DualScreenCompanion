// Message log: the last few dialogue and battle messages, recovered from the game's text ring buffer so a line that was tapped past
// can be read again. Fed with a copy of the ring (UTF-16, game control codes included) a few times a second.
#ifndef DIALOGLOG_H
#define DIALOGLOG_H
#include <stdint.h>

#define DLOG_MAX 24
#define DLOG_LEN 200
#define DLOG_RING_WORDS 512   // the ring holds 512 halfwords

void dlogReset(void);
// Parses a copy of the ring and appends messages that are new.
void dlogPoll(const uint16_t* ring, int words);
int dlogCount(void);
const char* dlogText(int i);   // 0 = oldest
unsigned dlogVersion(void);    // changes whenever the log changes
#endif
