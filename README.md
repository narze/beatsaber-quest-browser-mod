# QuestBrowser

An in-game web browser for Beat Saber on Quest (Quest 3 / 3S / 2 / Pro).

Target: Beat Saber **1.40.8_7379**, Scotland2 mod loader.

## Use

1. Install `QuestBrowser.qmod` with [ModsBeforeFriday](https://mbf.bsquest.xyz/) or QuestPatcher. It installs BSML, custom-types, and beatsaber-hook too if they are missing.
2. In the main menu, select **Browser** in the mods list on the left.
3. Controls:
   - Point at the page and pull the trigger to click. Hold the trigger and move to drag.
   - Move the thumbstick up or down to scroll the page under the pointer.
   - **URL** opens a keyboard. Type an address (`example.com`) or a search (`beat saber mods`).
   - **Type** opens a keyboard that types into the selected field of the page. **Enter** and **Del** send these keys.
   - **<**, **>**, **Reload**, **Home** work like in a desktop browser.

The page stops (timers, video, audio) when you close the browser screen.

## How it works

```
 Beat Saber (Unity, il2cpp)                        Android main thread
 ┌──────────────────────────────┐   JNI    ┌───────────────────────────────┐
 │ BrowserScreen (MonoBehaviour)│ ───────▶ │ BrowserBridge (Java, our dex) │
 │  - RawImage + Texture2D      │          │  - off-screen WebView          │
 │  - VRPointer hit → touch     │ ◀─────── │  - draws to Bitmap at 30 fps   │
 │  - thumbstick → wheel scroll │  frames  │  - double-buffered ByteBuffer  │
 └──────────────────────────────┘          └───────────────────────────────┘
```

- `java/src/.../BrowserBridge.java` is compiled to a dex at build time (`cmake/dex.cmake`) and embedded in the `.so` with an assembler `.incbin`. At runtime, `src/JniBridge.cpp` loads it with `InMemoryDexClassLoader`, so the mod is a single file.
- `src/BrowserScreen.cpp` builds the BSML UI, copies new frames into a `Texture2D`, and converts laser pointer hits (`VRPointer.cursorPosition`) into page pixels.
- `include/browser_logic.hpp` holds the game-independent logic (URL parsing, coordinate mapping, touch state). It has host unit tests.

## Build

Needs [QPM](https://github.com/QuestPackageManager/QPM.CLI/releases/latest), CMake, Ninja, Python 3, a JDK (11+), and the Android SDK (`ANDROID_HOME`, for `android.jar` and `d8`). `mise install` gives you CMake, Ninja, Java, and Python at the pinned versions.

```sh
qpm ndk resolve -d
qpm restore
qpm s build     # build the .so
qpm qmod zip    # build QuestBrowser.qmod
```

Install on a connected Quest and watch the log:

```sh
qpm s copy      # push the mod and restart Beat Saber
qpm s log       # or: adb logcat -s QuestBrowser
```

## Test

Host unit tests (no device):

```sh
cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
```

End-to-end test of the Java bridge on an Android emulator or device. It builds a small harness APK that loads the same dex the way the mod does. Then it taps, types, clicks, and scrolls, and saves the frames to `build-harness/`:

```sh
tests/android-harness/run.sh
```

## Known limits

- Video plays in software draw mode, so it can be slow or black. Text pages and most web apps work.
- The page is 1280 x 720 at 30 fps. Increase the size for sharper text, or decrease it for better frame rate.
- Pages can not open the Android system keyboard. Use the **Type** button.
- Main menu only. The browser is not available during a song.

## Credits

* [Metalit](https://github.com/Metalit), [Lauriethefish](https://github.com/Lauriethefish), [Fern](https://github.com/Fernthedev), [Bobby Shmurner](https://github.com/BobbyShmurner) for: [this template](https://github.com/Metalit/quest-mod-template)
* [Metalit](https://github.com/Metalit), [Sc2ad](https://github.com/Sc2ad), [zoller27osu](https://github.com/zoller27osu), [jakibaki](https://github.com/jakibaki) for: [beatsaber-hook](https://github.com/QuestPackageManager/beatsaber-hook)
* [Fern](https://github.com/Fernthedev) for: [QPM](https://github.com/QuestPackageManager/QPM.CLI), [bs-cordl](https://github.com/QuestPackageManager/bs-cordl), [paperlog](https://github.com/Fernthedev/paperlog)
* [RedBrumbler](https://github.com/RedBrumbler) and bsq-ports for: [Quest-BSML](https://github.com/bsq-ports/Quest-BSML)
