#include <stdlib.h>
#include <string.h>
#include "gm_object.h"
#include "gm_gl.h"

#define MAXOBJ 1024
static GMObject pool[MAXOBJ];
static int      order;

void gmo_set_hv(GMObject* o, float h, float v)
{
    o->speed = sqrtf(h*h + v*v);
    if (h != 0.0f || v != 0.0f)
        o->direction = gm_repeat(atan2f(-v, h) * 180.0f / GM_PI, 360.0f);
}

void gmo_reset(void){ memset(pool,0,sizeof pool); order=0; }

GMObject* gmo_create(const GMType* type, float x, float y)
{
    GMObject* o = NULL;
    for (int i=0;i<MAXOBJ;i++) if(!pool[i].active){ o=&pool[i]; break; }
    if(!o) return NULL;
    memset(o,0,sizeof *o);
    o->type=type; o->active=true; o->createOrder=order++;
    o->x=x; o->y=y; o->xstart=x; o->ystart=y;
    o->image_alpha=1; o->image_xscale=1; o->image_yscale=1; o->image_blend=C_WHITE;
    o->visible=true; o->gravity_direction=270;
    o->depth = type->depth_default;
    for(int i=0;i<12;i++) o->alarm[i]=-1;
    if(type->create) type->create(o);
    return o;
}

void gmo_destroy(GMObject* o){ o->destroyed=true; }

GMObject* gmo_first(const GMType* type)
{
    for(int i=0;i<MAXOBJ;i++) if(pool[i].active && pool[i].type==type) return &pool[i];
    return NULL;
}
int gmo_count(const GMType* type)
{
    int n=0; for(int i=0;i<MAXOBJ;i++) if(pool[i].active && pool[i].type==type) n++; return n;
}

static void step_one(GMObject* o)
{
    o->xprev=o->x; o->yprev=o->y;

    for(int i=0;i<12;i++)
        if(o->alarm[i]>0){ o->alarm[i]--; if(o->alarm[i]==0){ o->alarm[i]=-1; if(o->type->alarm) o->type->alarm(o,i); } }
    if(o->destroyed){ o->active=false; return; }

    if(o->type->step) o->type->step(o);
    if(o->destroyed){ o->active=false; return; }

    // built-in motion: friction, gravity, move
    if(o->speed>0){ o->speed-=o->friction; if(o->speed<0) o->speed=0; }
    else if(o->speed<0){ o->speed+=o->friction; if(o->speed>0) o->speed=0; }
    if(o->gravity!=0)
        gmo_set_hv(o, gmo_hspeed(o)+o->gravity*gm_dcos(o->gravity_direction),
                      gmo_vspeed(o)-o->gravity*gm_dsin(o->gravity_direction));
    o->x += gmo_hspeed(o); o->y += gmo_vspeed(o);

    if(o->sprite && o->image_speed!=0)
        o->image_index = gm_repeat(o->image_index + o->image_speed, o->sprite->frames);

    if(o->x<-64 || o->x>gm_room_width+64 || o->y<-64 || o->y>gm_room_height+64)
        if(o->type->other0) o->type->other0(o);
    if(o->destroyed) o->active=false;
}

void gmo_step_all(void)
{
    // Reap anything destroyed since last step (incl. draw-time destroys: Flash/Message),
    // then step the survivors. step_one also reaps mid-step self-destructs (e.g. Par).
    for(int i=0;i<MAXOBJ;i++){
        if(!pool[i].active) continue;
        if(pool[i].destroyed){ pool[i].active=false; continue; }
        step_one(&pool[i]);
    }
}

void gmo_dispatch_key_escape(void)
{
    for(int i=0;i<MAXOBJ;i++) if(pool[i].active && pool[i].type->key_escape) pool[i].type->key_escape(&pool[i]);
}
void gmo_dispatch_mouse(bool pressed, bool released)
{
    for(int i=0;i<MAXOBJ;i++){
        if(!pool[i].active) continue;
        if(pressed  && pool[i].type->mouse_pressed)  pool[i].type->mouse_pressed(&pool[i]);
        if(released && pool[i].type->mouse_released) pool[i].type->mouse_released(&pool[i]);
    }
}

// depth-sorted draw (higher depth first = behind; tie-break by create order)
static int cmp(const void* a, const void* b)
{
    const GMObject* oa=*(const GMObject* const*)a;
    const GMObject* ob=*(const GMObject* const*)b;
    if(oa->depth != ob->depth) return ob->depth - oa->depth;   // higher first
    return oa->createOrder - ob->createOrder;
}

void gmo_draw_all(void)
{
    GMObject* list[MAXOBJ]; int n=0;
    for(int i=0;i<MAXOBJ;i++) if(pool[i].active) list[n++]=&pool[i];
    qsort(list,n,sizeof(GMObject*),cmp);

    gmgl_set_projection(view_angle);            // baseline
    for(int i=0;i<n;i++){
        GMObject* o=list[i];
        if(!o->visible) continue;
        if(o->type->draw) o->type->draw(o);
        else if(o->sprite)
            gmgl_sprite_default(o->sprite,o->image_index,o->x,o->y,
                                o->image_xscale,o->image_yscale,o->image_angle,o->image_blend,o->image_alpha);
    }
}
