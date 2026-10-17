// MIT. Shared release-before-press policy for an existing accepted Core modMenu tick.
// No device reads, panel, hotkeys or game policy. The host resets on a new Core epoch
// and supplies eligibility; loss of eligibility or Live input requires a new release.
#pragma once
#include <cstdint>

namespace dbce { namespace controls {
struct MenuSample
{
    uint64_t sequence{};
    bool live{}, pressed{};
};

class MenuEdge
{
    uint64_t sequence_{};
    bool armed_{}, held_{};
public:
    void reset() { sequence_ = 0; armed_ = held_ = false; }
    bool consume(MenuSample sample, bool eligible)
    {
        if (!eligible || !sample.live || !sample.sequence) { reset(); return false; }
        if (sample.sequence <= sequence_) { return false; }
        sequence_ = sample.sequence;
        if (!sample.pressed) { armed_ = true; held_ = false; return false; }
        const bool request = armed_ && !held_;
        held_ = true;
        return request;
    }
};
}}
