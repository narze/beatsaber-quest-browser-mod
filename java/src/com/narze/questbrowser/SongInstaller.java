package com.narze.questbrowser;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Installs BeatSaver maps that the browser opens (One-Click links and map downloads) into
 * the SongCore song folder. One map at a time, on a background thread. The native side
 * shows status() in the toolbar and reloads SongCore after takeInstalled() is not 0.
 */
final class SongInstaller {
    private static final String TAG = "QuestBrowser";
    private static final String USER_AGENT = "QuestBrowser (Beat Saber mod)";
    private static final int TIMEOUT_MS = 20000;
    // Limits against broken or hostile zip files.
    private static final long MAX_BYTES = 200L * 1024 * 1024;
    private static final int MAX_ENTRIES = 1000;
    private static final long STATUS_MS = 5000;

    private static final ExecutorService worker = Executors.newSingleThreadExecutor(r -> {
        Thread t = new Thread(r, "QuestBrowserSongs");
        t.setDaemon(true);
        return t;
    });
    private static final Set<String> queued = ConcurrentHashMap.newKeySet();
    private static final AtomicInteger installed = new AtomicInteger();
    private static volatile String songsDir;
    private static volatile String status = "";
    // When status stops showing, in System.currentTimeMillis(). 0: until the next status.
    private static volatile long statusUntil;

    private SongInstaller() {}

    static void setSongsDir(String dir) {
        songsDir = dir == null || dir.isEmpty() ? null : dir;
        Log.i(TAG, "Song folder: " + songsDir);
    }

    /** Starts installing the map that link points to. False if link is not a map link. */
    static boolean offer(String link) {
        final String api = SongLinks.apiUrlFor(link);
        if (api == null) return false;
        if (!queued.add(api)) return true;
        if (queued.size() > 1) show("Queued map download", 0);
        worker.execute(() -> {
            try {
                install(api);
            } finally {
                queued.remove(api);
            }
        });
        return true;
    }

    /** A download that is not a map: tell the user that only maps can be downloaded. */
    static void rejectDownload(String url) {
        Log.i(TAG, "Download not supported: " + url);
        show("Only BeatSaver maps can be downloaded", STATUS_MS);
    }

    static String status() {
        long until = statusUntil;
        if (until != 0 && System.currentTimeMillis() > until) return "";
        return status;
    }

    static int takeInstalled() {
        return installed.getAndSet(0);
    }

    private static void show(String text, long durationMs) {
        status = text;
        statusUntil = durationMs == 0 ? 0 : System.currentTimeMillis() + durationMs;
    }

    private static void install(String api) {
        String name = "map";
        File temp = null;
        try {
            String dir = songsDir;
            if (dir == null) throw new IOException("SongCore song folder not known");
            show("Finding map...", 0);
            JSONObject map = new JSONObject(readText(api));
            String key = map.getString("id");
            JSONObject meta = map.optJSONObject("metadata");
            String song = meta == null ? "" : meta.optString("songName", "");
            String mapper = meta == null ? "" : meta.optString("levelAuthorName", "");
            if (!song.isEmpty()) name = song;
            String downloadUrl = publishedVersion(map).getString("downloadURL");

            File songs = new File(dir);
            File target = new File(songs, SongLinks.folderName(key, song, mapper));
            if (isInstalled(songs, key)) {
                show("Already installed: " + name, STATUS_MS);
                return;
            }

            // Unpack next to the song folder, so that SongCore never sees a half-written map.
            File work = new File(songs.getParentFile(), ".QuestBrowserDownloads");
            temp = new File(work, key);
            deleteTree(temp);
            if (!temp.mkdirs()) throw new IOException("Cannot create " + temp);
            download(downloadUrl, temp, name);
            if (!hasInfoFile(temp)) throw new IOException("No Info.dat in the map");
            if (!songs.isDirectory() && !songs.mkdirs()) throw new IOException("Cannot create " + songs);
            if (!temp.renameTo(target)) throw new IOException("Cannot move the map to " + target);
            temp = null;
            installed.incrementAndGet();
            Log.i(TAG, "Installed map " + target);
            show("Installed: " + name, STATUS_MS);
        } catch (Exception e) {
            Log.e(TAG, "Map install failed for " + api, e);
            show("Download failed: " + name, STATUS_MS);
        } finally {
            if (temp != null) deleteTree(temp);
        }
    }

    private static JSONObject publishedVersion(JSONObject map) throws Exception {
        JSONArray versions = map.getJSONArray("versions");
        for (int i = 0; i < versions.length(); i++) {
            JSONObject v = versions.getJSONObject(i);
            if ("Published".equals(v.optString("state"))) return v;
        }
        return versions.getJSONObject(0);
    }

    // Installed by us ("key (...)") or by another downloader that names folders by key.
    private static boolean isInstalled(File songs, String key) {
        String[] names = songs.list();
        if (names == null) return false;
        for (String n : names) {
            if (n.equalsIgnoreCase(key) || n.toLowerCase(Locale.ROOT).startsWith(key.toLowerCase(Locale.ROOT) + " ")) return true;
        }
        return false;
    }

    private static boolean hasInfoFile(File dir) {
        String[] names = dir.list();
        if (names == null) return false;
        for (String n : names) if (n.equalsIgnoreCase("info.dat")) return true;
        return false;
    }

    private static HttpURLConnection open(String url) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(TIMEOUT_MS);
        c.setReadTimeout(TIMEOUT_MS);
        c.setRequestProperty("User-Agent", USER_AGENT);
        int code = c.getResponseCode();
        if (code != HttpURLConnection.HTTP_OK) {
            c.disconnect();
            throw new IOException("HTTP " + code + " for " + url);
        }
        return c;
    }

    private static String readText(String url) throws IOException {
        HttpURLConnection c = open(url);
        try (InputStream in = c.getInputStream()) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[16384];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            return new String(out.toByteArray(), StandardCharsets.UTF_8);
        } finally {
            c.disconnect();
        }
    }

    private static void download(String url, File dir, final String name) throws IOException {
        HttpURLConnection c = open(url);
        final long total = c.getContentLengthLong();
        try (InputStream raw = c.getInputStream(); ZipInputStream zip = new ZipInputStream(new Progress(raw, total, name))) {
            byte[] buf = new byte[65536];
            long written = 0;
            int entries = 0;
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (++entries > MAX_ENTRIES) throw new IOException("Too many files in the map");
                String path = SongLinks.safeEntryName(entry.getName());
                if (path == null) continue;
                File file = new File(dir, path);
                File parent = file.getParentFile();
                if (parent != null && !parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create " + parent);
                try (OutputStream out = new FileOutputStream(file)) {
                    int n;
                    while ((n = zip.read(buf)) > 0) {
                        written += n;
                        if (written > MAX_BYTES) throw new IOException("Map is too large");
                        out.write(buf, 0, n);
                    }
                }
            }
        } finally {
            c.disconnect();
        }
    }

    private static void deleteTree(File f) {
        File[] children = f.listFiles();
        if (children != null) for (File child : children) deleteTree(child);
        if (f.exists() && !f.delete()) Log.w(TAG, "Cannot delete " + f);
    }

    /** Counts downloaded bytes and shows the percentage. */
    private static final class Progress extends FilterInputStream {
        private final long total;
        private final String name;
        private long read;
        private int shown = -1;

        Progress(InputStream in, long total, String name) {
            super(in);
            this.total = total;
            this.name = name;
            show("Downloading " + name, 0);
        }

        @Override
        public int read() throws IOException {
            int b = super.read();
            if (b >= 0) count(1);
            return b;
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            int n = super.read(b, off, len);
            if (n > 0) count(n);
            return n;
        }

        private void count(int n) {
            read += n;
            if (total <= 0) return;
            int percent = (int) Math.min(100, read * 100 / total);
            if (percent != shown) {
                shown = percent;
                show("Downloading " + name + " " + percent + "%", 0);
            }
        }
    }
}
