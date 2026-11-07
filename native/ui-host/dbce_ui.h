/* dbce_ui.h: the C API of dbce_ui.dll, the shared F6 panel core hosted inside a game whose mod is not a native DLL
 * (Unity Mono, Mono 2.0 and IL2CPP first; UI-2, docs/architecture/2026-11/00-overview.md section 3, host A).
 *
 * Loading the DLL is all it takes to start it: a worker thread installs the panel's DXGI present hook (D3D11, and
 * D3D12 when d3d12.dll is already loaded) and the input hook follows the game's swap chain window on its first
 * Present, exactly as in the native mods. F6 opens the panel, F8 stops force feedback when the mod offers it, Esc
 * closes. A D3D9 game also hands over its device (dbce_ui_attach_d3d9).
 *
 * The mod talks to it only through these calls, on its own thread (a Unity Update): it gives the core its settings
 * descriptor, pushes current values, and polls the player's changes and pressed actions. The core never calls the mod:
 * nothing here takes a callback, so no managed code runs on the game's render thread.
 *
 * Strings are UTF-8 and null-terminated. Every function is safe from any thread. Integers 1/0 are true/false.
 * C ABI: add functions, never change or remove one (dbce_ui_version reports DBCE_UI_API_VERSION).
 */
#ifndef DBCE_UI_H
#define DBCE_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#define DBCE_UI_API_VERSION 1
#define DBCE_UI_CALL __cdecl

/* Navigation commands (dbce_ui_navigate): the same path as the keyboard's arrows, Enter or Space, and Esc, so the
 * wheel's D-pad and two buttons drive F6 exactly as the keyboard does. Up/Down move the focus between the tab row and
 * the page's controls; Left/Right change the focused control, or the page on the tab row; Select flips a toggle, steps
 * a choice, presses an action, starts a binding capture or opens More options; Back cancels a binding capture, else
 * closes. Up to Select apply on the next drawn frame, one per frame. */
enum {
    DBCE_UI_NAV_UP = 0, DBCE_UI_NAV_DOWN = 1, DBCE_UI_NAV_LEFT = 2, DBCE_UI_NAV_RIGHT = 3,
    DBCE_UI_NAV_SELECT = 4, DBCE_UI_NAV_BACK = 5, DBCE_UI_NAV_OPEN = 6, DBCE_UI_NAV_CLOSE = 7
};

/* dbce_ui_poll_event types. CHANGED carries the row's new value (toggle, choice or number by the row's kind); ACTION is
 * a pressed action row, or "panel.stop-ffb" for F8 when the mod offers it (DBCE_UI_FFB_STOP); CAPTURE asks the mod to
 * capture a binding (the row shows its help as the prompt until dbce_ui_set_text, or Back); CLEAR is a binding's x. */
enum { DBCE_UI_EVENT_NONE = 0, DBCE_UI_EVENT_CHANGED = 1, DBCE_UI_EVENT_ACTION = 2, DBCE_UI_EVENT_CAPTURE = 3, DBCE_UI_EVENT_CLEAR = 4 };

/* dbce_ui_set_text tones. */
enum { DBCE_UI_TONE_NORMAL = 0, DBCE_UI_TONE_GOOD = 1, DBCE_UI_TONE_WARNING = 2, DBCE_UI_TONE_ERROR = 3 };

/* dbce_ui_configure flags. */
enum { DBCE_UI_FFB_STOP = 1 };   /* F8 sends the "panel.stop-ffb" action; the mod reports its state with dbce_ui_set_ffb_on */

/* dbce_ui_status bits. */
enum { DBCE_UI_STATUS_HOOKED = 1, DBCE_UI_STATUS_DRAWING = 2, DBCE_UI_STATUS_CONFIGURED = 4, DBCE_UI_STATUS_D3D9 = 8 };

/* The standard pages, for dbce_ui_state. */
enum { DBCE_UI_PAGE_CONTROLS = 0, DBCE_UI_PAGE_FFB = 1, DBCE_UI_PAGE_DISPLAY = 2, DBCE_UI_PAGE_CAMERAS = 3,
       DBCE_UI_PAGE_TELEMETRY = 4, DBCE_UI_PAGE_HELP = 5, DBCE_UI_PAGE_DEVELOPER = 6 };

int DBCE_UI_CALL dbce_ui_version(void);

/* The settings descriptor (the "settings" block of dbce-mod.json; native/panel/dbce_panel_descriptor.h), read strictly.
 * ini_path (may be null) is the mod's canonical config, where the panel keeps its own [Panel] View, Scale, Page and
 * ShowFrameRate. Every row starts at its default; push the saved values next. Calling it again replaces the pages.
 * 0 on a refused descriptor, with the one-line reason in error (may be null). */
int DBCE_UI_CALL dbce_ui_configure(const char *descriptor_json, int length, const char *ini_path, int flags,
                                   char *error, int error_size);

/* Current values; 0 when no row has that id or the kind does not match. Values the mod sets are not reported back. */
int DBCE_UI_CALL dbce_ui_set_toggle(const char *id, int value);
int DBCE_UI_CALL dbce_ui_set_choice(const char *id, int index);
int DBCE_UI_CALL dbce_ui_set_number(const char *id, double value);              /* percent, slider, number (clamped) */
int DBCE_UI_CALL dbce_ui_set_text(const char *id, const char *text, int tone);  /* status line, binding's input */
int DBCE_UI_CALL dbce_ui_set_row_visible(const char *id, int visible);
void DBCE_UI_CALL dbce_ui_set_ffb_on(int on);           /* with DBCE_UI_FFB_STOP: whether force feedback is on now */

/* The oldest change or command: 1 with the event filled in (id truncated to id_size), 0 when there is none. Any
 * output pointer may be null. */
int DBCE_UI_CALL dbce_ui_poll_event(int *type, char *id, int id_size, int *toggle, int *choice, double *number);

/* A navigation command (DBCE_UI_NAV_*): 0 when it does not apply (no descriptor, or the panel is closed). */
int DBCE_UI_CALL dbce_ui_navigate(int command);
int DBCE_UI_CALL dbce_ui_is_open(void);
/* The page shown (DBCE_UI_PAGE_*), whether More options is open, and the focused control's id ("" on the tab row), as
 * last drawn. Returns whether the panel is open. Any pointer may be null. */
int DBCE_UI_CALL dbce_ui_state(int *page, int *more_options, char *focus, int focus_size);

/* Up to two lines at the bottom of the centre screen for ms (3000 when 0), panel open or closed: camera names, the F7
 * view, notices. A new toast replaces the old one. */
void DBCE_UI_CALL dbce_ui_show_toast(const char *line1, const char *line2, unsigned ms);
/* After the mod saved a change (STD-044): "Saved" for two seconds, or the failure. */
void DBCE_UI_CALL dbce_ui_note_saved(int ok, const char *detail);

/* D3D9 games: the game's IDirect3DDevice9, or any resource of it (Unity's Texture.GetNativeTexturePtr() is an
 * IDirect3DBaseTexture9). The device is taken on the render thread, at the next call of the function
 * dbce_ui_render_event_func returns with DBCE_UI_RENDER_ATTACH_D3D9 (Unity: GL.IssuePluginEvent(func, 1)). 0 when it
 * is neither. */
int DBCE_UI_CALL dbce_ui_attach_d3d9(void *device_or_resource);
/* A void (__stdcall *)(int event_id) for GL.IssuePluginEvent, run on the render thread: ATTACH finishes
 * dbce_ui_attach_d3d9; DETACH lets go of that device before the game releases it (a device recreated for a mode
 * change: detach, then attach the new one). */
enum { DBCE_UI_RENDER_ATTACH_D3D9 = 1, DBCE_UI_RENDER_DETACH_D3D9 = 2 };
void *DBCE_UI_CALL dbce_ui_render_event_func(void);

/* DBCE_UI_STATUS_* bits: the present hook is installed, it draws on a swap chain, a descriptor is set, a D3D9 device
 * is attached. */
int DBCE_UI_CALL dbce_ui_status(void);
/* The oldest unread log line (the hooks, the swap chain, refusals): 1 with the line, 0 when there is none. The same
 * lines go to dbce_ui.log beside the DLL. */
int DBCE_UI_CALL dbce_ui_poll_log(char *line, int size);

#ifdef __cplusplus
}
#endif

#endif /* DBCE_UI_H */
