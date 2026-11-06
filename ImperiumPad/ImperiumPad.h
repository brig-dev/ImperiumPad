// ImperiumPad.h - dichiarazioni comuni a tutti i moduli
// Mod per Imperivm RTC HD "Great Battles of Rome" (gbr.exe, 32 bit), proxy di wininet.dll.
// Linguaggio: C++17. Compilatore: MSVC (Visual Studio Community 2026), Win32 (x86), runtime statico /MT.
#pragma once

#if !defined(_M_IX86)
#error "ImperiumPad va compilato per Win32 (x86): gbr.exe e' a 32 bit."
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#define IMPERIUMPAD_VERSION "0.10.3 (crea truppe: classi con la tecnologia, elenco stabile)"
#define IMPERIUMPAD_VERSION_NUM L"0.10.3"

// --- ImperiumPad.cpp: log e informazioni generali -----------------------------
void        Log(const char* fmt, ...);
const char* GameDir();       // cartella del gioco (dove sta wininet.dll)
HMODULE     SelfModule();    // handle di questa DLL

// --- Config.cpp: ImperiumPad.ini ----------------------------------------------
enum class PadSource { Auto, XInput, Keyboard, Laptop };   // Laptop = simulatore con le lettere, senza tastierino
enum class GlyphStyle { Steam, Xbox, PlayStation };       // simboli disegnati sulla barra e nomi dei tasti nell'aiuto
enum class CursorMode { Auto, Real, Virtual };            // cursore di Windows o disegnato dal pad (0.8.0)

struct Config {
    std::string language     = "Auto";  // lingua dei testi a schermo: Auto (quella del gioco), Italian, English, Spanish
    PadSource padSource      = PadSource::Auto;
    GlyphStyle glyphs        = GlyphStyle::Steam;
    bool      hideKeyLetters = true;    // col pad attivo spegne le lettere della tastiera che Resurrected scrive sulla barra
    bool      barSymbolsAlways = false; // simboli sui pulsanti della barra sempre (true) o solo con RT tenuto (0.8.3)
    float     uiScale        = 1.0f;    // dimensione di simboli, pannelli e radiali (0.10.0): 0,8 / 1 / 1,25 / 1,5
    int       controllerIndex = 0;      // primo pad XInput
    int       tickMs         = 8;       // periodo del ciclo principale (ms)
    float     leftDeadzone   = 0.20f;   // zona morta della leva sinistra (0..1)
    float     rightDeadzone  = 0.25f;   // zona morta della leva destra (0..1)
    float     cursorSpeed    = 1.3f;    // velocita' massima del cursore, altezze della finestra al secondo (0.8.1: era 0,9)
    float     cursorCurve    = 2.0f;    // curva di risposta della leva (1 = lineare)
    int       cursorRampMs   = 300;     // accelerazione: dal 35% al 100% della velocita' in questo tempo (0.8.1: era 400)
    float     cursorNudge    = 0.012f;  // passo della croce nei menu, frazione dell'altezza
    CursorMode cursorMode    = CursorMode::Auto;   // Auto: disegnato solo se SetCursorPos non funziona
    float     magnet         = 0.5f;    // calamita: velocita' del cursore sopra oggetti e pulsanti (1 = spenta)
    float     vibration      = 1.0f;    // forza della vibrazione (0 = spenta)
    float     slowMotion     = 0.35f;   // velocita' della partita con un radiale aperto, solo in singolo (1 = spento)
    WORD      remap[16]      = {};      // tasti riassegnati: bit del tasto fisico -> maschera del tasto logico
    bool      remapOn        = false;
    float     cameraOn       = 0.50f;   // leva destra: soglia oltre la quale la camera scorre (frecce; nei menu: salto)
    bool      cameraAnalog   = true;    // camera analogica (0.6.0); false = frecce del gioco come prima
    float     cameraSpeed    = 1.0f;    // velocita' piena della camera analogica, rispetto a quella delle frecce del gioco
    float     cameraCurve    = 1.6f;    // curva di risposta della leva destra (1 = lineare)
    int       tapMs          = 300;     // pressione piu' breve di cosi' = tocco (dorsali, View)
    bool      keyLog         = false;   // scrive nel log i tasti del simulatore
    bool      acceptInjected = false;   // il simulatore accetta i tasti mandati da programma (solo per le prove automatiche)
    bool      hookKeyState   = true;    // modificatori virtuali (Maiusc del pad) con GetKeyState agganciato
    int       diagIntervalMs = 1000;    // ogni quanto il log controlla finestra, controller e stato del gioco
    bool      memory         = true;    // letture della memoria del gioco (sola lettura)
};
extern Config g_cfg;
void LoadConfig();
const char* GlyphStyleName(GlyphStyle s);
void SaveGlyphStyle(GlyphStyle s);        // scrive [Pad] Glyphs nell'ini (scelta fatta nel gioco)
void ConfigWrite(const char* section, const char* key, const char* value);   // scrive una voce dell'ini (menu nel gioco)
WORD RemapButtons(WORD buttons);          // applica [Buttons] (tasti riassegnati)
int  ButtonCount();                       // tasti riassegnabili (0.10.0, pagina delle impostazioni)
const char* ButtonId(int i);              // nome in [Buttons]: A B X Y LB RB View Menu L3 R3 Up Down Left Right
WORD ButtonMaskAt(int i);
int  ButtonTarget(int i);                 // a quale tasto corrisponde ora il tasto fisico i
void SetButtonTarget(int i, int target);  // riassegna e scrive l'ini (target == i: come all'inizio)

// --- Lang.cpp: testi a schermo in italiano, inglese e spagnolo (0.7.0) ----------
enum class Lang { Italian, English, Spanish };
enum Txt {
    T_StatusLaptopOn,
    T_StatusLaptopOff,
    T_StatusNumpadOn,
    T_StatusNumpadOff,
    T_StatusController,
    T_StatusNoController,
    T_BadgeHelp,
    T_PsCross,
    T_PsCircle,
    T_PsSquare,
    T_PsTriangle,
    T_HelpTitle,
    T_HelpHeldPlusKey,
    T_PadLeftStick,
    T_PadRightStick,
    T_PadUp,
    T_PadDown,
    T_PadLeft,
    T_PadRight,
    T_PadMenus,
    T_KeyArrows,
    T_KeyPlusKey,
    T_KeyNumDot,
    T_KeyEnter,
    T_KeyHome,
    T_KeyEnd,
    T_KeyDel,
    T_KeyPgDn,
    T_KeyPgUp,
    T_KeyIns,
    T_ActCursor,
    T_ActCamera,
    T_ActSelect,
    T_ActAction,
    T_ActCancel,
    T_ActShift,
    T_ActBarCommand,
    T_ActHeroes,
    T_ActHeroArmy,
    T_ActLastAlert,
    T_ActHealthBars,
    T_ActSeeThrough,
    T_ActMenuJump,
    T_ActBigMap,
    T_ActCentre,
    T_ActGameMenu,
    T_ActInfo,
    T_FootLaptop,
    T_FootNumpad,
    T_FootStyle,
    T_HintRelease,
    T_HintRun,
    T_HintSelect,
    T_HintToAssign,
    T_HintToSelect,
    T_HintAssignRun,
    T_HintCancel,
    T_RadNoCommands,
    T_RadTiltCommand,
    T_RadUnnamed,
    T_RadAssignChoose,
    T_RadTiltGroup,
    T_RadAssignTo,
    T_RadGroupOne,
    T_RadGroupMany,
    T_RadGroupEmpty,
    T_RadEmpty,
    T_SetTitle,
    T_SetFoot,
    T_SetLanguage,
    T_SetSymbols,
    T_SetCursorSpeed,
    T_SetCameraSpeed,
    T_SetAnalogCamera,
    T_SetMagnet,
    T_SetVibration,
    T_SetSlowMotion,
    T_SetHideLetters,
    T_SetCursorMode,
    T_ValGame,
    T_ValAuto,
    T_ValOn,
    T_ValOff,
    T_ValLight,
    T_ValMedium,
    T_ValStrong,
    T_ValCursorReal,
    T_ValCursorDrawn,
    T_BadgeVirtualCursor,
    T_BadgeNoKeyboard,
    T_PadYMenus,
    T_ActKeyboard,
    T_PadSettings,
    T_ActSettings,
    T_SetBarSymbols,
    T_ValWhileHeld,
    T_ValAlways,
    T_BadgeBarHint,
    T_EmptyHelp,
    T_EmptyDiplomacy,
    T_EmptyQuickSave,
    T_EmptyParty,
    T_EmptyNotes,
    T_EmptyQuickLoad,
    T_EmptyMainMenu,
    T_EmptyChat,
    T_EmptyMinimap,
    T_RadNoGroups,
    T_HintNoGroups,
    T_HintNoSelection,
    T_ChGold,
    T_ChFood,
    T_ChMaxOnce,
    T_ChMaxPop,
    T_ChPopulation,
    T_ChRepair,
    T_ChCapture,
    T_ChHeal,
    T_ChRation,
    T_ChLevel,
    T_ChLevelMax,
    T_ChKill,
    T_ChExpHeroes,
    T_ChExpTroops,
    T_ChMap,
    T_ChVision,
    T_ChPeace,
    T_ChTech,
    T_PadHeld,
    T_ChShortHeroes,
    T_ChShortTroops,
    T_RadTiltCheat,
    T_HintUse,
    T_BadgeCheatsOff,
    T_BadgeCheatsNoRes,
    T_BadgeCheatSent,
    T_ActCheats,
    T_ChSpawnMenu,
    T_SpawnTitle,
    T_SpawnNone,
    T_SpawnTilt,
    T_SpawnHint,
    T_BadgeSpawned,
    T_BadgeSelHealthUp,
    T_BadgeSelHealthDown,
    T_BadgeSelExpUp,
    T_BadgeSelExpDown,
    T_BadgeSpeed,
    T_BadgeSpeedOnline,
    T_BadgeSettlement,
    T_BadgeNoSettlement,
    T_ActHalfSelect,
    T_ActSpeed,
    T_ActNextSettlement,
    T_PadPlusUpDown,
    T_PadPlusDpad,
    T_SetUiScale,
    T_ValSmall,
    T_ValNormal,
    T_ValLarge,
    T_ValHuge,
    T_SetButtons,
    T_ValOpen,
    T_ButtonsTitle,
    T_ButtonsFoot,
    T_ButtonsSame,
    T_BadgeNothingToAssign,
    T_ChBonus,
    T_ChAttack,
    T_ChDefense,
    T_ChFog,
    T_ChSpeed10,
    T_GrpUnit,
    T_GrpUnits,
    T_GrpHero,
    T_GrpHeroUnit,
    T_GrpHeroUnits,
    T_GrpHeroes,
    T_GrpHeroesUnits,
    T_GrpHealth,
    T_GrpSelection,
    T_SpawnNoBuilding,
    T_Count
};
void           LangInit();                // sceglie la lingua (ini o Settings.ini del gioco)
Lang           CurrentLang();
const wchar_t* Tr(Txt id);                // testo nella lingua scelta
std::wstring   TrF(Txt id, ...);          // testo con %s / %d riempiti

// --- Pad.cpp: lettura del controller o del simulatore da tastiera ---------------
// Pulsanti con i valori di XInput (posizioni: A sud, B est, X ovest, Y nord).
enum : WORD {
    PAD_DPAD_UP = 0x0001, PAD_DPAD_DOWN = 0x0002, PAD_DPAD_LEFT = 0x0004, PAD_DPAD_RIGHT = 0x0008,
    PAD_START = 0x0010, PAD_BACK = 0x0020, PAD_LS = 0x0040, PAD_RS = 0x0080,
    PAD_LB = 0x0100, PAD_RB = 0x0200, PAD_A = 0x1000, PAD_B = 0x2000, PAD_X = 0x4000, PAD_Y = 0x8000,
};
// Posti dei comandi della barra con RT tenuto, nell'ordine dei pulsanti visibili della barra (0.5.0).
constexpr int  kSlotCount = 12;
constexpr WORD kSlotMask[kSlotCount] = { PAD_A, PAD_B, PAD_X, PAD_Y, PAD_LB, PAD_RB, PAD_DPAD_UP, PAD_DPAD_RIGHT,
                                         PAD_DPAD_DOWN, PAD_DPAD_LEFT, PAD_LS, PAD_RS };
struct PadState {
    bool  connected = false;
    float lx = 0, ly = 0, rx = 0, ry = 0;   // -1..1, y positivo in alto
    float lt = 0, rt = 0;                   // 0..1
    WORD  buttons = 0;
};
extern std::atomic<HWND> g_gameHwnd;      // finestra del gioco (classe "OSWndClass")
extern std::atomic<DWORD> g_gameThread;   // thread che possiede la finestra del gioco (riceve la tastiera)
void        PadInit();
bool        PadSimulatorActive();         // simulatore da tastiera acceso (Bloc Maiusc, o Bloc Num con le lettere, lo spegne)
void        PadRead(PadState& out);
void        PadRumble(float low, float high);   // vibrazione 0..1 dei due motori (solo controller XInput)
const char* PadSourceName();

// --- Input.cpp: cursore, clic e tasti verso il gioco, modificatori virtuali -----
int  InputPatchImport(HMODULE mod, void* target, void* hook);   // tabella di import: voci verso target -> hook
void InputInstallHooks();                 // GetKeyState/GetAsyncKeyState nelle tabelle di import di gbr.exe e Resurrected
void InputSetShift(bool on);              // Maiusc virtuale tenuto (LT)
void InputShiftPulse(DWORD ms);           // Maiusc virtuale per ms millisecondi (Maiusc+tasto)
bool InputCursorClient(POINT& p);         // posizione attuale del cursore in coordinate client
void InputCursorMoveTo(int x, int y);     // sposta il cursore vero (coordinate client, gia' limitate alla finestra)
void InputMouse(int button, bool down, bool doubleClick);   // 0 sinistro, 1 destro; nel punto del cursore
void InputKey(UINT vk, bool down);        // tasto premuto o rilasciato (PostMessage alla finestra)
void InputKeyTap(UINT vk);                // premuto e rilasciato
void InputCtrlPulse(DWORD ms);            // Ctrl virtuale per ms millisecondi, senza Maiusc (Ctrl+cifra = assegna il gruppo)
void InputShiftSuppress(DWORD ms);        // Maiusc virtuale spento per ms millisecondi anche con LT tenuto (0.10.0)
void InputClickAt(int x, int y);          // clic sinistro in quel punto (coordinate client) senza spostare il cursore (0.8.3)
void InputSetVirtualCursor(bool on);      // cursore disegnato dal pad invece di quello di Windows (0.8.0)
bool InputVirtualCursor();

// --- Game.cpp: letture in sola lettura della memoria del gioco -----------------
struct GameState {
    bool     ok = false;
    uint32_t game = 0, mapView = 0, player = 0, selManager = 0, focus = 0;
    uint32_t selKind = 0, selOwn = 0, selCount = 0, pendingCmd = 0, paused = 0, editor = 0;
    uint32_t alerts = 0;                  // avvisi in elenco (cresce quando ne arriva uno)
    bool     mapViewShown = false;
    bool InMap() const { return ok && game && mapView && focus == mapView && mapViewShown && !editor; }
};
void GameCheckExecutable();               // impronta di gbr.exe: abilita le letture solo per l'exe conosciuto
bool GameKnownExe();
bool GameRead(GameState& s);              // false se l'exe non e' conosciuto o la lettura fallisce

// Finestre dell'interfaccia e barra comandi (0.5.0). Chiamano metodi virtuali del gioco e leggono strutture che solo il
// thread del gioco modifica: vanno chiamate SOLO nel thread del gioco (dall'aggancio di SetDIBitsToDevice).
// Rettangoli in coordinate del buffer del gioco, destra e basso esclusi.
enum UiKind { UiButton = 0, UiCombo = 1, UiList = 2, UiSlider = 3 };
struct UiControl { RECT r; int kind; };
int  GameUiControls(UiControl* out, int max, uint32_t* top);   // caselle visibili della finestra con il fuoco
int  GameBarButtons(RECT* rects, int* ids, int max);           // pulsanti visibili della barra comandi, in ordine
bool GameBarClick(int id, char* name, int cap);               // clic sul comando id come il giocatore; name = nome interno
int  GameEmptyMenuButtons(RECT* rects, int* ids, int max);     // pulsanti del menu senza selezione, da sinistra (0.8.3)
void GameResurrectedLetters(bool hide);   // nasconde (true) o rimette (false) le lettere di Resurrected sulla barra
bool GameResurrectedLoaded();             // c'e' la winmm.dll di Imperivm Resurrected, qualsiasi versione (0.10.0)
int  GameCheatSpawn(const char* cls, int count);   // trucco "crea truppe" della classe cls (thread del gioco)
bool GameCommandParam(int id, char* out, int cap); // parametro del comando (per "train..." la classe dell'unita')
bool GameCommandName(int id, char* out, int cap);  // nome interno del comando (trainIMilitiaman...)
int  GameNextSettlement(int& index, int& total);   // camera sul prossimo municipio proprio (thread del gioco); 0 = nessuno
int  GameCheat(const char* verb, int value);   // trucco di Resurrected in coda (thread del gioco); 0 = niente (0.9.0)
void GameCheatTick();                     // manda il prossimo comando dei trucchi (thread del gioco, ogni copia)
bool GameCommandTitle(int id, char* out, int cap);             // titolo italiano del comando, senza marcatori (Windows-1252)
bool GameGroupCounts(int counts[10]);                          // elementi dei gruppi 0..9 del giocatore locale
// 0.10.1: contenuto dei gruppi 0..9 e (posto 10) della selezione: elementi, eroi, salute media in % (-1 = non si sa)
struct GroupInfo { int count = 0, heroes = 0, hp = -1; };
bool GameGroupInfo(GroupInfo groups[11]);                      // SOLO nel thread del gioco
bool GameCameraMove(int dx, int dy);                           // sposta la vista come il timer di scorrimento (thread del gioco)
bool GameScrollParams(int& speed, int& align, int& stepMax);   // velocita' delle frecce, allineamento e passo massimo
bool GameObjectAt(int x, int y);                               // oggetto visibile in quel punto del buffer (thread del gioco)
bool GameSelectedHero(int& handle, int& hp, int& maxHp);       // eroe proprio selezionato (thread del gioco)
bool GameSpeed(int& speed, bool& multiplayer);                 // velocita' della partita (1000 = normale)
bool GameSetSpeed(int speed);                                  // solo in partita singola (thread del gioco)

// --- Overlay.cpp: disegno nell'immagine del gioco (aggancio di SetDIBitsToDevice) -
void OverlayInstall();
void OverlayTick(HWND hwnd);                                         // ridisegno con schermo fermo, statistiche
void OverlayBadge(const wchar_t* line1, const wchar_t* line2, DWORD ms);   // riquadro in alto a sinistra per ms
void OverlayHelp(bool on, const wchar_t* title, const std::vector<std::wstring>* rows, const wchar_t* foot);   // righe "pad	simulatore	azione"
void OverlayPad(bool active, bool inMap, GlyphStyle style, bool layer, int page);   // simboli sulla barra (layer = RT, page = RT+LT)
void OverlayCursor(bool on, int x, int y, int clientW, int clientH);   // cursore disegnato (coordinate client)
bool OverlayBufferSize(int& w, int& h);   // dimensioni del buffer del gioco
void OverlayBarRects(std::vector<RECT>& out);   // pulsanti visibili della barra o del menu senza selezione (buffer)
bool OverlayUiControls(std::vector<UiControl>& out, int& bufW, int& bufH);   // ultime caselle lette dal gioco (menu)
void OverlayBarCommand(int slot);         // RT + tasto: comando nel posto 'slot' della barra, eseguito nel thread del gioco
void OverlayTestBar(const std::vector<RECT>* rects);   // solo per tools/test_overlay: pulsanti finti della barra
void OverlayTestTitles(const std::vector<std::wstring>& titles);   // solo per tools/test_overlay: titoli di prova
void OverlayRadial(int kind, int highlight, bool assign, const wchar_t* hint);   // 0 chiuso, 1 comandi, 2 gruppi
int  OverlayRadialCount();                // voci del radiale aperto (0 finche' il thread del gioco non le ha lette)
bool OverlayGroupCounts(int counts[10]);  // radiale dei gruppi aperto: elementi dei gruppi 0..9 (0.8.4)
// Radiale dei trucchi (0.9.0, tipo 3): voci decise dal thread del pad; colore 0 bronzo, 1 oro, 2 cibo, 3 rosso, 4 azzurro;
// icona disegnata nel disco (0.9.1).
enum CheatIconId { CI_None, CI_Coins, CI_Loaf, CI_House, CI_People, CI_Hammer, CI_Flag, CI_WhiteFlag, CI_Heart, CI_Star,
                   CI_Skull, CI_Map, CI_Eye, CI_Book, CI_Swords, CI_Shield, CI_Hourglass, CI_Cloud };
struct CheatEntry { std::wstring label, title; int color; int icon; };
void OverlayCheatItems(const std::vector<CheatEntry>& items);
void OverlayCheat(const char* verb, int value);   // trucco da mandare nel thread del gioco
void OverlaySpawn(int index, int count);   // 0.10.0: crea count unita' della voce index del radiale "crea truppe" (tipo 4)
void OverlayNextSettlement();             // 0.10.0: camera sul prossimo insediamento proprio (thread del gioco)

// --- GameTick.cpp: lavori nel thread del gioco (camera 0.6.0, il resto 0.8.0) ----
void CameraInstall();
bool CameraAnalog();                      // aggancio riuscito e camera analogica accesa
void CameraSet(float vx, float vy);       // velocita' -1..1 (x a destra, y in su), 0 = ferma
void TickCursor(int bufX, int bufY);      // posizione del cursore nel buffer, per la calamita (-1 = nessuna)
bool TickHover();                         // sotto il cursore c'e' un oggetto
int  TickHeroHealth(int& handle);         // salute in percento dell'eroe proprio selezionato, -1 se nessuno
void TickSlowMotion(bool on);             // tempo rallentato (radiale aperto)
void TickSpeedStep(int dir);              // 0.10.0: velocita' della partita su (+1) o giu' (-1), solo in singolo; 10 = 10x o normale (0.10.1)
int  TickSpeedResult();                   // velocita' dopo l'ultimo passo (0 nulla di nuovo, -1 in rete)

// --- Steam.cpp: tastiera a schermo di Steam (0.8.0) ------------------------------
bool SteamKeyboard(int x, int y, int w, int h);

// --- Core.cpp: thread principale --------------------------------------------
DWORD WINAPI CoreThread(LPVOID);
