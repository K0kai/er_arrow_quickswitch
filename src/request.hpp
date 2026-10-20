#pragma once
#include <cstdint>

namespace arrow_switch {
// One bounded input request; leaving gameplay/focus cancels it immediately.
struct Request {
    int direction{};
    std::uint64_t pressed{}, mouseReleased{}, lastSwitch{};
    bool mouseWasHeld{};

    void cancel() { direction=0; mouseReleased=0; mouseWasHeld=false; }
    void submit(int value, std::uint64_t now) { direction=value; pressed=now; }
    bool ready(std::uint64_t now, bool allowed, bool mouseHeld,
               std::uint64_t settle, std::uint64_t timeout, std::uint64_t cooldown) {
        if (!allowed) { cancel(); return false; }
        if (mouseHeld) { mouseWasHeld=true; mouseReleased=0; }
        else if (mouseWasHeld) { mouseWasHeld=false; mouseReleased=now; }
        if (!direction) return false;
        if (now-pressed >= timeout) { cancel(); return false; }
        if (mouseHeld || (mouseReleased && now-mouseReleased<settle)) return false;
        return !lastSwitch || now-lastSwitch>=cooldown;
    }
    int take(std::uint64_t now) { int value=direction; direction=0; lastSwitch=now; return value; }
};
}
