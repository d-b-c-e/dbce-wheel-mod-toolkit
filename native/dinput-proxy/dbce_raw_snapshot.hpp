#pragma once
// Optional in-process copies of game-owned reads. No device ownership or polling.
#include <windows.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <thread>

namespace dbce { namespace input {

class RawSnapshotStore {
public:
    struct Snapshot {
        GUID instance{};
        std::uint64_t generation=0, serial=0, invocation=0, descriptor=0, startedMs=0, completedMs=0;
        int format=0;
        DWORD bytes=0;
        DIJOYSTATE2 state{}; // bytes identifies the exact valid standard-format prefix.
    };
    struct Ticket {
        const void *device=nullptr;
        std::uint64_t generation=0, serial=0, invocation=0, descriptor=0, startedMs=0;
        int format=0;
    };
    struct Mutation { const void *device=nullptr;std::uint64_t generation=0,serial=0; };
    RawSnapshotStore()=default;
    RawSnapshotStore(const RawSnapshotStore &)=delete;
    RawSnapshotStore &operator=(const RawSnapshotStore &)=delete;
    void Disable() { std::lock_guard<std::mutex> lock(mutex_);disable(); }

    std::uint64_t Register(const void *device, const GUID &reported, bool identified, bool hooksAdmitted=true, std::uint64_t expectedGeneration=0) {
        if(!device)return 0;
        const auto generation=nextGeneration(); // Separate lock; never nested with this store's mutex.
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device || !healthy_) return 0;
        if(!hooksAdmitted) { disable();return 0; }
        Entry *e=find(device);
        if(expectedGeneration&&(!e||e->generation!=expectedGeneration))return 0;
        if (e) invalidateGuid(e->instance);
        if (!e) for (auto &candidate:entries_) if (!candidate.device) { e=&candidate; break; }
        if (!e || !generation) { disable(); return 0; }
        invalidateGuid(reported);
        if(!identified||zero(reported))clearSamples();
        *e=Entry{};e->device=device;e->instance=reported;e->identified=identified&&!zero(reported);
        e->eligible=hooksAdmitted;e->generation=generation;
        return e->generation;
    }
    Mutation BeginMutation(const void *device, bool formatMayChange=false) {
        std::lock_guard<std::mutex> lock(mutex_);
        Entry *e=find(device);if(!e)return {};
        invalidate(*e);
        if(hasMutation(*e)) { e->overlap=true;e->format=0; }
        else e->overlap=false;
        if(formatMayChange)e->format=0;
        if(!advanceDescriptor(*e))return {};
        for(auto &id:e->mutations)if(!id) { id=e->serial;return {device,e->generation,id}; }
        disable();return {};
    }
    void EndMutation(const Mutation &m, int validatedFormat=-1) {
        std::lock_guard<std::mutex> lock(mutex_);
        Entry *e=match(m.device,m.generation);if(!e)return;
        bool found=false;
        for(auto &id:e->mutations)if(id==m.serial&&m.serial) { id=0;found=true;break; }
        if(!found)return;
        invalidate(*e);if(!advanceDescriptor(*e))return;
        if(validatedFormat>=0&&!e->overlap&&!hasMutation(*e))e->format=validatedFormat;
    }
    std::uint64_t Generation(const void *device) {
        std::lock_guard<std::mutex> lock(mutex_);
        const Entry *e=find(device);return e?e->generation:0;
    }
    void Invalidate(const void *device, std::uint64_t generation) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Entry *e=match(device,generation)) invalidate(*e);
    }
    void Retire(const void *device, std::uint64_t generation) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Entry *e=match(device,generation)) { GUID guid=e->instance;bool identified=e->identified;*e=Entry{};if(identified)invalidateGuid(guid);else clearSamples(); }
    }

    // Invocation belongs to the game adapter, never to a wall clock/read ordinal.
    std::uint64_t BeginInvocation() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (occupied_) { admitted_=false;clearSamples();return 0; }
        if (!healthy_ || nextInvocation_==(std::numeric_limits<std::uint64_t>::max)()) { disable();return 0; }
        occupied_=admitted_=true;sealed_=false;owner_=std::this_thread::get_id();
        invocation_=++nextInvocation_;clearSamples();return invocation_;
    }
    bool Seal(std::uint64_t invocation) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!owned(invocation)||!admitted_||sealed_) return false;
        sealed_=true;return true;
    }
    void EndInvocation(std::uint64_t invocation) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!owned(invocation)) return;
        admitted_=sealed_=occupied_=false;invocation_=0;clearSamples();
    }

    // GetTickCount64 observations measure read age, not a hardware sample time.
    Ticket BeginRead(const void *device, std::uint64_t nowMs) {
        std::lock_guard<std::mutex> lock(mutex_);
        Entry *e=find(device);if (!e) return {};
        invalidate(*e); // Advance first, then stamp the returned reservation.
        if(e->timeKnown&&nowMs<e->lastStartedMs)return {};
        e->timeKnown=true;e->lastStartedMs=nowMs;
        Ticket t;t.device=device;t.generation=e->generation;t.serial=e->serial;t.startedMs=nowMs;t.format=e->format;
        t.descriptor=e->descriptor;
        if (healthy_&&occupied_&&admitted_&&!sealed_&&owner_==std::this_thread::get_id()&&e->identified&&e->eligible&&!hasMutation(*e))
            t.invocation=invocation_;
        return t;
    }
    void CompleteRead(const Ticket &t, HRESULT hr, DWORD cb, const void *data, std::uint64_t nowMs) {
        Snapshot next;
        const bool standard=(t.format==1&&cb==sizeof(DIJOYSTATE))||(t.format==2&&cb==sizeof(DIJOYSTATE2));
        const bool success=SUCCEEDED(hr)&&data&&standard&&nowMs>=t.startedMs;
        if (success) { next.bytes=cb;next.format=t.format;std::memcpy(&next.state,data,cb); }
        std::lock_guard<std::mutex> lock(mutex_);
        Entry *e=match(t.device,t.generation);if (!e) return; // Old freed-address completion cannot touch its successor.
        if (!success) { invalidate(*e);return; }
        if (!owned(t.invocation)||!admitted_||sealed_||!healthy_||e->serial!=t.serial||e->format!=t.format||e->descriptor!=t.descriptor||hasMutation(*e)) {
            // A stale earlier success must not erase a newer accepted sample.
            if (e->serial==t.serial) invalidate(*e);
            return;
        }
        next.instance=e->instance;next.generation=e->generation;next.serial=t.serial;
        next.invocation=t.invocation;next.descriptor=t.descriptor;next.startedMs=t.startedMs;next.completedMs=nowMs;
        e->sample=next;e->valid=true;
    }
    bool ReadUnique(const GUID &instance, std::uint64_t invocation, std::uint64_t nowMs, std::uint64_t maximumAgeMs, Snapshot &out) {
        std::lock_guard<std::mutex> lock(mutex_);
        out=Snapshot{}; // A failed copy cannot leave a last-good value in caller output.
        if (!healthy_||!owned(invocation)||!admitted_||!sealed_||zero(instance)) return false;
        Entry *selected=nullptr;
        for (auto &e:entries_) if(e.device) {
            if (!e.identified) return false;
            if (same(e.instance,instance)) { if(selected)return false;selected=&e; }
        }
        if (!selected||!selected->eligible||!selected->valid||hasMutation(*selected)||selected->sample.descriptor!=selected->descriptor||
            selected->sample.invocation!=invocation||selected->sample.serial!=selected->serial||
            nowMs<selected->sample.completedMs||nowMs<selected->sample.startedMs||nowMs-selected->sample.startedMs>maximumAgeMs) return false;
        out=selected->sample;return true;
    }
private:
    struct Entry {
        const void *device=nullptr;GUID instance{};std::uint64_t generation=0,serial=0,descriptor=1;
        std::array<std::uint64_t,8> mutations{};bool overlap=false;
        std::uint64_t lastStartedMs=0;bool timeKnown=false;
        int format=0;bool identified=false,eligible=false,valid=false;Snapshot sample;
    };
    std::mutex mutex_;
    std::array<Entry,64> entries_{};
    std::thread::id owner_;
    std::uint64_t serial_=0,nextInvocation_=0,invocation_=0;
    bool healthy_=true,occupied_=false,admitted_=false,sealed_=false;
    static bool same(const GUID &a,const GUID &b) { return std::memcmp(&a,&b,sizeof a)==0; }
    static bool zero(const GUID &a) { const GUID z{};return same(a,z); }
    static std::uint64_t nextGeneration() {
        // Inline function static state is shared by ordinary/fenced TUs in the module.
        static std::mutex mutex;static std::uint64_t value=0;
        std::lock_guard<std::mutex> lock(mutex);
        if(value==(std::numeric_limits<std::uint64_t>::max)())return 0;
        return ++value;
    }
    static bool hasMutation(const Entry &e) { for(auto id:e.mutations)if(id)return true;return false; }
    bool advanceDescriptor(Entry &e) {
        if(e.descriptor==(std::numeric_limits<std::uint64_t>::max)()) { disable();return false; }
        ++e.descriptor;return true;
    }
    Entry *find(const void *p) { for(auto &e:entries_)if(e.device==p&&p)return &e;return nullptr; }
    Entry *match(const void *p,std::uint64_t g) { Entry *e=find(p);return e&&g&&e->generation==g?e:nullptr; }
    bool owned(std::uint64_t i) const { return occupied_&&i&&i==invocation_&&owner_==std::this_thread::get_id(); }
    void disable() { healthy_=admitted_=false;for(auto &e:entries_)e.valid=false; }
    void invalidate(Entry &e) {
        e.valid=false;
        if(serial_==(std::numeric_limits<std::uint64_t>::max)()) { disable();return; }
        e.serial=++serial_;
    }
    void clearSamples() { for(auto &e:entries_)if(e.device)invalidate(e); }
    void invalidateGuid(const GUID &g) { for(auto &e:entries_)if(e.device&&same(e.instance,g))invalidate(e); }
};

// Ends the exact generation's mutation on all C++ exits, with COM still called
// outside the store lock. A successful format is committed only at scope exit.
class RawSnapshotMutation {
    RawSnapshotStore *store_;
    RawSnapshotStore::Mutation ticket_;
    int format_=-1;
public:
    RawSnapshotMutation(RawSnapshotStore *store,const void *device,bool formatMayChange=false):
        store_(store),ticket_(store?store->BeginMutation(device,formatMayChange):RawSnapshotStore::Mutation{}) { }
    ~RawSnapshotMutation() { if(store_)store_->EndMutation(ticket_,format_); }
    RawSnapshotMutation(const RawSnapshotMutation &)=delete;
    RawSnapshotMutation &operator=(const RawSnapshotMutation &)=delete;
    std::uint64_t Generation() const { return ticket_.generation; }
    void ValidatedFormat(int value) { format_=value; }
};

}}
