// Software audio mixer over audout (48kHz stereo s16).
// Music stems share ONE playback cursor, so they are inherently phase-locked;
// the per-bar randomization callback fires exactly when the cursor wraps (drift-free).
#pragma once
#include <stdint.h>

#define MUSIC_LEN 307200          // 6.4s @ 48000 (all stems identical length)

void gm_audio_init(void);
void gm_audio_exit(void);
void gm_audio_set_pcm(int sndid, int16_t* data, int frames);
void gm_audio_on_loop(void (*cb)(void));   // called on music cursor wrap
void gm_audio_start_music(void);           // begin mixing (cursor = 0)

// GM sound API
void  sound_play(int sndid);               // one-shot SFX (blip/message)
void  sound_stop(int sndid);
void  sound_volume(int sndid, float v);    // music: base volume
void  music_gate(int sndid, float g);      // music: per-loop gate
float music_base(int sndid);
void  set_music_base(int sndid, float v);
