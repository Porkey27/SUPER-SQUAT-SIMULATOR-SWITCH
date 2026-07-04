#pragma once
#include "gm_object.h"

extern const GMType obj_Shape_type;
extern const GMType obj_Par_type;
extern const GMType obj_PlayerTrail_type;
extern const GMType obj_Flash_type;
extern const GMType obj_Message_type;
extern const GMType obj_MusicControl_type;
extern const GMType obj_Player_type;
extern const GMType obj_Control_type;

// music state (single MusicControl); read by obj_Player
int  music_current_bass(void);
int  music_current_lead(void);
void obj_music_loop(void);          // per-loop roll, registered as audio wrap callback

// bridges implemented in main.c
bool gm_key_pressed(int vk);        // frame == step, so no buffering needed

// quit flag (Control alarm0 / game_end)
extern bool g_quit;
