#include <switch.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glad/glad.h>
#include <stdio.h>
#include <stdlib.h>

#include "gm.h"
#include "gm_gl.h"
#include "gm_assets.h"
#include "gm_audio.h"
#include "gm_object.h"
#include "objects.h"

static EGLDisplay s_display = EGL_NO_DISPLAY;
static EGLContext s_context = EGL_NO_CONTEXT;
static EGLSurface s_surface = EGL_NO_SURFACE;

static bool initEgl(NWindow* win)
{
    s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!s_display) return false;
    eglInitialize(s_display, NULL, NULL);
    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) goto fail1;

    EGLConfig config; EGLint numConfigs;
    static const EGLint attr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE
    };
    eglChooseConfig(s_display, attr, &config, 1, &numConfigs);
    if (numConfigs == 0) goto fail1;

    s_surface = eglCreateWindowSurface(s_display, config, win, NULL);
    if (!s_surface) goto fail1;

    static const EGLint ctxattr[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
        EGL_CONTEXT_MAJOR_VERSION_KHR, 3, EGL_CONTEXT_MINOR_VERSION_KHR, 3, EGL_NONE
    };
    s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctxattr);
    if (!s_context) goto fail2;

    eglMakeCurrent(s_display, s_surface, s_surface, s_context);
    return true;
fail2:
    eglDestroySurface(s_display, s_surface); s_surface = EGL_NO_SURFACE;
fail1:
    eglTerminate(s_display); s_display = EGL_NO_DISPLAY;
    return false;
}

static void deinitEgl(void)
{
    if (s_display) {
        eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s_context) eglDestroyContext(s_display, s_context);
        if (s_surface) eglDestroySurface(s_display, s_surface);
        eglTerminate(s_display);
    }
}

// ---- input bridge (frame == step, so a single edge read is enough) --------
static u64 s_edges;
bool gm_key_pressed(int vk)
{
    switch (vk) {
        case VK_DOWN:   return (s_edges & HidNpadButton_Down) != 0;
        case VK_UP:     return (s_edges & HidNpadButton_Up)   != 0;
        case VK_ESCAPE: return (s_edges & HidNpadButton_Plus) != 0;
    }
    return false;
}

int main(int argc, char** argv)
{
    (void)argc; (void)argv;
    romfsInit();

    freopen("sdmc:/SuperSquatSim_log.txt", "w", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);   // unbuffered so we see output even if it later hangs/crashes
    printf("=== boot ===\n");

    if (!initEgl(nwindowGetDefault())) { printf("initEgl FAILED\n"); romfsExit(); return EXIT_FAILURE; }
    printf("egl ok, display=%p surface=%p context=%p\n", (void*)s_display, (void*)s_surface, (void*)s_context);

    int gladok = gladLoadGL();
    printf("gladLoadGL = %d, GL_VERSION=%s, GL_RENDERER=%s\n", gladok,
           (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));

    eglSwapInterval(s_display, 1);      // vsync -> 60Hz -> one step per frame

    gm_init(armGetSystemTick());
    gmgl_init();
    printf("gmgl_init done, glGetError=0x%x\n", glGetError());
    gm_assets_load();
    printf("assets loaded, glGetError=0x%x\n", glGetError());
    gm_audio_init();
    printf("audio init done\n");
    gm_audio_on_loop(obj_music_loop);
    printf("audio loop callback set\n");

    // boot the room: obj_Control (forces gameMode=2, spawns music) + obj_Player
    gmo_reset();
    printf("gmo_reset done\n");
    gmo_create(&obj_Control_type, 0, 0);
    printf("obj_Control created\n");
    gmo_create(&obj_Player_type, gm_room_width / 2.0f, gm_room_height / 2.0f);
    printf("obj_Player created\n");

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    printf("pad init done, entering main loop\n");

    bool firstFrame = true;

    while (appletMainLoop() && !g_quit) {
        padUpdate(&pad);
        s_edges = padGetButtonsDown(&pad);

        if (s_edges & HidNpadButton_Plus) gmo_dispatch_key_escape();
        gmo_step_all();

        // pillarbox the 4:3 room inside the 16:9 surface
        EGLint w = 1280, h = 720;
        EGLint qw = 0, qh = 0;
        eglQuerySurface(s_display, s_surface, EGL_WIDTH,  &qw);
        eglQuerySurface(s_display, s_surface, EGL_HEIGHT, &qh);
        if (qw > 0 && qh > 0) { w = qw; h = qh; }   // driver returned 0/0 on this hardware; fall back to defaults
        int vw = h * 4 / 3, vh = h;
        if (vw > w) { vw = w; vh = w * 3 / 4; }
        glViewport((w - vw) / 2, (h - vh) / 2, vw, vh);

        if (firstFrame) printf("first frame: surface w=%d h=%d -> viewport %d,%d %dx%d\n",
                                w, h, (w - vw) / 2, (h - vh) / 2, vw, vh);

        gmgl_frame_begin();
        gmo_draw_all();
        gmgl_frame_end();

        eglSwapBuffers(s_display, s_surface);

        if (firstFrame) {
            printf("first frame: post-swap glGetError=0x%x\n", glGetError());
            firstFrame = false;
        }
    }

    gm_audio_exit();
    deinitEgl();
    romfsExit();
    return 0;
}
