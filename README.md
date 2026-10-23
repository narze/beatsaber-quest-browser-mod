# QuestBrowser

An in-game web browser for Beat Saber on Quest (Quest 3 / 3S / 2 / Pro).

Target: Beat Saber **1.40.8_7379**, Scotland2 mod loader.

## Use

1. Install `QuestBrowser.qmod` with [ModsBeforeFriday](https://mbf.bsquest.xyz/) or QuestPatcher. It installs BSML, custom-types, and beatsaber-hook too if they are missing.
2. The browser is one floating window. It stays open in the menu and in songs, and the page does not reload between them.
   - **Browser** in the mods list on the left of the main menu shows or hides it.
   - Menu: click the page, and drag the handle below the window to move it.
   - Song: the page shows, but you can not click it. Pause to click it or to move it.
   - The menu and songs each keep their own window position.
3. Controls:
   - Point at the page and pull the trigger to click. Hold the trigger and move to drag.
   - Move the thumbstick up or down to scroll the page under the pointer.
   - **URL** opens the keyboard to type an address. The title line shows what you type. Enter loads it. Type an address (`example.com`) or a search (`beat saber mods`).
   - Tap a text field on the page to open the keyboard, or press **Keyboard**. **Hide** closes it.
   - The keyboard has a number row with symbols, Tab, Caps, Shift (one key), Ctrl (for Ctrl+A/C/V/Z), Esc, arrow keys, `.com`, Del, and Enter.
   - **<**, **>**, **Reload**, **Home** work like in a desktop browser.
   - Fullscreen video fills the browser window. **<** or **Esc** leaves fullscreen.
4. Mod settings > **Browser**: show the window during songs or not, and reset the positions.

The page stops (timers, video, audio) while the window is hidden. It updates at 60 fps in the menu and while paused, and at 30 fps while a song plays.

## How it works

```
 Beat Saber (Unity, il2cpp)                        Android main thread
 ┌──────────────────────────────┐   JNI    ┌───────────────────────────────┐
 │ BrowserScreen (MonoBehaviour)│ ───────▶ │ BrowserBridge (Java, our dex) │
 │  - RawImage + Texture2D      │          │  - off-screen WebView          │
 │  - VRPointer hit → touch     │ ◀─────── │  - GPU draw at 60 fps (30 song)│
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

- Video plays at the page frame rate (60 fps in the menu, 30 fps in songs). Protected video (DRM) may stay black.
- The page is 1280 x 720. Increase the size for sharper text.
- The Quest system keyboard can not show over Beat Saber (the game does not declare `oculus.software.overlay_keyboard`), so the mod draws its own keyboard. Text with no key on a US keyboard is typed with JavaScript, which some pages ignore.
- Solo songs only. The window does not show in multiplayer.

## Credits

* [Metalit](https://github.com/Metalit), [Lauriethefish](https://github.com/Lauriethefish), [Fern](https://github.com/Fernthedev), [Bobby Shmurner](https://github.com/BobbyShmurner) for: [this template](https://github.com/Metalit/quest-mod-template)
* [Metalit](https://github.com/Metalit), [Sc2ad](https://github.com/Sc2ad), [zoller27osu](https://github.com/zoller27osu), [jakibaki](https://github.com/jakibaki) for: [beatsaber-hook](https://github.com/QuestPackageManager/beatsaber-hook)
* [Fern](https://github.com/Fernthedev) for: [QPM](https://github.com/QuestPackageManager/QPM.CLI), [bs-cordl](https://github.com/QuestPackageManager/bs-cordl), [paperlog](https://github.com/Fernthedev/paperlog)
* [RedBrumbler](https://github.com/RedBrumbler) and bsq-ports for: [Quest-BSML](https://github.com/bsq-ports/Quest-BSML)
