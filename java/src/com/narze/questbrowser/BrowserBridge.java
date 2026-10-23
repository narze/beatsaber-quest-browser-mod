package com.narze.questbrowser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.FrameLayout;

import org.json.JSONObject;

import java.nio.ByteBuffer;

/**
 * Off-screen WebView that the native mod drives through JNI.
 *
 * All public static methods are safe to call from any thread. WebView work is
 * posted to the Android main thread. Frames are drawn into a Bitmap at a fixed
 * rate and copied into one of two direct ByteBuffers (RGBA, top row first).
 * The native side calls acquireFrame() / releaseFrame() from the Unity thread.
 */
public final class BrowserBridge {
    private static final String TAG = "QuestBrowser";
    private static final Handler ui = new Handler(Looper.getMainLooper());

    private static final Object frameLock = new Object();
    private static ByteBuffer front;
    private static ByteBuffer back;
    private static long frameSeq;
    private static long acquiredSeq;
    private static boolean frontLocked;

    private static int width;
    private static int height;
    private static int frameIntervalMs = 33;
    private static volatile boolean active;
    private static volatile String currentUrl = "";
    private static volatile String currentTitle = "";
    private static volatile int progress = 100;

    private static WebView webView;
    private static FrameLayout container;
    private static Bitmap bitmap;
    private static Canvas canvas;
    private static long downTime;

    private BrowserBridge() {}

    // ---- Lifecycle -------------------------------------------------------

    /** Creates the WebView. host is an Activity or other Context, or null. */
    public static void init(final Object host, final int w, final int h, final String startUrl) {
        synchronized (frameLock) {
            width = w;
            height = h;
            front = ByteBuffer.allocateDirect(w * h * 4);
            back = ByteBuffer.allocateDirect(w * h * 4);
            frameSeq = 0;
            acquiredSeq = 0;
        }
        ui.post(new Runnable() {
            @Override
            public void run() {
                try {
                    create(host, startUrl);
                } catch (Throwable t) {
                    Log.e(TAG, "WebView creation failed", t);
                }
            }
        });
    }

    @SuppressLint("SetJavaScriptEnabled")
    private static void create(Object host, String startUrl) {
        if (webView != null) return;
        Context ctx = resolveContext(host);
        if (ctx == null) {
            Log.e(TAG, "No Context found, can not create WebView");
            return;
        }

        webView = new WebView(ctx);
        webView.setLayerType(View.LAYER_TYPE_SOFTWARE, null);
        webView.setFocusable(true);
        webView.setFocusableInTouchMode(true);
        webView.setVerticalScrollBarEnabled(true);

        WebSettings s = webView.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setUseWideViewPort(true);
        s.setLoadWithOverviewMode(true);
        s.setSupportZoom(true);
        s.setBuiltInZoomControls(true);
        s.setDisplayZoomControls(false);
        s.setMediaPlaybackRequiresUserGesture(false);
        // Links with target=_blank open in this view.
        s.setSupportMultipleWindows(false);

        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                currentUrl = url;
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                currentUrl = url;
            }
        });
        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onReceivedTitle(WebView view, String title) {
                currentTitle = title == null ? "" : title;
            }

            @Override
            public void onProgressChanged(WebView view, int p) {
                progress = p;
            }
        });

        // Chromium only runs timers and animations for a WebView that is in a
        // window. The window of a VR app is not shown in the headset, so we put
        // the view in it and draw it ourselves.
        container = new FrameLayout(ctx);
        container.addView(webView, new FrameLayout.LayoutParams(width, height));
        if (ctx instanceof Activity) {
            ((Activity) ctx).addContentView(container, new ViewGroup.LayoutParams(width, height));
        }
        layout();

        bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        canvas = new Canvas(bitmap);

        webView.loadUrl(startUrl);
        Log.i(TAG, "WebView created " + width + "x" + height + " in " + ctx.getClass().getName());
        if (active) startRenderLoop();
    }

    private static Context resolveContext(Object host) {
        if (host instanceof Context) return (Context) host;
        try {
            Class<?> at = Class.forName("android.app.ActivityThread");
            Object app = at.getMethod("currentApplication").invoke(null);
            if (app instanceof Context) return (Context) app;
        } catch (Throwable t) {
            Log.w(TAG, "ActivityThread.currentApplication failed", t);
        }
        return null;
    }

    private static void layout() {
        int ws = View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY);
        int hs = View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY);
        webView.measure(ws, hs);
        webView.layout(0, 0, width, height);
    }

    /** Starts or stops page rendering, timers, and media. */
    public static void setActive(final boolean value) {
        active = value;
        ui.post(new Runnable() {
            @Override
            public void run() {
                if (webView == null) return;
                if (value) {
                    webView.onResume();
                    webView.resumeTimers();
                    startRenderLoop();
                } else {
                    webView.onPause();
                    webView.pauseTimers();
                }
            }
        });
    }

    public static void setFrameRate(int fps) {
        frameIntervalMs = Math.max(1, 1000 / Math.max(1, Math.min(fps, 90)));
    }

    // ---- Rendering -------------------------------------------------------

    private static boolean loopRunning;

    private static void startRenderLoop() {
        if (loopRunning) return;
        loopRunning = true;
        ui.post(renderTick);
    }

    private static final Runnable renderTick = new Runnable() {
        @Override
        public void run() {
            if (!active || webView == null) {
                loopRunning = false;
                return;
            }
            long start = SystemClock.uptimeMillis();
            try {
                drawFrame();
            } catch (Throwable t) {
                Log.e(TAG, "drawFrame failed", t);
            }
            long spent = SystemClock.uptimeMillis() - start;
            ui.postDelayed(this, Math.max(1, frameIntervalMs - spent));
        }
    };

    private static void drawFrame() {
        if (webView.getWidth() != width || webView.getHeight() != height) layout();
        bitmap.eraseColor(0xFFFFFFFF);
        // WebView draws its scroll position as a canvas translation.
        int save = canvas.save();
        canvas.translate(-webView.getScrollX(), -webView.getScrollY());
        webView.draw(canvas);
        canvas.restoreToCount(save);

        synchronized (frameLock) {
            back.rewind();
            bitmap.copyPixelsToBuffer(back);
            if (!frontLocked) {
                ByteBuffer t = front;
                front = back;
                back = t;
                frameSeq++;
            }
        }
    }

    /**
     * Returns the newest frame if there is a frame that the caller has not
     * seen, else null. The buffer stays valid until releaseFrame().
     */
    public static ByteBuffer acquireFrame() {
        synchronized (frameLock) {
            if (frameSeq == acquiredSeq || front == null) return null;
            acquiredSeq = frameSeq;
            frontLocked = true;
            return front;
        }
    }

    public static void releaseFrame() {
        synchronized (frameLock) {
            frontLocked = false;
        }
    }

    public static int getWidth() {
        return width;
    }

    public static int getHeight() {
        return height;
    }

    // ---- Navigation ------------------------------------------------------

    public static void loadUrl(final String url) {
        currentUrl = url;
        post(new Runnable() {
            @Override
            public void run() {
                webView.loadUrl(url);
            }
        });
    }

    public static void goBack() {
        post(new Runnable() {
            @Override
            public void run() {
                if (webView.canGoBack()) webView.goBack();
            }
        });
    }

    public static void goForward() {
        post(new Runnable() {
            @Override
            public void run() {
                if (webView.canGoForward()) webView.goForward();
            }
        });
    }

    public static void reload() {
        post(new Runnable() {
            @Override
            public void run() {
                webView.reload();
            }
        });
    }

    public static String getUrl() {
        return currentUrl;
    }

    public static String getTitle() {
        return currentTitle;
    }

    public static int getProgress() {
        return progress;
    }

    // ---- Input -----------------------------------------------------------

    /** action is a MotionEvent action: 0 down, 1 up, 2 move, 3 cancel. */
    public static void touch(final int action, final float x, final float y) {
        post(new Runnable() {
            @Override
            public void run() {
                long now = SystemClock.uptimeMillis();
                if (action == MotionEvent.ACTION_DOWN) {
                    downTime = now;
                    if (!webView.hasFocus()) webView.requestFocus();
                }
                MotionEvent e = MotionEvent.obtain(downTime, now, action, x, y, 0);
                e.setSource(InputDevice.SOURCE_TOUCHSCREEN);
                webView.dispatchTouchEvent(e);
                e.recycle();
            }
        });
    }

    /** Sends a mouse wheel event. amount > 0 scrolls up. */
    public static void scroll(final float x, final float y, final float amount) {
        post(new Runnable() {
            @Override
            public void run() {
                MotionEvent.PointerProperties[] props = {new MotionEvent.PointerProperties()};
                props[0].id = 0;
                props[0].toolType = MotionEvent.TOOL_TYPE_MOUSE;
                MotionEvent.PointerCoords[] coords = {new MotionEvent.PointerCoords()};
                coords[0].x = x;
                coords[0].y = y;
                coords[0].setAxisValue(MotionEvent.AXIS_VSCROLL, amount);
                long now = SystemClock.uptimeMillis();
                MotionEvent e = MotionEvent.obtain(now, now, MotionEvent.ACTION_SCROLL, 1, props, coords,
                        0, 0, 1f, 1f, 0, 0, InputDevice.SOURCE_MOUSE, 0);
                webView.dispatchGenericMotionEvent(e);
                e.recycle();
            }
        });
    }

    /** Inserts text at the caret of the focused element in the page. */
    public static void typeText(final String text) {
        post(new Runnable() {
            @Override
            public void run() {
                String js = "(function(t){var e=document.activeElement;"
                        + "if(!e)return;"
                        + "if(!document.execCommand('insertText',false,t)&&'value' in e){"
                        + "e.value+=t;e.dispatchEvent(new Event('input',{bubbles:true}));}"
                        + "})(" + JSONObject.quote(text) + ")";
                webView.evaluateJavascript(js, null);
            }
        });
    }

    /** Sends a key press, for example KeyEvent.KEYCODE_ENTER (66) or KEYCODE_DEL (67). */
    public static void pressKey(final int keyCode) {
        post(new Runnable() {
            @Override
            public void run() {
                webView.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keyCode));
                webView.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keyCode));
            }
        });
    }

    private static void post(final Runnable r) {
        ui.post(new Runnable() {
            @Override
            public void run() {
                if (webView == null) return;
                try {
                    r.run();
                } catch (Throwable t) {
                    Log.e(TAG, "WebView call failed", t);
                }
            }
        });
    }
}
