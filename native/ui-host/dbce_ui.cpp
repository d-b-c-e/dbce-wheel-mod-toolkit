// dbce_ui.dll: the shared F6 panel core in-process for non-native engines (see dbce_ui.h). UI-2.
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <deque>
#include <string>
#include "dbce_ui.h"
#include "../panel/dbce_panel.h"
#include "../panel/dbce_panel_descriptor.h"
#include "../panel/dbce_overlay_d3d11.h"
#include "../panel/dbce_overlay_d3d12.h"
#include "../panel/dbce_overlay_d3d9.h"

namespace {

namespace panel = dbce::panel;

// What the panel points at (descriptor, ini path) lives here until the next configure has replaced it.
struct Setup {
    panel::Descriptor descriptor;
    std::string iniPath;
};

SRWLOCK g_apiLock = SRWLOCK_INIT;        // configure, against the start thread's placeholder
Setup *g_setup;
bool g_configured;
volatile LONG g_status;
volatile LONG g_ffbOn = 1;

SRWLOCK g_ownLock = SRWLOCK_INIT;        // dbce_ui's own events (F8) and log lines
std::deque<panel::PanelEvent> g_ownEvents;
std::deque<std::string> g_logLines;
HANDLE g_logFile = INVALID_HANDLE_VALUE;

IDirect3DDevice9 *g_pending9;            // dbce_ui_attach_d3d9's device, taken on the render thread (one reference)
SRWLOCK g_d3d9Lock = SRWLOCK_INIT;

void setStatus(LONG bit) { InterlockedOr(&g_status, bit); }

void logLine(const char *line)
{
    if (!line) return;
    if (strstr(line, "present hook installed")) setStatus(DBCE_UI_STATUS_HOOKED);
    if (strstr(line, "drawing on the swap chain") || strstr(line, "drawing on D3D9 device")) setStatus(DBCE_UI_STATUS_DRAWING);
    AcquireSRWLockExclusive(&g_ownLock);
    if (g_logLines.size() >= 64) g_logLines.pop_front();
    g_logLines.emplace_back(line);
    if (g_logFile != INVALID_HANDLE_VALUE) {
        DWORD wrote = 0;
        WriteFile(g_logFile, line, (DWORD)strlen(line), &wrote, nullptr);
        WriteFile(g_logFile, "\r\n", 2, &wrote, nullptr);
    }
    ReleaseSRWLockExclusive(&g_ownLock);
}

void logf(const char *fmt, ...)
{
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(b, sizeof b, _TRUNCATE, fmt, ap);
    va_end(ap);
    logLine(b);
}

bool ffbOn() { return InterlockedCompareExchange(&g_ffbOn, 0, 0) != 0; }
void stopFfb()   // F8, on the window thread: the mod turns its force feedback off when it polls this
{
    panel::PanelEvent e;
    e.type = panel::PanelEvent::Type::Action;
    e.id = "panel.stop-ffb";
    AcquireSRWLockExclusive(&g_ownLock);
    if (g_ownEvents.size() < 16) g_ownEvents.push_back(e);
    ReleaseSRWLockExclusive(&g_ownLock);
}

// Under g_apiLock. The panel copies the config; the setup it points at stays until the next one replaced it.
void configureLocked(Setup *setup, int flags)
{
    panel::Config c;
    c.title = setup->descriptor.product.c_str();
    c.modName = setup->descriptor.product.c_str();
    c.iniPath = setup->iniPath.empty() ? nullptr : setup->iniPath.c_str();
    c.descriptor = &setup->descriptor;
    c.log = logLine;
    if (flags & DBCE_UI_FFB_STOP) { c.ffbOn = ffbOn; c.stopFfb = stopFfb; }
    panel::Configure(c);
    delete g_setup;
    g_setup = setup;
}

void openLog(HMODULE self)
{
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    if (!n || n >= MAX_PATH - 16) return;
    wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash) return;
    wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"dbce_ui.log");
    g_logFile = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

// The hooks start here, off the loader lock: a placeholder page set until the mod configures (F6 then shows Help).
DWORD WINAPI startThread(LPVOID self)
{
    openLog((HMODULE)self);
    AcquireSRWLockExclusive(&g_apiLock);
    if (!g_configured) {
        Setup *s = new Setup;
        s->descriptor.product = "Wheel mod";
        configureLocked(s, 0);
    }
    ReleaseSRWLockExclusive(&g_apiLock);
    // D3D12 only where the game already uses it: a D3D11 game never gets a D3D12 probe device.
    const bool d3d12 = GetModuleHandleW(L"d3d12.dll") != nullptr;
    if (d3d12) dbce::overlay::EnableD3D12();
    logf("dbce_ui: API %d, starting the present hook (D3D11%s)", DBCE_UI_API_VERSION, d3d12 ? " and D3D12" : "");
    dbce::overlay::StartD3D11(logLine);
    return 0;
}

void copyOut(char *out, int size, const std::string &s)
{
    if (out && size > 0) strncpy_s(out, (size_t)size, s.c_str(), _TRUNCATE);
}

IDirect3DDevice9 *g_attached9;           // the device StartD3D9 took (render thread; no reference of ours)

// GL.IssuePluginEvent on the render thread: 1 takes the device dbce_ui_attach_d3d9 handed over, 2 lets go of it before
// the game releases it (dbce_overlay_d3d9.h StopD3D9).
void __stdcall renderEvent(int id)
{
    if (id == DBCE_UI_RENDER_DETACH_D3D9) {
        if (g_attached9) dbce::overlay::StopD3D9(g_attached9);
        g_attached9 = nullptr;
        InterlockedAnd(&g_status, ~(LONG)DBCE_UI_STATUS_D3D9);
        return;
    }
    AcquireSRWLockExclusive(&g_d3d9Lock);
    IDirect3DDevice9 *d = g_pending9;
    g_pending9 = nullptr;
    ReleaseSRWLockExclusive(&g_d3d9Lock);
    if (!d) return;
    if (dbce::overlay::StartD3D9(d, logLine)) { g_attached9 = d; setStatus(DBCE_UI_STATUS_D3D9); }
    d->Release();
}

} // namespace

extern "C" {

int DBCE_UI_CALL dbce_ui_version(void) { return DBCE_UI_API_VERSION; }

int DBCE_UI_CALL dbce_ui_configure(const char *json, int length, const char *iniPath, int flags, char *error, int errorSize)
{
    if (error && errorSize > 0) error[0] = 0;
    if (!json) { copyOut(error, errorSize, "no descriptor"); return 0; }
    Setup *s = new Setup;
    std::string why;
    if (!panel::ParseDescriptor(json, length >= 0 ? (size_t)length : strlen(json), s->descriptor, why)) {
        delete s;
        copyOut(error, errorSize, why);
        logf("dbce_ui: descriptor refused: %s", why.c_str());
        return 0;
    }
    if (iniPath) s->iniPath = iniPath;
    AcquireSRWLockExclusive(&g_apiLock);
    char line[256];   // before the lock goes: another configure may replace (and free) this setup after it
    sprintf_s(line, "dbce_ui: %.120s %.32s, %d rows", s->descriptor.product.c_str(), s->descriptor.version.c_str(), (int)s->descriptor.rows.size());
    configureLocked(s, flags);
    g_configured = true;
    ReleaseSRWLockExclusive(&g_apiLock);
    AcquireSRWLockExclusive(&g_ownLock);
    g_ownEvents.clear();
    ReleaseSRWLockExclusive(&g_ownLock);
    setStatus(DBCE_UI_STATUS_CONFIGURED);
    logLine(line);
    return 1;
}

int DBCE_UI_CALL dbce_ui_set_toggle(const char *id, int value) { return panel::SetToggle(id, value != 0); }
int DBCE_UI_CALL dbce_ui_set_choice(const char *id, int index) { return panel::SetChoice(id, index); }
int DBCE_UI_CALL dbce_ui_set_number(const char *id, double value) { return panel::SetNumber(id, value); }
int DBCE_UI_CALL dbce_ui_set_text(const char *id, const char *text, int tone)
{
    const panel::Tone t = tone == DBCE_UI_TONE_GOOD ? panel::Tone::Good : tone == DBCE_UI_TONE_WARNING ? panel::Tone::Warning :
                          tone == DBCE_UI_TONE_ERROR ? panel::Tone::Error : panel::Tone::Normal;
    return panel::SetText(id, text, t);
}
int DBCE_UI_CALL dbce_ui_set_row_visible(const char *id, int visible) { return panel::SetRowVisible(id, visible != 0); }
void DBCE_UI_CALL dbce_ui_set_ffb_on(int on) { InterlockedExchange(&g_ffbOn, on ? 1 : 0); }

int DBCE_UI_CALL dbce_ui_poll_event(int *type, char *id, int idSize, int *toggle, int *choice, double *number)
{
    panel::PanelEvent e;
    AcquireSRWLockExclusive(&g_ownLock);
    bool any = !g_ownEvents.empty();
    if (any) { e = g_ownEvents.front(); g_ownEvents.pop_front(); }
    ReleaseSRWLockExclusive(&g_ownLock);
    if (!any) any = panel::PollEvent(e);
    if (!any) { if (type) *type = DBCE_UI_EVENT_NONE; copyOut(id, idSize, std::string()); return 0; }
    if (type) *type = e.type == panel::PanelEvent::Type::Changed ? DBCE_UI_EVENT_CHANGED :
                      e.type == panel::PanelEvent::Type::Action ? DBCE_UI_EVENT_ACTION :
                      e.type == panel::PanelEvent::Type::Capture ? DBCE_UI_EVENT_CAPTURE : DBCE_UI_EVENT_CLEAR;
    copyOut(id, idSize, e.id);
    if (toggle) *toggle = e.toggle ? 1 : 0;
    if (choice) *choice = e.choice;
    if (number) *number = e.number;
    return 1;
}

int DBCE_UI_CALL dbce_ui_navigate(int command)
{
    if (command < DBCE_UI_NAV_UP || command > DBCE_UI_NAV_CLOSE) return 0;
    if (!g_configured && command != DBCE_UI_NAV_CLOSE) return 0;   // the placeholder pages are not navigable
    return panel::Navigate((panel::Nav)command);
}
int DBCE_UI_CALL dbce_ui_is_open(void) { return panel::IsOpen(); }
int DBCE_UI_CALL dbce_ui_state(int *page, int *more, char *focus, int focusSize)
{
    if (page) *page = (int)panel::DescriptorPage();
    if (more) *more = panel::DescriptorMoreOptions() ? 1 : 0;
    if (focus && focusSize > 0) panel::NavFocus(focus, (size_t)focusSize);
    return panel::IsOpen();
}

void DBCE_UI_CALL dbce_ui_show_toast(const char *line1, const char *line2, unsigned ms) { panel::ShowToast(line1, line2, ms ? ms : 3000); }
void DBCE_UI_CALL dbce_ui_note_saved(int ok, const char *detail) { panel::NoteSaved(ok != 0, detail); }

int DBCE_UI_CALL dbce_ui_attach_d3d9(void *p)
{
    if (!p) return 0;
    IUnknown *u = (IUnknown *)p;
    IDirect3DDevice9 *d = nullptr;
    if (FAILED(u->QueryInterface(__uuidof(IDirect3DDevice9), (void **)&d)) || !d) {
        IDirect3DResource9 *r = nullptr;
        if (SUCCEEDED(u->QueryInterface(__uuidof(IDirect3DResource9), (void **)&r)) && r) {
            if (FAILED(r->GetDevice(&d))) d = nullptr;
            r->Release();
        }
    }
    if (!d) { logLine("dbce_ui: attach_d3d9 got neither a D3D9 device nor a resource of one"); return 0; }
    AcquireSRWLockExclusive(&g_d3d9Lock);
    if (g_pending9) g_pending9->Release();
    g_pending9 = d;
    ReleaseSRWLockExclusive(&g_d3d9Lock);
    return 1;
}
void *DBCE_UI_CALL dbce_ui_render_event_func(void) { return (void *)&renderEvent; }

int DBCE_UI_CALL dbce_ui_status(void) { return (int)InterlockedCompareExchange(&g_status, 0, 0); }
int DBCE_UI_CALL dbce_ui_poll_log(char *line, int size)
{
    std::string s;
    AcquireSRWLockExclusive(&g_ownLock);
    const bool any = !g_logLines.empty();
    if (any) { s = g_logLines.front(); g_logLines.pop_front(); }
    ReleaseSRWLockExclusive(&g_ownLock);
    copyOut(line, size, s);
    return any;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        // Started off the loader lock: the thread runs once LoadLibrary returns. Never unloaded: the hooks stay in.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)&DllMain, &pinned);
        HANDLE t = CreateThread(nullptr, 0, startThread, self, 0, nullptr);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
