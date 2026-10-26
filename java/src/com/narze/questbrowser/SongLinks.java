package com.narze.questbrowser;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Rules for map links and map files. No Android classes, so that it can be unit tested
 * on the host (see tests/java/SongLinksTest.java).
 */
public final class SongLinks {
    static final String API = "https://api.beatsaver.com/maps/";
    static final int MAX_FOLDER_NAME = 120;

    // One-Click buttons on BeatSaver and BeastSaber: beatsaver://<map key>.
    private static final Pattern ONE_CLICK = Pattern.compile("(?i)beatsaver://([0-9a-f]{1,8})/?");
    // Download buttons on BeatSaver and BeatLeader: https://<cdn>.beatsaver.com/<hash>.zip.
    private static final Pattern MAP_ZIP =
            Pattern.compile("(?i)https://([a-z0-9-]+\\.)*beatsaver\\.com/([0-9a-f]{40})\\.zip([?#].*)?");

    private SongLinks() {}

    /** The BeatSaver API URL of the map that a link points to, or null if it is not a map link. */
    public static String apiUrlFor(String link) {
        if (link == null) return null;
        Matcher m = ONE_CLICK.matcher(link);
        if (m.matches()) return API + "id/" + m.group(1).toLowerCase(Locale.ROOT);
        m = MAP_ZIP.matcher(link);
        if (m.matches()) return API + "hash/" + m.group(2).toLowerCase(Locale.ROOT);
        return null;
    }

    /** Folder name for an installed map: "key (song - mapper)", safe as a file name. */
    public static String folderName(String key, String songName, String mapper) {
        String song = clean(songName);
        String by = clean(mapper);
        String name = key;
        if (!song.isEmpty() && !by.isEmpty()) name += " (" + song + " - " + by + ")";
        else if (!song.isEmpty()) name += " (" + song + ")";
        return name.length() > MAX_FOLDER_NAME ? name.substring(0, MAX_FOLDER_NAME) : name;
    }

    private static String clean(String s) {
        if (s == null) return "";
        String out = s.replaceAll("[\\\\/:*?\"<>|\\p{Cntrl}]", " ").replaceAll("\\s+", " ").trim();
        return out.replaceAll("[. ]+$", "");
    }

    /**
     * The relative path to write a zip entry to, or null to skip it: folders, and paths that
     * would leave the song folder.
     */
    public static String safeEntryName(String entryName) {
        if (entryName == null || entryName.isEmpty()) return null;
        String name = entryName.replace('\\', '/');
        if (name.endsWith("/") || name.startsWith("/") || name.matches("^[A-Za-z]:.*")) return null;
        List<String> parts = new ArrayList<>();
        for (String part : name.split("/")) {
            if (part.isEmpty() || part.equals(".")) continue;
            if (part.equals("..")) return null;
            parts.add(part);
        }
        return parts.isEmpty() ? null : String.join("/", parts);
    }
}
