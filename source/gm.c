#include "gm.h"

int   gm_room_speed  = 60;
float gm_room_width  = 640.0f;
float gm_room_height = 480.0f;
float view_xview = 0, view_yview = 0, view_wview = 640, view_hview = 480;
float view_angle = 0.0f;

float   draw_alpha = 1.0f;
GMColor draw_color = {1,1,1,1};
int     draw_blend = BM_NORMAL, draw_halign = FA_LEFT, draw_valign = FA_TOP;

// ---- xorshift128 PRNG -------------------------------------------------
static uint32_t s0=0x9e3779b9u, s1=0x243f6a88u, s2=0xb7e15162u, s3=0xdeadbeefu;

void gm_init(uint64_t seed)
{
    s0 = (uint32_t)(seed ^ 0x9e3779b9u);
    s1 = (uint32_t)((seed >> 32) ^ 0x243f6a88u);
    s2 = 0xb7e15162u ^ (uint32_t)seed;
    s3 = 0xdeadbeefu ^ (uint32_t)(seed >> 17);
    if (!(s0|s1|s2|s3)) s0 = 1;
}

static inline uint32_t xr(void)
{
    uint32_t t = s3;
    uint32_t s = s0;
    s3 = s2; s2 = s1; s1 = s;
    t ^= t << 11; t ^= t >> 8;
    s0 = t ^ s ^ (s >> 19);
    return s0;
}
static inline float frand(void){ return (xr() >> 8) * (1.0f / 16777216.0f); } // [0,1)

float gm_random(float n){ return frand() * n; }
float gm_random_range(float a, float b){ return a + frand() * (b - a); }
int   gm_irandom(int n){ return (int)(frand() * (n + 1)); }   // [0,n]
float gm_choose2(float a, float b){ return (xr() & 1) ? b : a; }
float gm_choose3(float a, float b, float c){ uint32_t r = xr() % 3u; return r==0?a:(r==1?b:c); }

GMColor gm_make_color_hsv(float h, float s, float v)
{
    h = gm_repeat(h, 256.0f) / 255.0f * 6.0f;   // 0..6
    s = s / 255.0f; v = v / 255.0f;
    int i = (int)h; float f = h - i;
    float p = v*(1-s), q = v*(1-s*f), t = v*(1-s*(1-f));
    GMColor c; c.a = 1.0f;
    switch (i % 6) {
        case 0: c.r=v; c.g=t; c.b=p; break;
        case 1: c.r=q; c.g=v; c.b=p; break;
        case 2: c.r=p; c.g=v; c.b=t; break;
        case 3: c.r=p; c.g=q; c.b=v; break;
        case 4: c.r=t; c.g=p; c.b=v; break;
        default:c.r=v; c.g=p; c.b=q; break;
    }
    return c;
}
