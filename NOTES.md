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

## Decisions
(none yet)
