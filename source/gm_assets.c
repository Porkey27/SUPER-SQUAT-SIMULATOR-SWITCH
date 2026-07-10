#include <glad/glad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gm_assets.h"
#include "gm_audio.h"
#include "gm81_extract.h"

GMSprite spr_Font, spr_Gradient, spr_Player, spr_Shapes, spr_Swirl;

// Where the user's own (legally-owned, freely-downloadable) copy of the
// game's exe needs to live. We never bundle/redistribute its assets --
// they're read from this file at runtime, on the user's own SD card.
#define GAME_EXE_PATH "sdmc:/switch/SuperSquatSim/SSS.exe"

static GMSprite make_sprite_from_gm81(const char* name)
{
    GMSprite s = {0};
    const Gm81Sprite* src = gm81_find_sprite(name);
    if (!src) {
        printf("[assets] sprite '%s' not found in exe -- is it the right game/version?\n", name);
        // 1x1 placeholder so the game doesn't crash outright; it'll just
        // render as a blank/white square where this sprite should be.
        static uint32_t white = 0xFFFFFFFFu;
        GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,&white);
        s.tex=t; s.texW=s.texH=s.frameW=s.frameH=1; s.frames=1;
        return s;
    }

    GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    uint32_t stripW = src->frame_w * src->frame_count;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,stripW,src->frame_h,0,GL_RGBA,GL_UNSIGNED_BYTE,src->rgba);

    s.tex = t;
    s.texW = (int)stripW; s.texH = (int)src->frame_h;
    s.frameW = (int)src->frame_w; s.frameH = (int)src->frame_h;
    s.frames = (int)src->frame_count;
    // NOTE: origin_x/y straight from the game's own data now -- supersedes
    // any hand-guessed values from earlier ports.
    s.originX = (float)src->origin_x; s.originY = (float)src->origin_y;
    printf("[assets] built '%s' (%dx%d, %d frames, origin %d,%d)\n",
           name, s.frameW, s.frameH, s.frames, src->origin_x, src->origin_y);
    return s;
}

static int16_t* load_pcm_from_gm81(const char* name, int* frames)
{
    const Gm81Sound* src = gm81_find_sound(name);
    if (!src) {
        printf("[assets] sound '%s' not found in exe\n", name);
        *frames = 0; return NULL;
    }
    int16_t* pcm = gm81_sound_to_pcm48(src, frames);
    if (!pcm) printf("[assets] sound '%s' failed to decode (bad WAV?)\n", name);
    return pcm;
}

// Returns 1 if the exe was found and parsed successfully. Call this
// before gm_assets_load(); if it fails, show the user an error rather
// than proceeding (gm_assets_load() will otherwise silently fall back to
// blank placeholder sprites/silent sounds).
int gm_assets_check_source(void)
{
    if (gm81_open(GAME_EXE_PATH)) return 1;
    printf("[assets] Could not find/parse the game at:\n  " GAME_EXE_PATH "\n");
    printf("[assets] Place your own copy of SSS.exe there\n");
    printf("[assets] (free download: gamejolt.com/games/super-squat-simulator/11729)\n");
    return 0;
}

void gm_assets_load(void)
{
    spr_Font     = make_sprite_from_gm81("spr_Font");
    spr_Gradient = make_sprite_from_gm81("spr_Gradient");
    spr_Player   = make_sprite_from_gm81("spr_Player");
    spr_Shapes   = make_sprite_from_gm81("spr_Shapes");
    spr_Swirl    = make_sprite_from_gm81("spr_Swirl");

    static const char* names[SND_COUNT] = {
        "snd_Bass1","snd_Bass2","snd_Bass3","snd_Bass4",
        "snd_Lead1","snd_Lead1Harmony","snd_Melody1","snd_Melody2",
        "snd_BassDrum","snd_Percussion","snd_HiHatDouble","snd_Tom",
        "snd_Blip1","snd_Blip2","snd_Message"
    };
    for(int i=0;i<SND_COUNT;i++){
        int fr; int16_t* p = load_pcm_from_gm81(names[i], &fr);
        gm_audio_set_pcm(i, p, fr);
    }
}
