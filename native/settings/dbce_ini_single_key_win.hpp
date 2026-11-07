// Atomic owned-key saves shared by panel, force and consumer writers across translation units.
#pragma once
#include "dbce_ini_settings_win.hpp"
#include <mutex>

namespace dbce::settings {
inline bool saveKeys(const char* path, const char* section, const Changes& changes) {
    // One inline-function static for the entire program, including single-key and batch callers.
    static std::mutex writers;
    const std::lock_guard<std::mutex> guard(writers);
    if (!path || !section || changes.empty()) { SetLastError(ERROR_INVALID_PARAMETER); return false; }
    // First use may create the INI or section, never its directory. Malformed input is refused.
    return save(path, section, changes, true);
}
inline bool saveKey(const char* path, const char* section, const char* key, const char* value) {
    if (!key || !value) { SetLastError(ERROR_INVALID_PARAMETER); return false; }
    return saveKeys(path, section, {{key, value}});
}
} // namespace dbce::settings
