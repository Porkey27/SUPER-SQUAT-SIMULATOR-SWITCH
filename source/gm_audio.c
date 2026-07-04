#include <switch.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "gm_audio.h"
#include "gm_assets.h"   // NMUSIC, SND_* ids

#define NBUF       4
#define BUFFRAMES  1024
#define BUFBYTES   (BUFFRAMES * 4)     // stereo s16 -> 4 bytes/frame (== 0x1000)
#define NVOICE     8

static int16_t* pcm[SND_COUNT];
static int      pcmFrames[SND_COUNT];

static float base[NMUSIC];   // unlock volume (set by obj_Player via sound_volume)
static float gate[NMUSIC];   // per-loop gate (set by obj_MusicControl roll)
static int   musicPos;       // shared cursor, audio-thread owned
static bool  musicOn;
static void (*onLoop)(void);

typedef struct { int id, pos; bool active; } Voice;
static Voice voices[NVOICE];

static Mutex mtx;
static Thread thread;
static bool   running;

static AudioOutBuffer buffers[NBUF];
static void*          bufmem[NBUF];

void gm_audio_set_pcm(int id, int16_t* data, int frames){ pcm[id]=data; pcmFrames[id]=frames; }
void gm_audio_on_loop(void (*cb)(void)){ onLoop = cb; }

void sound_volume(int id, float v){ if(id>=0 && id<NMUSIC) base[id] = v<0?0:(v>1?1:v); }
void music_gate (int id, float g){ if(id>=0 && id<NMUSIC) gate[id] = g; }
float music_base(int id){ return (id>=0 && id<NMUSIC) ? base[id] : 0.0f; }
void set_music_base(int id, float v){ if(id>=0 && id<NMUSIC) base[id] = v<0?0:(v>1?1:v); }

void sound_stop(int id){
    if(id<0) return;
    mutexLock(&mtx);
    for(int i=0;i<NVOICE;i++) if(voices[i].active && voices[i].id==id) voices[i].active=false;
    mutexUnlock(&mtx);
}
void sound_play(int id){        // one-shot voice (SFX); music is driven by the mixer
    if(id<0 || !pcm[id]) return;
    mutexLock(&mtx);
    int slot=-1; for(int i=0;i<NVOICE;i++) if(!voices[i].active){ slot=i; break; }
    if(slot<0) slot=0;          // steal
    voices[slot].id=id; voices[slot].pos=0; voices[slot].active=true;
    mutexUnlock(&mtx);
}
void gm_audio_start_music(void){ musicPos=0; musicOn=true; }

static void mix(int16_t* out, int frames)
{
    mutexLock(&mtx);
    for(int f=0; f<frames; f++){
        float L=0, R=0;
        if(musicOn){
            int mp = musicPos;
            for(int i=0;i<NMUSIC;i++){
                float g = base[i]*gate[i];
                if(g>0.0001f && pcm[i]){
                    L += pcm[i][mp*2]   * g;
                    R += pcm[i][mp*2+1] * g;
                }
            }
            if(++musicPos >= MUSIC_LEN){ musicPos=0; if(onLoop) onLoop(); }
        }
        for(int v=0; v<NVOICE; v++){
            if(!voices[v].active) continue;
            int id=voices[v].id, p=voices[v].pos;
            L += pcm[id][p*2]; R += pcm[id][p*2+1];
            if(++voices[v].pos >= pcmFrames[id]) voices[v].active=false;
        }
        if(L> 32767) L= 32767; if(L<-32768) L=-32768;
        if(R> 32767) R= 32767; if(R<-32768) R=-32768;
        out[f*2]   = (int16_t)L;
        out[f*2+1] = (int16_t)R;
    }
    mutexUnlock(&mtx);
}

static void audioThread(void* arg)
{
    (void)arg;
    while(running){
        AudioOutBuffer* released=NULL; u32 n=0;
        if(R_FAILED(audoutGetReleasedAudioOutBuffer(&released,&n)) || n==0){ svcSleepThread(1000000); continue; } // 1ms yield
        mix((int16_t*)released->buffer, BUFFRAMES);
        armDCacheFlush(released->buffer, BUFBYTES);
        audoutAppendAudioOutBuffer(released);
    }
}

void gm_audio_init(void)
{
    for(int i=0;i<NMUSIC;i++){ base[i]=0; gate[i]=1; }
    mutexInit(&mtx);
    audoutInitialize();
    audoutStartAudioOut();

    for(int i=0;i<NBUF;i++){
        bufmem[i] = memalign(0x1000, BUFBYTES);
        memset(bufmem[i], 0, BUFBYTES);
        armDCacheFlush(bufmem[i], BUFBYTES);
        buffers[i].next = NULL;
        buffers[i].buffer = bufmem[i];
        buffers[i].buffer_size = BUFBYTES;
        buffers[i].data_size = BUFBYTES;
        buffers[i].data_offset = 0;
        audoutAppendAudioOutBuffer(&buffers[i]);
    }
    running = true;
    threadCreate(&thread, audioThread, NULL, NULL, 0x8000, 0x2C, -2);
    threadStart(&thread);
}

void gm_audio_exit(void)
{
    running = false;
    threadWaitForExit(&thread);
    threadClose(&thread);
    audoutStopAudioOut();
    audoutExit();
    for(int i=0;i<NBUF;i++) free(bufmem[i]);
}
