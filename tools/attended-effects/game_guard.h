#pragma once
#include <cstring>
#include <cstdio>
namespace attended {
// Exact executable basenames, case-insensitive. Install directories are not evidence.
inline bool recognizedGame(const char* name) {
    for(const char* game : {"artofrally.exe","art of rally.exe","OR2006C2C.exe","Gravel.exe","Gravel-Win64-Shipping.exe","iRacingArcade.exe"})
        if(_stricmp(name,game)==0)return true;
    return false;
}
struct ProcessRow { const char* name; unsigned long pid; };
enum class ScanStep { Item, End, Error };
struct GameCheck {
    bool blocked=false; char reason[512]="No recognized game process found.";
    void failure(unsigned long error) {blocked=true;sprintf_s(reason,"Process enumeration failed (error %lu). Connect/test blocked.",error);}
    void game(const ProcessRow& row) {blocked=true;sprintf_s(reason,"Close %s (PID %lu) before connecting or testing.",row.name,row.pid);}
};
template<class Reader> GameCheck checkGames(Reader& reader) {
    GameCheck result;ProcessRow row={};
    ScanStep step=reader.first(row);
    // A failed first read is never silently treated as an empty/clear snapshot.
    if(step!=ScanStep::Item) {result.failure(reader.error());return result;}
    for(;;) {
        if(recognizedGame(row.name)) {result.game(row);return result;}
        step=reader.next(row);
        if(step==ScanStep::End)return result;
        if(step!=ScanStep::Item) {result.failure(reader.error());return result;}
    }
}
}
