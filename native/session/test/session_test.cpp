#include "../session_writer.h"
#include "../../dinput-proxy/muted_effect.h"
#include <cassert>
#include <limits>
#include <iostream>

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::wstring root(argv[1], argv[1] + strlen(argv[1]));
    using namespace dbce::session;
    auto path = [&](const wchar_t* name) { return root + L"/" + name + L".jsonl"; };
    Metadata m{"Native fixture", "test", "native-session-1", {{"escaped", "quoted\" slash\\ line\n tab\t"}}, {{"value", "ratio"}}};
    {
        Writer w; assert(w.open(path(L"valid"), m));
        assert(w.sample(0, {{"value", -0.12345678912345678}}));
        assert(w.sample(0.016, {{"value", 0}})); assert(w.stop()); assert(w.completed());
        assert(!w.sample(1, {{"value", 1}}));
        Writer other; assert(!other.open(path(L"valid"), m));
    }
    { Writer w; assert(w.open(path(L"abandoned"), m)); assert(w.sample(0, {{"value", 1}})); }
    { Writer w; assert(w.open(path(L"invalid"), m)); assert(!w.sample(0, {{"value", std::numeric_limits<double>::quiet_NaN()}})); assert(!w.stop()); }
    { Writer w; assert(w.open(path(L"order"), m)); assert(w.sample(0, {{"value", 1}})); assert(!w.sample(0, {{"value", 2}})); }
    { Writer w; Limits l; l.seconds = 1; assert(w.open(path(L"duration"), m, l)); assert(w.sample(0, {{"value", 1}})); assert(!w.sample(2, {{"value", 2}})); assert(!w.completed()); }
    { Writer w; Limits l; l.bytes = 16384; assert(w.open(path(L"size"), m, l)); int i = 0; while(w.sample(++i * .001, {{"value", 1}})) {} assert(w.reason() == "fileSizeLimit"); }
    { Writer w; Limits l; l.recordBytes = 1024; assert(w.open(path(L"record"), m, l)); Channels c; for (int i=0; i<100; ++i) c["field"+std::to_string(i)] = i; assert(!w.sample(0, c)); assert(w.reason() == "recordSizeLimit"); }
    { Writer w; auto bad = m; bad.game = "\xff"; bool rejected = false; try { w.open(path(L"badmetadata"), bad); } catch (const std::invalid_argument&) { rejected = true; } assert(rejected); }
    DICONSTANTFORCE force{7531}; DWORD axis=DIJOFS_X; LONG dir=-1;
    DIEFFECT request{}; request.dwSize=sizeof request; request.dwGain=8721;
    request.cAxes=1; request.rgdwAxes=&axis; request.rglDirection=&dir;
    request.cbTypeSpecificParams=sizeof force; request.lpvTypeSpecificParams=&force;
    dbce::dproxy::MutedEffectWrite muted;
    assert(muted.copy(&request, DIEP_TYPESPECIFICPARAMS | DIEP_START));
    assert(muted.effect.dwGain == 0 && (muted.flags & DIEP_GAIN) && !(muted.flags & DIEP_START));
    assert(request.dwGain==8721 && force.lMagnitude==7531 && dir==-1);
    assert(muted.effect.lpvTypeSpecificParams == &force);
    request.dwSize=1; assert(!muted.copy(&request, DIEP_ALLPARAMS));
    assert(!muted.copy(nullptr, 0));
    std::cout << "PASS native session boundaries and effect-copy policy\n";
}
