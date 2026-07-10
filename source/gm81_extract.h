#pragma once
#include <stdint.h>

typedef struct {
    char*    name;
    int32_t  origin_x, origin_y;
    uint32_t frame_count;
    uint32_t frame_w, frame_h;
    uint8_t* rgba;   // frame_w*frame_count wide x frame_h tall, RGBA8888, owned
} Gm81Sprite;

typedef struct {
    char*    name;
    uint8_t* wav_data; // raw RIFF/WAVE bytes, NOT owned independently (freed with the parser's internal buffers -- copy out anything you need to keep past gm81_close)
    uint32_t wav_len;
} Gm81Sound;

// Parses `exe_path` (a GM8.1 executable) in place, building an in-memory
// index of every sprite/sound it contains. Returns 1 on success, 0 if the
// file couldn't be opened or isn't a recognized GM8.1 exe.
int gm81_open(const char* exe_path);

// Look up an asset by its in-game resource name (e.g. "spr_Player",
// "snd_Bass1"). Returns NULL if not found. Pointers are valid for the
// lifetime of the process (no gm81_close in this version -- the whole
// index is meant to be built once at startup and kept for the game's
// lifetime).
const Gm81Sprite* gm81_find_sprite(const char* name);
const Gm81Sound*  gm81_find_sound(const char* name);

// Decodes a sound's embedded WAV to 48kHz stereo s16 PCM (linear
// resample from whatever rate the source WAV actually is). Caller owns
// the returned buffer (free() it). Returns NULL on parse failure.
int16_t* gm81_sound_to_pcm48(const Gm81Sound* snd, int* out_frames);
