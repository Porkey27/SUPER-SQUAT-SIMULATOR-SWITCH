// GM immediate-mode drawing (GMDraw analog), backed by OpenGL core.
// All coordinates are GM room coordinates (origin top-left, y DOWN).
#pragma once
#include "gm.h"
#include "gm_assets.h"

// tunables — flip if rotation is mirrored
extern float GM_VIEW_ANGLE_SIGN;
extern float GM_SPRITE_ANGLE_SIGN;
#define GM_FONT_SEP 1

void gmgl_init(void);                 // compile shaders, VBO, white tex
void gmgl_frame_begin(void);          // clear + reset batch
void gmgl_frame_end(void);            // flush

void gmgl_set_projection(float angleDeg);   // view rect rotated by angle about centre

void gmgl_rectangle(float x1,float y1,float x2,float y2, GMColor col, float alpha, int blend);
void gmgl_sprite_stretched(const GMSprite* s,int sub,float x,float y,float w,float h,
                           GMColor col,float alpha,int blend);
void gmgl_sprite_ext(const GMSprite* s,int sub,float x,float y,float xs,float ys,
                     float angDeg,GMColor col,float alpha,int blend);
void gmgl_sprite_default(const GMSprite* s,float image_index,float x,float y,
                         float xs,float ys,float angDeg,GMColor blend,float alpha);
// draw_text_transformed with sprite font; '#' = newline, glyph frame = ord(c)-33
void gmgl_text_transformed(const GMSprite* font,float x,float y,const char* str,
                           float xs,float ys,float angDeg);
