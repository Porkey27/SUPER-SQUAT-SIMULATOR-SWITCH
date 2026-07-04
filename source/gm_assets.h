// Asset registry: sprites (strip-sliced GL textures) and sound ids.
// Header stays platform-free (tex is a uint32_t GL name) so game logic compiles
// without GL headers.
#pragma once
#include <stdint.h>
#include "gm.h"

typedef struct {
    uint32_t tex;                 // GL texture name
    int texW, texH;
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
