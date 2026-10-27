package com.narze.questbrowser.harness;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;

import dalvik.system.InMemoryDexClassLoader;

/**
 * Loads questbrowser.dex the same way the native mod does and drives the
 * BrowserBridge API: tap an input, type text, tap a button, scroll, and save
 * frames. run.sh checks the page title and the PNG files.
 */
public class HarnessActivity extends Activity {
    private static final String TAG = "QBHarness";
    private static final int W = 1280;
    private static final int H = 720;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private Class<?> bridge;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        // Same hidden API exemption as the mod does before it starts the browser.
        System.loadLibrary("qbharness");
        try {
            byte[] dex = readAsset("questbrowser.dex");
            ClassLoader loader = new InMemoryDexClassLoader(ByteBuffer.wrap(dex), getClassLoader());
            bridge = loader.loadClass("com.narze.questbrowser.BrowserBridge");
            boolean tabsMode = "tabs".equals(getIntent().getStringExtra("mode"));
            // Tabs test: restore two tabs with the second one open, like after a restart.
            String[] urls = tabsMode
                    ? new String[] {"file:///android_asset/test.html", "file:///android_asset/tab2.html"}
                    : new String[] {"file:///android_asset/test.html"};
            call("init", new Class<?>[] {Object.class, int.class, int.class, String[].class, int.class},
                    this, W, H, urls, tabsMode ? 1 : 0);
            call("setActive", new Class<?>[] {boolean.class}, true);
        } catch (Throwable t) {
            Log.e(TAG, "FAIL setup", t);
            return;
        }

        if ("tabs".equals(getIntent().getStringExtra("mode"))) {
            runTabsTest();
            return;
        }
        if ("songs".equals(getIntent().getStringExtra("mode"))) {
            runSongsTest();
            return;
        }
        if ("viewport".equals(getIntent().getStringExtra("mode"))) {
            call("loadUrl", new Class<?>[] {String.class}, "file:///android_asset/viewport.html");
            whenTitle("wide clicks=0", () -> {
                call("setViewport", new Class<?>[] {int.class, int.class}, 720, 1280);
                whenTitle("portrait clicks=0", () -> {
                    tap(W * 0.25f, H * 0.25f);
                    whenTitle("portrait clicks=1", () -> {
                        Log.i(TAG, "VIEWPORT_PORTRAIT_CLICK_OK");
                        call("setViewport", new Class<?>[] {int.class, int.class}, 1600, 800);
                        whenTitle("wide clicks=1", () -> {
                            tap(W * 0.25f, H * 0.25f);
                            whenTitle("wide clicks=2", () -> {
                                Log.i(TAG, "VIEWPORT_WIDE_CLICK_OK");
                                saveFrame("viewport.png");
                                Log.i(TAG, "VIEWPORT_DONE");
                            });
                        });
                    });
                });
            });
            return;
        }
        if ("keyboard".equals(getIntent().getStringExtra("mode"))) {
            runKeyboardTest();
            return;
        }
        if ("http".equals(getIntent().getStringExtra("mode"))) {
            // run.sh serves tests/android-harness/http on the host; 10.0.2.2 is the host from the emulator.
            call("loadUrl", new Class<?>[] {String.class}, "http://10.0.2.2:8765/index.html");
            whenTitle("http ok", () -> {
                Log.i(TAG, "HTTP_TITLE " + call("getTitle", new Class<?>[0]));
                Log.i(TAG, "HTTP_DONE");
            }, 50);
            return;
        }
        if ("fullscreen".equals(getIntent().getStringExtra("mode"))) {
            call("setFrameRate", new Class<?>[] {int.class}, 60);
            call("loadUrl", new Class<?>[] {String.class}, "file:///android_asset/video.html");
            whenTitle("playing", () -> {
                at(1000, () -> measureFps("FS_FPS_BEFORE"));
                at(3500, () -> tap(W * 0.25f, H * 0.25f));
                at(5500, () -> {
                    Log.i(TAG, "FS_STATE " + call("isFullscreen", new Class<?>[0]));
                    measureFps("FS_FPS_AFTER");
                });
                at(8000, () -> {
                    Log.i(TAG, "FS_MAGENTA " + magentaFraction());
                    saveFrame("fullscreen.png");
                    Log.i(TAG, "FS_DONE");
                });
            });
            return;
        }
        if ("video".equals(getIntent().getStringExtra("mode"))) {
            call("loadUrl", new Class<?>[] {String.class}, "file:///android_asset/video.html");
            at(6500, () -> saveFrame("video.png"));
            at(7000, () -> {
                Log.i(TAG, "VIDEO_TITLE " + call("getTitle", new Class<?>[0]));
                Log.i(TAG, "VIDEO_MAGENTA " + magentaFraction());
                Log.i(TAG, "VIDEO_DONE");
            });
            return;
        }

        // Centers of the elements in test.html, in page pixels.
        final float inputX = W / 2f, inputY = H * 0.10f;
        final float buttonX = W / 2f, buttonY = H * 0.40f;

        // Start when test.html has loaded, so that a slow first start does not lose the taps.
        whenTitle("ready", () -> {
        at(500, () -> {
            saveFrame("frame1.png");
            tap(inputX, inputY);
        });
        at(1500, () -> call("typeText", new Class<?>[] {String.class}, "hello quest"));
        at(2500, () -> tap(buttonX, buttonY));
        at(3500, () -> call("scroll", new Class<?>[] {float.class, float.class, float.class}, W / 2f, H * 0.8f, -5f));
        at(5500, () -> {
            saveFrame("frame2.png");
            Log.i(TAG, "TITLE " + call("getTitle", new Class<?>[0]));
            Log.i(TAG, "URL " + call("getUrl", new Class<?>[0]));
            Log.i(TAG, "DONE");
        });
        });
    }

    /**
     * Keyboard test. Drives the same bridge calls as the in-game keyboard:
     * 1. Address: beginUrlEditing, sendText, Enter.
     * 2. Page field: tap it (the bridge asks for the keyboard), sendText, Del, Ctrl+A.
     */
    private void runKeyboardTest() {
        final Class<?>[] text = {String.class};
        final Class<?>[] key = {int.class, int.class};
        at(3000, () -> {
            call("beginUrlEditing", text, "https://old.example/");
            call("sendText", text, "example.org/page");
        });
        at(3500, () -> Log.i(TAG, "DRAFT " + call("getUrlDraft", new Class<?>[0])));
        at(3600, () -> call("sendKey", key, 66, 0));
        at(4000, () -> {
            Log.i(TAG, "SUBMITTED " + call("takeSubmittedUrl", new Class<?>[0]));
            Log.i(TAG, "DRAFT_AFTER " + call("getUrlDraft", new Class<?>[0]));
            call("takeKeyboardRequest", new Class<?>[0]);  // clear
            tap(W / 2f, H * 0.10f);
        });
        at(5000, () -> {
            Log.i(TAG, "KEYBOARD_REQUESTED " + call("takeKeyboardRequest", new Class<?>[0]));
            call("sendText", text, "Typed!");
            call("sendKey", key, 67, 0);  // Del removes "!"
        });
        at(6000, () -> Log.i(TAG, "TITLE1 " + call("getTitle", new Class<?>[0])));
        at(6100, () -> {
            call("sendKey", key, 29, 0x1000);  // Ctrl+A selects all
            call("sendText", text, "x");       // replaces the selection
        });
        at(7000, () -> {
            Log.i(TAG, "TITLE2 " + call("getTitle", new Class<?>[0]));
            Log.i(TAG, "KEYBOARD_DONE");
        });
    }

    /** Counts new frames from the bridge for 2 seconds on a background thread, then logs "label fps". */
    private void measureFps(String label) {
        new Thread(() -> {
            int frames = 0;
            long end = android.os.SystemClock.uptimeMillis() + 2000;
            while (android.os.SystemClock.uptimeMillis() < end) {
                Object buf = call("acquireFrame", new Class<?>[0]);
                if (buf != null) {
                    frames++;
                    call("releaseFrame", new Class<?>[0]);
                }
                try { Thread.sleep(4); } catch (InterruptedException ignored) {}
            }
            Log.i(TAG, label + " " + (frames / 2));
        }).start();
    }

    /** Share of pixels in the newest frame that are close to magenta (255, 0, 255). */
    private String magentaFraction() {
        ByteBuffer buf = (ByteBuffer) call("acquireFrame", new Class<?>[0]);
        if (buf == null) {
            // No new frame since the last one; wait for the next one.
            try { Thread.sleep(100); } catch (InterruptedException ignored) {}
            buf = (ByteBuffer) call("acquireFrame", new Class<?>[0]);
            if (buf == null) return "noframe";
        }
        try {
            int magenta = 0, total = 0;
            for (int y = 0; y < H; y += 8) {
                for (int x = 0; x < W; x += 8) {
                    int i = (y * W + x) * 4;
                    int r = buf.get(i) & 0xFF, g = buf.get(i + 1) & 0xFF, b = buf.get(i + 2) & 0xFF;
                    if (r > 180 && g < 90 && b > 180) magenta++;
                    total++;
                }
            }
            return String.format(java.util.Locale.US, "%.2f", magenta / (float) total);
        } finally {
            call("releaseFrame", new Class<?>[0]);
        }
    }

    private void logTabs(String label) {
        String[] urls = (String[]) call("getTabUrls", new Class<?>[0]);
        String[] titles = (String[]) call("getTabTitles", new Class<?>[0]);
        Log.i(TAG, label + " count=" + urls.length + " active=" + call("getActiveTab", new Class<?>[0])
                + " titles=" + String.join("|", titles));
    }

    /** Restore, lazy loading, new tab, switch, close. run.sh checks the log lines. */
    private void runTabsTest() {
        whenTitle("tab two", () -> {
            logTabs("TABS_RESTORED");  // tab 0 is not loaded yet, so its title is empty
            call("newTab", new Class<?>[] {String.class}, "file:///android_asset/test.html");
            at(300, () -> whenTitle("ready", () -> {
                logTabs("TABS_NEW");
                call("selectTab", new Class<?>[] {int.class}, 1);
                at(300, () -> whenTitle("tab two", () -> {
                    logTabs("TABS_SELECTED");
                    // Close the open tab (1); the next tab (the new one) becomes active at index 1.
                    call("closeTab", new Class<?>[] {int.class, int.class}, 1, 1);
                    at(300, () -> whenTitle("ready", () -> {
                        logTabs("TABS_CLOSED");
                        Log.i(TAG, "TABS_DONE");
                    }, 50));
                }, 50));
            }, 50));
        }, 50);
    }

    /** One-Click link, then the zip link of the same map. run.sh checks the log lines and the folder. */
    private void runSongsTest() {
        File songs = new File(getExternalFilesDir(null), "CustomLevels");
        deleteTree(songs);
        call("setSongsDir", new Class<?>[] {String.class}, songs.getPath());
        call("loadUrl", new Class<?>[] {String.class}, "file:///android_asset/songs.html");
        whenTitle("songs", () -> {
            tap(W / 2f, H / 4f);  // One-Click link
            whenSongStatus("Installed", () -> {
                Log.i(TAG, "SONGS_URL_AFTER_ONECLICK " + call("getUrl", new Class<?>[0]));
                Log.i(TAG, "SONGS_INSTALLED " + call("takeSongsInstalled", new Class<?>[0]));
                String[] names = songs.list();
                for (String n : names == null ? new String[0] : names) {
                    String[] files = new File(songs, n).list();
                    Log.i(TAG, "SONGS_FOLDER " + n + " files=" + (files == null ? 0 : files.length));
                }
                tap(W / 2f, H * 3 / 4f);  // zip link of the same map
                whenSongStatus("Already installed", () -> {
                    Log.i(TAG, "SONGS_INSTALLED_AGAIN " + call("takeSongsInstalled", new Class<?>[0]));
                    Log.i(TAG, "SONGS_DONE");
                });
            });
        });
    }

    /** Runs r once the song status starts with prefix (checks every 200 ms, up to 60 s). */
    private void whenSongStatus(String prefix, Runnable r) {
        whenSongStatus(prefix, r, 300);
    }

    private void whenSongStatus(String prefix, Runnable r, int triesLeft) {
        String status = (String) call("getSongStatus", new Class<?>[0]);
        if (status != null && !status.isEmpty()) Log.i(TAG, "SONGS_STATUS " + status);
        if ((status != null && status.startsWith(prefix)) || triesLeft == 0) {
            r.run();
            return;
        }
        at(200, () -> whenSongStatus(prefix, r, triesLeft - 1));
    }

    private static void deleteTree(File f) {
        File[] children = f.listFiles();
        if (children != null) for (File child : children) deleteTree(child);
        f.delete();
    }

    /** Runs r once the page title is title (checks every 200 ms, up to 20 s). */
    private void whenTitle(String title, Runnable r) {
        whenTitle(title, r, 100);
    }

    private void whenTitle(String title, Runnable r, int triesLeft) {
        if (title.equals(call("getTitle", new Class<?>[0])) || triesLeft == 0) {
            r.run();
            return;
        }
        at(200, () -> whenTitle(title, r, triesLeft - 1));
    }

    private void tap(float x, float y) {
        Class<?>[] sig = {int.class, float.class, float.class};
        call("touch", sig, 0, x, y);
        call("touch", sig, 1, x, y);
    }

    private void saveFrame(String name) {
        ByteBuffer buf = (ByteBuffer) call("acquireFrame", new Class<?>[0]);
        if (buf == null) {
            Log.e(TAG, "FAIL no frame for " + name);
            return;
        }
        try {
            Bitmap bmp = Bitmap.createBitmap(W, H, Bitmap.Config.ARGB_8888);
            buf.rewind();
            bmp.copyPixelsFromBuffer(buf);
            File out = new File(getExternalFilesDir(null), name);
            try (FileOutputStream fos = new FileOutputStream(out)) {
                bmp.compress(Bitmap.CompressFormat.PNG, 100, fos);
            }
            Log.i(TAG, "SAVED " + out);
        } catch (Exception e) {
            Log.e(TAG, "FAIL save " + name, e);
        } finally {
            call("releaseFrame", new Class<?>[0]);
        }
    }

    private Object call(String name, Class<?>[] sig, Object... args) {
        try {
            Method m = bridge.getMethod(name, sig);
            return m.invoke(null, args);
        } catch (Exception e) {
            Log.e(TAG, "FAIL call " + name, e);
            return null;
        }
    }

    private void at(long delayMs, Runnable r) {
        handler.postDelayed(r, delayMs);
    }

    private byte[] readAsset(String name) throws Exception {
        try (InputStream in = getAssets().open(name)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] b = new byte[8192];
            int n;
            while ((n = in.read(b)) > 0) out.write(b, 0, n);
            return out.toByteArray();
        }
    }
}
