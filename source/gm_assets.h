// Asset registry: sprites (strip-sliced textures) and sound ids.
// Game-logic files (gm_object.c/objects.c) only ever pass GMSprite* through
// to gmgl_*, they never touch the texture handle fields directly, so each
// platform backend is free to stash whatever it needs here behind #ifdef.
#pragma once
#include <stdint.h>
#if defined(__3DS__)
#include <citro3d.h>
#endif
#include "gm.h"

typedef struct {
#if defined(__3DS__)
    C3D_Tex tex3ds;                // GPU texture object (owns its own linear-mem alloc)
    int texPW, texPH;              // padded power-of-two dims backing tex3ds
#else
    uint32_t tex;                  // GL texture name
#endif
    int texW, texH;                // *actual* content pixel dims (used for UV math below)
    int frameW, frameH, frames;
    float originX, originY;       // pivot in px (y-down)
} GMSprite;

// UV of a frame (single-row strips): v spans full height.
static inline void gmsprite_uv(const GMSprite* s, int sub, float* u0, float* u1)
{
    if (s->frames > 0) sub = ((sub % s->frames) + s->frames) % s->frames;
    *u0 = (float)(sub * s->frameW) / s->texW;
    *u1 = (float)(sub * s->frameW + s->frameW) / s->texW;
}

// loaded sprites
extern GMSprite spr_Font, spr_Gradient, spr_Player, spr_Shapes, spr_Swirl;

// sound ids: 0..11 = looping music stems (6.4s), 12..14 = one-shot SFX
enum {
    SND_BASS1, SND_BASS2, SND_BASS3, SND_BASS4,
    SND_LEAD1, SND_LEAD1HARMONY, SND_MELODY1, SND_MELODY2,
    SND_BASSDRUM, SND_PERCUSSION, SND_HIHATDOUBLE, SND_TOM,   // 12 music
    SND_BLIP1, SND_BLIP2, SND_MESSAGE,                        // 3 sfx
    SND_COUNT
};
#define NMUSIC 12

// load all textures (GL) and sounds (PCM into RAM). Implemented in platform .c files.
void gm_assets_load(void);
