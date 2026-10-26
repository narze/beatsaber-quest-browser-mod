#!/usr/bin/env bash
# End-to-end test of the Java browser bridge on an Android device or emulator.
# Builds questbrowser.dex and a small harness APK, installs it, and checks that
# the off-screen WebView renders frames and reacts to touch, typing, and scroll.
#
# Usage: tests/android-harness/run.sh   (needs adb with one device, a JDK, and ANDROID_HOME)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}}"
BT="$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)"
ANDROID_JAR="$(ls -d "$SDK"/platforms/android-* | sort -V | tail -1)/android.jar"
OUT="$ROOT/build-harness"
PKG=com.narze.questbrowser.harness

rm -rf "$OUT" && mkdir -p "$OUT"/{bridge,harness,assets,apk}

echo "== Build bridge dex"
mkdir -p "$OUT/stubs"
javac --release 11 -Xlint:-options -classpath "$ANDROID_JAR" -d "$OUT/stubs" $(find "$ROOT/java/stubs" -name '*.java')
javac --release 11 -Xlint:-options -classpath "$ANDROID_JAR:$OUT/stubs" -d "$OUT/bridge" \
    $(find "$ROOT/java/src" -name '*.java')
jar cf "$OUT/bridge.jar" -C "$OUT/bridge" .
"$BT/d8" --release --min-api 29 --lib "$ANDROID_JAR" --output "$OUT/bridge" "$OUT/bridge.jar"
cp "$OUT/bridge/classes.dex" "$OUT/assets/questbrowser.dex"
cp "$HERE/assets/"* "$OUT/assets/"

echo "== Build harness native library (hidden API exemption, as in the mod)"
NDK="$(cat "$ROOT/ndkpath.txt")"
mkdir -p "$OUT/apk/lib/arm64-v8a"
"$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/clang++" --target=aarch64-linux-android29 -std=c++20 -O2 \
    -shared -fPIC -static-libstdc++ -I "$ROOT/include" \
    "$ROOT/src/hidden_api.cpp" "$HERE/jni/harness_jni.cpp" -o "$OUT/apk/lib/arm64-v8a/libqbharness.so"

echo "== Build harness APK"
javac --release 11 -Xlint:-options -classpath "$ANDROID_JAR" -d "$OUT/harness" \
    $(find "$HERE/src" -name '*.java')
jar cf "$OUT/harness.jar" -C "$OUT/harness" .
"$BT/d8" --release --min-api 29 --lib "$ANDROID_JAR" --output "$OUT/apk" "$OUT/harness.jar"
"$BT/aapt2" compile --dir "$HERE/res" -o "$OUT/res.zip"
"$BT/aapt2" link -o "$OUT/base.apk" -I "$ANDROID_JAR" --manifest "$HERE/AndroidManifest.xml" -A "$OUT/assets" "$OUT/res.zip"
(cd "$OUT/apk" && zip -q "$OUT/base.apk" classes.dex lib/arm64-v8a/libqbharness.so)
"$BT/zipalign" -f 4 "$OUT/base.apk" "$OUT/aligned.apk"
"$BT/apksigner" sign --ks "$HOME/.android/debug.keystore" --ks-pass pass:android \
    --out "$OUT/harness.apk" "$OUT/aligned.apk"

echo "== Run on device"
adb install -r "$OUT/harness.apk" >/dev/null
adb logcat -c
adb shell am force-stop "$PKG"
adb shell am start -n "$PKG/.HarnessActivity" >/dev/null

for _ in $(seq 1 30); do
    if adb logcat -d -s QBHarness:* | grep -q "DONE\|FAIL"; then break; fi
    sleep 1
done
adb logcat -d -s QBHarness:* QuestBrowser:* | tee "$OUT/logcat.txt"

DIR="/sdcard/Android/data/$PKG/files"
adb pull "$DIR/frame1.png" "$OUT/frame1.png" >/dev/null
adb pull "$DIR/frame2.png" "$OUT/frame2.png" >/dev/null

echo "== Keyboard test"
echo "== Viewport resize and remapped input test"
adb logcat -c
adb shell am force-stop "$PKG"
adb shell am start -n "$PKG/.HarnessActivity" --es mode viewport >/dev/null
vp=0
for _ in $(seq 1 40); do
    if adb logcat -d -s QBHarness:* | grep -q "VIEWPORT_DONE\|FAIL"; then break; fi
    sleep 0.5
done
adb logcat -d -s QBHarness:* QuestBrowser:* > "$OUT/logcat-viewport.txt"
for want in VIEWPORT_PORTRAIT_CLICK_OK VIEWPORT_WIDE_CLICK_OK VIEWPORT_DONE; do
    grep -q "$want" "$OUT/logcat-viewport.txt" || { echo "FAIL: expected $want"; vp=1; }
done

waitlog() {
    for _ in $(seq 1 30); do
        if adb logcat -d -s QBHarness:* | grep -q "$1"; then return 0; fi
        sleep 0.5
    done
    return 1
}
adb logcat -c
adb shell am force-stop "$PKG"
adb shell am start -n "$PKG/.HarnessActivity" --es mode keyboard >/dev/null
kb=0
waitlog "KEYBOARD_DONE" || { echo "FAIL: keyboard test did not finish"; kb=1; }
for want in "DRAFT example.org/page" "SUBMITTED example.org/page" "DRAFT_AFTER null" \
            "KEYBOARD_REQUESTED true" "TITLE1 clicks=0 text=Typed " "TITLE2 clicks=0 text=x "; do
    adb logcat -d -s QBHarness:* | grep -q "$want" || { echo "FAIL: expected '$want'"; kb=1; }
done
adb logcat -d -s QBHarness:* QuestBrowser:* | tee "$OUT/logcat-keyboard.txt"

echo "== Video test"
adb logcat -c
adb shell am force-stop "$PKG"
adb shell am start -n "$PKG/.HarnessActivity" --es mode video >/dev/null
vid=0
waitlog "VIDEO_DONE" || { echo "FAIL: video test did not finish"; vid=1; }
adb logcat -d -s QBHarness:* QuestBrowser:* | tee "$OUT/logcat-video.txt" | grep -E "VIDEO|render"
adb pull "$DIR/video.png" "$OUT/video.png" >/dev/null 2>&1 || true
grep -q "FAIL" "$OUT/logcat-video.txt" && { echo "FAIL: harness reported a failure in the video test"; vid=1; }
frac=$(grep -o "VIDEO_MAGENTA [0-9.]*" "$OUT/logcat-video.txt" | awk '{print $2}')
awk -v f="${frac:-0}" 'BEGIN { exit !(f >= 0.5) }' || { echo "FAIL: video frames are not drawn (magenta share ${frac:-none}, want >= 0.5)"; vid=1; }

echo "== Fullscreen test"
adb logcat -c
adb shell am force-stop "$PKG"
adb shell am start -n "$PKG/.HarnessActivity" --es mode fullscreen >/dev/null
fs=0
waitlog "FS_DONE" || { echo "FAIL: fullscreen test did not finish"; fs=1; }
adb logcat -d -s QBHarness:* QuestBrowser:* | tee "$OUT/logcat-fullscreen.txt" | grep -E "FS_|Fullscreen"
adb pull "$DIR/fullscreen.png" "$OUT/fullscreen.png" >/dev/null 2>&1 || true
grep -q "FS_STATE true" "$OUT/logcat-fullscreen.txt" || { echo "FAIL: page did not enter fullscreen"; fs=1; }
before=$(grep -o "FS_FPS_BEFORE [0-9]*" "$OUT/logcat-fullscreen.txt" | awk '{print $2}')
after=$(grep -o "FS_FPS_AFTER [0-9]*" "$OUT/logcat-fullscreen.txt" | awk '{print $2}')
# The emulator frame rate varies by about 25% from run to run; a real problem is a much larger drop.
awk -v a="${after:-0}" -v b="${before:-0}" 'BEGIN { exit !(a >= 20 && a >= b * 0.6) }' \
    || { echo "FAIL: frame rate dropped in fullscreen (before ${before:-none}/s, after ${after:-none}/s)"; fs=1; }
fmag=$(grep -o "FS_MAGENTA [0-9.]*" "$OUT/logcat-fullscreen.txt" | awk '{print $2}')
awk -v f="${fmag:-0}" 'BEGIN { exit !(f >= 0.5) }' || { echo "FAIL: video not drawn in fullscreen (magenta ${fmag:-none})"; fs=1; }

echo "== Plain HTTP test"
python3 -m http.server 8765 --bind 127.0.0.1 --directory "$HERE/http" >/dev/null 2>&1 &
HTTP_PID=$!
trap 'kill $HTTP_PID 2>/dev/null' EXIT
sleep 1
adb logcat -c
adb shell am force-stop "$PKG"
adb shell am start -n "$PKG/.HarnessActivity" --es mode http >/dev/null
http=0
waitlog "HTTP_DONE" || true
adb logcat -d -s QBHarness:* QuestBrowser:* | tee "$OUT/logcat-http.txt" | grep -E "HTTP_|leartext"
grep -q "HTTP_TITLE http ok" "$OUT/logcat-http.txt" || { echo "FAIL: plain HTTP page did not load"; http=1; }

echo "== Tabs test"
adb logcat -c
adb shell am force-stop "$PKG"
adb shell am start -n "$PKG/.HarnessActivity" --es mode tabs >/dev/null
tabs=0
waitlog "TABS_DONE" || { echo "FAIL: tabs test did not finish"; tabs=1; }
adb logcat -d -s QBHarness:* QuestBrowser:* | tee "$OUT/logcat-tabs.txt" | grep -E "TABS_"
for want in "TABS_RESTORED count=2 active=1 titles=|tab two" \
            "TABS_NEW count=3 active=2 titles=|tab two|ready" \
            "TABS_SELECTED count=3 active=1" \
            "TABS_CLOSED count=2 active=1 titles=|ready"; do
    grep -qF "$want" "$OUT/logcat-tabs.txt" || { echo "FAIL: expected '$want'"; tabs=1; }
done

echo "== Map install test (needs internet: BeatSaver map 1a2b)"
adb logcat -c
adb shell am force-stop "$PKG"
adb shell am start -n "$PKG/.HarnessActivity" --es mode songs >/dev/null
songs=0
for _ in $(seq 1 120); do
    if adb logcat -d -s QBHarness:* | grep -q "SONGS_DONE"; then break; fi
    sleep 0.5
done
adb logcat -d -s QBHarness:* QuestBrowser:* | tee "$OUT/logcat-songs.txt" | grep -E "SONGS_|map"
for want in "SONGS_URL_AFTER_ONECLICK file:///android_asset/songs.html" "SONGS_INSTALLED 1" \
            "SONGS_FOLDER 1a2b (Believer RU Cover - BCPowerhouse) files=" \
            "SONGS_STATUS Already installed: Believer RU Cover" "SONGS_INSTALLED_AGAIN 0" "SONGS_DONE"; do
    grep -qF "$want" "$OUT/logcat-songs.txt" || { echo "FAIL: expected '$want'"; songs=1; }
done
adb shell ls "$DIR/CustomLevels/1a2b\ \(Believer\ RU\ Cover\ -\ BCPowerhouse\)/" | grep -qi "^info.dat$" \
    || { echo "FAIL: installed map has no Info.dat"; songs=1; }

echo "== Check"
fail=0
grep -q "FAIL" "$OUT/logcat.txt" && { echo "FAIL: harness reported a failure"; fail=1; }
grep -q "TITLE clicks=1 text=hello quest y=[1-9]" "$OUT/logcat.txt" \
    || { echo "FAIL: expected title 'clicks=1 text=hello quest y=>0'"; fail=1; }
[ -s "$OUT/frame1.png" ] && [ -s "$OUT/frame2.png" ] || { echo "FAIL: frames missing"; fail=1; }
echo "Frames: $OUT/frame1.png $OUT/frame2.png"
[ $kb -eq 0 ] || fail=1
[ $vid -eq 0 ] || fail=1
[ $fs -eq 0 ] || fail=1
[ $http -eq 0 ] || fail=1
[ $vp -eq 0 ] || fail=1
[ $tabs -eq 0 ] || fail=1
[ $songs -eq 0 ] || fail=1
[ $fail -eq 0 ] && echo "PASS"
exit $fail
