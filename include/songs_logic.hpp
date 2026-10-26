#pragma once

// Game-independent rules for maps installed from the browser.
// Unit tested on the host (see tests/songs_test.cpp).

namespace QuestBrowser::Songs {

// New maps show up in the song list after SongCore reloads its songs. Reload only in the
// menu, and not while SongCore is already reloading (it may have read the folder already).
class RefreshGate {
   public:
    void Installed(int count) {
        if (count > 0) pending += count;
    }
    // True once when a reload should start now.
    bool Take(bool inMenu, bool refreshing) {
        if (pending == 0 || !inMenu || refreshing) return false;
        pending = 0;
        return true;
    }

   private:
    int pending = 0;
};

}  // namespace QuestBrowser::Songs
