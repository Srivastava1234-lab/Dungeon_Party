#ifndef DUNGEON_PARTY_AUDIO_H
#define DUNGEON_PARTY_AUDIO_H

#include <string>

// Small sound helper on top of miniaudio. Client only: the server never plays sounds.
bool initAudio();
void shutdownAudio();
void playVoice(const std::string& path); // stops the previous voice line, then plays this one

#endif // DUNGEON_PARTY_AUDIO_H