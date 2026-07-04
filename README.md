# Super Squat Simulator — Nintendo Switch (devkitPro / libnx) port

A faithful C re-implementation of Nik Sudan's GameMaker 8.1 *Super Squat Simulator*
for **homebrew (CFW) Switch**, built on devkitA64 + libnx + switch-mesa (OpenGL via EGL).
GameJolt is stripped and `gameMode` is forced to `2`, so it drops straight into play.

This is a from-scratch port of the runtime, mirroring the Unity C# port 1:1 — same GM
semantics (60 Hz lockstep, room-space y-down, rotating view projection, alarm model,
per-bar music randomization). It is **not** a repackaging of the Unity/C# build; libnx
cannot run C#.

Controls: **D-pad Down = crouch, D-pad Up = stand** (each Up = +1 squat). **+ = quit.**

---

## Build

### 1. Install the toolchain + portlibs
Install devkitPro (pacman), then the Switch packages this port links against:

```
sudo dkp-pacman -S switch-dev switch-glad switch-mesa
```

`switch-mesa` provides OpenGL over EGL; `switch-glad` provides the `-lglad` loader and
`<glad/glad.h>`. (`switch-dev` pulls in libnx + devkitA64.)

### 2. Build
```
cd SuperSquatSwitch
make
```
Produces `SuperSquatSim.nro`.

### 3. Install on the console
Copy `SuperSquatSim.nro` to your SD card under `/switch/` and launch it from the
homebrew menu (hbmenu) on Atmosphère/CFW.

---

## Layout

```
Makefile              standard devkitPro switch template; ROMFS enabled,
                      LIBS = -lglad -lEGL -lglapi -ldrm_nouveau -lnx -lm
romfs/gfx/*.png       sprite strips (loaded at runtime via stb_image -> GL textures)
romfs/sfx/*.raw       audio, pre-resampled to 48 kHz stereo s16 headerless PCM
source/
  gm.[ch]             RNG (xorshift), GM math, HSV, view + draw state, constants
  gm_gl.[ch]          immediate-mode renderer over OpenGL core (VBO batcher + shader)
  gm_assets.[ch]      texture + audio loading, sprite strip slicing, sound ids
  gm_audio.[ch]       audout software mixer (threaded)
  gm_object.[ch]      base object: motion, 12 alarms, registry, depth-sorted draw
  objects.[ch]        the 8 objects (Shape, Par, PlayerTrail, Flash, Message,
                      MusicControl, Player, Control)
  main.c              EGL/GL init, romfs, world boot, 60 Hz vsync loop
  stb_image.h         PNG loader (public domain)
```

## Design notes / knobs

- **Frame == step.** `eglSwapInterval(1)` locks to 60 Hz, and one iteration of the main
  loop is exactly one GM step. No accumulator and no input edge-buffering are needed
  (unlike the Unity port), because the console's vsync lockstep *is* GM's model. Input is
  a single `padGetButtonsDown` read per frame.

- **Audio is a software mixer over `audout`** (fixed 48 kHz stereo). All 13 music stems
  share **one playback cursor**, so they are inherently phase-locked; the per-bar
  randomization (`obj_music_loop`) fires exactly when the cursor wraps at 307200 frames
  (= 6.4 s), so it can never drift against the beat. Stems were resampled to 48 kHz at
  build time (via ffmpeg) specifically so the runtime mixer needs no resampling.
  Volume model is `base * gate`: `sound_volume()` sets each stem's unlock volume
  (obj_Player's job), `music_gate()` sets the per-loop on/off (obj_MusicControl's job).

- **Rendering** is immediate-mode, matching the GML Draw events: a small VBO batcher
  with a GLSL 3.30 tinted-textured-quad shader, flushing on texture/blend/projection
  change. `view_angle` rotates the whole world via the projection matrix; the HUD,
  background rect and additive gradient are drawn under projection angle 0, exactly like
  the original `d3d_set_projection_ortho(...,0)` split. The 4:3 room is pillarboxed
  inside the 16:9 surface with `glViewport`.

- **Orientation knobs.** If rotation ends up mirrored, flip `GM_VIEW_ANGLE_SIGN` /
  `GM_SPRITE_ANGLE_SIGN` (top of `gm_gl.c`). Swirl pivot lives in `gm_assets.c`
  (`make_sprite("spr_Swirl", 336, 168, 32)`).

- **obj_PlayerTrail** is never spawned — the trail-spawn code is kept commented in
  `obj_Player` (`u.player.a==1` branch), matching the original GML.

- **Known benign race:** `obj_music_loop` runs on the audio thread (it fires on sample
  wrap). It reads the player's `squatCount` and calls the shared RNG, which the main
  thread also uses. Worst case is a slightly stale squat count or a one-off gate value
  for a single 6.4 s loop — purely cosmetic, never a crash. The SFX voice pool is
  mutex-guarded; music `base`/`gate` are lock-free floats.

## Validation

The portable game logic (`gm.c`, `gm_object.c`, `objects.c`) was compiled clean under
`gcc -std=c11 -Wall -Wextra` against host stubs and run for 40k steps: the squat mechanic,
alarm debounce, ascension at 250 (rainbow mode + 25 background shapes), message/flash
spawning and reaping, and the music roll all behave. The platform layer
(`main.c`, `gm_gl.c`, `gm_assets.c`, `gm_audio.c`) follows the official switch-examples
conventions (EGL/GL init from the `lenny` example, audout + threaded mixer, modern pad
input) but must be compiled with the devkitPro toolchain on your machine — it can't be
built in this environment.
