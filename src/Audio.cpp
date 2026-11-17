#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "Audio.h"

#include <iostream>

namespace {
ma_engine engine;
bool engineReady = false;
ma_sound voice;
bool voiceLoaded = false;
}

bool initAudio() {
    if (ma_engine_init(nullptr, &engine) != MA_SUCCESS) {
        std::cerr << "Audio: could not start the sound engine\n";
        return false;
    }
    engineReady = true;
    return true;
}

void shutdownAudio() {
    if (voiceLoaded) { ma_sound_uninit(&voice); voiceLoaded = false; }
    if (engineReady) { ma_engine_uninit(&engine); engineReady = false; }
}

void playVoice(const std::string& path) {
    if (!engineReady) return;
    if (voiceLoaded) { ma_sound_uninit(&voice); voiceLoaded = false; }
    if (ma_sound_init_from_file(&engine, path.c_str(), 0, nullptr, nullptr, &voice) != MA_SUCCESS) {
        std::cerr << "Audio: could not load " << path << "\n";
        return;
    }
    voiceLoaded = true;
    ma_sound_start(&voice);
}