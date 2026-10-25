package com.narze.questbrowser;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.Log;
import android.view.Choreographer;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.FrameLayout;

import org.json.JSONObject;

import java.nio.ByteBuffer;
import java.util.ArrayList;

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
    // The transport texture stays fixed; the WebView viewport follows the VR window.
    private static int viewWidth;
    private static int viewHeight;
    private static int frameIntervalMs = 33;
    private static volatile boolean active;
    private static volatile String currentUrl = "";
    private static volatile String currentTitle = "";
    private static volatile int progress = 100;

    // The WebView of the active tab. Only this one is in the container, drawn, and gets input.
    private static WebView webView;
    private static FrameLayout container;
    private static Context viewContext;

    // ---- Tabs ------------------------------------------------------------
    // Each tab gets its WebView when it is first opened, so restored tabs do not all
    // load at start. Background tabs are paused. The native side reads the snapshots
    // below (written on the main thread) and decides which tab to open or close.

    private static final int MAX_TABS = 10;

    private static final class Tab {
        String url;
        String title = "";
        WebView view;

        Tab(String url) {
            this.url = url;
        }
    }

    private static final ArrayList<Tab> tabs = new ArrayList<>();
    private static int activeTab;
    private static volatile String[] tabUrls = new String[0];
    private static volatile String[] tabTitles = new String[0];
    private static volatile int activeTabIndex;
    private static volatile int tabsVersion;
    private static Bitmap bitmap;
    private static Canvas canvas;

    // Hardware path: the WebView draws with the GPU into an ImageReader surface, and a
    // copy thread moves each image into the frame buffers. Only this path shows video,
    // because Chromium decodes video on the GPU. If it fails, we use the software
    // path (draw into a Bitmap), which shows pages but not video.
    private static boolean hardwareRender = true;
    private static ImageReader reader;
    private static Surface readerSurface;
    private static HandlerThread copyThread;
    private static long downTime;

    // Hidden address field. The in-game keyboard types into it; the native side
    // shows the draft and loads the URL that is submitted with Enter.
    private static EditText urlField;
    private static volatile boolean editingUrl;
    private static volatile String urlDraft = "";
    private static volatile String submittedUrl;
    // Set when the user taps a text field in the page, so the native side opens its keyboard.
    private static volatile boolean keyboardRequested;

    private BrowserBridge() {}

    // ---- Lifecycle -------------------------------------------------------

    /**
     * Creates the browser with tabs for urls, and opens tab activeIndex.
     * host is an Activity or other Context, or null.
     */
    public static void init(final Object host, final int w, final int h, final String[] urls, final int activeIndex) {
        synchronized (frameLock) {
            width = w;
            height = h;
            viewWidth = w;
            viewHeight = h;
            front = ByteBuffer.allocateDirect(w * h * 4);
            back = ByteBuffer.allocateDirect(w * h * 4);
            frameSeq = 0;
            acquiredSeq = 0;
        }
        ui.post(new Runnable() {
            @Override
            public void run() {
                try {
                    create(host, urls, activeIndex);
                } catch (Throwable t) {
                    Log.e(TAG, "WebView creation failed", t);
                }
            }
        });
    }

    /** Lets the WebView load plain http:// pages, which Beat Saber's network policy blocks. */
    private static void allowCleartextHttp() {
        try {
            libcore.net.NetworkSecurityPolicy current = libcore.net.NetworkSecurityPolicy.getInstance();
            if (!(current instanceof PermissiveNetworkPolicy)) {
                libcore.net.NetworkSecurityPolicy.setInstance(new PermissiveNetworkPolicy(current));
            }
            Log.i(TAG, "Plain HTTP allowed: "
                    + android.security.NetworkSecurityPolicy.getInstance().isCleartextTrafficPermitted("example.com"));
        } catch (Throwable t) {
            Log.w(TAG, "Could not allow plain HTTP; http:// pages will not load", t);
        }
    }

    private static void create(Object host, String[] urls, int activeIndex) {
        if (container != null) return;
        allowCleartextHttp();
        Context ctx = resolveContext(host);
        if (ctx == null) {
            Log.e(TAG, "No Context found, can not create WebView");
            return;
        }
        viewContext = ctx;

        // Chromium only runs timers and animations for a WebView that is in a
        // window. The window of a VR app is not shown in the headset, so we put
        // the view in it and draw it ourselves.
        container = new FrameLayout(ctx) {
            @Override
            protected void dispatchDraw(Canvas c) {
                // The window of a VR app is not shown, and Beat Saber's window draws in
                // software: drawing the page there would cost a CPU redraw for nothing.
                // We draw the page ourselves in drawFrame().
            }
        };
        if (ctx instanceof Activity) {
            ((Activity) ctx).addContentView(container, new ViewGroup.LayoutParams(width, height));
        }
        createUrlField(ctx);

        bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        canvas = new Canvas(bitmap);
        createReader();

        for (String url : urls) {
            if (tabs.size() < MAX_TABS && url != null && !url.isEmpty()) tabs.add(new Tab(url));
        }
        if (tabs.isEmpty()) tabs.add(new Tab("about:blank"));
        activeTab = -1;
        activateTab(Math.max(0, Math.min(activeIndex, tabs.size() - 1)));
        Log.i(TAG, "Browser created " + width + "x" + height + " with " + tabs.size() + " tabs in " + ctx.getClass().getName());
        if (active) startRenderLoop();
    }

    @SuppressLint("SetJavaScriptEnabled")
    private static WebView newWebView(final Tab tab) {
        WebView view = new WebView(viewContext);
        view.setFocusable(true);
        view.setFocusableInTouchMode(true);
        view.setVerticalScrollBarEnabled(true);
        if (!hardwareRender) view.setLayerType(View.LAYER_TYPE_SOFTWARE, null);

        WebSettings s = view.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setUseWideViewPort(true);
        s.setLoadWithOverviewMode(true);
        s.setSupportZoom(true);
        s.setBuiltInZoomControls(true);
        s.setDisplayZoomControls(false);
        s.setMediaPlaybackRequiresUserGesture(false);
        // Links with target=_blank open in this tab.
        s.setSupportMultipleWindows(false);

        view.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageStarted(WebView v, String url, Bitmap favicon) {
                tab.url = url;
                if (v == webView) currentUrl = url;
                publishTabs();
            }

            @Override
            public void onPageFinished(WebView v, String url) {
                tab.url = url;
                if (v == webView) currentUrl = url;
                publishTabs();
            }
        });
        view.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onReceivedTitle(WebView v, String title) {
                tab.title = title == null ? "" : title;
                if (v == webView) currentTitle = tab.title;
                publishTabs();
            }

            @Override
            public void onProgressChanged(WebView v, int p) {
                if (v == webView) progress = p;
            }

            // A page asked for fullscreen (for example the video player). WebView gives us
            // a view with the fullscreen content; we draw it, and send input to it, instead
            // of the page until the page leaves fullscreen.
            @Override
            public void onShowCustomView(View fullscreen, CustomViewCallback callback) {
                if (tab.view != webView) {
                    // Only the open tab can go fullscreen.
                    callback.onCustomViewHidden();
                    return;
                }
                if (customView != null) exitFullscreen();
                customView = fullscreen;
                customViewCallback = callback;
                container.addView(fullscreen, new FrameLayout.LayoutParams(viewWidth, viewHeight));
                layout();
                fullscreen.requestFocus();
                Log.i(TAG, "Fullscreen on");
            }

            @Override
            public void onHideCustomView() {
                removeCustomView();
            }
        });
        return view;
    }

    /** Main thread: makes tab index the active tab. Creates and loads its WebView on first use. */
    private static void activateTab(int index) {
        if (index < 0 || index >= tabs.size() || index == activeTab) return;
        if (customView != null) exitFullscreen();
        editingUrl = false;
        if (webView != null) {
            webView.onPause();
            container.removeView(webView);
        }
        activeTab = index;
        Tab tab = tabs.get(index);
        boolean fresh = tab.view == null;
        if (fresh) tab.view = newWebView(tab);
        webView = tab.view;
        container.addView(webView, 0, new FrameLayout.LayoutParams(viewWidth, viewHeight));
        layout();
        if (fresh) webView.loadUrl(tab.url);
        if (active) webView.onResume();
        else webView.onPause();
        webView.requestFocus();
        currentUrl = tab.url;
        currentTitle = tab.title;
        progress = 100;
        publishTabs();
    }

    /** Main thread: copies the tab list for the native side. */
    private static void publishTabs() {
        String[] urls = new String[tabs.size()];
        String[] titles = new String[tabs.size()];
        for (int i = 0; i < tabs.size(); i++) {
            urls[i] = tabs.get(i).url;
            titles[i] = tabs.get(i).title;
        }
        tabUrls = urls;
        tabTitles = titles;
        activeTabIndex = activeTab;
        tabsVersion++;
    }

    /** Opens a new tab with url and makes it active. Does nothing at the tab limit. */
    public static void newTab(final String url) {
        ui.post(() -> {
            if (container == null || tabs.size() >= MAX_TABS) return;
            tabs.add(new Tab(url));
            activateTab(tabs.size() - 1);
        });
    }

    /** Opens tab index. */
    public static void selectTab(final int index) {
        ui.post(() -> {
            if (container != null) activateTab(index);
        });
    }

    /**
     * Closes tab index; newActive is the active tab afterwards (index in the list
     * without the closed tab). The native side keeps at least one tab.
     */
    public static void closeTab(final int index, final int newActive) {
        ui.post(() -> {
            if (container == null || index < 0 || index >= tabs.size() || tabs.size() < 2) return;
            Tab tab = tabs.remove(index);
            if (index == activeTab) {
                if (customView != null) exitFullscreen();
                container.removeView(webView);
                webView = null;
                activeTab = -1;
                activateTab(Math.max(0, Math.min(newActive, tabs.size() - 1)));
            } else {
                if (index < activeTab) activeTab--;
                publishTabs();
            }
            if (tab.view != null) tab.view.destroy();
        });
    }

    /** Changes each time a tab opens, closes, or changes its URL or title. */
    public static int getTabsVersion() {
        return tabsVersion;
    }

    public static String[] getTabUrls() {
        return tabUrls;
    }

    public static String[] getTabTitles() {
        return tabTitles;
    }

    public static int getActiveTab() {
        return activeTabIndex;
    }

    private static void createUrlField(Context ctx) {
        urlField = new EditText(ctx);
        urlField.setSingleLine(true);
        urlField.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        urlField.setImeOptions(EditorInfo.IME_ACTION_GO | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        urlField.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                urlDraft = s.toString();
            }
        });
        urlField.setOnEditorActionListener((v, actionId, event) -> {
            if (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER) {
                // Act on key down only, and consume the matching key up.
                if (event.getAction() != KeyEvent.ACTION_DOWN) return true;
            } else if (actionId != EditorInfo.IME_ACTION_GO && actionId != EditorInfo.IME_ACTION_DONE) {
                return false;
            }
            submittedUrl = v.getText().toString();
            endUrlEditing();
            return true;
        });
        urlField.setOnFocusChangeListener((v, hasFocus) -> {
            if (!hasFocus) editingUrl = false;
        });
        container.addView(urlField, new FrameLayout.LayoutParams(width, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    private static void endUrlEditing() {
        editingUrl = false;
        webView.requestFocus();
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
        int ws = View.MeasureSpec.makeMeasureSpec(viewWidth, View.MeasureSpec.EXACTLY);
        int hs = View.MeasureSpec.makeMeasureSpec(viewHeight, View.MeasureSpec.EXACTLY);
        if (webView != null) {
            webView.measure(ws, hs);
            webView.layout(0, 0, viewWidth, viewHeight);
        }
        if (customView != null) {
            customView.measure(ws, hs);
            customView.layout(0, 0, viewWidth, viewHeight);
        }
    }

    public static void setViewport(final int w, final int h) {
        if (w < 1 || h < 1 || w > 4096 || h > 4096) return;
        post(() -> {
            viewWidth = w;
            viewHeight = h;
            if (container.getLayoutParams() != null) {
                ViewGroup.LayoutParams params = container.getLayoutParams();
                params.width = w;
                params.height = h;
                container.setLayoutParams(params);
            }
            if (webView != null) webView.setLayoutParams(new FrameLayout.LayoutParams(w, h));
            if (customView != null) customView.setLayoutParams(new FrameLayout.LayoutParams(w, h));
            layout();
            if (webView != null) webView.invalidate();
            Log.i(TAG, "Viewport " + w + "x" + h);
        });
    }

    // ---- Fullscreen ------------------------------------------------------

    private static View customView;
    private static WebChromeClient.CustomViewCallback customViewCallback;

    /** The view to draw and to send input to: the fullscreen view if there is one, else the page. */
    private static View target() {
        return customView != null ? customView : webView;
    }

    private static void removeCustomView() {
        if (customView == null) return;
        container.removeView(customView);
        customView = null;
        customViewCallback = null;
        webView.requestFocus();
        Log.i(TAG, "Fullscreen off");
    }

    /** True while a page shows fullscreen content. */
    public static boolean isFullscreen() {
        return customView != null;
    }

    /** Asks the page to leave fullscreen. WebView then calls onHideCustomView. */
    private static void exitFullscreen() {
        WebChromeClient.CustomViewCallback callback = customViewCallback;
        if (callback != null) callback.onCustomViewHidden();
        removeCustomView();
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
                    editingUrl = false;
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
        nextFrameNanos = 0;
        Choreographer.getInstance().postFrameCallback(renderTick);
    }

    private static long nextFrameNanos;

    // Runs on each display vsync and draws when the next frame at the target rate is due,
    // so frames come at even times.
    private static final Choreographer.FrameCallback renderTick = new Choreographer.FrameCallback() {
        @Override
        public void doFrame(long frameTimeNanos) {
            if (!active || webView == null) {
                loopRunning = false;
                return;
            }
            long interval = frameIntervalMs * 1_000_000L;
            // Draw up to half a 90 Hz vsync early, else 60 fps would round down to 45.
            if (frameTimeNanos >= nextFrameNanos - 5_500_000L) {
                // After a stall, start again from now instead of drawing a burst.
                nextFrameNanos = Math.max(nextFrameNanos + interval, frameTimeNanos);
                long start = SystemClock.uptimeMillis();
                try {
                    drawFrame();
                } catch (Throwable t) {
                    Log.e(TAG, "drawFrame failed", t);
                }
                stats(SystemClock.uptimeMillis() - start);
            }
            Choreographer.getInstance().postFrameCallback(this);
        }
    };

    private static void createReader() {
        try {
            reader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, 3,
                    HardwareBuffer.USAGE_CPU_READ_OFTEN | HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
            copyThread = new HandlerThread("QuestBrowserCopy");
            copyThread.start();
            reader.setOnImageAvailableListener(r -> {
                Image image = r.acquireLatestImage();
                if (image == null) return;
                long t0 = SystemClock.elapsedRealtimeNanos();
                try {
                    copyImage(image);
                    copies++;
                    copyNanos += SystemClock.elapsedRealtimeNanos() - t0;
                } finally {
                    image.close();
                }
            }, new Handler(copyThread.getLooper()));
            readerSurface = reader.getSurface();
        } catch (Throwable t) {
            Log.w(TAG, "ImageReader not available, using software rendering", t);
            useSoftwareRender();
        }
    }

    private static void useSoftwareRender() {
        hardwareRender = false;
        // Tabs created later get the software layer in newWebView().
        for (Tab tab : tabs) {
            if (tab.view != null) tab.view.setLayerType(View.LAYER_TYPE_SOFTWARE, null);
        }
        Log.i(TAG, "Render path: software (no video)");
    }

    private static boolean loggedHardware;

    // Performance counters, logged every 30 seconds while the page runs.
    private static volatile int copies;
    private static volatile long copyNanos;
    private static int draws;
    private static long drawMillis;
    private static long statsStart;

    private static void stats(long drawMs) {
        draws++;
        drawMillis += drawMs;
        long now = SystemClock.uptimeMillis();
        if (statsStart == 0) statsStart = now;
        long span = now - statsStart;
        if (span < 30000) return;
        float sec = span / 1000f;
        Log.i(TAG, String.format(java.util.Locale.US,
                "Stats: draw %.1f/s avg %.1f ms, copy %.1f/s avg %.1f ms, path %s",
                draws / sec, draws == 0 ? 0f : drawMillis / (float) draws,
                copies / sec, copies == 0 ? 0f : copyNanos / 1e6f / copies,
                hardwareRender ? "hardware" : "software"));
        draws = 0;
        drawMillis = 0;
        copies = 0;
        copyNanos = 0;
        statsStart = now;
    }

    private static void drawFrame() {
        if (webView.getWidth() != viewWidth || webView.getHeight() != viewHeight
                || (customView != null && (customView.getWidth() != viewWidth || customView.getHeight() != viewHeight))) {
            layout();
        }
        if (hardwareRender) {
            try {
                Canvas c = readerSurface.lockHardwareCanvas();
                int save = c.save();
                try {
                    c.drawColor(Color.WHITE);
                    c.scale(width / (float) viewWidth, height / (float) viewHeight);
                    // WebView draws its scroll position as a canvas translation.
                    View v = target();
                    c.translate(-v.getScrollX(), -v.getScrollY());
                    v.draw(c);
                } finally {
                    c.restoreToCount(save);
                    readerSurface.unlockCanvasAndPost(c);
                }
                if (!loggedHardware) {
                    loggedHardware = true;
                    Log.i(TAG, "Render path: hardware");
                }
                return;
            } catch (Throwable t) {
                Log.w(TAG, "Hardware rendering failed", t);
                useSoftwareRender();
            }
        }
        bitmap.eraseColor(0xFFFFFFFF);
        // WebView draws its scroll position as a canvas translation.
        int save = canvas.save();
        canvas.scale(width / (float) viewWidth, height / (float) viewHeight);
        View v = target();
        canvas.translate(-v.getScrollX(), -v.getScrollY());
        v.draw(canvas);
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

    /** Copy thread: moves an RGBA image into the back buffer, then swaps it to the front. */
    private static void copyImage(Image image) {
        Image.Plane plane = image.getPlanes()[0];
        ByteBuffer src = plane.getBuffer();
        int rowStride = plane.getRowStride();
        int rowBytes = width * 4;
        synchronized (frameLock) {
            back.rewind();
            if (rowStride == rowBytes) {
                ByteBuffer all = src.duplicate();
                all.limit(Math.min(all.capacity(), rowBytes * height));
                back.put(all);
            } else {
                // Rows can have padding at the end; copy the pixels of each row only.
                ByteBuffer row = src.duplicate();
                for (int y = 0; y < height; y++) {
                    row.limit(y * rowStride + rowBytes);
                    row.position(y * rowStride);
                    back.put(row);
                }
            }
            back.rewind();
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
                if (customView != null) exitFullscreen();
                webView.loadUrl(url);
            }
        });
    }

    public static void goBack() {
        post(new Runnable() {
            @Override
            public void run() {
                // Back leaves fullscreen first, like on a phone.
                if (customView != null) exitFullscreen();
                else if (webView.canGoBack()) webView.goBack();
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
                View t = target();
                if (action == MotionEvent.ACTION_DOWN) {
                    downTime = now;
                    if (!t.hasFocus()) t.requestFocus();
                }
                MotionEvent e = MotionEvent.obtain(downTime, now, action,
                        x * viewWidth / width, y * viewHeight / height, 0);
                e.setSource(InputDevice.SOURCE_TOUCHSCREEN);
                t.dispatchTouchEvent(e);
                e.recycle();
                if (action == MotionEvent.ACTION_UP) ui.postDelayed(BrowserBridge::showImeIfEditableFocused, 150);
            }
        });
    }

    private static final String IS_EDITABLE_JS = "(function(){var e=document.activeElement;"
            + "return !!e&&(e.isContentEditable||/^(INPUT|TEXTAREA)$/.test(e.tagName));})()";

    private static void showImeIfEditableFocused() {
        if (webView == null) return;
        webView.evaluateJavascript(IS_EDITABLE_JS, result -> {
            if ("true".equals(result)) keyboardRequested = true;
        });
    }

    // ---- Keyboard --------------------------------------------------------
    // The keyboard is drawn in the game. These calls send its keys to the
    // focused field: the hidden address field while it is in use, else the page.

    /** Starts typing an address in the hidden address field, filled with url and selected. */
    public static void beginUrlEditing(final String url) {
        post(new Runnable() {
            @Override
            public void run() {
                urlField.setText(url);
                urlField.selectAll();
                urlField.requestFocus();
                editingUrl = true;
            }
        });
    }

    /** The address being typed, or null when the address field is not in use. */
    public static String getUrlDraft() {
        return editingUrl ? urlDraft : null;
    }

    /** Returns the address submitted with Enter once, else null. */
    public static String takeSubmittedUrl() {
        String url = submittedUrl;
        submittedUrl = null;
        return url;
    }

    /** True once after the user tapped a text field in the page. */
    public static boolean takeKeyboardRequest() {
        boolean requested = keyboardRequested;
        keyboardRequested = false;
        return requested;
    }

    /** Types text into the focused field. */
    public static void sendText(final String text) {
        post(new Runnable() {
            @Override
            public void run() {
                if (editingUrl) {
                    Editable e = urlField.getText();
                    int a = Math.max(0, Math.min(urlField.getSelectionStart(), urlField.getSelectionEnd()));
                    int b = Math.max(0, Math.max(urlField.getSelectionStart(), urlField.getSelectionEnd()));
                    e.replace(a, b, text);
                    return;
                }
                if (!webView.hasFocus()) webView.requestFocus();
                // Key events work in every page; text that has no keys (for example Thai) is inserted with JS.
                KeyEvent[] events = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD).getEvents(text.toCharArray());
                if (events != null) {
                    for (KeyEvent e : events) webView.dispatchKeyEvent(e);
                } else {
                    typeText(text);
                }
            }
        });
    }

    /** Sends a key press with a meta state, for example KEYCODE_DEL (67) or Ctrl+A (29, META_CTRL_ON). */
    public static void sendKey(final int keyCode, final int metaState) {
        post(new Runnable() {
            @Override
            public void run() {
                if (editingUrl && keyCode == KeyEvent.KEYCODE_ESCAPE) {
                    endUrlEditing();
                    return;
                }
                if (customView != null && keyCode == KeyEvent.KEYCODE_ESCAPE) {
                    exitFullscreen();
                    return;
                }
                View target = editingUrl ? urlField : target();
                if (target != urlField && !target.hasFocus()) target.requestFocus();
                long now = SystemClock.uptimeMillis();
                target.dispatchKeyEvent(new KeyEvent(now, now, KeyEvent.ACTION_DOWN, keyCode, 0, metaState));
                target.dispatchKeyEvent(new KeyEvent(now, now, KeyEvent.ACTION_UP, keyCode, 0, metaState));
            }
        });
    }

    /** Stops typing an address, if that is in progress. */
    public static void cancelUrlEditing() {
        post(new Runnable() {
            @Override
            public void run() {
                if (editingUrl) endUrlEditing();
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
                coords[0].x = x * viewWidth / width;
                coords[0].y = y * viewHeight / height;
                coords[0].setAxisValue(MotionEvent.AXIS_VSCROLL, amount);
                long now = SystemClock.uptimeMillis();
                MotionEvent e = MotionEvent.obtain(now, now, MotionEvent.ACTION_SCROLL, 1, props, coords,
                        0, 0, 1f, 1f, 0, 0, InputDevice.SOURCE_MOUSE, 0);
                target().dispatchGenericMotionEvent(e);
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
