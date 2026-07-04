// GM runtime core: constants, RNG, math, colour, view + draw state.
// Angles are DEGREES where GM uses degrees; gm_sin/gm_cos take RADIANS like GML.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define GM_PI 3.14159265358979f

typedef struct { float r, g, b, a; } GMColor;

// virtual keys / alignments / blend modes
enum { VK_UP = 273, VK_DOWN = 274, VK_ENTER = 13, VK_ESCAPE = 27 };
enum { FA_LEFT = 0, FA_CENTER = 1, FA_RIGHT = 2 };
enum { FA_TOP = 0, FA_MIDDLE = 1, FA_BOTTOM = 2 };
enum { BM_NORMAL = 0, BM_ADD = 1 };

// room / view
extern int   gm_room_speed;
extern float gm_room_width, gm_room_height;
extern float view_xview, view_yview, view_wview, view_hview, view_angle;

// draw state (set by draw_set_*)
extern float   draw_alpha;
extern GMColor draw_color;
extern int     draw_blend, draw_halign, draw_valign;

static const GMColor C_WHITE = {1,1,1,1};
static const GMColor C_BLACK = {0,0,0,1};

void gm_init(uint64_t seed);

// RNG (GM semantics)
float gm_random(float n);            // [0,n)
float gm_random_range(float a, float b);
int   gm_irandom(int n);             // [0,n]
float gm_choose2(float a, float b);
float gm_choose3(float a, float b, float c);

// math
static inline float gm_dsin(float d){ return sinf(d * GM_PI / 180.0f); }
static inline float gm_dcos(float d){ return cosf(d * GM_PI / 180.0f); }
static inline float gm_degtorad(float d){ return d * GM_PI / 180.0f; }
static inline float gm_sin(float r){ return sinf(r); }
static inline float gm_cos(float r){ return cosf(r); }
static inline float gm_floor(float x){ return floorf(x); }
static inline int   gm_sign(float x){ return x > 0 ? 1 : (x < 0 ? -1 : 0); }
static inline float gm_repeat(float a, float b){ float r = fmodf(a,b); return r < 0 ? r + b : r; }
static inline float gm_clamp01(float v){ return v < 0 ? 0 : (v > 1 ? 1 : v); }

// colour
GMColor gm_make_color_hsv(float h, float s, float v);   // h,s,v all 0..255
static inline GMColor gm_rgb(float r, float g, float b){ GMColor c={r,g,b,1}; return c; }

// draw-state setters
static inline void draw_set_alpha(float a){ draw_alpha = a; }
static inline void draw_set_color(GMColor c){ draw_color = c; }
static inline void draw_set_blend_mode(int m){ draw_blend = m; }
static inline void draw_set_halign(int h){ draw_halign = h; }
static inline void draw_set_valign(int v){ draw_valign = v; }
