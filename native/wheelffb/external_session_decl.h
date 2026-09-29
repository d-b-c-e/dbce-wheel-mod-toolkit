// Internal declarations. Only successful explicit Enter opts into this mode.
#pragma once
#include <atomic>
#include "external_policy.h"
static std::atomic<bool> g_externalMode(false);
static std::atomic<unsigned> g_externalDeviceCalls(0);
static bool ExternalOwnerValid();
static void ExternalLegacyCancel(bool close);
static int64_t ExternalReadTime();
static void ExternalNoteRead(int slot,bool valid,int64_t started);
static void ExternalInvalidateReads();
static void ExternalQueueInputClose(bool aux);
// Every input path which may touch a device/table shares the same gate as
// output/cleanup. The policy watchdog never needs this gate to revoke authority.
struct ExternalDeviceGuard {
    bool held,allowed;
    ExternalDeviceGuard():held(g_externalMode.load()),allowed(true) {
        if(held){
            EnterCriticalSection(&g_lock);
            // CRITICAL_SECTION is recursive. A driver may pump our HWND and
            // reenter on this SAME thread. Logical exclusion is required too.
            if(g_externalDeviceCalls.load()){LeaveCriticalSection(&g_lock);held=false;allowed=false;}
            else ++g_externalDeviceCalls;
        }
    }
    ~ExternalDeviceGuard(){if(held){--g_externalDeviceCalls;LeaveCriticalSection(&g_lock);}}
};
struct ExternalReadResult {
    int slot;bool valid;int64_t started;
    explicit ExternalReadResult(int value):slot(value),valid(false),started(g_externalMode.load()?ExternalReadTime():0){}
    ~ExternalReadResult(){if(g_externalMode.load())ExternalNoteRead(slot,valid,started);}
};
