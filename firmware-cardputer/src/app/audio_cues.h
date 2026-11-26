// Deck speaker cues, scheduled by the main loop.
#pragma once

#include <cstddef>
#include "model/signal_hunt.h"

namespace app {

class AudioCues {
public:
    void waypoint()
    {
        static constexpr Note notes[] = {{1047, 55}, {1047, 55}, {1568, 100}};
        play(notes);
    }

    void clear() { next_ = nullptr; }

    void updateHunt(bool active, bool fresh)
    {
        if (!active && hunt_cue_) clear();
        if (active && fresh != target_fresh_) {
            static constexpr Note acquire[] = {{784, 65}, {1175, 95}};
            static constexpr Note lost[] = {{659, 65}, {392, 110}};
            if (fresh) play(acquire);
            else play(lost);
            hunt_cue_ = true;
        }
        target_fresh_ = active && fresh;
    }

    bool busy() const { return next_ != nullptr; }

    model::GeigerPulse poll(uint32_t now_ms)
    {
        if (!busy() || now_ms - last_note_ms_ < wait_ms_) return {};
        if (!remaining_) {
            clear();
            return {};
        }
        const Note note = *next_++;
        --remaining_;
        last_note_ms_ = now_ms;
        wait_ms_ = note.duration_ms + 25;
        model::GeigerPulse pulse;
        pulse.fire = true;
        pulse.freq_hz = note.freq_hz;
        pulse.duration_ms = note.duration_ms;
        return pulse;
    }

private:
    struct Note { uint16_t freq_hz, duration_ms; };

    template <size_t N>
    void play(const Note (&notes)[N])
    {
        hunt_cue_ = false;
        next_ = notes;
        remaining_ = N;
        wait_ms_ = 0;
    }

    const Note *next_ = nullptr;
    size_t remaining_ = 0;
    uint32_t last_note_ms_ = 0;
    uint32_t wait_ms_ = 0;
    bool target_fresh_ = false;
    bool hunt_cue_ = false;
};

}  // namespace app
