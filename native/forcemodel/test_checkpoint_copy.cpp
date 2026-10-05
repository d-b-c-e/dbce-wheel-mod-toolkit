// Assessment fixture only. No recorder codec, public snapshot API or native output.
#include "force_profile.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <cstring>

using namespace dbce::force;
namespace {
const char* const model_blob = "67860969fe78d01abfcd046451a309859c67bebe";
const char* const parser_blob = "207b229f633ccc6e3acca01953659201d9be1784";
const std::array<float ModelSettings::*,19> model_fields = {{
    &ModelSettings::spring_strength,&ModelSettings::spring_full_speed_mps,&ModelSettings::damper_strength,
    &ModelSettings::damper_static_fraction,&ModelSettings::lateral_weight,&ModelSettings::lateral_force_reference,
    &ModelSettings::lateral_g_reference,&ModelSettings::trail_floor,&ModelSettings::trail_slip_span,
    &ModelSettings::grip_loss,&ModelSettings::weight_transfer,&ModelSettings::weight_transfer_min,
    &ModelSettings::weight_transfer_max,&ModelSettings::texture_strength,&ModelSettings::texture_hz,
    &ModelSettings::impact_strength,&ModelSettings::impact_seconds,&ModelSettings::shift_strength,&ModelSettings::shift_seconds
}};
const std::array<float ShaperSettings::*,14> shaper_fields = {{
    &ShaperSettings::deadzone,&ShaperSettings::attack_smoothing,&ShaperSettings::decay_smoothing,
    &ShaperSettings::soft_saturation,&ShaperSettings::slew_per_second,&ShaperSettings::output_deadband,
    &ShaperSettings::fade_start_kmh,&ShaperSettings::fade_full_kmh,&ShaperSettings::ramp_seconds,
    &ShaperSettings::peak_limit,&ShaperSettings::smoothing_ms,&ShaperSettings::impulse_bypass,
    &ShaperSettings::settle_below,&ShaperSettings::releveling
}};
static_assert(std::is_trivially_copyable<Model>::value && std::is_trivially_copyable<Shaper>::value,
              "Fixture accepts only owning value state; no referenced/dynamic private storage");
static_assert(std::is_nothrow_copy_assignable<Model>::value && std::is_nothrow_copy_assignable<Shaper>::value,
              "Both commit assignments must be nonthrowing");

bool exact(float a,float b) { uint32_t x,y; std::memcpy(&x,&a,sizeof(x)); std::memcpy(&y,&b,sizeof(y)); return x==y; }
bool same(const ModelSettings& a,const ModelSettings& b) {
    for(auto field:model_fields) if(!exact(a.*field,b.*field)) return false;
    return true;
}
bool same(const ShaperSettings& a,const ShaperSettings& b) {
    if(a.strength!=b.strength || a.invert!=b.invert) return false;
    for(auto field:shaper_fields) if(!exact(a.*field,b.*field)) return false;
    return true;
}
bool eligible(const ModelSettings& m,const ShaperSettings& h) {
    // Bounded test configuration only; does not alter permissive profile parsing.
    for(auto field:model_fields) if(!std::isfinite(m.*field) || std::fabs(m.*field)>1000000.f) return false;
    for(auto field:shaper_fields) if(!std::isfinite(h.*field) || std::fabs(h.*field)>1000000.f) return false;
    return h.strength>=0 && h.strength<=100 && h.peak_limit>=0 && h.peak_limit<=1;
}
struct Context {
    uint32_t fixture_revision=1;
    std::string model_source=model_blob, parser_source=parser_blob, profile_id;
    ModelSettings model; ShaperSettings shaper;
    explicit Context(const Profile& p):profile_id(p.id()),model(p.model),shaper(p.shaper) {}
    bool valid() const {
        return fixture_revision==1 && model_source==model_blob && parser_source==parser_blob &&
               !profile_id.empty() && eligible(model,shaper);
    }
};
// Opaque trusted in-process value copies. No numeric state arrays, raw bytes or readers.
// This deliberately cannot import the PC proposal's v1 snapshot representation.
class CopyCheckpoint {
public:
    static CopyCheckpoint capture(const Model& m,const Shaper& h,const Context& context) {
        if(!context.valid() || !same(m.settings,context.model) || !same(h.settings,context.shaper) ||
           !std::isfinite(m.last_structural) || !std::isfinite(h.last()))
            throw std::runtime_error("capture context/configuration ineligible");
        return CopyCheckpoint(m,h,context);
    }
    bool restore(Model& m,Shaper& h,const Context& expected) const {
        if(!expected.valid() || expected.fixture_revision!=context_.fixture_revision ||
           expected.model_source!=context_.model_source || expected.parser_source!=context_.parser_source ||
           expected.profile_id!=context_.profile_id || !same(expected.model,context_.model) ||
           !same(expected.shaper,context_.shaper) || !same(m.settings,context_.model) ||
           !same(h.settings,context_.shaper)) return false;
        // No mutation until both configurations/context pass; both assignments cannot throw.
        m=model_; h=shaper_; return true;
    }
private:
    CopyCheckpoint(const Model& m,const Shaper& h,const Context& c):model_(m),shaper_(h),context_(c) {}
    const Model model_; const Shaper shaper_; const Context context_;
};
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
ShaperSettings plain() {
    ShaperSettings s; s.deadzone=s.output_deadband=s.soft_saturation=s.slew_per_second=0;
    s.fade_start_kmh=s.fade_full_kmh=s.ramp_seconds=0; s.attack_smoothing=s.decay_smoothing=0;
    return s;
}
// Exact 31-value projection from the PC proposal, for incompatibility proof only.
std::array<double,31> legacy_values(const ModelSettings& m,const ShaperSettings& s) {
    return {{m.spring_strength,m.spring_full_speed_mps,m.damper_strength,m.damper_static_fraction,
        m.lateral_weight,m.lateral_force_reference,m.lateral_g_reference,m.trail_floor,m.trail_slip_span,
        m.grip_loss,m.weight_transfer,m.weight_transfer_min,m.weight_transfer_max,m.texture_strength,
        m.texture_hz,m.impact_strength,m.impact_seconds,m.shift_strength,m.shift_seconds,
        double(s.strength),double(s.invert),s.deadzone,s.attack_smoothing,s.decay_smoothing,
        s.soft_saturation,s.slew_per_second,s.output_deadband,s.fade_start_kmh,s.fade_full_kmh,
        s.ramp_seconds,s.peak_limit}};
}
void legacy_incompatibility() {
    auto a=plain(),b=a; a.smoothing_ms=20; b.smoothing_ms=200;
    require(legacy_values(ModelSettings(),a)==legacy_values(ModelSettings(),b),"legacy configuration projection differs unexpectedly");
    require(!same(a,b),"full typed configuration ignored new smoothing setting");
    Shaper x(a),y(b); float xa=x.shape(.5f,40,.02f),yb=y.shape(.5f,40,.02f);
    require(xa!=yb,"different smoothing settings failed to distinguish actual production behavior");
    auto history=plain(); history.impulse_bypass=.5f; history.attack_smoothing=history.decay_smoothing=.95f; history.output_deadband=.1f; Shaper positive(history),negative(history);
    require(positive.shape(.01f,40,.02f)==0 && negative.shape(-.01f,40,.02f)==0,"history setup failed");
    require(positive.last()==negative.last() && positive.started()==negative.started(),"public output/ramp state setup differs");
    float pa=positive.shape(.5f,40,.02f),na=negative.shape(.5f,40,.02f);
    require(pa==0 && na==.5f,"missing previous-input history did not distinguish continuation");
    std::printf("legacy projection collision: %.9g vs %.9g; omitted history continuation: %.9g vs %.9g\n",xa,yb,pa,na);
}
Inputs input(int frame) {
    Inputs i; i.has_steer=i.has_steer_rate=i.has_lateral_g=i.has_longitudinal_g=i.has_slip=true;
    i.steer=.4f*std::sin(float(frame)*.17f); i.steer_rate=.6f*std::cos(float(frame)*.13f);
    i.speed_mps=20.f+float(frame%7); i.lateral_g=.35f*std::cos(float(frame)*.11f);
    i.longitudinal_g=.2f*std::sin(float(frame)*.07f); i.front_slip_angle_deg=4; i.ideal_slip_angle_deg=6;
    i.impact=(frame%29==0)? .7f:0; i.gear_shift=(frame%17==0); return i;
}
float tick(Model& m,Shaper& h,int frame) {
    auto i=input(frame); float dt=(frame%3==0)? .031f:(frame%3==1)? .009f:.02f;
    return h.shape(m.compute(i,dt),i.speed_mps*3.6f,dt,m.last_was_event);
}
bool visible_same(const Model& a,const Shaper& x,const Model& b,const Shaper& y) {
    return a.last_was_event==b.last_was_event && a.last_structural==b.last_structural &&
           a.last_input_rejected==b.last_input_rejected && x.last()==y.last() &&
           x.started()==y.started() && x.last_input_rejected==y.last_input_rejected;
}
void same_continuation(Model a,Shaper x,Model b,Shaper y,int begin=80) {
    require(visible_same(a,x,b,y),"checkpoint changed visible event/rejection/output state");
    for(int frame=begin;frame<begin+70;++frame) {
        require(tick(a,x,frame)==tick(b,y,frame),"exact copied-state continuation differs");
        require(visible_same(a,x,b,y),"copied diagnostics/event/restart state differs");
    }
}
int main_test(const char* profiles) {
    legacy_incompatibility(); std::vector<Profile> tunes;
    const char* ids[]={"arcade-outrun@1","arcade-outrun@2","arcade-outrun@3","simlite@1","simlite@2",
                      "cruisn-vunit@1","cruisn-vunit@2","cruisn-vunit@3","cruisn-vunit@4","arcade-generic@1"};
    for(auto id:ids) { Profile p; std::string why; require(load_profile_dir(profiles,id,p,&why),why.c_str()); tunes.push_back(p); }
    Profile synthetic=tunes[2]; synthetic.name="fixture-only-copy"; synthetic.version=1;
    synthetic.model.texture_strength=.07f; synthetic.model.impact_strength=.3f; synthetic.model.shift_strength=.2f;
    synthetic.shaper.smoothing_ms=55; synthetic.shaper.impulse_bypass=.18f;
    synthetic.shaper.settle_below=.004f; synthetic.shaper.releveling=.01f; synthetic.shaper.ramp_seconds=2;
    tunes.push_back(synthetic); int replays=0,rejections=0;
    for(const auto& p:tunes) {
        const Context context(p); const int checkpoints[]={0,1,7,17,33,65};
        for(int checkpoint:checkpoints) for(int mode=0;mode<4;++mode) {
            Model m(p.model); Shaper h(p.shaper);
            for(int frame=0;frame<checkpoint;++frame) tick(m,h,frame);
            if(mode==1) {m.reset();h.reset();} else if(mode==2) h.restart_ramp(); else if(mode==3) {
                auto bad=input(0); bad.steer=std::numeric_limits<float>::quiet_NaN();
                m.compute(bad,.02f); h.shape(std::numeric_limits<float>::quiet_NaN(),40,.02f);
                require(m.last_input_rejected && h.last_input_rejected,"rejection-state setup failed");
            }
            const auto captured=CopyCheckpoint::capture(m,h,context);
            Model restored(p.model); Shaper shaped(p.shaper);
            require(captured.restore(restored,shaped,context),"trusted checkpoint restore failed");
            same_continuation(m,h,restored,shaped,checkpoint); ++replays;
        }
        Model source(p.model); Shaper shape(p.shaper); for(int f=0;f<33;++f) tick(source,shape,f);
        const auto captured=CopyCheckpoint::capture(source,shape,context);
        auto reject=[&](Context expected,Model destination,Shaper output) {
            Model control=destination; Shaper control_output=output;
            require(!captured.restore(destination,output,expected),"foreign context/configuration accepted");
            same_continuation(destination,output,control,control_output); ++rejections;
        };
        auto destinations=[&]() { Model m(p.model); Shaper h(p.shaper); for(int f=0;f<9;++f) tick(m,h,f); return std::make_pair(m,h); };
        for(int kind=0;kind<4;++kind) {
            Context foreign=context; if(kind==0) foreign.fixture_revision=2; if(kind==1) foreign.model_source="foreign";
            if(kind==2) foreign.parser_source="foreign"; if(kind==3) foreign.profile_id="different-profile@1";
            auto d=destinations(); reject(foreign,d.first,d.second);
        }
        for(auto field:model_fields) {auto d=destinations(); d.first.settings.*field+=.125f; reject(context,d.first,d.second);}
        for(auto field:shaper_fields) {auto d=destinations(); d.second.settings.*field+=.125f; reject(context,d.first,d.second);}
        {auto d=destinations();++d.second.settings.strength;reject(context,d.first,d.second);}
        {auto d=destinations();d.second.settings.invert=!d.second.settings.invert;reject(context,d.first,d.second);}
        // Captured configuration is owning and immutable; later changes to original context cannot update it.
        Context edited=context; edited.shaper.smoothing_ms+=1; auto d=destinations(); reject(edited,d.first,d.second);
        edited=context; edited.model.spring_strength=std::numeric_limits<float>::quiet_NaN(); d=destinations(); reject(edited,d.first,d.second);
        edited=context; edited.shaper.smoothing_ms=1000001; d=destinations(); reject(edited,d.first,d.second);
    }
    std::printf("PASS: %d current-model copy replays; %d context/configuration rejection and transactional checks; %zu profiles; no friend/header/native output/codec changes\n",replays,rejections,tunes.size());
    return 0;
}
} // fixture namespace
int main(int argc,char** argv) {
    try { require(argc==2,"profiles directory required"); return main_test(argv[1]); }
    catch(const std::exception& e) {std::printf("FAIL: %s\n",e.what());return 1;}
}
