// Steam.cpp - tastiera a schermo di Steam per i campi di testo (0.8.0)
// Linguaggio: C++17, MSVC, Win32 (x86).
//
// Il pad non sa scrivere: per il nome di un salvataggio o la chat serve una tastiera. Steam ne ha una che compare sopra
// il gioco e manda i tasti alla finestra con il fuoco (ISteamUtils::ShowFloatingGamepadTextInput). Funziona con Steam in
// Big Picture e sulla Steam Machine o sullo Steam Deck; sul desktop di Windows di solito non compare.
// Si usa la steam_api.dll che Imperivm Resurrected mette accanto al gioco e che lui stesso inizializza (SteamAPI_InitFlat
// in src/net/steamnet.cpp, 3 s dopo l'avvio, in modo "dispatch manuale"): l'interfaccia "piatta" in C delle
// esportazioni, senza intestazioni di Steamworks. Con Resurrected si aspetta che la inizializzi lui; senza Resurrected
// la si inizializza qui (si puo' fare piu' volte: Steam conta le inizializzazioni).

#include "ImperiumPad.h"
#include <cstdio>
#include <cstring>

namespace {

using SteamUtils_t = void*(__cdecl*)();
using InitFlat_t = int(__cdecl*)(char* errMsg1024);
using ShowFloating_t = bool(__cdecl*)(void* utils, int mode, int x, int y, int w, int h);

} // namespace

bool SteamKeyboard(int x, int y, int w, int h)
{
    HMODULE api = GetModuleHandleA("steam_api.dll");
    if (!api) {
        char path[MAX_PATH];
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\steam_api.dll", GameDir());
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) api = LoadLibraryA(path);
    }
    if (!api) { Log("Tastiera di Steam: steam_api.dll non presente"); return false; }
    const auto utilsFn = reinterpret_cast<SteamUtils_t>(GetProcAddress(api, "SteamAPI_SteamUtils_v011"));
    const auto show = reinterpret_cast<ShowFloating_t>(GetProcAddress(api, "SteamAPI_ISteamUtils_ShowFloatingGamepadTextInput"));
    if (!utilsFn || !show) { Log("Tastiera di Steam: steam_api.dll troppo vecchia (mancano le funzioni)"); return false; }
    void* utils = utilsFn();
    // Con Resurrected presente Steam lo inizializza lui, 3 s dopo l'avvio e in modo "dispatch manuale": se lo facessimo
    // prima noi nel modo normale, i suoi messaggi di rete potrebbero non arrivare piu'. Quindi si aspetta lui.
    HMODULE winmm = GetModuleHandleA("winmm.dll");
    char winmmPath[MAX_PATH] = {};
    if (winmm) GetModuleFileNameA(winmm, winmmPath, MAX_PATH);
    const bool resurrected = winmm && _strnicmp(winmmPath, GameDir(), strlen(GameDir())) == 0;
    if (!utils && resurrected) { Log("Tastiera di Steam: Steam non ancora pronto (lo inizializza Resurrected), riprova tra poco"); return false; }
    if (!utils) {
        const auto init = reinterpret_cast<InitFlat_t>(GetProcAddress(api, "SteamAPI_InitFlat"));
        char err[1024] = {};
        const int r = init ? init(err) : -1;
        Log("Tastiera di Steam: Steam non ancora pronto, inizializzato qui (%d %s)", r, err);
        utils = utilsFn();
    }
    if (!utils) { Log("Tastiera di Steam: Steam non disponibile (gioco non avviato da Steam?)"); return false; }
    const bool ok = show(utils, 0 /* una riga */, x, y, w, h);
    Log("Tastiera di Steam: %s", ok ? "chiesta (compare con Steam in Big Picture o sulla Steam Machine)" : "Steam ha rifiutato (non in Big Picture?)");
    return ok;
}
