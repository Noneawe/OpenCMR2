# NOTES — SBS (Cardboard) work on OpenCMR2

## Stage 0 — baseline

Base commit: `6e201df71a1c347b2101bb11f54ddc422756cc52` ("Close saved games before replacing them on Windows").
`UPSTREAM` (CMR2Decomp): `47667cb07964f0cbe1fdee46fbbfa26b8cb28e6e`.

### Tools (desktop, verified)
- clang 21.1.8 (Ubuntu), gcc-15, cmake 4.2.3, ninja, glslang
- JDK 25 (project needs 17+)
- SDL3: vendored via FetchContent, tag `release-3.4.18`

### Linux build (verified: desktop)
```
cmake --preset linux -DSDL_X11_XTEST=OFF
cmake --build --preset linux
ctest --preset linux
```
`-DSDL_X11_XTEST=OFF` is needed only because the XTEST dev package is not installed
(`libxtst-dev` would remove the need). Desktop only; does not affect Android.
Result: 25/25 tests pass, `ffb_wheel` skipped.

### Android (debug build verified: desktop only, not run on a phone)
- Gradle: `android/app/build.gradle.kts` — compileSdk 37, targetSdk 37, minSdk 29, ndkVersion `30.0.16248370`.
- SDK at `~/Android/Sdk` (`ANDROID_HOME` not set, no `android/local.properties`).
  Installed: platforms 34/35/36/36.1, build-tools up to 37.0.0, NDK 27.2 and 28.2, cmake 3.22.1.
  Missing: platform 37, NDK 30.0.16248370 (Gradle should download if licenses accepted).
- Build: `cd android && ANDROID_HOME=~/Android/Sdk ./gradlew assembleDebug` — BUILD SUCCESSFUL (4m44s, Gradle 9.8.1,
  downloaded missing NDK 30 / platform 37). APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
- Cutout: added `android/app/src/main/res/values-v28/themes.xml` overriding `Theme.OpenCMR2.Game` with
  `windowLayoutInDisplayCutoutMode=shortEdges`. Builds. NOT verified that window size == screen size:
  the game activity needs the original game files, which the emulator does not have. TODO check on phone
  (log window size vs `wm size`).
- Emulator smoke test (emulator-5554, Android 17, 1080x2400, swiftshader): APK installs, SetupActivity
  starts and shows "Welcome to OpenCMR2" with no crash in logcat. Game/Vulkan path not exercised.
  Note: a physical phone was also on adb (wireless); use `adb -s emulator-5554`.
- 16 KB pages: verified in debug APK. All `.so` (arm64-v8a, x86_64) have LOAD align `0x4000`; `zipalign -c -P 16 4` OK.
  Not tested on a 16 KB device.

## Stage 1 — renderer recon (read from code, commit a25b6d0 + cutout; nothing run yet)

### Where things are
- **Screen size the game sees.** `game/Graphics.cpp:~380-395`: `g_pGraphics->resX/resY` come from the saved mode
  or `Gfx_GetDefaultDisplayMode()` (`src/render/gfx_core.cpp`, `LandscapeDesktopSize`/`BuildModes`), then
  `Gfx_SetVideoMode(resX, resY, fullscreen)`. Everything else (viewports, HUD, `View_SetPlayerProjection`
  aspect in `game/FixedPoint.cpp:760`) derives from `resX/resY`. So the game can be shown one eye's size
  by changing what the mode list/default returns and what the executor allocates for the back buffer.
- **Back buffer is already offscreen.** `src/render/gfx_gpu.cpp` `SetVideoMode` creates `backColor`/`backDepth`/`backdrop`
  textures; `Execute()` renders into `backColor`, then `SDL_BlitGPUTexture` to the swapchain with
  `GetPresentationRect()` (`src/render/presentation.h`; letterbox, or full when `stretch`). This is the hook
  for per-eye presentation, lens mesh, shift, margin.
- **Record/replay already exists.** `src/render/gfx_core.cpp` records every `Gfx_*` call into a `GfxFrame`
  (`src/render/gfx_internal.h`: `commands` = DRAW/CLEAR/TARGET/BACKDROP, `states` = full `GfxDrawState` per draw,
  incl. world/view/projection matrices, viewport, `screenW/H`). `Gfx_Present` hands the frame to a `GfxExecutor`
  (`gfx_gpu.cpp` SDL_GPU, `gfx_gl.cpp` OpenGL 3.3, null). Nothing from the game is re-run to draw twice.
- **Transforms.** `Gfx_SetTransform` (`gfx_core.cpp:530`) just stores into `s_state`. Game callers: projection only in
  `CGraphics::SetProjection` (`game/Graphics.cpp:1632-1687`; called per frame/view and mid-frame to change
  near/far, `game/Game.cpp:2754-2756`; D3D row-major, `_34=1`, `_43` etc.), view in `game/SceneNode.cpp:2478`
  (camera) and cube-map face code (`game/Graphics.cpp:2490`, saved/restored 2591). World per object.
  Because the projection/view live per recorded draw state, the executor can alter them at replay.
- **2D.** HUD/menus/sprites/sky layers are `Gfx_DrawPrimitiveTL` (`GfxCommand::transformed`, `GfxTLVertex`).
  Executor draws them with a full-target viewport (`gfx_gpu.cpp`, `if (c.transformed) vp = ...`).
  Menu scaling uses `Gfx_SetVirtualScreen` (`game/Sprite.cpp:1165-1290`, `MenuScreen_Begin/End`), mapped by
  `ToTarget()` in the executor. Sky/backdrop is `Tri2D_DrawLayer(4)` = 2D, i.e. zero parallax (fine, infinity).
- **Frame structure (race).** `Game_DrawSceneViewport` (`game/Game.cpp:~2670`) per viewport (split screen = several):
  `Gfx_SetViewport(pRect)`, 2D layer 4, `Gfx_CaptureBackdrop` (distance fade), 3D, particles, then
  viewport reset to full screen and 2D layers 3/2/... Present: `Graphics_PresentFrameAndResetCounters`
  (`game/Graphics.cpp:2307`) -> `Gfx_Present`. Car reflection cube faces are drawn first into render-target
  textures (`Graphics_RenderNodeCubeMapFaces`, `game/Graphics.cpp:~2420-2595`); executor runs those in phase 0.
- **Movies.** `src/video/movie.cpp:Movie_Present`: `Gfx_PushState`, one `GfxTLVertex` quad, `Gfx_Present` or
  `Gfx_PresentStretched` (when `video.stretch_movies`), `Gfx_PopState`. Boot video: `Game.cpp:~949`.
- **Other back-buffer users.** `Gfx_ReadBackBuffer` (lens-flare visibility, `game/Graphics.cpp:~3917`, reads the previous
  presented frame at game pixel coords); `Gfx_CaptureBackdrop` copies the whole back buffer to `backdrop`.
- Options read with `Sys_GetOption("video.*")` (`src/port/sys.h`); no `[vr]` section yet.

### Decision (proposal): replay the recorded frame twice inside the executor
Not "double call the game's draw" (game draw has side effects: particles, flare sampling, counters) and not a new
recorder (exists). Plan:
1. Game sees eye-sized screen (`resX/resY` = eye W x H); executor allocates `backColor` 2W x H.
2. `Execute()` runs the back-buffer commands twice (left, right): viewport/scissor/clear rectangles offset by
   `eye * W` (extend `ToTarget`), `backdrop` copy limited to the eye rectangle, flare readback uses left eye.
3. For the right/left eye only non-`transformed`, `target == NULL` draws with a perspective projection (`_34 == 1`)
   get `view' = view * T(-+eyeShift)` and off-axis shear on `projection` (`_31`); 2D (`transformed`), cube faces
   and orthographic draws are unchanged => HUD/menu/movies have zero parallax automatically.
4. Present: blit each eye rectangle to its half of the swapchain via a lens-distortion mesh (SDL_GPU), with
   shift/margin/render_scale. `Gfx_PresentStretched` (movies) uses the same path with no parallax.
5. Stage 3 ("cinema") is the same path with both eyes identical (no view change).
Game logic runs once per frame (nothing in `game/` re-executed). Expected `game/` changes: none.

### Risks / open points
- `BeginPass` merges leading clears (`cleared` counter) and the cube-face phase split; replaying twice in one
  command buffer needs the depth clear per eye (full-screen clear command becomes two eye-rect clears).
- Mid-frame `SetProjection(0,0,...)` changes near/far only; the shear must be applied per draw state, not once.
- GL executor (`gfx_gl.cpp`) would need the same work; scope SBS to SDL_GPU (Vulkan, used on Android), GL stays flat.
- World units of CMR2 (16.16 fixed point; metres not confirmed) needed for `separation`/`convergence` — Stage 4.
- Billboards/particles are oriented by the game from the single camera; slight mismatch per eye expected.
- Split-screen (several viewports) and `CGameInfo::IsSplitBarEnabled()` need a check in VR (disable?).
- Not yet read: input/touch overlay code (Stage 2), `Hud_BeginView`, cockpit camera/near plane (Stage 4).

### Flat-mode reference frames (desktop, real GPU, before any SBS change)
Game data: extracted from `CMR2.iso` (folders `Game`, `FrontEnd`, `Sounds`, `CountrySpecific`, `SetupRep`) to
`~/cmr2game`, outside the repo. Never commit it.

Machine: Intel UHD 630 (Mesa), Wayland, log says `SDL_GPU driver vulkan`, back buffer 1920x1080.
Commands (from repo root; `build/` is git-ignored):
```
python3 tools/stage_baseline.py run --data ~/cmr2game --probe build/linux/tests/stage_probe \
    --output build/vr-baseline/logic --rates 60          # headless; makes inputs.csv; all traces identical
SDL_VIDEODRIVER=wayland SDL_AUDIODRIVER=dummy XDG_DATA_HOME=$(mktemp -d) \
  OPENCMR2_FRAME_HASH=$PWD/build/vr-baseline/flat/hash-a.txt \
  OPENCMR2_FRAME_DUMP=$PWD/build/vr-baseline/flat/dump-a OPENCMR2_FRAME_DUMP_EVERY=100 \
  build/linux/tests/stage_probe ~/cmr2game build/vr-baseline/flat/trace-a.jsonl 60 500 \
  build/vr-baseline/logic/inputs.csv
```
Results (two identical runs a/b, 500 ticks, 1851 presented frames, 19 BMPs each):
- Physics trace a == b == headless reference (cmp identical).
- Frame hashes: frames 0-980 identical; from 981 (race) 534 of 1851 differ. So hashes alone are NOT a
  valid flat-mode regression check on this GPU; render has run-to-run noise (cause not investigated; docs
  mention frame-dependent visual RNG).
- Noise floor in the dumped frames (a vs b): mean abs diff 0.0003-0.092 / 255, max pixel diff 15-65,
  up to 11231 pixels over 8 levels. Use a tolerance (e.g. mean < 0.2) when comparing `[vr] enabled = 0` with
  this baseline; do not expect equal hashes.
- Frames saved: `build/vr-baseline/flat/dump-{a,b}/frame-NNNNNN.bmp` (local only, not committed).
- Verified on desktop only. Nothing about phone/goggles.

## Stage 2 — gamepad, no touch in VR

Findings (code read):
- Menus and race work with a pad by default (`src/input/bindings.cpp`, `s_gamepad`): D-pad/left stick move,
  A confirm, B back, left stick steers, triggers throttle/brake, A handbrake, Y camera, Start/Back pause.
- Android overlay (`TouchControls.kt`) hid only while a gamepad was in use and came back on ANY touch
  (`onInput`), so a finger on the phone in goggles would bring it back. `ControllerProfiles.touchOnly` could also
  make the native side ignore controllers.
- Settings file on Android is `<filesDir>/opencmr2.ini` (SDL_GetPrefPath = internal storage dir).

Change (commit "Stage 2"): `[vr] enabled = 0|1` in the default ini template (`src/platform/sys.cpp`);
`VrSettings.kt` parses it; with `enabled = 1` `TouchControls.show()` never shows the overlay (touch cannot bring it
back) and `GameActivity.useTouchControlsOnly()` returns false. Preview in the layout editor is unaffected.
Existing installs: the key is absent = 0 (template is written only for new files).
Tests: `VrSettingsTest` (parser, 2 cases) passes; `ctest --preset linux` 25/25; `assembleDebug` OK.
Not verified: behaviour on a device/emulator with game data (needs a game folder there), real pad navigation of
every menu, any goggles.

Open points for Stage 2:
- Text entry (driver name, LAN chat) uses Android's on-screen bar when there is no hardware keyboard: touch only.
  Needs a pad-only answer (or accept a plain keyboard for setup).
- The Android Back button/gesture maps to Escape; with a pad, Start/Back are bound in the game.
- Setup screens (`SetupActivity`, folder picker) are touch; do them before putting the phone in goggles.
- Switching VR on needs editing `opencmr2.ini` for now; live settings are an open Stage 3 decision.

## Stage 3 — cinema mode (3a: fixed settings; no live adjustment yet)

What it does (SDL_GPU renderer only; `[vr] enabled = 1`, CMake `OPENCMR2_VR`, default ON):
- Game sees one eye as its screen: `Gfx_VrEyeSize` = (desktop W / 2, H) x `render_scale`, even; the display mode list
  is that single mode (`src/render/gfx_core.cpp`). `Execute()` is unchanged: it renders once into `backColor` (eye size).
- At present, `ComposeEyes` (`src/render/gfx_gpu.cpp`) draws `backColor` once per eye into its half of the swapchain
  with `shaders/lens.frag`: shift, margin, lens correction `c + d*f(r)/f(re)` (re at the nearest edge, not the
  corners), black where the sample is outside the image. Same picture in both eyes. Movies use the same path
  (`stretch` ignored in VR). HUD/menus are in the same buffer, so they are distorted together.
- Math in `src/render/vr_layout.h` (pure; `tests/vr_layout.cpp` in ctest), settings in `src/render/vr.cpp`.
- Gate `Gfx_VrActive()`: needs the build option, `[vr] enabled = 1` and SDL_GPU; with GL/null renderer it logs and stays off.
- Frame hash/dump in VR save the COMPOSED picture (an extra capture pass, only when `OPENCMR2_FRAME_HASH` is set).

Deviations from CLAUDE.md:
- Per-pixel lens correction in the fragment shader instead of a 32x32 mesh (same formula, no mesh/vertex buffer).
  Whether it matches a mesh visually in goggles is not checked.
- `render_scale` sets the game's video mode, so it needs a restart; lens/shift/margin are read every frame
  (`Vr_LoadConfig` in `ComposeEyes`) but there is no UI to change them yet (3b: pad combo + save to ini).
- Not touched: the GL renderer (VR off there), 16 KB alignment (not re-checked after this change).

Verified on desktop (Intel UHD 630, Wayland, Vulkan, 1920x1080 window => 960x1080 eyes, render_scale 0.75 => game 720x810):
- `ctest --preset linux` 26/26 (new `vr_layout`), `assembleDebug` OK (libmain.so contains the VR code).
- Physics trace with VR on == headless reference (cmp identical): game logic runs once per frame, unchanged.
- Eye difference (shift 0, margin 0): left half vs right half of the composed frames: mean 0.0000, max 0 in all
  18 dumped 1920x1080 frames. (Only proves both eyes get the same image; it does not test the lens.)
- Looked at two composed frames (boot video, race): picture barrel-distorted, corners black, HUD upright, both
  eyes equal. The race HUD fits an eye of aspect 0.89 on desktop. Not looked at menus or the cockpit view.
- Flat mode (VR off) after the change: first 981 frame hashes identical to the baseline; race frames within the
  noise (mean diff 0.083 vs noise floor 0.092; max pixel 99 vs 65 in the noise run: isolated pixels, not examined).
Not verified: phone (aspect 1200x1080 per eye, cutout), emulator with game data, goggles (distortion, shift, margin,
comfort, FPS, heat). Nothing here says the values are good.

## Stage 4 — real stereo

`[vr] mode = stereo` (needs a restart; `separation`, `convergence` read every frame). Implementation in
`src/render/gfx_gpu.cpp` + `src/render/vr_layout.h`, no change in `game/`:
- Back buffer textures are 2 x eye width. The game still sees one eye. `Execute()` runs the back buffer's recorded
  commands twice (loop `eye`), once per half: viewport, scissor and clear rectangles offset by `eye * width`
  (`ToTarget(..., xOffset)`). Merged leading clears clear the whole texture (both halves). Cube-face reflection
  targets (phase 0) run once and are shared. Backdrop copy (distance fade) copies the whole texture; the shader reads it
  at `gl_FragCoord`, so each eye reads its own half.
- A draw gets the eye's matrices only if it is on the back buffer, not pre-transformed (2D) and its projection is
  perspective (`_34 == 1`, `_44 == 0`). `Vr_EyeMatrices`: view moved by -e along x (`_41 -= e*_14`), projection
  `_31 += e*_11/convergence` (off-axis, not toe-in). Lights are transformed with the eye's view (a copy of the draw
  state), so lighting and fog stay consistent. HUD, menus, sky/backdrop layers, movies are 2D, so no parallax.
- Present: `lens.frag` samples the eye's half (uniform x scale/offset).
- Units: the world looks like ~1 unit = 1 metre, estimated from ONE reading (HUD 33 mph in 1st gear vs ~14 units per
  50 ticks of the trace, if a tick is 1/50 s); not confirmed. So `separation = 0.065` and `convergence = 10` are guesses.

Verified on desktop (same machine as Stage 3, 720x810 game screen, 960x1080 per eye, stage_probe Finland 1):
- `vr_layout` test: at the convergence distance a point has the same x in both eyes and in the middle view, nearer
  points go left in the right eye (crossed), farther right, no vertical parallax, separation 0 = same camera. This
  fixed the sign convention; it is maths, not a look in goggles.
- Measured in a composed race frame (block matching, separation 0.065 / convergence 10): car body (near) is -9 px in
  the right eye, far trees +2 px: matches crossed/uncrossed disparity. HUD identical in both eyes.
- Eye difference, shift 0 / margin 0: `separation = 0`: mean 0.000-0.008, max 1 (menus) to 68 (race: a few
  pixels, cause not investigated); `separation = 0.065`: race frames mean 1.9-7.1, max up to 236.
- Physics trace identical to the headless reference with stereo on (game logic still runs once per frame).
- `ctest --preset linux` 26/26; `assembleDebug` OK; flat mode: first 981 frame hashes identical to the baseline, race
  frames mean diff 0.075 (noise floor 0.092).
NOT verified: other camera views (bumper, cockpit; the probe only drives the default chase view), particles /
reflections / shadows per eye, split screen (VR should not use it), GPU cost of the second pass (FPS), the phone,
the emulator, goggles (comfort, scale, convergence). GL renderer: no VR.

## Decisions
(none yet)
