package com.narze.questbrowser;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNull;

import org.junit.jupiter.api.Test;

class SongLinksTest {
    private static final String HASH = "e4c1f48eb6d8ccb0ae874b4852417d434af0f2d1";

    @Test
    void oneClickLinkGivesTheMapById() {
        assertEquals("https://api.beatsaver.com/maps/id/1a2b", SongLinks.apiUrlFor("beatsaver://1a2b"));
        assertEquals("https://api.beatsaver.com/maps/id/1a2b", SongLinks.apiUrlFor("beatsaver://1A2B/"));
        assertEquals("https://api.beatsaver.com/maps/id/3f1e5", SongLinks.apiUrlFor("BeatSaver://3f1e5"));
    }

    @Test
    void oneClickLinkWithABadKeyIsNotAMap() {
        assertNull(SongLinks.apiUrlFor("beatsaver://"));
        assertNull(SongLinks.apiUrlFor("beatsaver://1a2b?x=1"));
        assertNull(SongLinks.apiUrlFor("beatsaver://not-a-key"));
        assertNull(SongLinks.apiUrlFor("beatsaver://123456789abcdef"));
    }

    @Test
    void beatSaverZipGivesTheMapByHash() {
        String expected = "https://api.beatsaver.com/maps/hash/" + HASH;
        assertEquals(expected, SongLinks.apiUrlFor("https://r2cdn.beatsaver.com/" + HASH + ".zip"));
        assertEquals(expected, SongLinks.apiUrlFor("https://cdn.beatsaver.com/" + HASH.toUpperCase() + ".zip"));
        assertEquals(expected, SongLinks.apiUrlFor("https://eu.cdn.beatsaver.com/" + HASH + ".zip?download=1"));
    }

    @Test
    void otherLinksAreNotMaps() {
        assertNull(SongLinks.apiUrlFor(null));
        assertNull(SongLinks.apiUrlFor(""));
        assertNull(SongLinks.apiUrlFor("https://beatsaver.com/maps/1a2b"));
        assertNull(SongLinks.apiUrlFor("https://example.com/" + HASH + ".zip"));
        assertNull(SongLinks.apiUrlFor("https://beatsaver.com.example.com/" + HASH + ".zip"));
        assertNull(SongLinks.apiUrlFor("http://cdn.beatsaver.com/" + HASH + ".zip"));
        assertNull(SongLinks.apiUrlFor("https://cdn.beatsaver.com/abc.zip"));
    }

    @Test
    void folderNameHasKeySongAndMapper() {
        assertEquals("1a2b (Believer RU Cover - BCPowerhouse)",
                SongLinks.folderName("1a2b", "Believer RU Cover", "BCPowerhouse"));
    }

    @Test
    void folderNameDropsCharactersThatAreNotAllowedInFileNames() {
        assertEquals("1a2b (AC DC - Who Mapper)",
                SongLinks.folderName("1a2b", "AC/DC\t", "Who?  Mapper"));
        assertEquals("1a2b (Song - Mapper)", SongLinks.folderName("1a2b", "Song...", "<Mapper>"));
    }

    @Test
    void folderNameIsTheKeyWhenThereIsNoName() {
        assertEquals("1a2b", SongLinks.folderName("1a2b", "", null));
        assertEquals("1a2b (Song)", SongLinks.folderName("1a2b", "Song", "  "));
    }

    @Test
    void folderNameIsNotTooLong() {
        String name = SongLinks.folderName("1a2b", "x".repeat(300), "Mapper");
        assertEquals(SongLinks.MAX_FOLDER_NAME, name.length());
        assertEquals("1a2b (xxx", name.substring(0, 9));
    }

    @Test
    void safeEntryNameKeepsFilesInTheSongFolder() {
        assertEquals("Info.dat", SongLinks.safeEntryName("Info.dat"));
        assertEquals("sub/cover.png", SongLinks.safeEntryName("sub/cover.png"));
        assertEquals("sub/cover.png", SongLinks.safeEntryName("sub\\cover.png"));
        assertEquals("a/b.dat", SongLinks.safeEntryName("./a//b.dat"));
    }

    @Test
    void safeEntryNameRejectsPathsThatLeaveTheFolder() {
        assertNull(SongLinks.safeEntryName("../evil.so"));
        assertNull(SongLinks.safeEntryName("a/../../evil.so"));
        assertNull(SongLinks.safeEntryName("a\\..\\..\\evil.so"));
        assertNull(SongLinks.safeEntryName("/sdcard/evil.so"));
        assertNull(SongLinks.safeEntryName("C:/evil.so"));
    }

    @Test
    void safeEntryNameSkipsFolders() {
        assertNull(SongLinks.safeEntryName("sub/"));
        assertNull(SongLinks.safeEntryName(""));
        assertNull(SongLinks.safeEntryName(null));
    }

    private static final String PLAYLIST = "https://api.beatsaver.com/playlists/id/1280881/download";

    @Test
    void playlistOneClickGivesThePlaylistFile() {
        assertEquals(PLAYLIST + "/beatsaver-1280881.bplist",
                SongLinks.playlistUrlFor("bsplaylist://playlist/" + PLAYLIST + "/beatsaver-1280881.bplist"));
        assertEquals("https://bsaber.com/PlaylistAPI/22-01-01.bplist",
                SongLinks.playlistUrlFor("bsplaylist://playlist/https://bsaber.com/PlaylistAPI/22-01-01.bplist"));
    }

    @Test
    void playlistDownloadGivesThePlaylistFile() {
        assertEquals(PLAYLIST, SongLinks.playlistUrlFor(PLAYLIST));
        assertEquals(PLAYLIST + "/sign?x=1", SongLinks.playlistUrlFor(PLAYLIST + "/sign?x=1"));
    }

    @Test
    void otherLinksAreNotPlaylists() {
        assertNull(SongLinks.playlistUrlFor(null));
        assertNull(SongLinks.playlistUrlFor("bsplaylist://playlist/"));
        assertNull(SongLinks.playlistUrlFor("bsplaylist://playlist/http://api.beatsaver.com/x.bplist"));
        assertNull(SongLinks.playlistUrlFor("bsplaylist://playlist/https://example.com/x.bplist"));
        assertNull(SongLinks.playlistUrlFor("bsplaylist://playlist/https://beatsaver.com.example.com/x.bplist"));
        assertNull(SongLinks.playlistUrlFor("https://beatsaver.com/playlists/1280881"));
        assertNull(SongLinks.playlistUrlFor("https://example.com/playlists/id/1/download"));
        assertNull(SongLinks.playlistUrlFor("beatsaver://1a2b"));
    }

    @Test
    void playlistFileNameIsTheTitle() {
        assertEquals("Curator's Picks September 2026.bplist",
                SongLinks.playlistFileName("Curator's Picks September 2026"));
        assertEquals("Rock Metal.bplist", SongLinks.playlistFileName("Rock / Metal?"));
        assertEquals("playlist.bplist", SongLinks.playlistFileName(" ... "));
        assertEquals("playlist.bplist", SongLinks.playlistFileName(null));
    }
}
