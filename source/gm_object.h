// Base GM object. One struct instance == one GM object; behaviour via a GMType vtable.
#pragma once
#include "gm.h"
#include "gm_assets.h"

typedef struct GMObject GMObject;

typedef struct {
    const char* name;
    int  depth_default;
    void (*create)(GMObject*);
    void (*step)(GMObject*);
    void (*draw)(GMObject*);            // NULL => default sprite draw
    void (*alarm)(GMObject*, int);
    void (*other0)(GMObject*);          // Outside Room
    void (*key_escape)(GMObject*);
    void (*mouse_pressed)(GMObject*);
    void (*mouse_released)(GMObject*);
} GMType;

struct GMObject {
    const GMType* type;
    bool active;
    bool destroyed;
    int  createOrder;                   // registry tie-break for depth sort

    // transform (GM room space, y-down)
    float x, y, xprev, yprev, xstart, ystart;
    float speed, direction, friction, gravity, gravity_direction;

    // image state
    const GMSprite* sprite;             // NULL => no sprite
    float image_index, image_speed, image_alpha, image_xscale, image_yscale, image_angle;
    GMColor image_blend;
    int   depth; bool visible;

    int alarm[12];

    // per-object fields
    union {
        struct { float n, rot; } shape;
        struct {
            bool  messageSound;
            float n, squatCount, squatPhase;
            bool  tutMsg;
            float scale, scaleTarget, scaleTarget2;
            float viewMax, viewSpeed, angleAdd, targetAngle, viewAngle, targetViewAngle;
            bool  beatBlip;
            float a;
            bool  canPush;
            float msg, wob;
        } player;
        struct { int bass, lead; } music;
        struct { float a; } flash;
        struct { float a; int msgno; const char* msg; } message;
        struct { bool mouseButton, mouseButtonPress; int gameMode; bool quit; GMColor hud; } control;
    } u;
};

// derived motion props
static inline float gmo_hspeed(const GMObject* o){ return o->speed * gm_dcos(o->direction); }
static inline float gmo_vspeed(const GMObject* o){ return -o->speed * gm_dsin(o->direction); }
void gmo_set_hv(GMObject* o, float h, float v);

// registry / lifecycle
void      gmo_reset(void);
GMObject* gmo_create(const GMType* type, float x, float y);   // runs Create immediately
void      gmo_destroy(GMObject* o);
GMObject* gmo_first(const GMType* type);
int       gmo_count(const GMType* type);

void gmo_step_all(void);                    // one fixed step for every instance
void gmo_dispatch_key_escape(void);
void gmo_dispatch_mouse(bool pressed, bool released);
void gmo_draw_all(void);                    // depth-sorted draw pass
