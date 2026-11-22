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

## Stage 3b — live adjustment with the gamepad

Press both stick buttons (L3 + R3) of a gamepad (a toggle; first built as hold, which cannot be combined with the D-pad on a phone pad) while `[vr] enabled = 1`: all game input goes neutral
(`src/input/input_sdl.cpp`, after the players are updated), a list of the options is drawn by the port
(`src/render/vr_adjust.cpp`: own 5x7 font, 33 characters wide, fitted to the eye, recorded as the last draws before
present from `Gfx_Present`, so it is in both eyes with no parallax and goes through the lens). Up/Down choose
(shift, margin, lens k1, k2, and in stereo separation, convergence), Left/Right change (hold: after 0.4 s repeat, after
1.5 s five steps), A restores the starting value. Changes apply on the next frame (read every frame); pressing L3 + R3 again closes the list and
writes all six keys to `opencmr2.ini` (`Platform_SetSettingText`). Not live: `render_scale` and `mode` (restart).
Steps and ranges: `Vr_Params` in `src/render/vr_layout.h`. No change in `game/`.

Verified on desktop only: `vr_layout` test (steps stay on the grid and in range); a TEMPORARY forced-open run (code
removed again) rendered the list in both eyes (fits, readable, selection moves, value changes the picture, file saved
with the new values); `ctest` 26/26; Android `assembleDebug` OK; flat mode first 981 hashes and the physics trace
unchanged.
NOT verified: an actual gamepad (L3+R3 combination, D-pad, repeat feel; I could not press real sticks), Android,
goggles. A stick click can happen by accident; both together should be rare. Cinema mode list hides separation and
convergence.

## Stage 4 follow-up — other camera views in stereo (desktop, composed frames)

How: `tests/stage_probe.cpp` got `OPENCMR2_PROBE_CAMERA_AT_FRAMES="a,b,..."` (presses C for 6 race frames from each listed race frame);
run with `mode = stereo`, `shift = 0`, `margin = 0`, defaults otherwise, frames dumped every 40. With the camera key pressed the
physics trace differs from the reference from tick 177 on (a probe with extra input, not a VR effect; not investigated why the camera
key changes the car state). Frames looked at: a low/bumper-like view, a bonnet view and the cockpit (all views the key cycles to in
this run; chase view was checked before).
- All of them render in both eyes with a sane picture: no missing or doubled interior parts, HUD identical in both eyes, scenery
  consistent. Looked at a contact sheet of 9 frames and the cockpit frame at full size.
- Disparity by block matching, right eye relative to left (960 px wide eye, separation 0.065, convergence 10):
  cockpit steering wheel / dash: -179 px (about 19% of the eye width); windshield scenery: +4 px; low view ground: -80 px.
  So near parts have a very large crossed disparity at the default values. That is a prediction of discomfort or a picture
  that will not fuse in goggles (not measured on a person). The units guess (1 unit ~ 1 m) may also be off for the cockpit.
  Ideas, none done: smaller `separation` or a per-view value (the near plane differs per view: `g_viewNearPlanes`), clamping the
  disparity of near geometry, or bigger `convergence` in the cockpit.
- Particles, reflections, shadows: nothing odd in the frames above, but no dedicated check (no frame with heavy dust/rain).
- Not checked: replay/other camera sets, split screen, helicopter/replay views.

### Cockpit and bumper in stereo (checked in detail)
The probe now also logs the camera type (`OPENCMR2_PROBE_CAMERA_AT_FRAMES` run): cycling with C gives types 4 (chase, start),
5, 1, 2, 3. Looking at the frames, 5 is the low front view (bonnet edge at the bottom, near plane 0.346), 1/2/3 are three
in-car views (steering wheel, dash, windshield; near plane 0.1, zoom 0.625). I name them from the pictures, not from the code.
Grid block matching (cells of 120x100 px with texture, eye 960 px wide, separation 0.065, convergence 10), right eye vs left:
- bumper (5):   median 0, p10 -23, min -50 px (5% of the eye width).
- cockpit 1/2/3: median -6..+2, p10 about -95, min -170..-180 px (18-19%): the dash, wheel and A-pillar.
- Same cockpit with `separation = 0.02`, `convergence = 3`: p10 -26, min -48 px (5%), median 0. Linear in separation, as
  expected, so it is the lever; the best values need people in goggles.
No rendering fault seen in any of them (interior parts, windshield roof sticker, HUD equal in both eyes). Not measured: how it
feels. A 19% disparity is certainly far beyond comfortable fusion on a fixed-focus display; 5% is still large.

### Separate stereo values for the in-car views (done)
`interior_separation` (0.02) and `interior_convergence` (3), blended with `separation`/`convergence` by the projection's near
plane (`Vr_ViewStereo` in `src/render/vr_layout.h`: near 0.1 = in car, 1.0 = outside, linear between; the near plane is read from
the matrix as `-_43/_33`, so no change in `game/` and no jump while the camera blends). Both are in the in-game list (IN-CAR SEP,
IN-CAR CONV) and in the ini. Unit test covers outside, in-car, between and below 0.1.
Measured again with the defaults (same frames and method): chase unchanged (far objects +0..+6 px); bumper min -12 px (was -50);
cockpit 1/2/3 min -48..-50 px, p10 about -26 (was -170..-180 and -95). The 0.02 / 3 defaults are the values measured above, not
values tuned on a person: still 5% of the eye width at the dash. Flat mode: first 981 hashes and the trace unchanged.

## Stage 5 — head tracking (optional, default off)

`[vr] head_tracking = 1`, only with `enabled = 1` and `mode = stereo`. No change in `game/`.
- Android: `HeadTracker.kt` (started by `GameActivity` only when both flags are on, stopped on pause) reads
  `TYPE_GAME_ROTATION_VECTOR` (falls back to `TYPE_ROTATION_VECTOR`), makes a rotation matrix, remaps it for the display rotation
  (the phone lies on its side in goggles) and calls `HeadTracker.nativeRotation` (JNI, `src/render/vr_head.cpp`). The first sample is
  the straight-ahead pose; in the adjust list (L3 + R3) Y recentres. The list shows the hint line only with head tracking on.
- Native: `Vr_HeadViewMatrix` (`vr_layout.h`) = camera-space rotation (Direct3D row vectors) of the pose relative to the reference;
  `ExecuteGpu` puts it after the game's view (`view * head`) for the same draws that get the eye offset, before the eye translation, so
  the eyes turn with the head. It is an offset: the game's own camera still drives. Off => no cost, no change.
- Hardware axes: device x right, y up, z out of the screen; game camera x right, y up, z forward (z = -device z).

Verified on desktop with an injected pose (`tests/stage_probe.cpp`: `OPENCMR2_PROBE_HEAD="yaw,pitch,fromRaceFrame"`), stereo, chase view,
same frames compared against a run with the straight-ahead pose (physics trace identical in all three runs):
- yaw 20 deg right: the scenery moves left by 326 px of the 960 px eye; pitch 15 deg up: it moves down by 252 px. Directions are the
  ones a head turn gives. Size matches the projection (tan(angle) times the projection scale), i.e. the motion is 1:1.
- `vr_layout` test: right turn moves the point ahead left, looking up moves it down, same pose = identity, rotation stays orthonormal.
- `ctest` 26/26, `assembleDebug` OK (native lib has the JNI function in arm64-v8a and x86_64), Kotlin unit tests 3/3 (flag parser).
- Flat mode: first 981 hashes and trace unchanged.
NOT verified: the Android sensor path (not run on the emulator or phone: it needs the game folder in the app and a sensor),
the remap for each display rotation, drift of the game rotation vector, latency (the pose is taken once per presented frame, not
predicted), the Y glyph/hint line in the list (never looked at), comfort and motion sickness.
Limits seen or expected: only the stereo path moves (cinema mode ignores it); billboards, particles, lens flares and the 2D layers
are drawn by the game for its own single camera, so with big turns they can look wrong; the in-car views rotate the whole interior
with the camera, as a head turn would. Look at it with the range of real head motion before trusting it.

## Decisions
(none yet)

## R36S (ArkOS, RK3326, Mali-G31), flat mode, no VR

Cross-built on the desktop, not run on the device. `cmake/linux-aarch64-r36s.cmake`, a sysroot from an Ubuntu 20.04 arm64
container (glibc 2.31; `libc6-dev libstdc++-10-dev libgcc-10-dev libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev
libgles2-mesa-dev libudev-dev libasound2-dev libdbus-1-dev`, absolute symlinks made relative so `ld` follows them),
host `spirv-cross` on `PATH`, `R36S_SYSROOT` set:
`cmake -S . -B build/r36s -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/linux-aarch64-r36s.cmake -DCMAKE_BUILD_TYPE=Release
-DOPENCMR2_VR=OFF -DOPENCMR2_VENDORED_SDL=ON -DSDL_KMSDRM=ON -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF
-DSDL_VULKAN=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF`.
Result: needs glibc 2.30 at most (ArkOS 19.10 has 2.30), libstdc++ static, SDL3 shared beside the binary (KMSDRM and ALSA
loaded at run time). Run under qemu-aarch64 with the dummy video driver: starts and reports missing game data. Not
checked: OpenGL on Panfrost (reports 3.1; the launcher sets `MESA_GL_VERSION_OVERRIDE=3.3`), speed on 4x Cortex-A35 and 1 GB,
pad mapping, the 640x480 screen.

### R36S: OpenGL ES 3.0 mode (`-DOPENCMR2_RENDERER_GLES=ON`)
First R36S run failed: `Couldn't find matching EGL config`. The console (ArkOS, kernel 4.4) uses the Mali blob (`libEGL.so.1` ->
`libMali.so`, `libmali-bifrost-g31`): OpenGL ES only, no desktop OpenGL, no Vulkan ICD. Mesa 19.2 there is software only.
`gfx_gl.cpp` got an ES build of the same executor: ES 3.0 context, shaders through `spirv-cross --es --version 300` (all
varyings renamed incl. location 6; default float precision forced to highp at compile time), RGBA uploads with red/blue swapped
on the CPU (no BGRA), vertex pointers offset instead of BaseVertex, render targets read through their framebuffer.
Desktop OpenGL 3.3 and SDL_GPU unchanged (ctest 26/26). Desktop check of the ES build (Mesa ES 3.2, UHD 630, `graphics_api =
opengl`, stage_probe 500 ticks): trace identical to the flat baseline, 19 dumped frames within mean 0.12/255 of the SDL_GPU
baseline (race frame looked right). Not checked: the Mali driver itself, speed, S3TC support there (CPU-decoded if absent).

### R36S speed (OpenGL ES mode, Mali-G31, 640x480, Finland 1, stage_probe 500 ticks, `vsync = 0`)
Measured on the device (own timers in a throw-away build, not committed):
- Race frame before: 116-128 ms (8.6 fps); about 600 draws and 12.5k indices per frame, no render-target passes.
- CPU: game logic ~1 ms, command submission ~27 ms (draw calls ~19 ms: ~30 us each), then the wait for the GPU. With `glFinish` per
  frame: submit ~28 ms + ~22 ms of GPU left. Presentation is locked to the display's 60 Hz even with `vsync = 0` (frames are 16.7 ms multiples).
- Not the bottleneck: vertex lighting (switching it off changed nothing), render-target passes (none), the backdrop copy (~8 %),
  vertex count, baseking BaseVertex as 32-bit rebased indices (no change), merging consecutive draws (only ~10 of 600 share a state).
- The fragment shader was: a trivial one gave 52 ms; fill rate was ~65 ms of the frame.
- Fix: one fragment program per combination of stage set-up, alpha test, fog and distance fade, those values written into the shader
  as constants (`SpecialisedProgram`, ES only): race frames 52-65 ms (15-19 fps); at 1/2 and 1/4 of the pixels they stay 52 ms, so pixels
  no longer matter. Desktop ES frames within 0.13/255 of the SDL_GPU baseline, trace identical.
- 16 variants take ~3.9 s to compile on the Mali (~250 ms each: visible stutter). They are saved with `glGetProgramBinary` in
  `<user dir>/shadercache/` (named by a hash of source, `GL_VERSION`, `GL_RENDERER`): warm start 33 ms; frames of a cold and a warm run on
  the device equal (noise floor). A first desktop test of the cache looked broken: it reused one user dir, whose saved game state changed the run.
- Next, not done: submission and the GPU run one after the other (Mali renders at the swap), so a render thread would overlap them
  (hypothesis, ~28 ms per frame at best); fewer draw calls.

More R36S measurements (all `stage_probe`, per race frame, median of 19 windows of 60 frames; the display locks frames to 16.7 ms steps):
- No effect: skipping the distance-fade backdrop copy, depth without stencil, `glInvalidateFramebuffer` of the depth, skipping the readback,
  drawing the picture to the window with a triangle instead of `BlitFramebuffer` (picture checked equal, no time gained).
- Skipping the final window pass entirely gives 39 ms, but only because nothing then reads the picture (the driver can drop the work):
  it shows the CPU side alone (command recording) is ~35-39 ms. Skipping the 156 draws of <= 6 indices: mean 59 -> 47 ms.
- `glFlush` every 100 draws of the back buffer: mean 57.7 -> 47.7 ms (17 -> 21 fps), every 50 the same, every 200 49.9. Kept.
- Where the draws come from (race frame 1500, 409 draws): 360 indexed triangle lists of stage geometry, 156 of them <= 6 indices, 29
  2D (TL) draws. Consecutive draws differ mostly only in the texture (157) or the texture and world matrix (155).

### R36S: batching of opaque draws (ES mode)
Race frame 1500 (409 draws): 151 write depth without blending (the stage's opaque and cut-out surfaces: alpha test 128, no blend),
and fall into 44 distinct states; 174 blend with depth writes (foliage), 85 more are tiny or 2D. `BuildBatches` runs of such draws
(`depthFunc` LESS or LESSEQUAL, no blend, indexed triangle lists) are grouped by state content, buffer and vertex format, and each
group is one call (its indices copied together, 32 bits wide with the first vertex added). Order inside a run changes only which
of two surfaces at exactly the same depth wins. Desktop ES: trace identical, frames within 0.13/255 of the SDL_GPU baseline.
R36S (stage_probe): race frame mean 47.7 -> 42.7 ms (21 -> 23 fps), median 51.7 -> 39.0 ms.
Not done: the blended foliage (order matters); merging it would need the world matrix baked into the vertices on the CPU.

### R36S: what the CPU does (sampling profiler) and the load time
`OPENCMR2_SAMPLE=path` (`src/diagnostics/sampler.cpp`, `tools/sample_summary.py`): SIGPROF every 1 ms of CPU time, the program counters and
`/proc/self/maps` saved; `tools/sample_summary.py samples maps unstripped-exe aarch64-linux-gnu-nm` gives samples per function (the device has no perf,
strace or gdb). `OPENCMR2_GL_STATS=1` (ES): every 120 frames the time per frame, the executor's CPU time and the swap's.
Whole `stage_probe` run on the Mali-G31 / 4x Cortex-A35 (27.8 k CPU samples): 43.6 % `DistanceField::DistanceTransform` (fonts and vector icons),
8.7 % `OutlineFont_Replace`, 18 % the Mali driver. That is load time, not race time. With `[video] outline_fonts = 0`: wall time of the run 140 -> 83 s;
with the new `[video] vector_icons = 0` too: 67 s, 9.5 k samples, of them 51 % the Mali driver, 15 % libc, 11 % the executor. The R36S's screen is the game's own
640x480, where the original bitmaps are as sharp: both are 0 in the ini shipped for it.
Race frame (all of the above, `vsync = 0`): 38.8 ms = game 5.8 + executor on the CPU 17.1 + swap 16.0. A render thread could hide at most the game's
5.8 ms and has to synchronise the frame recording with the executor in many places (flush on texture changes, lock of render targets, readback, destroys),
so it was not built. Merging the blended foliage: 185 draws, 175 from static buffers (vertices cannot be baked on the CPU), only 17 adjacent pairs differ in the
world matrix alone: not worth it. `glGetString(GL_EXTENSIONS)` on the Mali: no S3TC (BC1/BC3 are decoded on the CPU), has ASTC LDR, ARM_shader_framebuffer_fetch,
EXT_texture_format_BGRA8888, EXT_disjoint_timer_query (measured 0.4 ms for 39 ms frames: no use). Memory: 918 MB, no swap, MemAvailable never below ~590 MB in a race.

libc's 15 % of the CPU in a race (sampler with the caller: the link register, which is the caller of a leaf such as memcpy): 6.9 % of all samples are inside
the Mali driver itself (its own copies; nothing to change from here except giving it less data), 2.6 % libc inside libc (malloc and friends), 1.1 % libdrm (the
page flip), and only ~3 % from this program: the executor's `memset`/`memcmp` (1.7 %), vector inserts of the recorded frame (1 %), `RecordDraw` (0.6 %), i.e.
~0.7 ms of a 22 ms CPU frame. Not worth a change (the vertex uniform block is 1.4 KB per distinct state, 8 lights of it; splitting it would save ~1 ms and touches the
SDL_GPU shader too).

## PortMaster package (R36S and other aarch64 CFWs)
`portmaster/` holds the text files of the package (launch script, port.json, gameinfo.xml, README.md, testing_thread.txt, default
settings). The zip is assembled outside the repository: `OpenCMR2.sh` beside `opencmr2/` with `opencmr2.aarch64` (stripped,
RUNPATH `$ORIGIN:$ORIGIN/libs.aarch64`), `libs.aarch64/libSDL3.so.0`, `licenses/`, `patches/` (`git format-patch 6e201df..HEAD`),
an empty `gamedata/` and `conf/`, and a real screenshot from the device. No game files are in it.
- PortMaster forbids bundling upstream SDL3. The bundled `libSDL3.so.0` is the sdl3-sdl2-backend shim (bmdhacks/SDL, branch
  `sdl2-backend`, commit 6057d79, SDL 3.5.0, built `-march=armv8-a`, glibc 2.29), which loads the device's SDL2. The game is still
  built against real SDL3 3.4. Launch script forwards the CFW's `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER` into `SDL3SHIM_SDL2_*`.
- Checked on the R36S (ArkOS, PortMaster in /opt/system/Tools): the script runs end to end from ssh, the pad is found through SDL2,
  OpenGL ES 3.0 on the Mali-G31 works through the shim, the intro and the Polish main menu draw, audio opens when
  EmulationStation is stopped (it holds the sound card otherwise). Not checked: sound heard, Start+Select exit and pad mapping
  from the device's own buttons, a launch from the Ports menu, other CFWs and GPUs (Panfrost), speed through the shim against the
  native SDL3 build.
- `porter` in port.json is a placeholder to fill in; `store` is empty (the game is no longer sold); the crew decides the repository.
