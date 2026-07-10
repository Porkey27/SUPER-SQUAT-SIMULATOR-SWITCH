#include <stdio.h>
#include <string.h>
#include <math.h>
#include "objects.h"
#include "gm_gl.h"
#include "gm_audio.h"

bool g_quit = false;
static void game_end(void){ g_quit = true; }

// single MusicControl state (also read by obj_Player and the audio-thread roll)
static int music_bass = SND_BASS1;
static int music_lead = SND_LEAD1;
int music_current_bass(void){ return music_bass; }
int music_current_lead(void){ return music_lead; }

// ===================== obj_Shape ===========================================
static void shape_create(GMObject* o)
{
    o->sprite = &spr_Shapes;
    o->image_speed = 0;
    o->image_index = gm_random(7);
    o->image_alpha = 0.5f;
    o->u.shape.n = 0;
    o->u.shape.rot = gm_random(4) * gm_choose2(1,-1);
    o->direction = 0;
    o->image_xscale = gm_random_range(1,2) * 1.5f;
    o->image_yscale = o->image_xscale;
    o->speed = gm_random_range(7,10) * 2;

    if (gm_irandom(3)==0){ o->x=-128;                 o->y=gm_random(gm_room_height); }
    if (gm_irandom(3)==1){ o->x=gm_room_width+128;    o->y=gm_random(gm_room_height); }
    if (gm_irandom(3)==2){ o->x=gm_random(gm_room_width); o->y=-128; }
    if (gm_irandom(3)==3){ o->x=gm_random(gm_room_width); o->y=gm_room_height+128; }

    o->alarm[0] = (int)gm_floor(gm_random_range(30,120));
}
static void shape_alarm(GMObject* o, int i)
{
    if(i!=0) return;
    o->u.shape.rot = gm_random(4) * gm_choose2(1,-1);
    o->alarm[0] = (int)gm_floor(gm_random_range(30,120));
}
static void shape_step(GMObject* o)
{
    o->u.shape.n += 1;
    o->direction += o->u.shape.rot;
    o->image_angle = o->direction + 5*gm_sin(o->u.shape.n*0.25f);
    o->image_xscale += gm_sin(o->u.shape.n/2.0f)/10.0f;
    o->image_yscale = o->image_xscale;
}
const GMType obj_Shape_type = { "obj_Shape",0, shape_create, shape_step, NULL, shape_alarm, NULL,NULL,NULL,NULL };

// ===================== obj_Par =============================================
static void par_create(GMObject* o)
{
    o->sprite = &spr_Shapes;
    o->image_speed = 0;
    o->image_index = gm_random(7);
    o->image_xscale = 5 + gm_random_range(-1,1);
    o->image_yscale = o->image_xscale;
    o->friction = 0.5f;
    o->image_angle = gm_random(360);
    o->direction = gm_random(360);
    o->speed = 15 + gm_random(2);
}
static void par_step(GMObject* o)
{
    o->image_angle += o->speed;
    o->image_xscale -= 0.1f;
    o->image_yscale = o->image_xscale;
    if(o->image_xscale<=0) gmo_destroy(o);
}
const GMType obj_Par_type = { "obj_Par",0, par_create, par_step, NULL,NULL,NULL,NULL,NULL,NULL };

// ===================== obj_PlayerTrail =====================================
static void trail_create(GMObject* o)
{
    o->sprite = &spr_Player;
    GMObject* p = gmo_first(&obj_Player_type);
    o->image_alpha = 0.1f;
    o->image_index = p ? p->u.player.squatPhase : 0;
    o->image_speed = 0;
    o->image_xscale = p ? p->u.player.scale + p->u.player.wob : 1;
    o->image_yscale = o->image_xscale;
    o->image_angle = p ? p->image_angle : 0;
    o->speed = 15;
}
static void trail_step(GMObject* o)
{
    o->image_alpha -= 0.005f;
    if(o->image_alpha<=0) gmo_destroy(o);
    o->image_angle += 5;
}
const GMType obj_PlayerTrail_type = { "obj_PlayerTrail",0, trail_create, trail_step, NULL,NULL,NULL,NULL,NULL,NULL };

// ===================== obj_Flash ===========================================
static void flash_create(GMObject* o){ o->u.flash.a = 1; }
static void flash_draw(GMObject* o)
{
    o->u.flash.a -= 0.05f;
    if(o->u.flash.a<=0) gmo_destroy(o);
    gmgl_set_projection(0);
    draw_set_alpha(o->u.flash.a);
    draw_set_color(C_WHITE);
    gmgl_rectangle(view_xview,view_yview,view_xview+view_wview,view_yview+view_hview,
                   draw_color,draw_alpha,draw_blend);
    gmgl_set_projection(view_angle);
}
const GMType obj_Flash_type = { "obj_Flash",-100, flash_create, NULL, flash_draw, NULL,NULL,NULL,NULL,NULL };

// ===================== obj_Message =========================================
static const char* MSGS[26] = {
    "DO YOU EVEN LIFT?","PUMP IT!","WOAH KNARLY!","HARDCORE!","FANTASTIC!","AMAZING!",
    "HOLY CHEESECAKE!","DELICIOUS!","MOTHER OF GOD!","DANG!","YOU'RE SMOKIN HOT!","DAT MUSCLE!",
    "BADMAN!","GREAT!","NOT BAD!","EXCELLENT!","WOWEE!","TRIPLE DECKER!","HOTTER THAN  FIRE!",
    "STUPENDOUS!","IMPRESSIVE!","ASTRONOMICAL!","OUT OF THIS WORLD!","OHHHH MYYYY!","OH MY LAWD!",
    "YOU'RE INVINCIBLE!"
};
static void message_create(GMObject* o)
{
    gmo_create(&obj_Flash_type, o->x, o->y);
    GMObject* p = gmo_first(&obj_Player_type);
    if(p) p->u.player.messageSound = true;
    o->u.message.a = 4;
    gmo_set_hv(o, 0, -5);            // vspeed = -5
    o->friction = 0.75f;
    o->image_angle = gm_random_range(-30,30);
    o->u.message.msgno = gm_irandom(25);
    int m = o->u.message.msgno; if(m<0)m=0; if(m>25)m=25;
    o->u.message.msg = MSGS[m];
}
static void message_draw(GMObject* o)
{
    o->u.message.a -= 0.05f;
    if(o->u.message.a<=0) gmo_destroy(o);
    draw_set_halign(FA_CENTER); draw_set_valign(FA_TOP);
    draw_set_alpha(o->u.message.a);
    draw_set_color(C_BLACK);
    gmgl_text_transformed(&spr_Font, o->x+3, o->y-200+3, o->u.message.msg,
                          4+gm_random(0.1f), 4+gm_random(0.1f), o->image_angle+gm_random_range(-5,5));
    draw_set_color(gm_make_color_hsv(gm_random(255),255,255));
    gmgl_text_transformed(&spr_Font, o->x, o->y-200, o->u.message.msg,
                          4+gm_random(0.1f), 4+gm_random(0.1f), o->image_angle+gm_random_range(-5,5));
}
const GMType obj_Message_type = { "obj_Message",-3, message_create, NULL, message_draw, NULL,NULL,NULL,NULL,NULL };

// ===================== obj_MusicControl ====================================
static void music_create(GMObject* o)
{
    (void)o;
    music_bass = SND_BASS1;
    music_lead = SND_LEAD1;
    gm_audio_start_music();
    set_music_base(SND_BASS1, 1);
    music_gate(SND_BASS1,1); music_gate(SND_BASS2,0); music_gate(SND_BASS3,0); music_gate(SND_BASS4,0);
    music_gate(SND_LEAD1,1); music_gate(SND_LEAD1HARMONY,1);
    music_gate(SND_MELODY1,1); music_gate(SND_MELODY2,1);
    music_gate(SND_BASSDRUM,1); music_gate(SND_PERCUSSION,1);
    music_gate(SND_HIHATDOUBLE,1); music_gate(SND_TOM,1);
}
const GMType obj_MusicControl_type = { "obj_MusicControl",0, music_create, NULL,NULL,NULL,NULL,NULL,NULL,NULL };

// per-loop randomization (fires on audio cursor wrap). See Unity obj_MusicControl.
void obj_music_loop(void)
{
    GMObject* p = gmo_first(&obj_Player_type);
    float squats = p ? p->u.player.squatCount : 0;

    int prevBass = music_bass;
    music_bass = (squats>=250) ? (SND_BASS1 + gm_irandom(3)) : SND_BASS1;
    music_lead = SND_LEAD1;

    music_gate(SND_BASS1, music_bass==SND_BASS1?1:0);
    music_gate(SND_BASS2, music_bass==SND_BASS2?1:0);
    music_gate(SND_BASS3, music_bass==SND_BASS3?1:0);
    music_gate(SND_BASS4, music_bass==SND_BASS4?1:0);
    if(music_bass!=prevBass) set_music_base(music_bass, music_base(prevBass));

    music_gate(SND_LEAD1,        gm_floor(gm_random(5))!=0 ?1:0);  // 4/5
    music_gate(SND_LEAD1HARMONY, gm_floor(gm_random(3))!=0 ?1:0);  // 2/3
    music_gate(SND_HIHATDOUBLE,  gm_floor(gm_random(4))==0 ?1:0);  // 1/4
    music_gate(SND_TOM,          gm_floor(gm_random(3))==0 ?1:0);  // 1/3
}

// ===================== obj_Player ==========================================
#define PL(o) ((o)->u.player)
static void player_create(GMObject* o)
{
    o->sprite = &spr_Player;
    PL(o).messageSound=false; PL(o).n=0; PL(o).squatCount=0; PL(o).squatPhase=0;
    o->image_alpha=0; PL(o).tutMsg=false;
    PL(o).scale=1; PL(o).scaleTarget=1; PL(o).scaleTarget2=1;
    PL(o).viewMax=0; PL(o).viewSpeed=1; PL(o).angleAdd=0;
    PL(o).targetAngle=0; PL(o).viewAngle=0; PL(o).targetViewAngle=0;
    o->alarm[0]=95; o->alarm[1]=11; o->alarm[2]=20; o->alarm[4]=48;
    PL(o).beatBlip=false; PL(o).a=0; PL(o).canPush=true; PL(o).msg=0;
}
static void player_alarm(GMObject* o, int i)
{
    switch(i){
    case 4:
        if(PL(o).messageSound){ sound_play(SND_MESSAGE); PL(o).messageSound=false; }
        o->alarm[4]=48; break;
    case 3:
        PL(o).canPush=true; break;
    case 2:
        o->alarm[2]=24;
        if(PL(o).a==0){ PL(o).scale=PL(o).scaleTarget2;     o->image_angle+=gm_choose2(PL(o).angleAdd,-PL(o).angleAdd); }
        else          { PL(o).scale=PL(o).scaleTarget2*2;   o->image_angle+=gm_choose2(PL(o).angleAdd*3,-PL(o).angleAdd*3); }
        break;
    case 1:
        if(PL(o).beatBlip){
            if(PL(o).squatPhase==0){
                if(PL(o).a==1) for(int k=0;k<8;k++) gmo_create(&obj_Par_type,o->x,o->y);
                sound_play(SND_BLIP2);
            } else sound_play(SND_BLIP1);
            PL(o).beatBlip=false;
        }
        o->alarm[1]=12; break;
    case 0:
        o->alarm[0]=(int)gm_choose3(24,48,96); break;
    }
}
static void player_step(GMObject* o)
{
    GMObject* ctrl = gmo_first(&obj_Control_type);
    int gameMode = ctrl ? ctrl->u.control.gameMode : 2;

    PL(o).n += 1;
    if(gameMode==2){
        if(PL(o).squatCount==0 && !PL(o).tutMsg) PL(o).tutMsg=true;
        o->image_alpha += (1-o->image_alpha)/10.0f;
        if(gm_key_pressed(VK_DOWN))
            if(PL(o).squatPhase==0 && PL(o).canPush){ PL(o).squatPhase=1; PL(o).beatBlip=true; PL(o).canPush=false; o->alarm[3]=5; }
        if(gm_key_pressed(VK_UP))
            if(PL(o).squatPhase==1 && PL(o).canPush){ PL(o).msg=0; PL(o).squatCount+=1; PL(o).squatPhase=0; PL(o).beatBlip=true; PL(o).canPush=false; o->alarm[3]=5; }
    }
    if(gameMode==1) o->image_alpha += (0.25f-o->image_alpha)/10.0f;
    if(gameMode==3) o->image_alpha += (0-o->image_alpha)/10.0f;

    PL(o).scale += (PL(o).scaleTarget-PL(o).scale)/5.0f;
    o->image_angle   += gm_sin(gm_degtorad((PL(o).targetAngle    +(o->speed*2*gm_sin(PL(o).n/5.0f)))-o->image_angle))*8;
    PL(o).viewAngle  += gm_sin(gm_degtorad((PL(o).targetViewAngle+(o->speed*2*gm_sin(PL(o).n/5.0f)))-PL(o).viewAngle))*8;
    view_angle = PL(o).viewAngle + PL(o).viewMax*gm_sin(PL(o).n/25.0f);

    if(PL(o).a==1){
        //GMObject* trail = gmo_create(&obj_PlayerTrail_type,o->x,o->y);
        //trail->direction = gm_repeat(PL(o).n,360);
    }

    PL(o).viewSpeed = 50-(PL(o).squatCount/20.0f);
    if(PL(o).a==0) PL(o).viewMax=PL(o).squatCount/50.0f; else PL(o).viewMax=PL(o).squatCount/10.0f;
    PL(o).angleAdd = PL(o).squatCount/100.0f;
    PL(o).scaleTarget2 = 1 + PL(o).squatCount/1000.0f;

    if(PL(o).squatCount>=50 && PL(o).squatCount<250) sound_volume(SND_MELODY1,1);
    if(PL(o).squatCount>=100) sound_volume(SND_BASSDRUM,1);
    if(PL(o).squatCount>=150) sound_volume(SND_PERCUSSION,1);
    if(PL(o).squatCount>=200 && PL(o).squatCount<250) sound_volume(music_bass,0);
    if(PL(o).squatCount>=250){
        sound_volume(music_bass,1); sound_volume(music_lead,1);
        sound_volume(SND_LEAD1HARMONY,1); sound_volume(SND_HIHATDOUBLE,1);
        sound_volume(SND_TOM,1); sound_volume(SND_MELODY2,1);
        if(PL(o).a==0){
            gmo_create(&obj_Flash_type,o->x,o->y);
            for(int k=0;k<25;k++) gmo_create(&obj_Shape_type,0,0);
            PL(o).a=1;
        }
    }
    if(fmodf(PL(o).squatCount,25.0f)==0 && PL(o).a==1 && PL(o).msg==0){
        PL(o).msg=1;
        gmo_create(&obj_Message_type,o->x,o->y);
    }
}
static void player_draw(GMObject* o)
{
    draw_set_alpha(o->image_alpha);
    draw_set_color(PL(o).a==0 ? C_WHITE : gm_make_color_hsv(PL(o).n,255,200));

    draw_set_depth3d(3.0f);   // backdrop: recedes furthest into the screen
    gmgl_set_projection(0);
    gmgl_rectangle(view_xview,view_yview,view_wview,view_hview, draw_color,draw_alpha,BM_NORMAL);
    gmgl_sprite_stretched(&spr_Gradient,0, view_xview,view_yview,view_wview,view_hview,
                          C_WHITE,draw_alpha,BM_ADD);
    gmgl_set_projection(view_angle);

    draw_set_depth3d(1.5f);   // swirl rings: mid-layer, between backdrop and player
    for(int s=0;s<8;s++)
        gmgl_sprite_ext(&spr_Swirl,0,o->x,o->y,2,2, PL(o).n*3+s*45, C_WHITE, PL(o).a*o->image_alpha, BM_NORMAL);
    GMColor hsv = gm_make_color_hsv(127+PL(o).n,255,255);
    for(int s=0;s<8;s++)
        gmgl_sprite_ext(&spr_Swirl,0,o->x,o->y,2,2, PL(o).n*6+s*45, hsv, PL(o).a*o->image_alpha, BM_NORMAL);

    draw_set_depth3d(-0.5f);  // player: pulled slightly toward the viewer
    PL(o).wob = (PL(o).a==0) ? 0 : 1.5f + gm_sin(PL(o).n/5.0f)/4.0f;
    gmgl_sprite_ext(&spr_Player,(int)PL(o).squatPhase,o->x,o->y,
                    PL(o).scale+PL(o).wob, PL(o).scale+PL(o).wob, o->image_angle, C_WHITE, o->image_alpha, BM_NORMAL);

    if(PL(o).tutMsg && PL(o).squatCount==0 && PL(o).squatPhase==0){
        draw_set_depth3d(-0.5f);
        draw_set_color(C_BLACK); draw_set_halign(FA_CENTER); draw_set_valign(FA_TOP);
        gmgl_text_transformed(&spr_Font, o->x, o->y-200, "SQUAT!", 4,4,0);
    }
    draw_set_depth3d(0.0f);
}
const GMType obj_Player_type = { "obj_Player",0, player_create, player_step, player_draw, player_alarm, NULL,NULL,NULL,NULL };

// ===================== obj_Control =========================================
#define CT(o) ((o)->u.control)
static void control_create(GMObject* o)
{
    CT(o).gameMode = 2;                              // GameJolt dropped -> straight to play
    gmo_create(&obj_MusicControl_type,0,0);
    CT(o).quit=false; CT(o).mouseButton=false; CT(o).mouseButtonPress=false;
    CT(o).hud=C_BLACK;
}
static void control_alarm(GMObject* o, int i){ (void)o; if(i==0) game_end(); }
static void control_key_escape(GMObject* o)
{
    if(CT(o).quit) return;
    CT(o).quit=true;
    CT(o).gameMode=3;
    o->alarm[0]=120;
}
static void control_mouse_pressed(GMObject* o){ CT(o).mouseButtonPress=true; CT(o).mouseButton=true; }
static void control_mouse_released(GMObject* o){ CT(o).mouseButton=false; }
static void control_draw(GMObject* o)
{
    GMObject* p = gmo_first(&obj_Player_type);
    float playerA = p ? p->u.player.a : 0;

    draw_set_depth3d(0.0f);   // HUD text sits at the screen plane, never popped/receded
    draw_set_alpha(1);
    gmgl_set_projection(0);
    if(playerA==1) CT(o).hud = gm_make_color_hsv(gm_random(255),255,255);
    draw_set_color(CT(o).hud);

    if(CT(o).gameMode==2){
        draw_set_halign(FA_LEFT); draw_set_valign(FA_TOP);
        int sc = p ? (int)p->u.player.squatCount : 0;
        char num[24]; snprintf(num,sizeof num,"#%d",sc);
        if(playerA==1){
            draw_set_color(C_BLACK); draw_set_alpha(1);
            gmgl_text_transformed(&spr_Font,48+2,64+2,"SQUATS ",2,2,0);
            gmgl_text_transformed(&spr_Font,48+2,64+2,num,4,4,0);
        }
        draw_set_color(CT(o).hud);
        gmgl_text_transformed(&spr_Font,48,64,"SQUATS ",2,2,0);
        gmgl_text_transformed(&spr_Font,48,64,num,4,4,0);
    }
    gmgl_set_projection(view_angle);
}
const GMType obj_Control_type = { "obj_Control",-100, control_create, NULL, control_draw, control_alarm, NULL,
                                  control_key_escape, control_mouse_pressed, control_mouse_released };
