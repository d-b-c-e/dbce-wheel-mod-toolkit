#define NOMINMAX
#include "dbce_controls_store_win.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace dbce::controls;
namespace e=dbce::controls::edit;
namespace fs=std::filesystem;
static int checks=0;
static void check(bool pass,const char* message){++checks;if(!pass)throw std::runtime_error(message);}
static bool policy(const std::string& action,Kind kind){return action=="steer"?kind==Kind::Axis:(action=="throttle"||action=="brake")&&(kind==Kind::Axis||kind==Kind::Button);}
static e::AcceptedObservation observation(uint64_t sequence,double seen){
    e::AcceptedObservation o;o.instance="{11111111-1111-1111-1111-111111111111}";o.product="{22222222-2222-2222-2222-222222222222}";
    o.name="Disconnected test wheel";o.identity_key="opaque interface path";o.epoch=1;o.sequence=sequence;o.seen=seen;
    o.axis_mask=o.range_mask=1;o.minimum[0]=0;o.maximum[0]=65535;o.sample.axes[0]=32767;o.button_mask.set(4);o.hat_mask=1;return o;
}
static Binding axis(){Binding b;b.kind=Kind::Axis;b.index=0;b.dev=observation(1,0).instance;b.prod=observation(1,0).product;b.min=0;b.max=65535;b.rest=32767;b.travel=1;b.hasRange=b.hasRest=true;b.name="Saved wheel";return b;}
static std::string profile(){return "\xEF\xBB\xBF; owner\r\n[Controls]\r\nSchema = 1\r\nProfile = Test\r\nRevision = old\r\nTransmission = Automatic\r\nsteer = "+format(axis())+"\r\nthrottle = \r\nbrake = \r\nFuture = keep\r\n[Other]\nCustom = exact";}
static void write(const fs::path& p,const std::string& text){std::ofstream f(p,std::ios::binary);f.write(text.data(),static_cast<std::streamsize>(text.size()));if(!f)throw std::runtime_error("fixture write failed");}
static std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static void capture(){
    e::CaptureFlow flow(policy);auto o=observation(1,0);check(flow.observe(o,0,true),"initial observation");check(flow.begin("steer",o.instance,0,true),"begin steering");
    o.sequence=2;o.seen=.9;o.sample.axes[0]=50000;flow.observe(o,.9,true);check(!flow.take_result(.9,true),"rest period does not capture");
    o.sequence=3;o.seen=1;o.sample.axes[0]=32767;flow.observe(o,1,true);
    o.sequence=4;o.seen=1.1;o.sample.axes[0]=50000;flow.observe(o,1.1,true);auto result=flow.take_result(1.1,true);
    check(result&&result->action=="steer","steering candidate");check(result->binding.rest==32767&&result->binding.travel==1&&!result->binding.inverted&&result->binding.cal.empty(),"exact schema1 rest travel no inferred inversion/calibration");
    check(fits("steer",result->binding).empty(),"canonical steering fit");check(!flow.take_result(1.1,true),"one result only");
    o.sequence=5;o.seen=2;flow.observe(o,2,true);check(flow.begin("brake",o.instance,2,true),"begin pedal button");
    o.sequence=6;o.seen=3;o.sample.buttons[4]=true;flow.observe(o,3,true);check(!flow.take_result(3,true),"held baseline button ignored");
    o.sequence=7;o.seen=3.1;o.sample.buttons[4]=false;flow.observe(o,3.1,true);
    o.sequence=8;o.seen=3.2;o.sample.buttons[4]=true;flow.observe(o,3.2,true);result=flow.take_result(3.2,true);check(result&&result->binding.kind==Kind::Button&&result->binding.index==4,"release repress pedal button");
    o.sequence=9;o.seen=4;o.sample.buttons[4]=false;flow.observe(o,4,true);flow.begin("throttle",o.instance,4,true);
    o.sequence=10;o.seen=5;flow.observe(o,5,true);o.sequence=11;o.seen=5.1;o.sample.buttons[4]=true;o.sample.axes[0]=10000;flow.observe(o,5.1,true);check(!flow.take_result(5.1,true),"ambiguous simultaneous input refused");
    o.sequence=12;o.seen=5.2;o.sample.axes[0]=70000;check(!flow.observe(o,5.2,true),"out of range rejects whole observation");check(flow.action().empty(),"bad observation cancels capture");
    e::CaptureFlow stale(policy);o=observation(1,0);stale.observe(o,0,true);stale.begin("steer",o.instance,0,true);o.seen=.2;check(!stale.observe(o,.2,true),"duplicate sequence ignored");stale.refresh(.3,true);check(stale.action().empty(),"duplicate does not refresh freshness");
    e::CaptureFlow epoch(policy);o=observation(1,0);epoch.observe(o,0,true);epoch.begin("steer",o.instance,0,true);o.epoch=2;o.sequence=2;o.seen=.1;epoch.observe(o,.1,true);check(epoch.action().empty(),"reconnect cancels capture");
    e::DeviceSelection selection;selection.select_device(1,o,.1);auto changed=o;changed.epoch=3;selection.reconcile(1,{changed},.1);check(selection.mode()==e::SelectionMode::Unavailable,"explicit reconnect choice not remapped");selection.select_saved();check(selection.mode()==e::SelectionMode::Saved,"explicit return to saved");
    selection.select_device(1,o,.1);selection.reconcile(2,{o},.1);check(selection.mode()==e::SelectionMode::Unavailable,"host replacement invalidates explicit choice");
    e::CaptureFlow context(policy);o=observation(1,0);context.observe(o,0,true);context.begin("brake",o.instance,0,true);o.sequence=2;o.seen=1;context.observe(o,1,true);o.sequence=3;o.seen=1.1;o.sample.buttons[4]=true;context.observe(o,1.1,true);check(!context.take_result(1.1,false),"candidate cannot cross focus loss");
    context.reader_stopped();o.sequence=2;o.seen=2;check(!context.observe(o,2,true),"reader stopped retains sequence watermark");
    e::CaptureFlow hats(policy);o=observation(1,0);hats.observe(o,0,true);hats.begin("brake",o.instance,0,true);o.sequence=2;o.seen=1;hats.observe(o,1,true);o.sequence=3;o.seen=1.1;o.sample.hats[0]=9000;hats.observe(o,1.1,true);check(!hats.take_result(1.1,true)&&hats.action().empty(),"consumer pedal hat refusal");
}
static void draft_and_save(const fs::path& root){
    auto original=profile();e::ProfileDraft draft(original,policy,false);std::string reason;
    check(!draft.has_draft()&&!draft.saved_pending_restart(),"clean initial draft");check(!draft.binding("brake"),"empty action imports as unbound");
    check(draft.set("steer",std::nullopt,reason),"explicit clear admitted");auto next=draft.preview("fixture-01");
    check(next.find("steer = \r\n")!=std::string::npos,"clear retains explicit empty row");check(next.find("Future = keep\r\n[Other]\nCustom = exact")!=std::string::npos,"mixed EOL unknown bytes retained");check(next.substr(0,3)==original.substr(0,3),"BOM retained");
    check(!draft.set("horn",axis(),reason),"unsupported action refuses without altering draft");
    auto file=root/L"controls.ini";write(file,original);auto saved=e::save(draft,file,"fixture-02");
    check(saved.saved&&saved.replaced,"actual Windows explicit replacement");check(read(saved.backup)==original,"exact pre-save backup");check(read(file)==draft.saved_bytes(),"exact readback");check(draft.active_bytes()==original&&draft.saved_pending_restart()&&!draft.has_draft(),"active immutable saved pending restart");
    check(draft.set("steer",axis(),reason),"new draft after save");auto outside=draft.saved_bytes()+"\n; outside";write(file,outside);auto refused=e::save(draft,file,"fixture-03");check(!refused.saved&&!refused.replaced&&read(file)==outside&&draft.has_draft(),"stale file refused with draft kept");
    draft.discard();check(!draft.has_draft(),"discard changes only local draft");
    std::string bad=original;bad.insert(bad.find("Schema = 1"),"SCHEMA = 1\r\n");bool rejected=false;try{(void)e::ProfileDraft(bad,policy);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"duplicate keys refused");
    bad="[Controls]\nSchema = 1\n";bad+="Future = "+std::string(65536-bad.size()-9,'x');rejected=false;try{(void)e::ProfileDraft(bad,policy);}catch(const std::invalid_argument&){rejected=true;}check(!rejected,"exact 64KiB Controls section accepted");bad+='x';rejected=false;try{(void)e::ProfileDraft(bad,policy);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Controls bound includes its section header");
    bad=original+char(0xFF);rejected=false;try{(void)e::ProfileDraft(bad,policy);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid UTF8 refused");
    e::ProfileDraft gated(original,policy,true);gated.set("steer",std::nullopt,reason);rejected=false;try{(void)gated.preview("fixture-04");}catch(const std::invalid_argument&){rejected=true;}check(rejected,"explicit optional require-steering policy");
    e::ProfileDraft locked(original,policy);locked.set("steer",std::nullopt,reason);write(file,original);e::store_detail::Handle hold(CreateFileW(file.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr));auto fail=e::save(locked,file,"fixture-05");check(!fail.saved&&!fail.replaced&&locked.has_draft(),"exclusive external lock refuses save");
}
int main(int argc,char** argv){
    try{if(argc!=2)throw std::runtime_error("Pass a fresh disposable fixture folder.");fs::path root=fs::absolute(argv[1]);if(fs::exists(root))throw std::runtime_error("Fixture folder must be new.");fs::create_directories(root);capture();draft_and_save(root);std::cout<<"PASS native binding editor "<<checks<<" checks; no device/provider/UI/game\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" checks\n";return 1;}
}
