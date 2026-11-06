// Input.cpp - cursore, clic e tasti verso il gioco; modificatori virtuali
// Linguaggio: C++17, MSVC, Win32 (x86).
//
// Il cursore e' quello vero di Windows: si sposta con SetCursorPos, cosi' Windows manda da solo i movimenti al gioco,
// il gioco mostra il cursore giusto (attacco, cattura...) e il suo scorrimento ai bordi funziona.
// Clic e tasti si mandano con PostMessage alla finestra del gioco: gbr.exe prende le coordinate dei clic dal
// messaggio (procedura di finestra 0x678a10) e il tasto da wParam; con Imperivm Resurrected i messaggi passano dalla
// sua procedura di finestra, che ricalcola le coordinate se la finestra e il gioco hanno dimensioni diverse.
// Maiusc "virtuale" (LT e Maiusc+tasto): il gioco legge i modificatori con GetKeyState (procedura di finestra e
// metodo +0x40 di CWinOS, 0x676b00), Resurrected con GetKeyState e GetAsyncKeyState; si agganciano le loro voci
// nelle tabelle di import di gbr.exe e della winmm.dll di Resurrected, senza modificare user32.
// Cursore disegnato (0.8.0): se SetCursorPos non sposta il cursore (rischio sotto gamescope sulla Steam Machine), il
// pad tiene una posizione propria: il gioco la riceve con i movimenti del mouse mandati con PostMessage (prende le
// coordinate dal messaggio) e l'overlay disegna una freccia in quel punto. Se il mouse vero si muove, si riparte da li'.

#include "ImperiumPad.h"
#include <cstring>

namespace {

using GetKeyState_t = SHORT (WINAPI*)(int);
GetKeyState_t g_realGetKeyState = nullptr;
GetKeyState_t g_realGetAsyncKeyState = nullptr;

std::atomic<bool>  g_shiftHeld{ false };
std::atomic<DWORD> g_shiftUntil{ 0 };
std::atomic<DWORD> g_ctrlUntil{ 0 };      // Ctrl virtuale a tempo (0.6.0); intanto il Maiusc virtuale tace
bool g_leftDown = false, g_rightDown = false;   // pulsanti tenuti dal pad (per wParam dei messaggi)
std::atomic<bool> g_virtual{ false };           // cursore disegnato invece di quello di Windows
std::atomic<int>  g_vx{ 0 }, g_vy{ 0 };         // sua posizione (coordinate client)
POINT g_lastReal{ -100000, -100000 };           // ultima posizione del cursore vero vista (schermo)

bool CtrlActive()
{
    const DWORD until = g_ctrlUntil.load();
    return until && static_cast<int>(until - GetTickCount()) > 0;
}

std::atomic<DWORD> g_noShiftUntil{ 0 };   // 0.10.0: Maiusc virtuale spento per poco (LT + croce = tasti di selezione)

bool ShiftActive()
{
    if (CtrlActive()) return false;
    const DWORD quiet = g_noShiftUntil.load();
    if (quiet && static_cast<int>(quiet - GetTickCount()) > 0) return false;
    if (g_shiftHeld.load()) return true;
    const DWORD until = g_shiftUntil.load();
    return until && static_cast<int>(until - GetTickCount()) > 0;
}

SHORT WINAPI HookGetKeyState(int vk)
{
    SHORT r = g_realGetKeyState(vk);
    if ((vk == VK_SHIFT || vk == VK_LSHIFT) && ShiftActive()) r = static_cast<SHORT>(r | 0x8000);
    if ((vk == VK_CONTROL || vk == VK_LCONTROL) && CtrlActive()) r = static_cast<SHORT>(r | 0x8000);
    return r;
}

SHORT WINAPI HookGetAsyncKeyState(int vk)
{
    SHORT r = g_realGetAsyncKeyState(vk);
    if ((vk == VK_SHIFT || vk == VK_LSHIFT) && ShiftActive()) r = static_cast<SHORT>(r | 0x8000);
    if ((vk == VK_CONTROL || vk == VK_LCONTROL) && CtrlActive()) r = static_cast<SHORT>(r | 0x8000);
    return r;
}

LPARAM ClientPoint(int x, int y) { return MAKELPARAM(static_cast<short>(x), static_cast<short>(y)); }

bool IsExtendedKey(UINT vk)
{
    switch (vk) {
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_HOME: case VK_END:
    case VK_PRIOR: case VK_NEXT: case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK:
        return true;
    default:
        return false;
    }
}

} // namespace

// Sostituisce nella tabella di import del modulo le voci che puntano a 'target' con 'hook'. Restituisce quante.
int InputPatchImport(HMODULE mod, void* target, void* hook)
{
    if (!mod || !target) return 0;
    auto base = reinterpret_cast<uint8_t*>(mod);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;
    int count = 0;
    for (auto imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp) {
        for (auto thunk = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + imp->FirstThunk); thunk->u1.Function; ++thunk) {
            if (reinterpret_cast<void*>(thunk->u1.Function) != target) continue;
            DWORD old = 0;
            if (VirtualProtect(&thunk->u1.Function, sizeof(DWORD), PAGE_READWRITE, &old)) {
                thunk->u1.Function = reinterpret_cast<DWORD>(hook);
                VirtualProtect(&thunk->u1.Function, sizeof(DWORD), old, &old);
                ++count;
            }
        }
    }
    return count;
}

void InputInstallHooks()
{
    HMODULE user32 = GetModuleHandleA("user32.dll");
    g_realGetKeyState = reinterpret_cast<GetKeyState_t>(GetProcAddress(user32, "GetKeyState"));
    g_realGetAsyncKeyState = reinterpret_cast<GetKeyState_t>(GetProcAddress(user32, "GetAsyncKeyState"));
    if (!g_cfg.hookKeyState) { Log("Modificatori virtuali spenti ([Input] VirtualModifiers=false)"); return; }

    HMODULE game = GetModuleHandleA(nullptr);
    const int g1 = InputPatchImport(game, reinterpret_cast<void*>(g_realGetKeyState), reinterpret_cast<void*>(&HookGetKeyState));
    const int g2 = InputPatchImport(game, reinterpret_cast<void*>(g_realGetAsyncKeyState), reinterpret_cast<void*>(&HookGetAsyncKeyState));
    int r1 = 0, r2 = 0;
    HMODULE winmm = GetModuleHandleA("winmm.dll");
    char path[MAX_PATH] = {};
    if (winmm) GetModuleFileNameA(winmm, path, MAX_PATH);
    const bool resurrected = winmm && _strnicmp(path, GameDir(), strlen(GameDir())) == 0;
    if (resurrected) {
        r1 = InputPatchImport(winmm, reinterpret_cast<void*>(g_realGetKeyState), reinterpret_cast<void*>(&HookGetKeyState));
        r2 = InputPatchImport(winmm, reinterpret_cast<void*>(g_realGetAsyncKeyState), reinterpret_cast<void*>(&HookGetAsyncKeyState));
    }
    Log("Modificatori virtuali: GetKeyState agganciato in gbr.exe (%d voci) e GetAsyncKeyState (%d)%s; in Resurrected "
        "GetKeyState %d, GetAsyncKeyState %d", g1, g2, g1 ? "" : " ATTENZIONE: nessuna voce trovata", r1, r2);
}

void InputSetShift(bool on) { g_shiftHeld.store(on); }

void InputShiftPulse(DWORD ms)
{
    DWORD until = GetTickCount() + ms;
    if (!until) until = 1;
    g_shiftUntil.store(until);
}

void InputShiftSuppress(DWORD ms)
{
    DWORD until = GetTickCount() + ms;
    if (!until) until = 1;
    g_noShiftUntil.store(until);
}

void InputCtrlPulse(DWORD ms)
{
    DWORD until = GetTickCount() + ms;
    if (!until) until = 1;
    g_ctrlUntil.store(until);
}

bool InputCursorClient(POINT& p)
{
    HWND hwnd = g_gameHwnd.load();
    if (!hwnd || !GetCursorPos(&p)) return false;
    if (g_virtual.load()) {
        // il mouse vero si e' mosso: il cursore disegnato riparte da li'
        if (p.x != g_lastReal.x || p.y != g_lastReal.y) {
            g_lastReal = p;
            POINT c = p;
            if (ScreenToClient(hwnd, &c)) { g_vx.store(c.x); g_vy.store(c.y); }
        }
        p.x = g_vx.load();
        p.y = g_vy.load();
        return true;
    }
    return ScreenToClient(hwnd, &p) != FALSE;
}

void InputSetVirtualCursor(bool on)
{
    if (on == g_virtual.load()) return;
    HWND hwnd = g_gameHwnd.load();
    POINT p{};
    if (on && hwnd && GetCursorPos(&p)) {
        g_lastReal = p;
        ScreenToClient(hwnd, &p);
        g_vx.store(p.x);
        g_vy.store(p.y);
    }
    g_virtual.store(on);
    Log("Cursore: %s", on ? "DISEGNATO dal pad (SetCursorPos non usato)" : "quello vero di Windows");
}

bool InputVirtualCursor() { return g_virtual.load(); }

void InputCursorMoveTo(int x, int y)
{
    HWND hwnd = g_gameHwnd.load();
    if (!hwnd) return;
    if (g_virtual.load()) {
        g_vx.store(x);
        g_vy.store(y);
    }
    else {
        POINT p{ x, y };
        ClientToScreen(hwnd, &p);
        SetCursorPos(p.x, p.y);
    }
    // Windows genera da solo il movimento del mouse dopo SetCursorPos; sotto Wine e gamescope non e' certo, quindi lo
    // si manda anche a mano (stesse coordinate: un doppione e' innocuo). Serve anche ai trascinamenti di A e X.
    WPARAM wp = 0;
    if (g_leftDown) wp |= MK_LBUTTON;
    if (g_rightDown) wp |= MK_RBUTTON;
    if (ShiftActive()) wp |= MK_SHIFT;
    PostMessageA(hwnd, WM_MOUSEMOVE, wp, ClientPoint(x, y));
}

void InputMouse(int button, bool down, bool doubleClick)
{
    HWND hwnd = g_gameHwnd.load();
    POINT p{};
    if (!hwnd || !InputCursorClient(p)) return;
    RECT c{};
    GetClientRect(hwnd, &c);
    if (down && (p.x < 0 || p.y < 0 || p.x >= c.right || p.y >= c.bottom)) {
        // cursore fuori dalla finestra del gioco (per esempio su un altro monitor): niente clic
        Log("Clic del pad non inviato: cursore fuori dalla finestra del gioco (%ld,%ld)", p.x, p.y);
        return;
    }
    if (!down && ((button == 0 && !g_leftDown) || (button == 1 && !g_rightDown))) return;   // pressione non inviata
    if (p.x < 0) p.x = 0;
    if (p.y < 0) p.y = 0;
    if (p.x >= c.right) p.x = c.right - 1;
    if (p.y >= c.bottom) p.y = c.bottom - 1;
    UINT msg;
    if (button == 0) {
        g_leftDown = down;
        msg = down ? (doubleClick ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN) : WM_LBUTTONUP;
    }
    else {
        g_rightDown = down;
        msg = down ? (doubleClick ? WM_RBUTTONDBLCLK : WM_RBUTTONDOWN) : WM_RBUTTONUP;
    }
    WPARAM wp = 0;
    if (g_leftDown) wp |= MK_LBUTTON;
    if (g_rightDown) wp |= MK_RBUTTON;
    if (ShiftActive()) wp |= MK_SHIFT;
    PostMessageA(hwnd, msg, wp, ClientPoint(p.x, p.y));
}

// Clic sinistro in un punto senza spostare il cursore (0.8.3, pulsanti del menu senza selezione): movimento, pressione e
// rilascio in quel punto, poi di nuovo il movimento dove sta il cursore. La procedura di finestra del gioco (0x678a10)
// trasforma ogni messaggio in un evento con le coordinate del messaggio (0x101 movimento, 0x111 pressione, 0x112
// rilascio), nello stesso ordine: il pulsante vede un clic completo. Non si fa con A o X tenuti (trascinamenti in corso).
void InputClickAt(int x, int y)
{
    HWND hwnd = g_gameHwnd.load();
    POINT back{};
    if (!hwnd || !InputCursorClient(back)) return;
    if (g_leftDown || g_rightDown) { Log("Clic nel punto %d,%d non inviato: A o X tenuto", x, y); return; }
    PostMessageA(hwnd, WM_MOUSEMOVE, 0, ClientPoint(x, y));
    PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, ClientPoint(x, y));
    PostMessageA(hwnd, WM_LBUTTONUP, 0, ClientPoint(x, y));
    PostMessageA(hwnd, WM_MOUSEMOVE, ShiftActive() ? MK_SHIFT : 0, ClientPoint(back.x, back.y));
}

void InputKey(UINT vk, bool down)
{
    HWND hwnd = g_gameHwnd.load();
    if (!hwnd) return;
    const UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    LPARAM lp = 1 | (static_cast<LPARAM>(scan & 0xff) << 16);
    if (IsExtendedKey(vk)) lp |= (1 << 24);
    if (!down) lp |= (1 << 30) | (1u << 31);
    PostMessageA(hwnd, down ? WM_KEYDOWN : WM_KEYUP, vk, lp);
}

void InputKeyTap(UINT vk)
{
    InputKey(vk, true);
    InputKey(vk, false);
}
