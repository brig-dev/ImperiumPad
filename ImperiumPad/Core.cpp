// Core.cpp - thread principale di ImperiumPad
// Linguaggio: C++17. Compilatore: MSVC (Visual Studio Community 2026), Win32 (x86).
//
// Versione 0.3.0 (passi 0.2.0 e 0.3.0 di docs/ImperiumPad_controller.md):
//  - leva sinistra = cursore vero di Windows (al bordo dello schermo la mappa scorre da sola);
//  - A = clic sinistro (tenuto = riquadro di selezione, doppio = stesso tipo o esercito intero);
//  - X = clic destro (comando predefinito; tenuto = orientamento della formazione);
//  - B = Esc solo se c'e' qualcosa da annullare (in mappa) o sempre (nei menu);
//  - LT tenuto = Maiusc virtuale;
//  - leva destra = camera (frecce del gioco, solo in mappa);
//  - croce: su F7 (esercito dell'eroe), giu' Q (ultimo avviso), sinistra accento grave (barre della salute),
//    destra tenuta barra rovesciata (vedere attraverso); nei menu la croce sposta il cursore a piccoli passi;
//  - RB/LB tocco = eroe successivo/precedente (virgola, Maiusc+virgola); L3 = Spazio (mappa grande), R3 = Home
//    (centra), Menu = F10 (menu di gioco), View tocco = I (pannello informazioni);
//  - Y, RB e LB tenuti: riservati ai radiali (versioni successive), solo nel log.
// Versione 0.5.0:
//  - nei menu la croce (e la leva destra) salta da una casella all'altra (pulsanti, tendine, elenchi, cursori) invece di
//    spostare il cursore a piccoli passi; le caselle le legge Overlay.cpp nel thread del gioco; dentro un elenco su/giu'
//    scorre le righe;
//  - in mappa, con il pad attivo, sui pulsanti della barra comandi compaiono i simboli del pad; RT tenuto + quel tasto
//    esegue il comando (stessa funzione del clic, 0x6b3500);
//  - simboli e nomi dei tasti nello stile Steam, Xbox o PlayStation ([Pad] Glyphs); tenendo View (aiuto a schermo) la
//    croce sinistra/destra cambia lo stile e lo salva nell'ini.
// Versione 0.6.0:
//  - camera analogica: la leva destra sposta la vista con velocita' proporzionale all'inclinazione e in ogni direzione
//    (Camera.cpp); senza aggancio resta il vecchio scorrimento con le frecce;
//  - RB tenuto: radiale dei comandi della barra (per l'eroe formazioni e abilita'), LB tenuto: radiale dei gruppi 1-0;
//    la leva sinistra sceglie (il cursore resta fermo), la croce sinistra/destra passa alla voce vicina, B annulla,
//    lasciando il dorsale si esegue; nei gruppi LT tenuto assegna la selezione (Ctrl+cifra) invece di selezionare.
// Versione 0.8.0:
//  - cursore disegnato: se SetCursorPos non sposta il cursore per 30 volte di fila (sotto gamescope), il pad passa da
//    solo al cursore disegnato dall'overlay ([Cursor] Mode=Auto; Real e Virtual lo fissano);
//  - calamita: sopra un oggetto della mappa, un pulsante della barra o una casella dei menu il cursore rallenta;
//  - vibrazione breve a ogni avviso nuovo, doppia quando l'eroe selezionato scende sotto un quarto della salute;
//  - tempo rallentato mentre un radiale e' aperto, solo in partita singola (GameTick.cpp);
//  - RT + LT + tasto: comandi della barra dal tredicesimo pulsante (i simboli si spostano li');
//  - Y nei menu: tastiera di Steam per scrivere (Steam.cpp);
//  - View tenuto + Menu: impostazioni di ImperiumPad nel gioco, salvate nell'ini;
//  - [Buttons] nell'ini: tasti riassegnati.
// Versione 0.8.3 (prova sulla Steam Machine del 31/10/2026: A sul simbolo dell'edificio selezionava invece di eseguire):
//  - i simboli sui pulsanti compaiono tenendo RT ([Pad] BarSymbols, anche nelle impostazioni), accanto alla barra resta
//    il segno di RT; la prima volta in mappa e con Y da solo un riquadro dice come si fa;
//  - senza selezione RT + tasto (e il radiale RB) vale per il menu senza selezione: guida, diplomazia, salvataggio e
//    caricamento rapido, seleziona gruppo, obiettivi, menu principale, conversazione, mappa strategica.
// Versione 0.8.4 (seconda prova sulla Steam Machine): radiale dei gruppi con LT che passa da seleziona ad assegna, niente
// azione su un gruppo vuoto o senza selezione, spiegazione nel radiale quando non ci sono gruppi; comandi del radiale
// non piu' "scaduti" per errore (Overlay.cpp).
// Versione 0.9.0: radiale dei trucchi di Imperivm Resurrected (dalla 0.9.2 Y tenuto in mappa), con voci secondo la selezione:
// edificio = oro, cibo (fino a +100000 per volta), popolazione, abitanti, riparazione, cattura; unita' = cura, viveri,
// livello 10 / 50 / 999, elimina; niente = mappa, visione, pace, tecnologie, esperienza. Solo se la stanza li permette.
// "In mappa" = partita in corso, vista della mappa visibile e con il fuoco (nessuna finestra del gioco aperta).
// Il pad agisce solo con il gioco in primo piano; uscendo, tutti i pulsanti e i tasti tenuti vengono rilasciati.

#include "ImperiumPad.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "version.lib")

namespace {

// --- Ambiente -------------------------------------------------------------------------------------------
std::string FileVersionString(const char* file, const char* key)
{
    DWORD dummy = 0;
    const DWORD size = GetFileVersionInfoSizeA(file, &dummy);
    if (!size) return "";
    std::vector<char> data(size);
    if (!GetFileVersionInfoA(file, 0, size, data.data())) return "";
    struct Lang { WORD lang, cp; } *langs = nullptr;
    UINT len = 0;
    if (!VerQueryValueA(data.data(), "\\VarFileInfo\\Translation", reinterpret_cast<void**>(&langs), &len) || len < sizeof(Lang))
        return "";
    char q[96];
    _snprintf_s(q, sizeof(q), _TRUNCATE, "\\StringFileInfo\\%04x%04x\\%s", langs[0].lang, langs[0].cp, key);
    char* value = nullptr;
    if (!VerQueryValueA(data.data(), q, reinterpret_cast<void**>(&value), &len) || !value) return "";
    return value;
}

void LogEnvironment()
{
    typedef const char*(__cdecl * WineVersion_t)();
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    auto wine = ntdll ? reinterpret_cast<WineVersion_t>(GetProcAddress(ntdll, "wine_get_version")) : nullptr;
    Log("Sistema: %s%s", wine ? "Wine " : "Windows", wine ? wine() : "");

    char steamApp[32] = {};
    GetEnvironmentVariableA("SteamAppId", steamApp, sizeof(steamApp));
    Log("Avviato da Steam: %s%s", steamApp[0] ? "si', SteamAppId=" : "no", steamApp);

    // Imperivm Resurrected si carica come winmm.dll dalla cartella del gioco
    HMODULE winmm = GetModuleHandleA("winmm.dll");
    char path[MAX_PATH] = {};
    if (winmm) GetModuleFileNameA(winmm, path, MAX_PATH);
    if (winmm && _strnicmp(path, GameDir(), strlen(GameDir())) == 0)
        Log("winmm.dll dalla cartella del gioco: %s, \"%s\" %s", path, FileVersionString(path, "ProductName").c_str(),
            FileVersionString(path, "ProductVersion").c_str());
    else
        Log("winmm.dll di sistema (%s): Imperivm Resurrected non caricato", winmm ? path : "non ancora caricata");
}

// --- Finestra del gioco ---------------------------------------------------------------------------------
BOOL CALLBACK FindGameWindow(HWND hwnd, LPARAM out)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    char cls[64] = {};
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strcmp(cls, "OSWndClass") != 0) return TRUE;   // classe registrata da gbr.exe a 0x679f40
    *reinterpret_cast<HWND*>(out) = hwnd;
    return FALSE;
}

void LogWindow(HWND hwnd, const char* when)
{
    RECT w{}, c{};
    GetWindowRect(hwnd, &w);
    GetClientRect(hwnd, &c);
    Log("Finestra del gioco (%s): HWND %p, finestra %ldx%ld a %ld,%ld, area client %ldx%ld, stile %08lx", when, hwnd,
        w.right - w.left, w.bottom - w.top, w.left, w.top, c.right - c.left, c.bottom - c.top,
        static_cast<unsigned long>(GetWindowLongA(hwnd, GWL_STYLE)));
}

void LogState(const GameState& s, const GameState& prev, bool force)
{
    if (!s.ok) return;
    const bool changed = s.game != prev.game || s.selKind != prev.selKind || s.selCount != prev.selCount ||
                         s.selOwn != prev.selOwn || s.pendingCmd != prev.pendingCmd || s.paused != prev.paused ||
                         s.focus != prev.focus || s.mapViewShown != prev.mapViewShown;
    if (!changed && !force) return;
    Log("Stato: partita %08x, giocatore %08x, pausa %u, vista %08x (visibile %u), fuoco %08x%s, selezione tipo %u "
        "numero %u propria %u, comando in attesa %08x", s.game, s.player, s.paused, s.mapView, s.mapViewShown ? 1 : 0,
        s.focus, s.InMap() ? " = mappa" : "", s.selKind, s.selCount, s.selOwn, s.pendingCmd);
}

// --- Overlay: stato del pad e aiuto ---------------------------------------------------------------------------
std::wstring PadStatus(bool connected)
{
    switch (g_cfg.padSource) {
    case PadSource::Laptop:
        return PadSimulatorActive() ? Tr(T_StatusLaptopOn) : Tr(T_StatusLaptopOff);
    case PadSource::Keyboard:
        return PadSimulatorActive() ? Tr(T_StatusNumpadOn) : Tr(T_StatusNumpadOff);
    default:
        return connected ? Tr(T_StatusController) : Tr(T_StatusNoController);
    }
}

// Nomi dei tasti nello stile scelto (simboli sulla barra e aiuto).
struct PadNames { const wchar_t *a, *b, *x, *y, *lb, *rb, *lt, *rt, *ls, *rs, *view, *menu, *style; };

PadNames Names()
{
    switch (g_cfg.glyphs) {
    case GlyphStyle::Xbox:
        return { L"A", L"B", L"X", L"Y", L"LB", L"RB", L"LT", L"RT", L"LS", L"RS", L"View", L"Menu", L"Xbox" };
    case GlyphStyle::PlayStation:   // nomi delle forme nella lingua scelta (si puo' cambiare nel gioco)
        return { Tr(T_PsCross), Tr(T_PsCircle), Tr(T_PsSquare), Tr(T_PsTriangle), L"L1", L"R1", L"L2", L"R2", L"L3", L"R3",
                 L"Create", L"Options", L"PlayStation" };
    default:
        return { L"A", L"B", L"X", L"Y", L"L1", L"R1", L"L2", L"R2", L"L3", L"R3", L"View", L"Menu", L"Steam" };
    }
}

// Nome a schermo di un tasto riassegnabile (posizione in ButtonCount), nello stile scelto (0.10.0).
std::wstring ButtonLabel(const PadNames& n, int i)
{
    const char* k = ButtonId(i);
    if (!strcmp(k, "A")) return n.a;
    if (!strcmp(k, "B")) return n.b;
    if (!strcmp(k, "X")) return n.x;
    if (!strcmp(k, "Y")) return n.y;
    if (!strcmp(k, "LB")) return n.lb;
    if (!strcmp(k, "RB")) return n.rb;
    if (!strcmp(k, "View")) return n.view;
    if (!strcmp(k, "Menu")) return n.menu;
    if (!strcmp(k, "L3")) return n.ls;
    if (!strcmp(k, "R3")) return n.rs;
    if (!strcmp(k, "Up")) return Tr(T_PadUp);
    if (!strcmp(k, "Down")) return Tr(T_PadDown);
    if (!strcmp(k, "Left")) return Tr(T_PadLeft);
    if (!strcmp(k, "Right")) return Tr(T_PadRight);
    return std::wstring(k, k + strlen(k));
}

void ShowStatus(bool connected, DWORD ms)
{
    std::wstring title = L"ImperiumPad ";
    title += IMPERIUMPAD_VERSION_NUM;
    title += L"   (";
    title += TrF(T_BadgeHelp, Names().view);
    title += L")";
    OverlayBadge(title.c_str(), PadStatus(connected).c_str(), ms);
}

// 0.8.3: come si usano i comandi della barra (RT tenuto + tasto), la prima volta che in mappa c'e' una barra e quando si
// preme Y da solo (che in mappa da solo non fa nulla).
void ShowBarHint(DWORD ms)
{
    std::wstring title = L"ImperiumPad ";
    title += IMPERIUMPAD_VERSION_NUM;
    OverlayBadge(title.c_str(), TrF(T_BadgeBarHint, Names().rt).c_str(), ms);
}

// Righe dell'aiuto: tasto del pad (nello stile scelto), tasto del simulatore (se attivo), azione. Stesso schema di
// docs/ImperiumPad_controller.md.
void ShowHelp(bool on)
{
    if (!on) { OverlayHelp(false, L"", nullptr, L""); return; }
    const PadNames n = Names();
    const std::wstring rt = TrF(T_HelpHeldPlusKey, n.rt), shoulders = std::wstring(n.rb) + L" / " + n.lb;
    const std::wstring menuKeys = std::wstring(Tr(T_KeyDel)) + L" " + Tr(T_KeyPgDn) + L" " + Tr(T_KeyHome) + L" " + Tr(T_KeyEnd);
    struct Row { std::wstring pad, laptop, numpad, action; };
    const Row rows[] = {
        { Tr(T_PadLeftStick), L"W A S D", L"8 4 6 2", Tr(T_ActCursor) },
        { Tr(T_PadRightStick), Tr(T_KeyArrows), Tr(T_KeyArrows), Tr(T_ActCamera) },
        { n.a, L"J", L"0", Tr(T_ActSelect) },
        { n.x, L"U", L"5", Tr(T_ActAction) },
        { n.b, L"K", Tr(T_KeyNumDot), Tr(T_ActCancel) },
        { n.lt, L"Y", L"+", Tr(T_ActShift) },
        { rt, TrF(T_KeyPlusKey, L"P"), TrF(T_KeyPlusKey, L"-"), TrF(T_ActBarCommand, n.lt) },
        { shoulders, L"O / H", std::wstring(L"* / ") + Tr(T_KeyEnter), Tr(T_ActHeroes) },
        { Tr(T_PadUp), L"C", Tr(T_KeyHome), Tr(T_ActHeroArmy) },
        { Tr(T_PadDown), L"V", Tr(T_KeyEnd), Tr(T_ActLastAlert) },
        { Tr(T_PadLeft), L"Z", Tr(T_KeyDel), Tr(T_ActHealthBars) },
        { Tr(T_PadRight), L"X", Tr(T_KeyPgDn), Tr(T_ActSeeThrough) },
        { Tr(T_PadMenus), L"Z X C V", menuKeys, Tr(T_ActMenuJump) },
        { n.ls, L"F", L"F12", Tr(T_ActBigMap) },
        { n.rs, L"R", L"F11", Tr(T_ActCentre) },
        { TrF(T_PadHeld, n.rs), L"R", L"F11", Tr(T_ActNextSettlement) },
        { TrF(T_PadPlusDpad, n.lt), L"Y + Z X C V", std::wstring(L"+ ") + menuKeys, Tr(T_ActHalfSelect) },
        { TrF(T_PadPlusUpDown, n.view), L"Q + C / V", std::wstring(Tr(T_KeyIns)) + L" + " + Tr(T_KeyHome) + L" / " + Tr(T_KeyEnd), Tr(T_ActSpeed) },
        { n.menu, L"E", Tr(T_KeyPgUp), Tr(T_ActGameMenu) },
        { n.view, L"Q", Tr(T_KeyIns), Tr(T_ActInfo) },
        { TrF(T_PadSettings, n.view, n.menu), L"Q + E", std::wstring(Tr(T_KeyIns)) + L" + " + Tr(T_KeyPgUp), Tr(T_ActSettings) },
        { TrF(T_PadHeld, n.y), L"I", L"/", Tr(T_ActCheats) },
        { TrF(T_PadYMenus, n.y), L"I", L"/", TrF(T_ActKeyboard, n.a) },
    };
    const bool laptop = g_cfg.padSource == PadSource::Laptop, numpad = g_cfg.padSource == PadSource::Keyboard;
    std::vector<std::wstring> lines;
    for (const Row& r : rows) {
        std::wstring line = r.pad;
        line += L"\t";
        if (laptop) line += r.laptop;
        else if (numpad) line += r.numpad;
        line += L"\t";
        line += r.action;
        lines.push_back(line);
    }
    const std::wstring title = TrF(T_HelpTitle, n.style);
    std::wstring foot = laptop ? Tr(T_FootLaptop) : numpad ? Tr(T_FootNumpad) : L"";
    foot += TrF(T_FootStyle, n.view);
    OverlayHelp(true, title.c_str(), &lines, foot.c_str());
}

// --- Trucchi (0.9.0) ------------------------------------------------------------------------------------------------
// Voci del radiale dei trucchi secondo la selezione. Sono i trucchi di Imperivm Resurrected 1.0.3 (verbi dei suoi comandi
// in chat; "s" + lettera = sulle unita' selezionate); valori massimi di Resurrected: 100000 per oro e cibo per volta,
// livello 999 (il gioco non scrive da nessuna parte un suo limite: da provare).
// 0.9.1: ogni voce ha un'icona (CheatIcon in Radial.cpp) e una scritta corta sotto (quantita'); colore = categoria
// (0 bronzo, 1 oro, 2 cibo, 3 rosso, 4 azzurro): bordo del disco e scritta.
struct Cheat { const char* verb; int value; Txt title; bool maxOnce; const wchar_t* label; int color; int icon; };

const Cheat kCheatsBuilding[] = {
    { "gold", 1000, T_ChGold, false, L"+1K", 1, CI_Coins },     { "gold", 10000, T_ChGold, false, L"+10K", 1, CI_Coins },
    { "gold", 100000, T_ChGold, true, L"MAX", 1, CI_Coins },    { "food", 1000, T_ChFood, false, L"+1K", 2, CI_Loaf },
    { "food", 10000, T_ChFood, false, L"+10K", 2, CI_Loaf },    { "food", 100000, T_ChFood, true, L"MAX", 2, CI_Loaf },
    { "maxpop", 50, T_ChMaxPop, false, L"+50", 0, CI_House },   { "population", 100, T_ChPopulation, false, L"+100", 0, CI_People },
    { "repair", 0, T_ChRepair, false, L"", 0, CI_Hammer },      { "capture", 0, T_ChCapture, false, L"", 3, CI_Flag },
    { "spawnmenu", 0, T_ChSpawnMenu, false, L"", 3, CI_Swords },   // 0.10.0: apre il radiale "crea truppe"
};
const int kSpawnCounts[] = { 1, 5, 10, 20, 50, 100 };   // quantita' del radiale "crea truppe" (100 = massimo di Resurrected)
const Cheat kCheatsUnits[] = {
    { "sh", 0, T_ChHeal, false, L"", 3, CI_Heart },             { "sf", 0, T_ChRation, false, L"", 2, CI_Loaf },
    { "sl", 10, T_ChLevel, false, L"10", 1, CI_Star },          { "sl", 50, T_ChLevel, false, L"50", 1, CI_Star },
    { "sl", 999, T_ChLevelMax, false, L"MAX", 1, CI_Star },     { "sk", 0, T_ChKill, false, L"", 3, CI_Skull },
};
const Cheat kCheatsNothing[] = {
    { "map", 0, T_ChMap, false, L"", 4, CI_Map },               { "vision", 0, T_ChVision, false, L"", 4, CI_Eye },
    { "peace", 0, T_ChPeace, false, L"", 0, CI_WhiteFlag },     { "tech", 0, T_ChTech, false, L"", 0, CI_Book },
    { "exph", 1000, T_ChExpHeroes, false, nullptr, 1, CI_Star }, { "expt", 1000, T_ChExpTroops, false, nullptr, 0, CI_Star },
};

// 0.10.1: trucchi classici della console di Imperivm (verbo con la @), eseguiti da ImperiumPad come script del gioco su
// questa macchina: solo in partita singola, e li' anche senza Resurrected o senza "Allow cheats".
// 0.10.2: attacco e difesa a parte (primo e secondo numero di AddBonus, come nelle avventure del gioco: "Veteran Defence"
// = AddBonus(0, 10, 0, 0, 0), arcieri AddBonus(9, 0, 0, 0, 0))
const Cheat kCheatsUnitsLocal[] = {
    { "@atk", 20, T_ChAttack, false, L"+20", 3, CI_Swords },    { "@atk", 100, T_ChAttack, false, L"+100", 3, CI_Swords },
    { "@def", 20, T_ChDefense, false, L"+20", 4, CI_Shield },   { "@def", 100, T_ChDefense, false, L"+100", 4, CI_Shield },
    { "@bonus", 0, T_ChBonus, false, L"MAX", 1, CI_Shield },
};
const Cheat kCheatsNothingLocal[] = {
    { "@fog", 0, T_ChFog, false, L"", 4, CI_Cloud },            { "@speed", 0, T_ChSpeed10, false, L"x10", 1, CI_Hourglass },
};

std::wstring CheatTitle(const Cheat& c)
{
    std::wstring t = TrF(c.title, c.value);
    if (c.maxOnce) t += Tr(T_ChMaxOnce);
    return t;
}

// "Allow cheats" (regola 0x8000000 di Resurrected) non sta nella memoria del gioco ma nella DLL di Resurrected; Resurrected
// la scrive pero' nel suo log a ogni partita ("game rules: 0x28000000 (room)"): si legge l'ultima riga di quel tipo.
// -1 = non si sa (log mancante), 0 spenti, 1 accesi.
int CheatsAllowed()
{
    char path[MAX_PATH];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\Logs\\imperivm\\imperivm-mod.log", GameDir());
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return -1;
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    const LONGLONG start = size.QuadPart > 262144 ? size.QuadPart - 262144 : 0;
    LARGE_INTEGER pos{};
    pos.QuadPart = start;
    SetFilePointerEx(f, pos, nullptr, FILE_BEGIN);
    std::string text(static_cast<size_t>(size.QuadPart - start), '\0');
    DWORD got = 0;
    if (!text.empty()) ReadFile(f, &text[0], static_cast<DWORD>(text.size()), &got, nullptr);
    CloseHandle(f);
    text.resize(got);
    const size_t at = text.rfind("game rules: 0x");
    if (at == std::string::npos) return -1;
    const unsigned long rules = strtoul(text.c_str() + at + 14, nullptr, 16);
    return (rules & 0x8000000UL) ? 1 : 0;
}

// --- Stato del pad e azioni ---------------------------------------------------------------------------------
struct Runtime {
    HWND  hwnd = nullptr;
    WORD  prev = 0;              // pulsanti del giro precedente
    DWORD pressedAt[16] = {};    // momento della pressione per bit
    bool  inMap = false;
    // cursore
    float fx = 0, fy = 0;        // posizione in virgola mobile (coordinate client)
    POINT lastSet{ -1, -1 };     // ultima posizione impostata da noi
    POINT beforeSet{ -1, -1 };   // dove era il cursore prima dell'ultimo spostamento chiesto
    int   noEffect = 0;          // spostamenti di fila senza effetto (SetCursorPos ignorato, per esempio sotto gamescope)
    bool  moving = false;
    DWORD movingSince = 0;
    // pulsanti tenuti verso il gioco
    bool  leftHeld = false, rightHeld = false, seeThroughHeld = false, shift = false;
    DWORD lastADown = 0;
    POINT lastADownPos{};
    // camera: frecce tenute (sinistra, destra, su, giu')
    bool  cam[4] = {};
    // croce e leva destra nei menu: ripetizione
    DWORD dpadNext = 0;
    int   stickDir = 0;          // direzione della leva destra nei menu: 1 destra, 2 sinistra, 3 giu', 4 su
    DWORD stickNext = 0;
    // pulsanti premuti per un'altra funzione (RT + tasto, cambio dei simboli): il rilascio non fa nulla
    WORD  consumed = 0;
    bool  rtHeld = false;
    // aiuto del pad mostrato (View tenuto)
    bool  helpShown = false;
    // radiale aperto (0.6.0): 0 nessuno, 1 comandi della barra (RB), 2 gruppi (LB); voce scelta
    int   radial = 0;
    int   radialHi = -1;
    bool  radialAssign = false;  // gruppi: assegna invece di selezionare (0.8.4: LT passa da uno all'altro)
    std::vector<Cheat> cheats;   // voci del radiale dei trucchi aperto (0.9.0, radiale 3)
    int   spawnCount = 2;        // 0.10.0: posto in kSpawnCounts del radiale "crea truppe" (radiale 4)
    int   settingsPage = 0;      // 0.10.0: 0 impostazioni, 1 tasti del controller
    int   buttonsSel = 0;
    DWORD speedAskedAt = 0;      // 0.10.0: passo di velocita' chiesto, si aspetta il risultato dal thread del gioco
    bool  ltHeld = false;        // LT oltre la soglia al giro precedente
    // impostazioni nel gioco (0.8.0)
    bool  settingsOpen = false;
    int   settingsSel = 0;
    // vibrazione (0.8.0): 0 nessuna, 1 avviso, 2 eroe in pericolo
    int   rumbleKind = 0;
    DWORD rumbleStart = 0;
    float rumbleLo = 0, rumbleHi = 0;
    bool  alertsKnown = false;
    uint32_t lastAlerts = 0;
    int   heroHandle = -1, heroPct = -1;
    // calamita (0.8.0): il cursore e' sopra qualcosa da cliccare
    bool  overTarget = false;
};

int BitIndex(WORD mask)
{
    for (int i = 0; i < 16; ++i) if (mask == (1u << i)) return i;
    return 0;
}

const UINT kCamVk[4] = { VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN };

void ReleaseCamera(Runtime& r)
{
    for (int i = 0; i < 4; ++i)
        if (r.cam[i]) { InputKey(kCamVk[i], false); r.cam[i] = false; }
    CameraSet(0, 0);
}

void CloseRadial(Runtime& r, const char* why)
{
    if (!r.radial) return;
    Log("Radiale %s chiuso (%s)", r.radial == 1 ? "dei comandi" : r.radial == 2 ? "dei gruppi" : r.radial == 3 ? "dei trucchi" : "crea truppe", why);
    r.radial = 0;
    r.radialHi = -1;
    OverlayRadial(0, -1, false, nullptr);
}

void ReleaseAll(Runtime& r, const char* why)
{
    bool any = r.leftHeld || r.rightHeld || r.seeThroughHeld || r.shift || r.cam[0] || r.cam[1] || r.cam[2] || r.cam[3];
    if (r.leftHeld) { InputMouse(0, false, false); r.leftHeld = false; }
    if (r.rightHeld) { InputMouse(1, false, false); r.rightHeld = false; }
    if (r.seeThroughHeld) { InputKey(VK_OEM_5, false); r.seeThroughHeld = false; }
    if (r.shift) { InputSetShift(false); r.shift = false; }
    ReleaseCamera(r);
    if (r.helpShown) { ShowHelp(false); r.helpShown = false; }
    CloseRadial(r, why);
    if (any) Log("Pulsanti e tasti tenuti rilasciati (%s)", why);
}

// Leva sinistra: cursore vero, con curva di risposta e accelerazione.
void UpdateCursor(Runtime& r, const PadState& p, float dt, DWORD now)
{
    RECT c{};
    GetClientRect(r.hwnd, &c);
    const int w = c.right - c.left, h = c.bottom - c.top;
    if (w <= 0 || h <= 0) return;

    const float m = std::sqrt(p.lx * p.lx + p.ly * p.ly);
    const float dz = g_cfg.leftDeadzone;
    if (m <= dz) { r.moving = false; return; }

    POINT cur{};
    if (!InputCursorClient(cur)) return;
    // Se dopo lo spostamento chiesto il cursore e' rimasto dove era, SetCursorPos non ha effetto: lo si dice nel log una
    // volta (rischio noto sotto gamescope; il ripiego sara' un cursore disegnato nell'overlay).
    if (r.moving && cur.x == r.beforeSet.x && cur.y == r.beforeSet.y && (cur.x != r.lastSet.x || cur.y != r.lastSet.y)) {
        if (++r.noEffect == 30) {
            Log("ATTENZIONE: il cursore non si sposta (SetCursorPos senza effetto per 30 volte di fila)");
            // 0.8.0: si passa al cursore disegnato, che il gioco riceve con i movimenti mandati con PostMessage
            if (g_cfg.cursorMode == CursorMode::Auto && !InputVirtualCursor()) {
                InputSetVirtualCursor(true);
                std::wstring title = L"ImperiumPad ";
                title += IMPERIUMPAD_VERSION_NUM;
                OverlayBadge(title.c_str(), Tr(T_BadgeVirtualCursor), 6000);
                InputCursorClient(cur);
                r.noEffect = 0;
            }
        }
    }
    else if (cur.x == r.lastSet.x && cur.y == r.lastSet.y) {
        r.noEffect = 0;
    }
    if (!r.moving || cur.x != r.lastSet.x || cur.y != r.lastSet.y) {
        // il cursore e' stato mosso col mouse (o e' la prima volta): si riparte da dove si trova
        r.fx = static_cast<float>(cur.x);
        r.fy = static_cast<float>(cur.y);
    }
    if (!r.moving) { r.moving = true; r.movingSince = now; }

    float norm = (m - dz) / (1.0f - dz);
    if (norm > 1.0f) norm = 1.0f;
    float ramp = 1.0f;
    if (g_cfg.cursorRampMs > 0) {
        ramp = 0.35f + 0.65f * static_cast<float>(now - r.movingSince) / static_cast<float>(g_cfg.cursorRampMs);
        if (ramp > 1.0f) ramp = 1.0f;
    }
    // calamita (0.8.0): sopra un oggetto, un pulsante della barra o una casella il cursore rallenta
    const float magnet = r.overTarget ? g_cfg.magnet : 1.0f;
    const float speed = g_cfg.cursorSpeed * static_cast<float>(h) * std::pow(norm, g_cfg.cursorCurve) * ramp * magnet;
    r.fx += speed * (p.lx / m) * dt;
    r.fy -= speed * (p.ly / m) * dt;
    if (r.fx < 0) r.fx = 0;
    if (r.fy < 0) r.fy = 0;
    if (r.fx > w - 1) r.fx = static_cast<float>(w - 1);
    if (r.fy > h - 1) r.fy = static_cast<float>(h - 1);
    const int x = static_cast<int>(r.fx + 0.5f), y = static_cast<int>(r.fy + 0.5f);
    if (x != cur.x || y != cur.y) {
        r.beforeSet = cur;
        InputCursorMoveTo(x, y);
        r.lastSet = { x, y };
    }
    else {
        r.lastSet = cur;
    }
}

void MoveCursor(Runtime& r, int x, int y)
{
    RECT c{};
    GetClientRect(r.hwnd, &c);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > c.right - 1) x = c.right - 1;
    if (y > c.bottom - 1) y = c.bottom - 1;
    InputCursorMoveTo(x, y);
    r.lastSet = { x, y };
    r.fx = static_cast<float>(x);
    r.fy = static_cast<float>(y);
}

void Nudge(Runtime& r, int dx, int dy)
{
    RECT c{};
    GetClientRect(r.hwnd, &c);
    POINT cur{};
    if (!InputCursorClient(cur)) return;
    const int step = static_cast<int>(g_cfg.cursorNudge * (c.bottom - c.top) + 0.5f);
    MoveCursor(r, cur.x + dx * (step > 0 ? step : 1), cur.y + dy * (step > 0 ? step : 1));
}

// Nei menu: salta alla casella piu' vicina nella direzione (dx, dy), con un solo asse diverso da zero. Le caselle sono
// quelle della finestra con il fuoco lette nel thread del gioco, in coordinate del buffer, che si portano in coordinate
// della finestra in proporzione. Punteggio = distanza lungo la direzione + 2 volte lo scarto di lato (centri delle
// caselle), come fanno di solito le console. Dentro un elenco su/giu' scorre di circa una riga finche' resta nell'elenco.
// Restituisce false se le caselle non sono note (il chiamante sposta il cursore a piccoli passi come prima).
bool SnapMove(Runtime& r, int dx, int dy)
{
    std::vector<UiControl> ctl;
    int bw = 0, bh = 0;
    if (!OverlayUiControls(ctl, bw, bh) || ctl.empty()) return false;
    RECT c{};
    GetClientRect(r.hwnd, &c);
    POINT cur{};
    if (!InputCursorClient(cur) || c.right <= 0 || c.bottom <= 0) return false;
    const float sx = static_cast<float>(c.right) / bw, sy = static_cast<float>(c.bottom) / bh;
    const float px = cur.x / sx, py = cur.y / sy;
    auto inside = [&](const RECT& q) { return px >= q.left && px < q.right && py >= q.top && py < q.bottom; };
    if (dy) {
        for (const UiControl& u : ctl) {
            if (u.kind != UiList || !inside(u.r)) continue;
            const float ny = py + dy * g_cfg.cursorNudge * 2.0f * bh;
            if (ny >= u.r.top + 2 && ny < u.r.bottom - 2) {
                const int y = static_cast<int>(ny * sy + 0.5f);
                Log("Menu: riga %s nell'elenco %ld,%ld-%ld,%ld, cursore a %ld,%d", dy > 0 ? "sotto" : "sopra", u.r.left, u.r.top,
                    u.r.right, u.r.bottom, cur.x, y);
                MoveCursor(r, cur.x, y);
                return true;
            }
        }
    }
    // fuori da ogni casella e niente in quella direzione: si va alla casella piu' vicina in assoluto
    bool onAny = false;
    for (const UiControl& u : ctl) if (inside(u.r)) { onAny = true; break; }
    int best = -1, nearest = -1;
    float bestScore = 0, nearestDist = 0;
    for (size_t i = 0; i < ctl.size(); ++i) {
        const RECT& q = ctl[i].r;
        const float qx = (q.left + q.right) / 2.0f, qy = (q.top + q.bottom) / 2.0f;
        if (inside(q) || qx < 0 || qy < 0 || qx >= bw || qy >= bh) continue;   // la casella attuale e quelle fuori schermo no
        const float ddx = qx - px, ddy = qy - py;
        const float dist = ddx * ddx + ddy * ddy;
        if (nearest < 0 || dist < nearestDist) { nearest = static_cast<int>(i); nearestDist = dist; }
        const float along = dx ? ddx * dx : ddy * dy, side = dx ? std::fabs(ddy) : std::fabs(ddx);
        if (along < 3) continue;
        const float score = along + 2 * side;
        if (best < 0 || score < bestScore) { best = static_cast<int>(i); bestScore = score; }
    }
    if (best < 0 && !onAny) best = nearest;
    // 0.5.2: niente in quella direzione partendo da una casella: si ricomincia dall'altra parte (dall'ultima alla prima),
    // scegliendo la casella piu' lontana nel verso opposto e meglio allineata
    if (best < 0) {
        for (size_t i = 0; i < ctl.size(); ++i) {
            const RECT& q = ctl[i].r;
            const float qx = (q.left + q.right) / 2.0f, qy = (q.top + q.bottom) / 2.0f;
            if (inside(q) || qx < 0 || qy < 0 || qx >= bw || qy >= bh) continue;
            const float ddx = qx - px, ddy = qy - py;
            const float along = dx ? ddx * dx : ddy * dy, side = dx ? std::fabs(ddy) : std::fabs(ddx);
            const float score = along + 2 * side;
            if (best < 0 || score < bestScore) { best = static_cast<int>(i); bestScore = score; }
        }
        if (best >= 0) Log("Menu: ultima casella in quella direzione, si ricomincia dall'altra parte");
    }
    if (best < 0) { Log("Menu: nessuna casella in quella direzione"); return true; }
    const RECT& q = ctl[best].r;
    const int x = static_cast<int>((q.left + q.right) / 2.0f * sx + 0.5f), y = static_cast<int>((q.top + q.bottom) / 2.0f * sy + 0.5f);
    static const char* kKind[4] = { "pulsante", "tendina", "elenco", "cursore" };
    Log("Menu: %s %ld,%ld-%ld,%ld, cursore a %d,%d", kKind[ctl[best].kind & 3], q.left, q.top, q.right, q.bottom, x, y);
    MoveCursor(r, x, y);
    return true;
}

// Un passo nei menu (croce o leva destra): a scatti sulle caselle, oppure a piccoli passi se non si conoscono.
void MenuStep(Runtime& r, int dx, int dy)
{
    const int sdx = dy ? 0 : dx;   // con due direzioni insieme vale quella verticale
    if ((dx || dy) && SnapMove(r, sdx, dy)) return;
    Log("Menu: caselle non note, cursore spostato a piccoli passi");
    Nudge(r, dx, dy);
}

// Leva destra: camera. Con l'aggancio (0.6.0) velocita' proporzionale all'inclinazione oltre la zona morta, con la
// curva [Camera] Curve, nella direzione vera della leva; altrimenti le frecce del gioco (scorrimento digitale).
void UpdateCamera(Runtime& r, const PadState& p)
{
    if (CameraAnalog()) {
        for (int i = 0; i < 4; ++i)
            if (r.cam[i]) { InputKey(kCamVk[i], false); r.cam[i] = false; }
        const float m = std::sqrt(p.rx * p.rx + p.ry * p.ry), dz = g_cfg.rightDeadzone;
        if (m <= dz) { CameraSet(0, 0); return; }
        float norm = (m - dz) / (1.0f - dz);
        if (norm > 1.0f) norm = 1.0f;
        const float k = std::pow(norm, g_cfg.cameraCurve) / m;
        CameraSet(p.rx * k, p.ry * k);
        return;
    }
    const float on = g_cfg.cameraOn, off = on - 0.15f;
    const float rx = std::fabs(p.rx) > g_cfg.rightDeadzone ? p.rx : 0.0f;
    const float ry = std::fabs(p.ry) > g_cfg.rightDeadzone ? p.ry : 0.0f;
    const float val[4] = { -rx, rx, ry, -ry };   // sinistra, destra, su, giu'
    for (int i = 0; i < 4; ++i) {
        const bool want = r.cam[i] ? val[i] > off : val[i] > on;
        if (want != r.cam[i]) {
            r.cam[i] = want;
            InputKey(kCamVk[i], want);
        }
    }
}

// --- Calamita (0.8.0) -----------------------------------------------------------------------------------------------
// Il cursore e' "sopra qualcosa" se il thread del gioco vede un oggetto in quel punto (mappa), se sta su un pulsante della
// barra (mappa) o su una casella del menu aperto. Coordinate del buffer del gioco, come le liste dell'overlay.
void UpdateMagnet(Runtime& r, bool padInUse)
{
    r.overTarget = false;
    POINT cur{};
    RECT c{};
    int bw = 0, bh = 0;
    if (!padInUse || !InputCursorClient(cur) || !GetClientRect(r.hwnd, &c) || c.right <= 0 || c.bottom <= 0 ||
        !OverlayBufferSize(bw, bh)) {
        TickCursor(-1, -1);
        return;
    }
    const int bx = cur.x * bw / c.right, by = cur.y * bh / c.bottom;
    TickCursor(r.inMap ? bx : -1, r.inMap ? by : -1);
    if (g_cfg.magnet >= 0.99f) return;
    auto inside = [&](const RECT& q) { return bx >= q.left && bx < q.right && by >= q.top && by < q.bottom; };
    if (r.inMap) {
        if (TickHover()) { r.overTarget = true; return; }
        std::vector<RECT> bar;
        OverlayBarRects(bar);
        for (const RECT& q : bar) if (inside(q)) { r.overTarget = true; return; }
    }
    else {
        std::vector<UiControl> ctl;
        int w = 0, h = 0;
        if (OverlayUiControls(ctl, w, h)) for (const UiControl& u : ctl) if (inside(u.r)) { r.overTarget = true; return; }
    }
}

// --- Vibrazione (0.8.0) ----------------------------------------------------------------------------------------------
void Rumble(Runtime& r, int kind, DWORD now)
{
    if (g_cfg.vibration <= 0.0f) return;
    r.rumbleKind = kind;
    r.rumbleStart = now;
}

// Avviso: un colpo breve e leggero. Eroe in pericolo: due colpi forti.
void UpdateRumble(Runtime& r, DWORD now, bool active)
{
    float lo = 0, hi = 0;
    if (!active) r.rumbleKind = 0;
    const DWORD t = now - r.rumbleStart;
    if (r.rumbleKind == 1) {
        if (t < 160) { lo = 0.35f; hi = 0.55f; } else r.rumbleKind = 0;
    }
    else if (r.rumbleKind == 2) {
        if (t < 180 || (t >= 300 && t < 480)) { lo = 0.9f; hi = 0.6f; }
        else if (t >= 480) r.rumbleKind = 0;
    }
    lo *= g_cfg.vibration;
    hi *= g_cfg.vibration;
    if (lo != r.rumbleLo || hi != r.rumbleHi) {
        PadRumble(lo, hi);
        r.rumbleLo = lo;
        r.rumbleHi = hi;
    }
}

void WatchEvents(Runtime& r, const GameState& gs, DWORD now)
{
    // avviso nuovo: il numero degli avvisi in elenco cresce
    if (gs.ok && gs.game) {
        if (r.alertsKnown && gs.alerts > r.lastAlerts) { Rumble(r, 1, now); Log("Avviso nuovo (%u in elenco): vibrazione", gs.alerts); }
        r.lastAlerts = gs.alerts;
        r.alertsKnown = true;
    }
    else {
        r.alertsKnown = false;
    }
    // eroe selezionato che scende sotto un quarto della salute
    int handle = -1;
    const int pct = TickHeroHealth(handle);
    if (handle >= 0 && handle == r.heroHandle && r.heroPct >= 25 && pct >= 0 && pct < 25) {
        Rumble(r, 2, now);
        Log("Eroe selezionato sotto un quarto della salute (%d%%): vibrazione", pct);
    }
    r.heroHandle = handle;
    r.heroPct = pct;
}

// --- Impostazioni nel gioco (0.8.0) -----------------------------------------------------------------------------------
// View tenuto + Menu apre un pannello (lo stesso dell'aiuto) con le opzioni principali; croce su/giu' sceglie,
// sinistra/destra o A cambia, B o Menu chiude. Ogni cambio vale subito e si scrive nell'ini.
// 0.10.0: dimensione di simboli e testi, e la pagina dei tasti del controller (A o destra la apre)
enum SetItem { S_Lang, S_Symbols, S_BarSymbols, S_UiScale, S_CursorSpeed, S_CameraSpeed, S_AnalogCamera, S_Magnet, S_Vibration,
               S_SlowMotion, S_HideLetters, S_Cursor, S_Buttons, S_Count };

const char* kLangKeys[4] = { "Auto", "Italian", "English", "Spanish" };
const float kMagnetVals[4] = { 1.0f, 0.7f, 0.5f, 0.35f };
const float kVibrationVals[3] = { 0.0f, 0.5f, 1.0f };
const float kScaleVals[4] = { 0.8f, 1.0f, 1.25f, 1.5f };

int Nearest(const float* vals, int n, float v)
{
    int best = 0;
    for (int i = 1; i < n; ++i) if (std::fabs(vals[i] - v) < std::fabs(vals[best] - v)) best = i;
    return best;
}

int LangIndex()
{
    for (int i = 0; i < 4; ++i) if (_stricmp(g_cfg.language.c_str(), kLangKeys[i]) == 0) return i;
    return 0;
}

std::wstring SettingValue(int item)
{
    wchar_t b[64];
    switch (item) {
    case S_Lang: {
        const int i = LangIndex();
        if (i == 0) return std::wstring(Tr(T_ValAuto)) + L" (" + Tr(T_ValGame) + L")";
        return i == 1 ? L"Italiano" : i == 2 ? L"English" : L"Espa\u00f1ol";
    }
    case S_Symbols: return Names().style;
    case S_BarSymbols: return g_cfg.barSymbolsAlways ? std::wstring(Tr(T_ValAlways)) : TrF(T_ValWhileHeld, Names().rt);
    case S_UiScale: {
        static const Txt names[4] = { T_ValSmall, T_ValNormal, T_ValLarge, T_ValHuge };
        const int k = Nearest(kScaleVals, 4, g_cfg.uiScale);
        swprintf_s(b, L"%s (%d%%)", Tr(names[k]), static_cast<int>(kScaleVals[k] * 100 + 0.5f));
        return b;
    }
    case S_Buttons: return Tr(T_ValOpen);
    case S_CursorSpeed: swprintf_s(b, L"%.1f", g_cfg.cursorSpeed); return b;
    case S_CameraSpeed: swprintf_s(b, L"%.1f", g_cfg.cameraSpeed); return b;
    case S_AnalogCamera: return Tr(g_cfg.cameraAnalog ? T_ValOn : T_ValOff);
    case S_Magnet: {
        static const Txt names[4] = { T_ValOff, T_ValLight, T_ValMedium, T_ValStrong };
        return Tr(names[Nearest(kMagnetVals, 4, g_cfg.magnet)]);
    }
    case S_Vibration: {
        static const Txt names[3] = { T_ValOff, T_ValLight, T_ValStrong };
        return Tr(names[Nearest(kVibrationVals, 3, g_cfg.vibration)]);
    }
    case S_SlowMotion: return Tr(g_cfg.slowMotion < 0.99f ? T_ValOn : T_ValOff);
    case S_HideLetters: return Tr(g_cfg.hideKeyLetters ? T_ValOn : T_ValOff);
    case S_Cursor:
        return Tr(g_cfg.cursorMode == CursorMode::Real ? T_ValCursorReal : g_cfg.cursorMode == CursorMode::Virtual ? T_ValCursorDrawn : T_ValAuto);
    default: return L"";
    }
}

void WriteFloat(const char* sec, const char* key, float v)
{
    char b[16];
    _snprintf_s(b, sizeof(b), _TRUNCATE, "%.2f", v);
    ConfigWrite(sec, key, b);
}

void SettingChange(Runtime& r, int item, int dir, DWORD now)
{
    switch (item) {
    case S_Lang: {
        const int i = (LangIndex() + (dir > 0 ? 1 : 3)) % 4;
        g_cfg.language = kLangKeys[i];
        ConfigWrite("General", "Language", kLangKeys[i]);
        LangInit();
        break;
    }
    case S_Symbols:
        SaveGlyphStyle(static_cast<GlyphStyle>((static_cast<int>(g_cfg.glyphs) + (dir > 0 ? 1 : 2)) % 3));
        break;
    case S_UiScale:
        g_cfg.uiScale = kScaleVals[(Nearest(kScaleVals, 4, g_cfg.uiScale) + (dir > 0 ? 1 : 3)) % 4];
        WriteFloat("Pad", "UiScale", g_cfg.uiScale);
        break;
    case S_Buttons:
        if (dir > 0) { r.settingsPage = 1; r.buttonsSel = 0; Log("Impostazioni: pagina dei tasti del controller"); }
        break;
    case S_BarSymbols:
        g_cfg.barSymbolsAlways = !g_cfg.barSymbolsAlways;
        ConfigWrite("Pad", "BarSymbols", g_cfg.barSymbolsAlways ? "Always" : "Hold");
        break;
    case S_CursorSpeed:
        g_cfg.cursorSpeed = (std::min)(2.5f, (std::max)(0.3f, std::round((g_cfg.cursorSpeed + 0.1f * dir) * 10) / 10));
        WriteFloat("Cursor", "Speed", g_cfg.cursorSpeed);
        break;
    case S_CameraSpeed:
        g_cfg.cameraSpeed = (std::min)(3.0f, (std::max)(0.3f, std::round((g_cfg.cameraSpeed + 0.1f * dir) * 10) / 10));
        WriteFloat("Camera", "Speed", g_cfg.cameraSpeed);
        break;
    case S_AnalogCamera:
        g_cfg.cameraAnalog = !g_cfg.cameraAnalog;
        ConfigWrite("Camera", "Analog", g_cfg.cameraAnalog ? "1" : "0");
        ReleaseCamera(r);
        break;
    case S_Magnet:
        g_cfg.magnet = kMagnetVals[(Nearest(kMagnetVals, 4, g_cfg.magnet) + (dir > 0 ? 1 : 3)) % 4];
        WriteFloat("Cursor", "Magnet", g_cfg.magnet);
        break;
    case S_Vibration:
        g_cfg.vibration = kVibrationVals[(Nearest(kVibrationVals, 3, g_cfg.vibration) + (dir > 0 ? 1 : 2)) % 3];
        WriteFloat("Pad", "Vibration", g_cfg.vibration);
        Rumble(r, 1, now);   // prova della forza scelta
        break;
    case S_SlowMotion:
        g_cfg.slowMotion = g_cfg.slowMotion < 0.99f ? 1.0f : 0.35f;
        WriteFloat("Radial", "SlowMotion", g_cfg.slowMotion);
        break;
    case S_HideLetters:
        g_cfg.hideKeyLetters = !g_cfg.hideKeyLetters;
        ConfigWrite("Pad", "HideKeyLetters", g_cfg.hideKeyLetters ? "1" : "0");
        break;
    case S_Cursor: {
        const int m = (static_cast<int>(g_cfg.cursorMode) + (dir > 0 ? 1 : 2)) % 3;
        g_cfg.cursorMode = static_cast<CursorMode>(m);
        static const char* kModes[3] = { "Auto", "Real", "Virtual" };
        ConfigWrite("Cursor", "Mode", kModes[m]);
        if (g_cfg.cursorMode == CursorMode::Virtual) InputSetVirtualCursor(true);
        if (g_cfg.cursorMode == CursorMode::Real) InputSetVirtualCursor(false);
        break;
    }
    default: break;
    }
}

void ShowSettings(const Runtime& r)
{
    static const Txt labels[S_Count] = { T_SetLanguage, T_SetSymbols, T_SetBarSymbols, T_SetUiScale, T_SetCursorSpeed,
                                         T_SetCameraSpeed, T_SetAnalogCamera, T_SetMagnet, T_SetVibration, T_SetSlowMotion,
                                         T_SetHideLetters, T_SetCursorMode, T_SetButtons };
    const PadNames n = Names();
    std::vector<std::wstring> rows;
    if (r.settingsPage == 1) {
        // pagina dei tasti (0.10.0): tasto fisico a sinistra, quello che fa a destra
        for (int i = 0; i < ButtonCount(); ++i) {
            const bool sel = i == r.buttonsSel;
            const int to = ButtonTarget(i);
            std::wstring line = sel ? L"\u25b6 " : L"    ";
            line += ButtonLabel(n, i);
            line += L"\t\t";
            std::wstring value = ButtonLabel(n, to);
            if (to == i) value += std::wstring(L"  ") + Tr(T_ButtonsSame);
            line += sel ? L"\u25c0  " + value + L"  \u25b6" : value;
            rows.push_back(line);
        }
        OverlayHelp(true, Tr(T_ButtonsTitle), &rows, TrF(T_ButtonsFoot, n.x, n.b).c_str());
        return;
    }
    for (int i = 0; i < S_Count; ++i) {
        const bool sel = i == r.settingsSel;
        std::wstring line = sel ? L"\u25b6 " : L"    ";
        line += Tr(labels[i]);
        line += L"\t\t";
        line += sel ? L"\u25c0  " + SettingValue(i) + L"  \u25b6" : SettingValue(i);
        rows.push_back(line);
    }
    OverlayHelp(true, Tr(T_SetTitle), &rows, TrF(T_SetFoot, n.a, n.b, n.menu).c_str());
}

// Restituisce true se il pannello delle impostazioni ha preso i tasti di questo giro.
bool HandleSettings(Runtime& r, WORD pressed, WORD released, DWORD now)
{
    if (!r.settingsOpen) return false;
    // i tasti premuti qui non devono fare nulla nemmeno quando si lasciano, anche a pannello chiuso
    r.consumed &= ~released;
    if (r.settingsPage == 1) {
        // pagina dei tasti: croce su/giu' sceglie, sinistra/destra cambia, X rimette tutto, B torna indietro
        const int nb = ButtonCount();
        if (pressed & PAD_DPAD_UP) r.buttonsSel = (r.buttonsSel + nb - 1) % nb;
        if (pressed & PAD_DPAD_DOWN) r.buttonsSel = (r.buttonsSel + 1) % nb;
        if (pressed & (PAD_DPAD_LEFT | PAD_DPAD_RIGHT | PAD_A)) {
            const int dir = (pressed & PAD_DPAD_LEFT) ? nb - 1 : 1;
            SetButtonTarget(r.buttonsSel, (ButtonTarget(r.buttonsSel) + dir) % nb);
        }
        if (pressed & PAD_X) { for (int i = 0; i < nb; ++i) if (ButtonTarget(i) != i) SetButtonTarget(i, i); Log("Tasti: tutti come all'inizio"); }
        if (pressed & (PAD_B | PAD_START)) r.settingsPage = 0;
        ShowSettings(r);
        r.consumed |= pressed;
        return true;
    }
    if (pressed & PAD_DPAD_UP) r.settingsSel = (r.settingsSel + S_Count - 1) % S_Count;
    if (pressed & PAD_DPAD_DOWN) r.settingsSel = (r.settingsSel + 1) % S_Count;
    if (pressed & (PAD_DPAD_RIGHT | PAD_A)) { SettingChange(r, r.settingsSel, 1, now); Log("Impostazioni: voce %d cambiata", r.settingsSel); }
    if (pressed & PAD_DPAD_LEFT) { SettingChange(r, r.settingsSel, -1, now); Log("Impostazioni: voce %d cambiata", r.settingsSel); }
    if (pressed & (PAD_B | PAD_START)) {
        r.settingsOpen = false;
        OverlayHelp(false, L"", nullptr, L"");
        Log("Impostazioni chiuse");
    }
    else {
        ShowSettings(r);
    }
    r.consumed |= pressed;
    return true;
}

void LogAction(const char* what)
{
    POINT p{};
    InputCursorClient(p);
    Log("%s (cursore %ld,%ld)", what, p.x, p.y);
}

void HandleButtons(Runtime& r, const PadState& p, const GameState& gs, DWORD now)
{
    const WORD cur = p.buttons, prev = r.prev;
    WORD pressed = cur & ~prev, released = prev & ~cur;
    for (int i = 0; i < 16; ++i) if (pressed & (1u << i)) r.pressedAt[i] = now;
    auto heldFor = [&](WORD mask) { return now - r.pressedAt[BitIndex(mask)]; };
    const bool ltNow = p.lt > 0.5f, ltPressed = ltNow && !r.ltHeld;   // LT appena premuto (0.8.4)
    r.ltHeld = ltNow;

    // impostazioni aperte: prendono tutti i tasti
    if (HandleSettings(r, pressed, released, now)) { r.prev = cur; return; }
    // aiuto a schermo (View tenuto) + Menu: apre le impostazioni
    if (r.helpShown && (pressed & PAD_START)) {
        r.settingsOpen = true;
        r.settingsPage = 0;
        r.settingsSel = 0;
        r.helpShown = false;
        r.consumed |= PAD_BACK | PAD_START;
        CloseRadial(r, "impostazioni aperte");
        ShowSettings(r);
        Log("Impostazioni aperte (View + Menu)");
        r.prev = cur;
        return;
    }

    // Radiale dei trucchi (0.9.0 con View + RB; 0.9.2 Y tenuto, un tasto solo come per gli altri radiali): voci secondo la
    // selezione; lasciando Y si usa la voce scelta. Restituisce false se i trucchi non si possono usare (riquadro a schermo).
    auto openCheats = [&]() -> bool {
        std::wstring title = L"ImperiumPad ";
        title += IMPERIUMPAD_VERSION_NUM;
        const int allowed = GameResurrectedLoaded() ? CheatsAllowed() : -2;
        int speed = 0;
        bool online = true;
        const bool single = GameSpeed(speed, online) && !online;   // 0.10.1: trucchi classici solo in partita singola
        {
            r.cheats.clear();
            const bool building = gs.selCount && gs.selKind >= 4, units = gs.selCount && !building;
            if (allowed != -2 && allowed != 0) {
                if (building) r.cheats.assign(std::begin(kCheatsBuilding), std::end(kCheatsBuilding));
                else if (units) r.cheats.assign(std::begin(kCheatsUnits), std::end(kCheatsUnits));
                else r.cheats.assign(std::begin(kCheatsNothing), std::end(kCheatsNothing));
            }
            if (single && units) r.cheats.insert(r.cheats.end(), std::begin(kCheatsUnitsLocal), std::end(kCheatsUnitsLocal));
            if (single && !gs.selCount) r.cheats.insert(r.cheats.end(), std::begin(kCheatsNothingLocal), std::end(kCheatsNothingLocal));
            if (r.cheats.empty()) {
                if (allowed == -2) { OverlayBadge(title.c_str(), Tr(T_BadgeCheatsNoRes), 5000); Log("Trucchi: Resurrected 1.0.3 non riconosciuto"); }
                else { OverlayBadge(title.c_str(), Tr(T_BadgeCheatsOff), 6000); Log("Trucchi: spenti nella stanza (log di Resurrected)%s", single ? "" : ", partita in rete"); }
                return false;
            }
            std::vector<CheatEntry> entries;
            for (const Cheat& c : r.cheats) {
                // esperienza: sotto la stella si scrive a chi va (eroi o soldati), nella lingua scelta
                const std::wstring label = c.label ? c.label : Tr(strcmp(c.verb, "exph") == 0 ? T_ChShortHeroes : T_ChShortTroops);
                entries.push_back({ label, CheatTitle(c), c.color, c.icon });
            }
            OverlayCheatItems(entries);
            r.radial = 3;
            r.radialHi = -1;
            Log("Y tenuto: radiale dei trucchi aperto (%u voci, selezione tipo %u numero %u%s%s)", static_cast<unsigned>(r.cheats.size()),
                gs.selKind, gs.selCount, allowed == -1 ? "; permesso della stanza non letto dal log di Resurrected" : "",
                single ? "; partita singola: anche i trucchi classici" : "");
        }
        return true;
    };

    // aiuto a schermo (View tenuto): freccia sinistra/destra cambia lo stile dei simboli e lo salva
    const WORD lr = pressed & (PAD_DPAD_LEFT | PAD_DPAD_RIGHT);
    if (r.helpShown && lr) {
        const int style = (static_cast<int>(g_cfg.glyphs) + ((lr & PAD_DPAD_RIGHT) ? 1 : 2)) % 3;
        SaveGlyphStyle(static_cast<GlyphStyle>(style));
        ShowHelp(true);
        r.consumed |= lr;
        pressed &= ~lr;
    }
    // 0.10.0: View tenuto + croce su/giu' in mappa = velocita' della partita (solo in singolo, nel thread del gioco)
    const WORD ud = pressed & (PAD_DPAD_UP | PAD_DPAD_DOWN);
    if (r.helpShown && r.inMap && ud) {
        TickSpeedStep((ud & PAD_DPAD_UP) ? 1 : -1);
        r.speedAskedAt = now;
        r.consumed |= ud;
        pressed &= ~ud;
        Log("View + croce: velocita' della partita %s", (ud & PAD_DPAD_UP) ? "su" : "giu'");
    }

    // RT tenuto in mappa: i tasti col simbolo sulla barra eseguono il comando di quel pulsante, senza la loro azione
    const bool rtNow = p.rt > 0.5f;
    if (rtNow != r.rtHeld) {
        r.rtHeld = rtNow;
        Log("RT %s", rtNow ? (r.inMap ? "tenuto: comandi della barra (RT + tasto col simbolo)" : "tenuto (nei menu nessuna azione)")
                           : "rilasciato");
    }
    if (rtNow && r.inMap) {
        const int page = p.lt > 0.5f ? 1 : 0;   // RT + LT: pulsanti dal tredicesimo (0.8.0)
        for (int i = 0; i < kSlotCount; ++i) {
            if (!(pressed & kSlotMask[i])) continue;
            OverlayBarCommand(i + page * kSlotCount);
            Log("RT%s + tasto: comando del posto %d della barra", page ? " + LT" : "", i + 1 + page * kSlotCount);
            r.consumed |= kSlotMask[i];
            pressed &= ~kSlotMask[i];
        }
    }
    const WORD skip = released & r.consumed;
    released &= ~skip;
    r.consumed &= ~skip;

    // Radiali (0.6.0): RB tenuto = comandi della barra, LB tenuto = gruppi. Aperto: la leva sinistra sceglie la voce (il
    // cursore resta fermo), la croce sinistra/destra passa alla voce vicina, B chiude senza fare nulla; lasciando il
    // dorsale si esegue la voce scelta. Gli altri tasti intanto non fanno nulla.
    // 0.8.4 (prova sulla Steam Machine: assegnare non riusciva): nei gruppi LT non va piu' tenuto fino al rilascio, che
    // lasciando insieme LB e LT finiva in "seleziona"; ogni pressione di LT passa da seleziona ad assegna e ritorno, e il
    // radiale si apre gia' in "assegna" se LT e' tenuto. Assegnare senza nulla selezionato e scegliere un gruppo vuoto non
    // fanno nulla (prima il gruppo vuoto toglieva la selezione).
    bool justOpened = false;
    if (r.inMap && !r.radial && !rtNow) {
        const DWORD tap = static_cast<DWORD>(g_cfg.tapMs);
        if ((cur & PAD_RB) && !(r.consumed & PAD_RB) && heldFor(PAD_RB) >= tap) {
            r.radial = 1; r.radialHi = -1; Log("RB tenuto: radiale dei comandi aperto");
        }
        else if ((cur & PAD_LB) && !(r.consumed & PAD_LB) && heldFor(PAD_LB) >= tap) {
            r.radial = 2; r.radialHi = -1; r.radialAssign = ltNow; justOpened = true;
            Log("LB tenuto: radiale dei gruppi aperto (%s)", ltNow ? "assegna, LT tenuto" : "seleziona");
        }
        else if ((cur & PAD_Y) && !(r.consumed & PAD_Y) && heldFor(PAD_Y) >= tap) {
            // trucchi non disponibili: Y si segna come usato, cosi' il riquadro non si ripete e il rilascio non fa nulla
            if (!openCheats()) r.consumed |= PAD_Y;
        }
    }
    if (r.radial == 2 && ltPressed && !justOpened) {
        r.radialAssign = !r.radialAssign;
        Log("Radiale dei gruppi: LT, ora %s", r.radialAssign ? "assegna" : "seleziona");
    }
    if (r.radial && !r.inMap) CloseRadial(r, "non si e' piu' in mappa");
    if (r.radial) {
        // radiale 4 (crea truppe, 0.10.0) resta aperto senza tasti tenuti: niente rilascio che esegue
        const WORD btn = r.radial == 4 ? 0 : r.radial == 2 ? PAD_LB : r.radial == 3 ? PAD_Y : PAD_RB;
        const int n = OverlayRadialCount();
        const float m = std::sqrt(p.lx * p.lx + p.ly * p.ly);
        if (n > 0 && m > 0.6f) {
            const float step = 6.2831853f / n;
            float a = std::atan2(p.lx, p.ly);   // 0 in alto, positivo in senso orario
            if (a < 0) a += 6.2831853f;
            const int hi = static_cast<int>(std::floor(a / step + 0.5f)) % n;
            if (hi != r.radialHi) { r.radialHi = hi; Log("Radiale: voce %d di %d", hi + 1, n); }
        }
        const WORD lr2 = pressed & (PAD_DPAD_LEFT | PAD_DPAD_RIGHT);
        if (n > 0 && lr2) {
            r.radialHi = r.radialHi < 0 ? 0 : (r.radialHi + ((lr2 & PAD_DPAD_RIGHT) ? 1 : n - 1)) % n;
            Log("Radiale: voce %d di %d (croce)", r.radialHi + 1, n);
        }
        if (r.radial == 4) {
            // croce su/giu': quantita'; A: crea (il radiale resta aperto per crearne altre)
            const int nc = static_cast<int>(sizeof(kSpawnCounts) / sizeof(kSpawnCounts[0]));
            if (pressed & PAD_DPAD_UP) r.spawnCount = (std::min)(nc - 1, r.spawnCount + 1);
            if (pressed & PAD_DPAD_DOWN) r.spawnCount = (std::max)(0, r.spawnCount - 1);
            if ((pressed & PAD_A) && r.radialHi >= 0 && r.radialHi < n) {
                OverlaySpawn(r.radialHi, kSpawnCounts[r.spawnCount]);
                Log("Radiale crea truppe: voce %d x%d", r.radialHi + 1, kSpawnCounts[r.spawnCount]);
            }
        }
        if (pressed & PAD_B) {
            r.consumed |= btn;
            CloseRadial(r, "B: annullato");
        }
        else if (released & btn) {
            const int hi = r.radialHi < n ? r.radialHi : -1;
            if (hi < 0) Log("Radiale: lasciato senza scegliere, nessuna azione");
            else if (r.radial == 1) {
                OverlayBarCommand(hi);
                Log("Radiale dei comandi: comando del posto %d della barra", hi + 1);
            }
            else if (r.radial == 3 && hi < static_cast<int>(r.cheats.size()) && strcmp(r.cheats[hi].verb, "spawnmenu") == 0) {
                // 0.10.0: "crea truppe" apre un secondo radiale che resta aperto senza tenere tasti (A crea, B chiude)
                released &= ~btn;
                r.radial = 4;
                r.radialHi = -1;
                Log("Radiale crea truppe aperto (quantita' %d)", kSpawnCounts[r.spawnCount]);
                r.prev = cur;
                return;
            }
            else if (r.radial == 3) {
                if (hi < static_cast<int>(r.cheats.size()) && strcmp(r.cheats[hi].verb, "@speed") == 0) {
                    // 0.10.1: nel thread del gioco come i passi di View + croce, dopo la fine del tempo rallentato; il riquadro
                    // con la velocita' arriva con il risultato
                    TickSpeedStep(10);
                    r.speedAskedAt = now;
                    Log("Radiale dei trucchi: velocita' 10x (o di nuovo normale)");
                }
                else if (hi < static_cast<int>(r.cheats.size())) {
                    const Cheat& c = r.cheats[hi];
                    OverlayCheat(c.verb, c.value);
                    std::wstring title = L"ImperiumPad ";
                    title += IMPERIUMPAD_VERSION_NUM;
                    OverlayBadge(title.c_str(), TrF(T_BadgeCheatSent, CheatTitle(c).c_str()).c_str(), 3000);
                    Log("Radiale dei trucchi: \"%s\" valore %d", c.verb, c.value);
                }
            }
            else {
                const int g = (hi + 1) % 10;
                int counts[10];
                const bool known = OverlayGroupCounts(counts);
                if (r.radialAssign && !gs.selCount) {
                    std::wstring title = L"ImperiumPad ";
                    title += IMPERIUMPAD_VERSION_NUM;
                    OverlayBadge(title.c_str(), Tr(T_BadgeNothingToAssign), 4000);
                    Log("Radiale dei gruppi: niente da assegnare al gruppo %d (nessuna selezione)", g);
                }
                else if (r.radialAssign) {
                    InputCtrlPulse(300);
                    InputKeyTap('0' + g);
                    Log("Radiale dei gruppi: Ctrl+%d (assegna la selezione, %u elementi, al gruppo %d)", g, gs.selCount, g);
                }
                else if (known && !counts[g]) {
                    Log("Radiale dei gruppi: gruppo %d vuoto, selezione lasciata com'era", g);
                }
                else {
                    InputKeyTap('0' + g);
                    Log("Radiale dei gruppi: %d (seleziona il gruppo; due volte di seguito centra la camera)", g);
                }
            }
            released &= ~btn;
            CloseRadial(r, "dorsale lasciato");
        }
        // mentre e' aperto, gli altri tasti non fanno nulla (i loro rilasci nemmeno)
        const WORD block = pressed & ~(PAD_BACK);
        r.consumed |= block;
        pressed &= ~block;
    }

    // A: clic sinistro, con doppio clic
    if (pressed & PAD_A) {
        POINT pos{};
        InputCursorClient(pos);
        const bool dbl = r.lastADown && now - r.lastADown <= GetDoubleClickTime() &&
                         std::abs(pos.x - r.lastADownPos.x) <= GetSystemMetrics(SM_CXDOUBLECLK) &&
                         std::abs(pos.y - r.lastADownPos.y) <= GetSystemMetrics(SM_CYDOUBLECLK);
        InputMouse(0, true, dbl);
        r.leftHeld = true;
        r.lastADown = dbl ? 0 : now;
        r.lastADownPos = pos;
        LogAction(dbl ? "A: doppio clic sinistro" : "A: clic sinistro giu'");
    }
    if (released & PAD_A) {
        InputMouse(0, false, false);
        r.leftHeld = false;
        LogAction("A: clic sinistro su");
    }

    // X: clic destro
    if (pressed & PAD_X) {
        InputMouse(1, true, false);
        r.rightHeld = true;
        LogAction("X: clic destro giu'");
    }
    if (released & PAD_X) {
        InputMouse(1, false, false);
        r.rightHeld = false;
        LogAction("X: clic destro su");
    }

    // B: Esc; in mappa solo se c'e' un comando in attesa o una selezione, con lo stesso criterio che il gioco usa per
    // Esc (0x5e8f1d: prima il comando in attesa a +0x11c, poi il numero della selezione [+0xf0+0x1c]); senza nessuno dei
    // due il gioco aprirebbe il menu, e il pad non lo fa per sbaglio
    if (pressed & PAD_B) {
        if (!r.inMap) { InputKeyTap(VK_ESCAPE); Log("B: Esc (menu)"); }
        else if (gs.pendingCmd || gs.selCount) {
            InputKeyTap(VK_ESCAPE);
            Log("B: Esc (comando in attesa %08x, selezione tipo %u numero %u)", gs.pendingCmd, gs.selKind, gs.selCount);
        }
        else Log("B: niente da annullare, Esc non inviato");
    }

    // LT: Maiusc virtuale tenuto (con RT tenuto LT cambia pagina dei comandi, niente Maiusc; 0.8.4: nemmeno con un radiale
    // aperto, dove LT sceglie tra seleziona e assegna)
    const bool wantShift = ltNow && !rtNow && !r.radial;
    if (wantShift != r.shift) {
        r.shift = wantShift;
        InputSetShift(wantShift);
        Log("LT: Maiusc virtuale %s", wantShift ? "premuto" : "rilasciato");
    }

    if (r.inMap) {
        // 0.10.0: LT tenuto + croce = tasti di selezione del gioco sulla selezione attuale (PagSu / PagGiu' meta' con piu'
        // o meno salute, Insert / Canc meta' con piu' o meno esperienza), senza il Maiusc virtuale di LT
        const WORD dp = pressed & (PAD_DPAD_UP | PAD_DPAD_DOWN | PAD_DPAD_LEFT | PAD_DPAD_RIGHT);
        if (dp && ltNow && !rtNow) {
            static const struct { WORD mask; UINT vk; Txt badge; const char* name; } kHalf[4] = {
                { PAD_DPAD_UP, VK_PRIOR, T_BadgeSelHealthUp, "PagSu" }, { PAD_DPAD_DOWN, VK_NEXT, T_BadgeSelHealthDown, "PagGiu'" },
                { PAD_DPAD_RIGHT, VK_INSERT, T_BadgeSelExpUp, "Insert" }, { PAD_DPAD_LEFT, VK_DELETE, T_BadgeSelExpDown, "Canc" } };
            for (const auto& h : kHalf) {
                if (!(dp & h.mask)) continue;
                InputShiftSuppress(250);
                InputKeyTap(h.vk);
                std::wstring title = L"ImperiumPad ";
                title += IMPERIUMPAD_VERSION_NUM;
                OverlayBadge(title.c_str(), Tr(h.badge), 1800);
                Log("LT + croce: %s (meta' della selezione)", h.name);
            }
            r.consumed |= dp;
            pressed &= ~dp;
        }
        if (pressed & PAD_DPAD_UP)   { InputKeyTap(VK_F7);   Log("Croce su: F7 (esercito dell'eroe)"); }
        if (pressed & PAD_DPAD_DOWN) { InputKeyTap('Q');     Log("Croce giu': Q (ultimo avviso)"); }
        if (pressed & PAD_DPAD_LEFT) { InputKeyTap(VK_OEM_3); Log("Croce sinistra: accento grave (barre della salute)"); }
        if (pressed & PAD_DPAD_RIGHT) { InputKey(VK_OEM_5, true); r.seeThroughHeld = true; Log("Croce destra: vedere attraverso (tenuto)"); }
        if (pressed & PAD_LS) { InputKeyTap(VK_SPACE); Log("L3: Spazio (mappa grande)"); }
        // 0.10.0: R3 tocco = Home (centra sulla selezione, al rilascio); R3 tenuto = camera sul prossimo insediamento proprio
        const DWORD tapR3 = static_cast<DWORD>(g_cfg.tapMs);
        if ((cur & PAD_RS) && !(r.consumed & PAD_RS) && heldFor(PAD_RS) >= tapR3) {
            OverlayNextSettlement();
            r.consumed |= PAD_RS;
            Log("R3 tenuto: prossimo insediamento");
        }
        if ((released & PAD_RS) && heldFor(PAD_RS) < tapR3) { InputKeyTap(VK_HOME);  Log("R3: Home (centra sulla selezione)"); }
        if (pressed & PAD_START) { InputKeyTap(VK_F10); Log("Menu: F10 (menu di gioco)"); }
        if (released & PAD_RB) {
            if (heldFor(PAD_RB) < static_cast<DWORD>(g_cfg.tapMs)) { InputKeyTap(VK_OEM_COMMA); Log("RB tocco: virgola (eroe successivo)"); }
            else Log("RB tenuto: nessun radiale (RT tenuto)");
        }
        if (released & PAD_LB) {
            if (heldFor(PAD_LB) < static_cast<DWORD>(g_cfg.tapMs)) {
                InputShiftPulse(250);
                InputKeyTap(VK_OEM_COMMA);
                Log("LB tocco: Maiusc+virgola (eroe precedente)");
            }
            else Log("LB tenuto: nessun radiale (RT tenuto)");
        }
    }
    else {
        // nei menu: la croce salta tra le caselle, con ripetizione se tenuta (0.5.0; prima piccoli passi del cursore)
        const WORD dpad = cur & ~r.consumed & (PAD_DPAD_UP | PAD_DPAD_DOWN | PAD_DPAD_LEFT | PAD_DPAD_RIGHT);
        if (dpad) {
            const bool first = (pressed & dpad) != 0;
            if (first) r.dpadNext = now + 400;
            if (first || static_cast<int>(now - r.dpadNext) >= 0) {
                if (!first) r.dpadNext = now + 150;
                MenuStep(r, (dpad & PAD_DPAD_RIGHT ? 1 : 0) - (dpad & PAD_DPAD_LEFT ? 1 : 0),
                         (dpad & PAD_DPAD_DOWN ? 1 : 0) - (dpad & PAD_DPAD_UP ? 1 : 0));
            }
        }
        // anche la leva destra (le frecce del simulatore), oltre la soglia della camera, sull'asse piu' inclinato
        int dir = 0;
        const float on = g_cfg.cameraOn;
        if (std::fabs(p.rx) > on && std::fabs(p.rx) >= std::fabs(p.ry)) dir = p.rx > 0 ? 1 : 2;
        else if (std::fabs(p.ry) > on) dir = p.ry > 0 ? 4 : 3;
        if (dir && (dir != r.stickDir || static_cast<int>(now - r.stickNext) >= 0)) {
            r.stickNext = now + (dir != r.stickDir ? 400 : 150);
            MenuStep(r, dir == 1 ? 1 : dir == 2 ? -1 : 0, dir == 3 ? 1 : dir == 4 ? -1 : 0);
        }
        r.stickDir = dir;
        // Menu durante una partita: chiude la finestra aperta (menu di gioco, diplomazia...) come Esc, come su console
        if (pressed & PAD_START) {
            if (gs.game) { InputKeyTap(VK_ESCAPE); Log("Menu: Esc (chiude la finestra, partita in corso)"); }
            else Log("Menu fuori partita: nessuna azione");
        }
        if (pressed & (PAD_LS | PAD_RS | PAD_LB | PAD_RB))
            Log("Pulsante %04x nei menu: nessuna azione", pressed & (PAD_LS | PAD_RS | PAD_LB | PAD_RB));
        // Y nei menu (0.8.0): tastiera di Steam per il campo di testo sotto il cursore (prima ci si clicca con A)
        if (pressed & PAD_Y) {
            POINT pos{};
            InputCursorClient(pos);
            RECT c{};
            GetClientRect(r.hwnd, &c);
            if (!SteamKeyboard(0, pos.y - c.bottom / 30, c.right, c.bottom / 15)) {
                std::wstring title = L"ImperiumPad ";
                title += IMPERIUMPAD_VERSION_NUM;
                OverlayBadge(title.c_str(), Tr(T_BadgeNoKeyboard), 5000);
            }
        }
    }
    if ((released & PAD_DPAD_RIGHT) && r.seeThroughHeld) { InputKey(VK_OEM_5, false); r.seeThroughHeld = false; }

    // View: tocco = pannello informazioni (solo in mappa); tenuto = aiuto del pad disegnato sullo schermo (ovunque)
    if ((cur & PAD_BACK) && !r.helpShown && !(r.consumed & PAD_BACK) && heldFor(PAD_BACK) >= static_cast<DWORD>(g_cfg.tapMs)) {
        ShowHelp(true);
        r.helpShown = true;
        Log("View tenuto: aiuto del pad a schermo");
    }
    if (released & PAD_BACK) {
        if (r.helpShown) { ShowHelp(false); r.helpShown = false; Log("View rilasciato: aiuto nascosto"); }
        else if (r.inMap) { InputKeyTap('I'); Log("View tocco: I (pannello informazioni)"); }
    }

    // 0.9.2: Y tenuto apre i trucchi, quindi il suggerimento della barra va al tocco breve (al rilascio)
    if ((released & PAD_Y) && r.inMap && heldFor(PAD_Y) < static_cast<DWORD>(g_cfg.tapMs)) {
        Log("Y tocco: suggerimento a schermo (con RT tenuto: comando della barra; tenuto: trucchi)");
        ShowBarHint(5000);
    }

    r.prev = cur;
}

// Ciclo del pad su una finestra del gioco, finche' esiste.
void RunOnWindow(HWND hwnd)
{
    Runtime r;
    r.hwnd = hwnd;
    GameState prevState;
    RECT lastClient{};
    GetClientRect(hwnd, &lastClient);
    DWORD last = GetTickCount(), nextDiag = 0;
    bool wasActive = false, wasConnected = false;
    bool simWas = PadSimulatorActive();
    bool statusShown = false, barHintShown = false;
    if (g_cfg.cursorMode == CursorMode::Virtual) InputSetVirtualCursor(true);

    while (IsWindow(hwnd)) {
        Sleep(static_cast<DWORD>(g_cfg.tickMs));
        const DWORD now = GetTickCount();
        float dt = (now - last) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        last = now;

        PadState pad;
        PadRead(pad);
        // [Buttons] (0.8.0); 0.10.0: con le impostazioni aperte i tasti restano quelli veri, cosi' una riassegnazione sbagliata
        // non impedisce di muoversi nel pannello o di chiuderlo
        if (!r.settingsOpen) pad.buttons = RemapButtons(pad.buttons);
        // risultato del passo di velocita' chiesto con View + croce (0.10.0)
        if (r.speedAskedAt) {
            const int sp = TickSpeedResult();
            if (sp || now - r.speedAskedAt > 600) {
                r.speedAskedAt = 0;
                std::wstring title = L"ImperiumPad ";
                title += IMPERIUMPAD_VERSION_NUM;
                if (sp > 0) OverlayBadge(title.c_str(), TrF(T_BadgeSpeed, (sp + 5) / 10).c_str(), 1800);
                else if (sp < 0) OverlayBadge(title.c_str(), Tr(T_BadgeSpeedOnline), 2500);
            }
        }
        // simulatore da tastiera acceso o spento (Bloc Maiusc con le lettere): un suono di Windows a ogni cambio
        if (g_cfg.padSource == PadSource::Keyboard || g_cfg.padSource == PadSource::Laptop) {
            const bool sim = PadSimulatorActive();
            if (sim != simWas) {
                MessageBeep(sim ? MB_OK : MB_ICONEXCLAMATION);
                Log("Simulatore da tastiera %s", sim ? "ACCESO (suono normale)" : "SPENTO (suono di avviso)");
                simWas = sim;
                ShowStatus(true, 3000);
            }
        }
        // riquadro di stato: 8 s all'inizio, 3 s quando il controller si collega o si scollega
        if (!statusShown) {
            ShowStatus(pad.connected, 8000);
            statusShown = true;
        }
        else if (pad.connected != wasConnected) {
            ShowStatus(pad.connected, 3000);
        }
        if (pad.connected != wasConnected) {
            Log("Pad %s (%s)", pad.connected ? "collegato" : "scollegato", PadSourceName());
            wasConnected = pad.connected;
        }

        OverlayTick(hwnd);

        GameState gs;
        GameRead(gs);
        const bool inMap = gs.InMap();
        if (static_cast<int>(now - nextDiag) >= 0) {
            nextDiag = now + static_cast<DWORD>(g_cfg.diagIntervalMs);
            LogState(gs, prevState, false);
            prevState = gs;
            RECT c{};
            GetClientRect(hwnd, &c);
            if (c.right != lastClient.right || c.bottom != lastClient.bottom) { lastClient = c; LogWindow(hwnd, "cambiata"); }
        }

        const bool active = pad.connected && GetForegroundWindow() == hwnd;
        // 0.5.3: col simulatore spento (Bloc Num) il pad risulta collegato ma non e' in uso: niente simboli e lettere di
        // Resurrected di nuovo visibili
        const bool simSource = g_cfg.padSource == PadSource::Keyboard || g_cfg.padSource == PadSource::Laptop;
        const bool padInUse = active && (!simSource || PadSimulatorActive());
        const bool layer = padInUse && inMap && pad.rt > 0.5f && !r.settingsOpen;
        OverlayPad(padInUse, inMap, g_cfg.glyphs, layer, layer && pad.lt > 0.5f ? 1 : 0);
        GameResurrectedLetters(g_cfg.hideKeyLetters && padInUse);
        {
            POINT cp{};
            RECT c{};
            const bool drawn = padInUse && InputVirtualCursor() && InputCursorClient(cp) && GetClientRect(hwnd, &c);
            OverlayCursor(drawn, cp.x, cp.y, c.right, c.bottom);
        }
        WatchEvents(r, gs, now);
        UpdateRumble(r, now, padInUse);
        TickSlowMotion(padInUse && r.radial != 0);
        if (!active) {
            if (wasActive) ReleaseAll(r, pad.connected ? "gioco non in primo piano" : "pad scollegato");
            wasActive = false;
            r.prev = 0;
            r.consumed = 0;
            r.rtHeld = false;
            r.stickDir = 0;
            r.moving = false;
            if (r.settingsOpen) { r.settingsOpen = false; OverlayHelp(false, L"", nullptr, L""); }
            TickCursor(-1, -1);
            continue;
        }
        if (!wasActive) { Log("Pad attivo sul gioco"); wasActive = true; }

        if (inMap != r.inMap) {
            ReleaseCamera(r);
            if (r.seeThroughHeld) { InputKey(VK_OEM_5, false); r.seeThroughHeld = false; }
            r.inMap = inMap;
            Log("Modalita': %s", inMap ? "mappa (partita in corso, nessuna finestra aperta)" : "menu e finestre");
        }

        // 0.8.3: la prima volta che in mappa compare la barra (o il menu senza selezione) si dice come si usano i comandi
        if (padInUse && inMap && !barHintShown && !r.rtHeld) {
            std::vector<RECT> bar;
            OverlayBarRects(bar);
            if (!bar.empty()) {
                ShowBarHint(8000);
                barHintShown = true;
                Log("Suggerimento a schermo: comandi della barra con RT tenuto + tasto");
            }
        }

        UpdateMagnet(r, padInUse);
        if (!r.radial && !r.settingsOpen) UpdateCursor(r, pad, dt, now);
        else r.moving = false;   // radiale o impostazioni aperti: la leva sinistra non muove il cursore
        if (r.inMap && !r.settingsOpen) UpdateCamera(r, pad); else ReleaseCamera(r);
        HandleButtons(r, pad, gs, now);
        if (r.radial) {
            const PadNames n = Names();
            std::wstring hint;
            int counts[10];
            bool anyGroup = !OverlayGroupCounts(counts);   // finche' non sono letti si fa come se ci fossero gruppi
            for (int i = 0; i < 10 && !anyGroup; ++i) anyGroup = counts[i] > 0;
            if (r.radial == 1) {
                hint = TrF(T_HintRelease, n.rb) + Tr(T_HintRun) + TrF(T_HintCancel, n.b);
            }
            else if (r.radial == 3) {
                hint = TrF(T_HintRelease, n.y) + Tr(T_HintUse) + TrF(T_HintCancel, n.b);
            }
            else if (r.radial == 4) {
                hint = TrF(T_SpawnHint, n.a, kSpawnCounts[r.spawnCount], n.b);
            }
            else if (r.radialAssign && !gs.selCount) {
                hint = TrF(T_HintNoSelection, n.b);   // 0.8.4: assegna senza nulla selezionato
            }
            else if (!r.radialAssign && !anyGroup) {
                hint = TrF(T_HintNoGroups, n.lb, n.lt, n.lb, n.b);   // 0.8.4: nessun gruppo ancora, come si crea
            }
            else {
                hint = TrF(T_HintRelease, n.lb) + Tr(r.radialAssign ? T_HintAssignRun : T_HintSelect) +
                       TrF(r.radialAssign ? T_HintToSelect : T_HintToAssign, n.lt) + TrF(T_HintCancel, n.b);
            }
            OverlayRadial(r.radial, r.radialHi, r.radial == 2 && r.radialAssign, hint.c_str());
        }
    }
    ReleaseAll(r, "finestra del gioco chiusa");
    GameResurrectedLetters(false);
    PadRumble(0, 0);
    TickSlowMotion(false);
}

} // namespace

DWORD WINAPI CoreThread(LPVOID)
{
    LoadConfig();
    LangInit();
    LogEnvironment();
    GameCheckExecutable();
    InputInstallHooks();
    OverlayInstall();
    CameraInstall();
    PadInit();

    // La finestra del gioco si cerca di nuovo se viene distrutta e ricreata (per esempio da Resurrected quando cambia
    // la modalita' video): il thread finisce solo con il processo.
    for (bool first = true;; first = false) {
        HWND hwnd = nullptr;
        for (int i = 0; !hwnd; ++i) {
            EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&hwnd));
            if (!hwnd) {
                if (i == 1200) Log("Finestra del gioco non ancora trovata dopo 2 minuti: si continua a cercare");
                Sleep(100);
            }
        }
        g_gameThread.store(GetWindowThreadProcessId(hwnd, nullptr));
        g_gameHwnd.store(hwnd);
        LogWindow(hwnd, first ? "trovata" : "ricreata");
        if (first) {
            LogEnvironment();   // a finestra creata Resurrected e' sicuramente caricato
            if (!GameKnownExe()) Log("Exe sconosciuto: il pad funziona solo come nei menu (clic, Esc, croce che sposta il cursore)");
        }
        RunOnWindow(hwnd);
        g_gameHwnd.store(nullptr);
        Log("Finestra del gioco chiusa: si cerca di nuovo");
    }
}
