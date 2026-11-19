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
- Cutout: no `layoutInDisplayCutoutMode` set anywhere (`values/themes.xml` only comments that SDL draws
  under cutouts). TODO: add `values-v28/styles.xml` with `shortEdges` on `Theme.OpenCMR2.Game`,
  confirm in log that window size == screen size.
- 16 KB pages: verified in debug APK. All `.so` (arm64-v8a, x86_64) have LOAD align `0x4000`; `zipalign -c -P 16 4` OK.
  Not tested on a 16 KB device.

## Decisions
(none yet)
