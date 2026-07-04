#include <glad/glad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "gm_assets.h"
#include "gm_audio.h"

GMSprite spr_Font, spr_Gradient, spr_Player, spr_Shapes, spr_Swirl;

static GLuint load_tex(const char* path, int* w, int* h)
{
    int n; unsigned char* px = stbi_load(path, w, h, &n, 4);
    if(!px){ printf("[assets] FAILED to load: %s\n", path); *w=*h=1; }
    else printf("[assets] loaded %s (%dx%d)\n", path, *w, *h);
    GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(px){ glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,*w,*h,0,GL_RGBA,GL_UNSIGNED_BYTE,px); stbi_image_free(px); }
    return t;
}

static GMSprite make_sprite(const char* name, int frameW, float ox, float oy)
{
    char path[128]; snprintf(path,sizeof path,"romfs:/gfx/%s.png",name);
    int w,h; GLuint t = load_tex(path,&w,&h);
    GMSprite s; s.tex=t; s.texW=w; s.texH=h;
    s.frameW=frameW; s.frameH=h; s.frames = frameW>0 ? w/frameW : 1;
    if(s.frames<1) s.frames=1;
    s.originX=ox; s.originY=oy;
    return s;
}

static int16_t* load_pcm(const char* name, int* frames)
{
    char path[128]; snprintf(path,sizeof path,"romfs:/sfx/%s.raw",name);
    FILE* f=fopen(path,"rb");
    if(!f){ printf("[assets] failed: %s\n",path); *frames=0; return NULL; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    int16_t* buf=(int16_t*)malloc(sz);
    fread(buf,1,sz,f); fclose(f);
    *frames = (int)(sz/4);          // stereo s16 -> 4 bytes/frame
    return buf;
}

void gm_assets_load(void)
{
    // frameW, originX, originY (centre origins for the rotating sprites)
    spr_Font     = make_sprite("spr_Font",     5,   0,   0);
    spr_Gradient = make_sprite("spr_Gradient", 640, 0,   0);
    spr_Player   = make_sprite("spr_Player",   192, 96,  96);
    spr_Shapes   = make_sprite("spr_Shapes",   32,  16,  16);
    spr_Swirl    = make_sprite("spr_Swirl",    336, 168, 32);

    static const char* names[SND_COUNT] = {
        "snd_Bass1","snd_Bass2","snd_Bass3","snd_Bass4",
        "snd_Lead1","snd_Lead1Harmony","snd_Melody1","snd_Melody2",
        "snd_BassDrum","snd_Percussion","snd_HiHatDouble","snd_Tom",
        "snd_Blip1","snd_Blip2","snd_Message"
    };
    for(int i=0;i<SND_COUNT;i++){
        int fr; int16_t* p = load_pcm(names[i], &fr);
        gm_audio_set_pcm(i, p, fr);
    }
}
