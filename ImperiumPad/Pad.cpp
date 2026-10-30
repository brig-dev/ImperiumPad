// Pad.cpp - lettura del controller
// Linguaggio: C++17, MSVC, Win32 (x86). Ripreso da SacredPad (Pad.cpp), stessa disposizione dei tasti.
//
// Tre sorgenti che producono lo stesso PadState:
//  - XInput: pad reale (sotto Steam, Steam Controller e DualSense arrivano cosi');
//  - Keyboard: pad simulato con il tastierino numerico, per provare senza controller;
//  - Laptop: pad simulato con le lettere, per i portatili senza tastierino.
// Nel simulatore i tasti usati vengono "mangiati" (il gioco non li vede) solo con il gioco in primo piano.
//  Tastierino: 8 4 6 2 (7 9 1 3 diagonali) = leva sinistra, frecce = leva destra, 0 = A, punto = B, 5 = X, / = Y,
//    * = RB, - = RT, + = LT, Invio = LB, Canc/PagGiu = croce sinistra/destra, Home/Fine = croce su/giu',
//    Ins = View, PagSu = Menu, F11 = R3, F12 = L3.
//  Lettere: W A S D = leva sinistra, frecce = leva destra, J = A, K = B, U = X, I = Y, O = RB, P = RT, Y = LT, H = LB,
//    Z/X = croce sinistra/destra, C/V = croce su/giu', Q = View, E = Menu, R = R3, F = L3.

#include "ImperiumPad.h"
#include <cmath>
#include <cstring>

std::atomic<HWND> g_gameHwnd{ nullptr };
std::atomic<DWORD> g_gameThread{ 0 };

namespace {

// --- XInput, caricato dinamicamente (nessuna dipendenza in fase di link) -----
struct XGamepad { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; };
struct XState   { DWORD dwPacketNumber; XGamepad Gamepad; };
using XInputGetState_t = DWORD (WINAPI*)(DWORD, XState*);
struct XVibration { WORD left, right; };
using XInputSetState_t = DWORD (WINAPI*)(DWORD, XVibration*);
XInputSetState_t g_xSetState = nullptr;     // vibrazione (0.8.0)

XInputGetState_t g_xGetState = nullptr;
char             g_xDll[32]  = "nessuna";

void LoadXInput()
{
    const char* names[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (const char* n : names) {
        if (HMODULE h = LoadLibraryA(n)) {
            g_xGetState = reinterpret_cast<XInputGetState_t>(GetProcAddress(h, "XInputGetState"));
            g_xSetState = reinterpret_cast<XInputSetState_t>(GetProcAddress(h, "XInputSetState"));
            if (g_xGetState) {
                strcpy_s(g_xDll, n);
                Log("XInput caricato: %s", n);
                return;
            }
        }
    }
    Log("XInput non disponibile");
}

float Stick(SHORT v) { float f = v / 32767.0f; return f < -1.0f ? -1.0f : (f > 1.0f ? 1.0f : f); }

bool ReadXInput(PadState& out)
{
    if (!g_xGetState) return false;
    // XInputGetState su uno slot vuoto e' lento: senza controller lo si chiede una volta al secondo (0.4.2)
    static DWORD nextTry = 0;
    if (nextTry && static_cast<int>(GetTickCount() - nextTry) < 0) return false;
    XState s{};
    if (g_xGetState(static_cast<DWORD>(g_cfg.controllerIndex), &s) != ERROR_SUCCESS) {
        nextTry = GetTickCount() + 1000;
        if (!nextTry) nextTry = 1;
        return false;
    }
    nextTry = 0;
    out.connected = true;
    out.lx = Stick(s.Gamepad.sThumbLX);  out.ly = Stick(s.Gamepad.sThumbLY);
    out.rx = Stick(s.Gamepad.sThumbRX);  out.ry = Stick(s.Gamepad.sThumbRY);
    out.lt = s.Gamepad.bLeftTrigger / 255.0f;
    out.rt = s.Gamepad.bRightTrigger / 255.0f;
    out.buttons = s.Gamepad.wButtons;
    return true;
}

// --- Simulatore da tastiera ------------------------------------------------------------
// Si riconoscono i tasti dal codice di scansione (posizione fisica): funziona con BlocNum acceso o spento, non
// confonde il tastierino con le frecce e vale anche per la tastiera italiana.
enum KeyBit {
    K7, K8, K9, K4, K6, K1, K2, K3, K0, KDot, KUp, KDown, KLeft, KRight,
    K5, KDiv, KMul, KSub, KAdd, KEnter, KDel, KPgDn, KHome, KEnd, KIns, KPgUp, KF11, KF12,
    KCount
};
std::atomic<unsigned> g_keys{ 0 };
bool g_kbEnabled = false;
bool g_laptop = false;
// Con le lettere il simulatore copre i tasti rapidi del gioco (il gioco usa quasi tutte le lettere nella barra comandi:
// E sacerdotessa, H eroe...), quindi si accende e si spegne. Storia: 0.3.1-0.3.2 attivo solo con il Bloc Maiusc acceso,
// 0.3.3 spento dal Bloc Maiusc acceso; alla prova della 0.4.0 il registro dei tasti ha mostrato che sul portatile della
// prova il tasto premuto era il Bloc Num (scansione 45 estesa, vk 0x90), non il Bloc Maiusc: per questo non si accendeva.
// 0.4.1: lo stato e' nostro, non dipende dalla spia. Il pad parte acceso; ogni pressione del Bloc Maiusc, o del Bloc Num
// con le lettere, lo spegne o lo riaccende, solo con il gioco in primo piano. A ogni cambio il thread principale fa un
// suono e mostra il riquadro di stato.
std::atomic<bool> g_simOn{ true };

bool SimActive() { return g_simOn.load(); }

int BitForLaptop(DWORD scan)
{
    switch (scan) {
    case 0x11: return K8;    case 0x1E: return K4;    case 0x1F: return K2;    case 0x20: return K6;   // W A S D
    case 0x24: return K0;    case 0x25: return KDot;                                                    // J = A, K = B
    case 0x16: return K5;    case 0x17: return KDiv;                                                    // U = X, I = Y
    case 0x18: return KMul;  case 0x19: return KSub;  case 0x15: return KAdd;                           // O = RB, P = RT, Y = LT
    case 0x23: return KEnter;                                                                           // H = LB
    case 0x2C: return KDel;  case 0x2D: return KPgDn; case 0x2E: return KHome; case 0x2F: return KEnd;  // Z X C V = croce sx dx su giu'
    case 0x10: return KIns;  case 0x12: return KPgUp;                                                   // Q = View, E = Menu
    case 0x13: return KF11;  case 0x21: return KF12;                                                    // R = R3, F = L3
    // 0.4.2: molti portatili senza tastierino ne hanno uno nascosto sulle lettere, attivo con il Bloc Num: allora U I O
    // J K P mandano i codici del tastierino (4 5 6 1 2 -). Si riconoscono anche quelli, cosi' il pad funziona con il
    // Bloc Num acceso o spento.
    case 0x4B: return K5;    case 0x4C: return KDiv;  case 0x4D: return KMul;                           // U I O (4 5 6)
    case 0x4F: return K0;    case 0x50: return KDot;  case 0x4A: return KSub;                           // J K P (1 2 -)
    default:   return -1;
    }
}

int BitForScan(DWORD scan)
{
    switch (scan) {
    case 0x47: return K7;  case 0x48: return K8;  case 0x49: return K9;
    case 0x4B: return K4;  case 0x4D: return K6;
    case 0x4F: return K1;  case 0x50: return K2;  case 0x51: return K3;
    case 0x52: return K0;  case 0x53: return KDot;
    case 0x4C: return K5;  case 0x37: return KMul;
    case 0x4A: return KSub; case 0x4E: return KAdd;
    case 0x57: return KF11; case 0x58: return KF12;
    default:   return -1;
    }
}

// Frecce e blocco Ins/Canc/Home/Fine/PagSu/PagGiu: stesso codice di scansione del tastierino, con il flag "esteso".
int BitForExtended(DWORD scan, bool laptop)
{
    switch (scan) {
    case 0x48: return KUp;    case 0x50: return KDown;
    case 0x4B: return KLeft;  case 0x4D: return KRight;
    default:   break;
    }
    if (laptop) return -1;     // con le lettere il blocco Ins/Canc resta al gioco
    switch (scan) {
    case 0x35: return KDiv;   case 0x1C: return KEnter;      // / e Invio del tastierino
    case 0x53: return KDel;   case 0x51: return KPgDn;
    case 0x47: return KHome;  case 0x4F: return KEnd;
    case 0x52: return KIns;   case 0x49: return KPgUp;
    default:   return -1;
    }
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
        static bool down[512];
        const unsigned idx = (k->scanCode & 0xff) | ((k->flags & LLKHF_EXTENDED) ? 0x100 : 0);
        const bool repeat = !(k->flags & LLKHF_UP) && down[idx];
        down[idx] = !(k->flags & LLKHF_UP);
        const bool gameFront = g_gameHwnd.load() && GetForegroundWindow() == g_gameHwnd.load();
        if ((k->vkCode == VK_CAPITAL || (g_laptop && k->vkCode == VK_NUMLOCK)) && !(k->flags & LLKHF_UP) && !repeat && gameFront) {
            const bool on = !g_simOn.load();
            g_simOn.store(on);
            Log("%s: pad simulato %s", k->vkCode == VK_CAPITAL ? "Bloc Maiusc" : "Bloc Num",
                on ? "ACCESO (le lettere fanno da pad)" : "SPENTO (tastiera normale per il gioco)");
        }
        int bit = -1;
        if ((!(k->flags & LLKHF_INJECTED) || g_cfg.acceptInjected) && SimActive())
            bit = (k->flags & LLKHF_EXTENDED) ? BitForExtended(k->scanCode, g_laptop)
                                              : (g_laptop ? BitForLaptop(k->scanCode) : BitForScan(k->scanCode));
        HWND game = g_gameHwnd.load();
        HWND fg = GetForegroundWindow();
        // solo con il gioco in primo piano: i tasti scritti in altre finestre non finiscono nel log
        if (g_cfg.keyLog && !(k->flags & LLKHF_UP) && !repeat && game && fg == game)
            Log("Tasto: scansione %02lX%s vk %02lX, bit %d%s", k->scanCode, (k->flags & LLKHF_EXTENDED) ? " esteso" : "",
                k->vkCode, bit, (k->flags & LLKHF_INJECTED) ? ", iniettato" : "");
        if (bit >= 0 && game && fg == game) {
            if (k->flags & LLKHF_UP) g_keys.fetch_and(~(1u << bit));
            else                     g_keys.fetch_or(1u << bit);
            return 1;   // il gioco non riceve il tasto
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

DWORD WINAPI KeyboardThread(LPVOID)
{
    // Un hook a basso livello richiede un ciclo di messaggi nel thread che lo installa.
    HHOOK hook = SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardProc, SelfModule(), 0);
    if (!hook) { Log("ERRORE: hook tastiera del simulatore non installato (%lu)", GetLastError()); return 0; }
    if (g_laptop)
        Log("Simulatore pad con le lettere, acceso all'avvio; Bloc Maiusc o Bloc Num lo spengono e riaccendono: W A S D = "
            "leva sinistra, frecce = leva destra, J = A, K = B, U = X, I = Y, O = RB, P = RT, Y = LT, H = LB, Z/X = croce "
            "sinistra/destra, C/V = croce su/giu', Q = View, E = Menu, R = R3, F = L3");
    else
        Log("Simulatore pad con il tastierino: 8/4/6/2 (+7/9/1/3) = leva sinistra, frecce = leva destra, 0 = A, punto = B, "
            "5 = X, / = Y, * = RB, - = RT, + = LT, Invio = LB, Canc/PagGiu = croce sinistra/destra, Home/Fine = croce "
            "su/giu', Ins = View, PagSu = Menu, F11 = R3, F12 = L3; Bloc Maiusc lo spegne e riaccende");
    if (g_cfg.acceptInjected)
        Log("ATTENZIONE: [Pad] AcceptInjected=1, il simulatore accetta anche i tasti mandati da programma (solo per le prove)");
    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    UnhookWindowsHookEx(hook);
    return 0;
}

bool ReadKeyboard(PadState& out)
{
    unsigned k = g_keys.load();
    HWND game = g_gameHwnd.load();
    if (!game || GetForegroundWindow() != game) { g_keys.store(0); k = 0; }   // niente tasti "incastrati"
    if (!SimActive()) { g_keys.store(0); k = 0; }                              // simulatore a lettere spento

    auto on = [k](int b) { return (k >> b) & 1u; };
    float x = 0, y = 0;
    if (on(K4) || on(K7) || on(K1)) x -= 1;
    if (on(K6) || on(K9) || on(K3)) x += 1;
    if (on(K8) || on(K7) || on(K9)) y += 1;
    if (on(K2) || on(K1) || on(K3)) y -= 1;
    float len = std::sqrt(x * x + y * y);
    if (len > 0) { x /= len; y /= len; }

    float rx = 0, ry = 0;
    if (on(KLeft))  rx -= 1;
    if (on(KRight)) rx += 1;
    if (on(KUp))    ry += 1;
    if (on(KDown))  ry -= 1;
    float rlen = std::sqrt(rx * rx + ry * ry);
    if (rlen > 0) { rx /= rlen; ry /= rlen; }

    out.connected = true;
    out.lx = x; out.ly = y;
    out.rx = rx; out.ry = ry;
    if (on(K0))    out.buttons |= PAD_A;
    if (on(KDot))  out.buttons |= PAD_B;
    if (on(K5))    out.buttons |= PAD_X;
    if (on(KDiv))  out.buttons |= PAD_Y;
    if (on(KMul))  out.buttons |= PAD_RB;
    if (on(KEnter))out.buttons |= PAD_LB;
    if (on(KDel))  out.buttons |= PAD_DPAD_LEFT;
    if (on(KPgDn)) out.buttons |= PAD_DPAD_RIGHT;
    if (on(KHome)) out.buttons |= PAD_DPAD_UP;
    if (on(KEnd))  out.buttons |= PAD_DPAD_DOWN;
    if (on(KIns))  out.buttons |= PAD_BACK;
    if (on(KPgUp)) out.buttons |= PAD_START;
    if (on(KF11))  out.buttons |= PAD_RS;
    if (on(KF12))  out.buttons |= PAD_LS;
    out.rt = on(KSub) ? 1.0f : 0.0f;
    out.lt = on(KAdd) ? 1.0f : 0.0f;
    return true;
}

const char* g_sourceName = "nessuna";

} // namespace

void PadInit()
{
    const bool sim = (g_cfg.padSource == PadSource::Keyboard || g_cfg.padSource == PadSource::Laptop);
    g_laptop = (g_cfg.padSource == PadSource::Laptop);
    if (!sim) LoadXInput();
    g_kbEnabled = sim;
    if (g_kbEnabled) {
        if (HANDLE th = CreateThread(nullptr, 0, KeyboardThread, nullptr, 0, nullptr)) CloseHandle(th);
    }
}

void PadRead(PadState& out)
{
    out = PadState{};
    const char* name = "nessuna";
    if (!g_kbEnabled && ReadXInput(out)) name = g_xDll;
    else if (g_kbEnabled && ReadKeyboard(out)) name = g_laptop ? "lettere" : "tastierino";

    if (name != g_sourceName) {
        Log("Sorgente del pad: %s", name);
        g_sourceName = name;
    }
}

const char* PadSourceName() { return g_sourceName; }

bool PadSimulatorActive() { return g_kbEnabled && SimActive(); }

// Vibrazione (0.8.0): motore grande (basse frequenze) e piccolo, 0..1; 0, 0 = ferma. Solo con un controller XInput;
// sotto Steam Input arriva anche allo Steam Controller e al DualSense.
void PadRumble(float low, float high)
{
    if (!g_xSetState || g_kbEnabled) return;
    auto w = [](float v) { return static_cast<WORD>((v < 0 ? 0 : v > 1 ? 1 : v) * 65535.0f); };
    XVibration v{ w(low), w(high) };
    g_xSetState(static_cast<DWORD>(g_cfg.controllerIndex), &v);
}
