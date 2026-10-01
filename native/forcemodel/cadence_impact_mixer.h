#pragma once
#include "impact_mixer.h"
#include <stdexcept>
#include <string>
namespace dbce { namespace force {
// Opt-in monotonic OUTPUT-clock interval contract, not simulation catch-up time. No device API.
class CadenceImpactMixer {
public:
    static constexpr double maximum_interval_seconds = .050;
    static constexpr int required_hold_timeout_ms = 150;
    const float peak_limit, slew_per_second, headroom;
    uint64_t rejected_events = 0;
    bool last_input_rejected = false, last_had_impact = false;
    float last_interval_pulse = 0, last_target = 0, last_command = 0;
    std::string status = "reset";
    explicit CadenceImpactMixer(float peak = .25f, float slew = 1.5f, float reserve = .25f)
        : peak_limit(peak), slew_per_second(slew), headroom(reserve) {
        if (!std::isfinite(peak) || peak < 0 || peak > 1 || !std::isfinite(slew) || slew <= 0 ||
            !std::isfinite(reserve) || reserve < 0 || reserve > .5f) throw std::invalid_argument("Invalid force budget");
    }
    void reset() {
        count_ = 0; last_end_ = last_event_time_ = -1;
        last_interval_pulse = last_target = last_command = 0;
        last_had_impact = last_input_rejected = false; status = "reset";
    }
    bool trigger(float amplitude, float direction, double seconds, bool input_verified = false) {
        if (!input_verified || !std::isfinite(amplitude) || amplitude <= 0 || amplitude > 1 ||
            (direction != -1 && direction != 1) || !std::isfinite(seconds) || seconds < 0 ||
            seconds < last_end_ || seconds < last_event_time_) {
            ++rejected_events; count_ = 0; status = input_verified ? "invalid-event" : "unverified-event"; return false;
        }
        if (count_ == 8) { ++rejected_events; status = "event-capacity"; return false; }
        events_[count_++] = Event{seconds, amplitude * direction}; last_event_time_ = seconds; return true;
    }
    float mix(float structural, double start, double end, bool active, bool impact_inputs_verified = false) {
        if (!active) { reset(); status = "inactive-zero"; return 0; }
        double dt = end - start;
        if (!std::isfinite(structural) || !std::isfinite(start) || !std::isfinite(end) || start < 0 || dt <= 0 ||
            dt > maximum_interval_seconds + 1e-9 || (last_end_ >= 0 && std::abs(start - last_end_) > 1e-9))
            return reject("invalid-or-stale-interval");
        if (!impact_inputs_verified) count_ = 0;
        double pulse = 0; last_had_impact = false;
        for (unsigned i = 0; i < count_; ++i) {
            const auto &e = events_[i];
            if (e.time > end || (last_end_ < 0 && e.time < start)) return reject("event-outside-initial-interval");
            if (e.time < end && e.time + .100 > start) {
                last_had_impact = true;
                pulse += e.amplitude * (ImpactMixer::envelope_integral(end-e.time) - ImpactMixer::envelope_integral(start-e.time)) / dt;
            }
        }
        last_interval_pulse = bounded(float(pulse), -1, 1);
        float structure = bounded(structural, -peak_limit, peak_limit);
        last_target = last_had_impact ? structure*(1-headroom) + peak_limit*headroom*last_interval_pulse : structure;
        last_target = bounded(last_target, -peak_limit, peak_limit);
        float step = slew_per_second * float(dt);
        last_command = bounded(bounded(last_target, last_command-step, last_command+step), -peak_limit, peak_limit);
        unsigned keep = 0;
        for (unsigned i = 0; i < count_; ++i) if (events_[i].time + .100 > end) events_[keep++] = events_[i];
        count_ = keep; last_end_ = end; last_input_rejected = false;
        status = impact_inputs_verified ? "bounded" : "structural-only-unverified-impacts";
        return last_command;
    }
private:
    struct Event { double time; float amplitude; };
    Event events_[8] = {};
    unsigned count_ = 0;
    double last_end_ = -1, last_event_time_ = -1;
    static float bounded(float v, float lo, float hi) { return (std::max)(lo, (std::min)(v, hi)); }
    float reject(const char *reason) { reset(); last_input_rejected = true; status = reason; return 0; }
};
}}
