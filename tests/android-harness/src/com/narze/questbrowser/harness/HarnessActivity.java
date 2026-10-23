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
        try {
            byte[] dex = readAsset("questbrowser.dex");
            ClassLoader loader = new InMemoryDexClassLoader(ByteBuffer.wrap(dex), getClassLoader());
            bridge = loader.loadClass("com.narze.questbrowser.BrowserBridge");
            call("init", new Class<?>[] {Object.class, int.class, int.class, String.class},
                    this, W, H, "file:///android_asset/test.html");
            call("setActive", new Class<?>[] {boolean.class}, true);
        } catch (Throwable t) {
            Log.e(TAG, "FAIL setup", t);
            return;
        }

        if ("keyboard".equals(getIntent().getStringExtra("mode"))) {
            runKeyboardTest();
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

        at(3000, () -> {
            saveFrame("frame1.png");
            tap(inputX, inputY);
        });
        at(4000, () -> call("typeText", new Class<?>[] {String.class}, "hello quest"));
        at(5000, () -> tap(buttonX, buttonY));
        at(6000, () -> call("scroll", new Class<?>[] {float.class, float.class, float.class}, W / 2f, H * 0.8f, -5f));
        at(8000, () -> {
            saveFrame("frame2.png");
            Log.i(TAG, "TITLE " + call("getTitle", new Class<?>[0]));
            Log.i(TAG, "URL " + call("getUrl", new Class<?>[0]));
            Log.i(TAG, "DONE");
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
