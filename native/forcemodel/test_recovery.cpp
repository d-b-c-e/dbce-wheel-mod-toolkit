#include "force_model.h"
#include <cstdio>
#include <limits>
using namespace dbce::force;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static ShaperSettings plain() { ShaperSettings s; s.deadzone=s.output_deadband=s.attack_smoothing=s.decay_smoothing=s.ramp_seconds=s.fade_start_kmh=s.fade_full_kmh=0; return s; }
int main() {
    auto s=plain(); s.releveling=.1f; Shaper sh(s);
    CHECK(sh.shape(.02f,40,.02f)==.02f); CHECK(sh.shape(-.02f,40,.02f)==-.02f);
    CHECK(sh.shape(.02f,40,.02f)==.02f); sh.shape(.8f,40,.02f); sh.settings.peak_limit=.75f;
    CHECK(sh.shape(.8f,40,.02f)==.75f); sh.settings.peak_limit=0; CHECK(sh.shape(.8f,40,.02f)==0);
    const float invalids[]={std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
    for (float bad:invalids) {
        for(int input=0;input<3;++input) {
            s=plain(); s.smoothing_ms=50; Shaper v(s),fresh(s); v.shape(.8f,40,.02f);
            CHECK(v.shape(input==0?bad:.5f,input==1?bad:40,input==2?bad:.02f)==0);
            CHECK(v.last_input_rejected && v.last()==0);
            CHECK(v.shape(.5f,40,.02f)==fresh.shape(.5f,40,.02f)); CHECK(!v.last_input_rejected);
        }
        float ShaperSettings::* fields[]={&ShaperSettings::deadzone,&ShaperSettings::attack_smoothing,&ShaperSettings::decay_smoothing,&ShaperSettings::soft_saturation,&ShaperSettings::slew_per_second,&ShaperSettings::output_deadband,&ShaperSettings::fade_start_kmh,&ShaperSettings::fade_full_kmh,&ShaperSettings::ramp_seconds,&ShaperSettings::peak_limit,&ShaperSettings::smoothing_ms,&ShaperSettings::impulse_bypass,&ShaperSettings::settle_below,&ShaperSettings::releveling};
        for(auto field:fields) { Shaper v(plain()); float original=v.settings.*field; v.settings.*field=bad;
            CHECK(v.shape(.5f,40,.02f)==0 && v.last_input_rejected); v.settings.*field=original;
            CHECK(v.shape(.5f,40,.02f)==.5f && !v.last_input_rejected); }
        ModelSettings ms; ms.impact_strength=.5f; Model model(ms); Inputs hit; hit.impact=1;
        model.compute(hit,.02f); CHECK(model.compute(hit,bad)==0 && model.last_input_rejected);
        CHECK(!model.last_was_event); CHECK(model.compute(Inputs(),.02f)==0 && !model.last_input_rejected);
        Inputs absent; absent.steer=bad; CHECK(model.compute(absent,.02f)==0 && !model.last_input_rejected);
        absent.has_steer=true; CHECK(model.compute(absent,.02f)==0 && model.last_input_rejected);
        model.settings.texture_hz=bad; CHECK(model.compute(hit,.02f)==0 && model.last_input_rejected);
        model.settings.texture_hz=12; CHECK(model.compute(hit,.02f)==.5f && !model.last_input_rejected);
    }
    s=plain(); s.strength=100; Shaper overflow(s);
    CHECK(overflow.shape(std::numeric_limits<float>::max(),40,.02f)==0 && overflow.last_input_rejected);
    CHECK(overflow.shape(.25f,40,.02f)==.5f && !overflow.last_input_rejected);
    std::puts("recovery invariants passed (sign changes, lowered limits, invalid inputs/settings, overflow, recovery)");
}
