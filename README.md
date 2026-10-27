# QuestBrowser

An in-game web browser for Beat Saber on Quest (Quest 3 / 3S / 2 / Pro).

Target: Beat Saber **1.40.8_7379**, Scotland2 mod loader.

## Use

1. Install `QuestBrowser.qmod` with [ModsBeforeFriday](https://mbf.bsquest.xyz/) or QuestPatcher. It installs BSML, custom-types, beatsaber-hook, and SongCore too if they are missing.
2. The browser is one floating window. It stays open in the menu and in songs, and the page does not reload between them.
   - **Browser** in the mods list on the left of the main menu opens the browser settings (also in Mod settings > **Browser**). The **Enable browser** switch there turns the browser on or off.
   - Lost the window? Hold the **reset keybind** (default **X + Y**) for 0.6 s. The window shows again at its default size, centered and raised above the song selection. It works in the menu and while paused, not while a song plays. Choose the combo, or Off, in the Browser settings.
   - Menu: click the page, drag the handle above the window to move it (while you hold it, push the thumbstick up or down to move it away or closer), and drag its corners and edges to resize it.
   - Song: the page shows, but you can not click it. Pause to click, move, or resize it.
   - The menu and songs each keep their own window position and size.
3. Controls:
   - The window shows only the page. Point at it and a slim toolbar fades in below it: **back**, **forward**, **reload**, the page title (click it to type an address or a search), **tabs** (with the number of tabs), and **menu**.
   - **Tabs** lists your tabs: open one, close one with **x**, or **+ New tab** (opens the keyboard). Up to 10 tabs; background tabs are paused. Tabs come back at the next start, and each loads when you open it.
   - **Menu**: Home, History, Keyboard, quick links to the map sites (BeatSaver, BeastSaber, BeatLeader), Zoom, Opacity, and the window shape. Panel text grows with the window, and panels are fully opaque.
   - Resize with the brackets on the corners and the bars on the edges; they turn blue when you point at them.
   - Tap a text field on the page to open the keyboard. It opens near you (50 cm in front, 45 cm below your eyes, tilted like a laptop keyboard) and stays there while you type. Drag its handle to move it; it opens there, relative to where you face, next time. **Reset keyboard position** in the settings brings it back. Fullscreen video fills the window; back or **Esc** leaves fullscreen.
   - Back, Forward, URL, and **Menu** stay visible. **Menu > Browse** contains Reload, Home, Keyboard, and History. **Menu > View** contains aspect, zoom, and opacity controls (**Fade - / +**). Close Menu to give the page more room.
   - Buttons and labels grow with distance. The minimum window size fits the controls; there is no maximum window width or height.
   - Point at the page and pull the trigger to click. Hold the trigger and move to drag.
   - Move the thumbstick up or down to scroll the page under the pointer.
   - Clicking outside both the browser window and keyboard also closes the keyboard.
   - **History** opens up to 100 recent URLs, newest first. Click a URL to revisit it, **Delete** to remove one, or **Clear all** to erase the list. **Newer** and **Older** switch pages. History stays saved across restarts; recording starts with this version.
   - **Opacity - / +** adjusts the window from 20% to 100% in 10% steps. The setting stays saved across restarts.
   - **Aspect** cycles Free, 16:9, 4:3, and Portrait (9:16). Presets constrain the page area, excluding the toolbar; select Free to unlock the ratio again. Width, height, and preset save separately for menus and songs.
   - **Zoom - / +** adjusts page zoom from 50% to 200%, without changing the window. Pages reflow to the new viewport. The keyboard follows the resized window. Minimum dimensions keep the toolbar and history usable.
   - The keyboard has a number row with symbols, Tab, Caps, Shift (one key), Ctrl (for Ctrl+A/C/V/Z), Esc, arrow keys, `.com`, Del, and Enter.
   - Tabs come back at the next start. Only the open tab loads; the others load when you open them.
   - **Install maps**: on BeatSaver or BeastSaber, click **One-Click**; on BeatSaver or BeatLeader, click the map **Download** button. The map installs into your custom songs (`<key> (<song> - <mapper>)`), and the toolbar shows the progress. The song list reloads when you are in the menu; maps from the pause menu show up after the song. A map that is already installed is not downloaded again. Other downloads are not supported.
   - **Install playlists**: on a BeatSaver or BeastSaber playlist, click **One-Click** or **Download**. The playlist is saved into the PlaylistManager playlists folder, and its maps install one by one. The song list and the playlists reload once, when all the maps are installed.
4. Browser settings: **Enable browser** (on by default; off hides the window, the toolbar and the keyboard, turns off the keybind, and from the next start the browser does not load at all), show it during songs, the reset keybind (Off, X + Y, A + B, X + A, Y + B, left, right, or both stick clicks), and **Reset menu position** / **Reset in-song position**. A reset puts that window back at its default size, centered and raised above the song selection.

The page stops (timers, video, audio) while the window is hidden. While a song plays, only the page shows; pause to see the controls. The move handle is above the window. It updates at 60 fps in the menu and while paused, and at 30 fps while a song plays.

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

Host unit tests (no device; the Java tests need a JDK):

```sh
cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
```

End-to-end test of the Java bridge on an Android emulator or device. It builds a small harness APK that loads the same dex the way the mod does. Then it taps, types, clicks, and scrolls, installs a BeatSaver map and a playlist (needs internet), and saves the frames to `build-harness/`:

```sh
tests/android-harness/run.sh
```

## Plain HTTP

Beat Saber's network security config blocks plain `http://`. The mod allows it again for the game process, so local servers such as `http://nas.local:8096` load. It swaps Android's process-wide `NetworkSecurityPolicy` for one that permits cleartext; that class is an Android internal API, so `src/hidden_api.cpp` exempts only this class from the hidden API policy first.

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

## License

MIT. See [LICENSE](LICENSE).
