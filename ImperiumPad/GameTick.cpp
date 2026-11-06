// GameTick.cpp - lavori fatti nel thread del gioco tra un fotogramma e l'altro (0.6.0 camera, 0.8.0 il resto)
// Linguaggio: C++17, MSVC, Win32 (x86).
//
// Si aggancia PeekMessageA nella tabella di import di gbr.exe: il ciclo dei messaggi del gioco la chiama tra un
// fotogramma e l'altro, dove gira anche il timer di scorrimento del gioco. Li' si fanno le cose che toccano lo stato del
// gioco o chiamano sue funzioni (non si possono fare dal thread del pad, e nel disegno sarebbe tardi):
//  - camera analogica (0.6.0): lo scorrimento del gioco e' solo digitale (frecce e bordi accendono dei bit e un timer
//    sposta la vista a velocita' fissa, 0x5b8100); qui la vista si sposta con gli stessi passi del timer
//    (GameCameraMove), ma con la velocita' proporzionale all'inclinazione della leva e nella sua direzione vera;
//  - calamita (0.8.0): se sotto il cursore c'e' un oggetto (GameObjectAt, la stessa ricerca dei clic), il thread del
//    pad rallenta il cursore;
//  - eroe selezionato (0.8.0): salute letta qui, la vibrazione la decide il thread del pad;
//  - tempo rallentato (0.8.0): mentre un radiale e' aperto, solo in partita singola, la velocita' della partita scende
//    (GameSetSpeed) e poi torna com'era.
// Il thread del pad scrive solo richieste (atomiche) e legge i risultati.

#include "ImperiumPad.h"
#include <algorithm>
#include <cmath>

namespace {

using PeekMessageA_t = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
PeekMessageA_t g_realPeek = nullptr;
bool g_installed = false;

// richieste del thread del pad
std::atomic<int>  g_vx{ 0 }, g_vy{ 0 };   // velocita' della camera, -10000..10000 (frazione della massima)
std::atomic<bool> g_want{ false };
std::atomic<int>  g_cursorX{ -1 }, g_cursorY{ -1 };   // cursore in coordinate del buffer del gioco (-1 = nessuno)
std::atomic<bool> g_slowWant{ false };
std::atomic<int>  g_speedStep{ 0 };     // 0.10.0: +1 / -1 = velocita' della partita su o giu'
std::atomic<int>  g_speedResult{ 0 };   // velocita' dopo il passo (0 = nulla di nuovo, -1 = in rete)
// risultati per il thread del pad
std::atomic<bool> g_hover{ false };
std::atomic<int>  g_heroHandle{ -1 }, g_heroPct{ -1 };

// solo nel thread del gioco
LARGE_INTEGER g_freq{}, g_last{};
bool  g_moving = false;
float g_accX = 0, g_accY = 0;
int   g_speed = 1280, g_align = 16, g_stepMax = 64;
unsigned g_steps = 0;
DWORD g_nextLook = 0;
bool  g_slowed = false;
int   g_savedSpeed = 0, g_slowSpeed = 0;
uint32_t g_slowGame = 0;

void Camera(const LARGE_INTEGER& now)
{
    if (!g_want.load()) {
        if (g_moving) { g_moving = false; Log("Camera analogica: ferma (%u passi)", g_steps); g_steps = 0; }
        g_accX = g_accY = 0;
        return;
    }
    if (!g_moving) {
        g_moving = true;
        g_last = now;
        GameScrollParams(g_speed, g_align, g_stepMax);
        Log("Camera analogica: in movimento (velocita' piena %d pixel/s x %.2f, passo allineato a %d)", g_speed,
            g_cfg.cameraSpeed, g_align);
        return;
    }
    double dt = static_cast<double>(now.QuadPart - g_last.QuadPart) / static_cast<double>(g_freq.QuadPart);
    if (dt < 0.004) return;
    if (dt > 0.1) dt = 0.1;
    g_last = now;
    GameState gs;
    if (!GameRead(gs) || !gs.InMap()) { g_accX = g_accY = 0; return; }
    const float speed = g_speed * g_cfg.cameraSpeed;
    g_accX += g_vx.load() / 10000.0f * speed * static_cast<float>(dt);
    g_accY -= g_vy.load() / 10000.0f * speed * static_cast<float>(dt);   // leva in su = vista verso l'alto
    // passi allineati come quelli del gioco, al massimo quattro passi massimi del timer per volta
    const float lim = static_cast<float>(g_stepMax * 4);
    if (g_accX > lim) g_accX = lim;
    if (g_accX < -lim) g_accX = -lim;
    if (g_accY > lim) g_accY = lim;
    if (g_accY < -lim) g_accY = -lim;
    const int dx = static_cast<int>(g_accX / g_align) * g_align, dy = static_cast<int>(g_accY / g_align) * g_align;
    if (!dx && !dy) return;
    g_accX -= dx;
    g_accY -= dy;
    if (GameCameraMove(dx, dy)) ++g_steps;
    else g_accX = g_accY = 0;
}

// Ogni 40 ms: oggetto sotto il cursore, eroe selezionato, tempo rallentato.
void Look(DWORD now)
{
    if (static_cast<int>(now - g_nextLook) < 0) return;
    g_nextLook = now + 40;
    GameState gs;
    const bool inMap = GameRead(gs) && gs.InMap();

    const int cx = g_cursorX.load(), cy = g_cursorY.load();
    g_hover.store(inMap && g_cfg.magnet < 0.99f && cx >= 0 && cy >= 0 && GameObjectAt(cx, cy));

    int handle = -1, hp = 0, maxHp = 0;
    if (inMap && GameSelectedHero(handle, hp, maxHp)) {
        g_heroHandle.store(handle);
        g_heroPct.store(hp * 100 / maxHp);
    }
    else {
        g_heroHandle.store(-1);
        g_heroPct.store(-1);
    }

    // tempo rallentato con un radiale aperto (solo in partita singola; mai in rete)
    int speed = 0;
    bool multi = false;
    bool haveSpeed = GameSpeed(speed, multi);
    // velocita' della partita scelta col pad (0.10.0, View + croce su/giu'): passi di un quarto, da 250 a 4000 (1000 =
    // normale per il gioco), solo in singolo e mai a radiale aperto
    // 0.10.1: step 10 = trucco classico "velocita' 10x" (10000, o di nuovo 1000 se e' gia' 10x). Si aspetta che il tempo
    // rallentato sia finito (il trucco si sceglie dal radiale): prima si leggerebbe la velocita' rallentata
    const int step = g_slowed ? 0 : g_speedStep.exchange(0);
    if (step && inMap && haveSpeed) {
        if (multi) g_speedResult.store(-1);
        else {
            int next = step == 10 ? (speed >= 10000 ? 1000 : 10000) : step > 0 ? speed * 5 / 4 : speed * 4 / 5;
            if (step != 10) next = (std::max)(250, (std::min)(4000, next));
            if (GameSetSpeed(next)) {
                Log("Velocita' della partita: da %d a %d", speed, next);
                speed = next;
            }
            g_speedResult.store(speed);
        }
    }
    const bool want = g_slowWant.load() && g_cfg.slowMotion > 0.0f && g_cfg.slowMotion < 1.0f && inMap && haveSpeed && !multi;
    if (g_slowed && gs.game != g_slowGame) {   // partita finita o cambiata: niente da rimettere
        g_slowed = false;
        Log("Tempo rallentato: partita cambiata, nulla da rimettere");
    }
    if (want && !g_slowed) {
        g_savedSpeed = speed;
        g_slowSpeed = (std::max)(1, static_cast<int>(speed * g_cfg.slowMotion + 0.5f));
        if (GameSetSpeed(g_slowSpeed)) {
            g_slowed = true;
            g_slowGame = gs.game;
            Log("Tempo rallentato: velocita' della partita da %d a %d (radiale aperto)", g_savedSpeed, g_slowSpeed);
        }
    }
    else if (!want && g_slowed) {
        // si rimette la velocita' di prima solo se nessuno l'ha cambiata nel frattempo
        if (haveSpeed && speed == g_slowSpeed) GameSetSpeed(g_savedSpeed);
        Log("Tempo rallentato: velocita' della partita di nuovo %d", haveSpeed && speed == g_slowSpeed ? g_savedSpeed : speed);
        g_slowed = false;
    }
}

void Tick()
{
    if (GetCurrentThreadId() != g_gameThread.load()) return;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    Camera(now);
    Look(GetTickCount());
}

BOOL WINAPI HookPeekMessageA(LPMSG msg, HWND hwnd, UINT min, UINT max, UINT remove)
{
    Tick();
    return g_realPeek(msg, hwnd, min, max, remove);
}

} // namespace

void CameraInstall()
{
    QueryPerformanceFrequency(&g_freq);
    g_realPeek = reinterpret_cast<PeekMessageA_t>(GetProcAddress(GetModuleHandleA("user32.dll"), "PeekMessageA"));
    if (!GameKnownExe()) { Log("Thread del gioco: exe sconosciuto, niente camera analogica, calamita sugli oggetti e tempo rallentato"); return; }
    const int n = InputPatchImport(GetModuleHandleA(nullptr), reinterpret_cast<void*>(g_realPeek),
                                   reinterpret_cast<void*>(&HookPeekMessageA));
    g_installed = n > 0;
    Log("Thread del gioco: PeekMessageA agganciato in gbr.exe (%d voci)%s; camera analogica %s", n, n ? "" : " ATTENZIONE",
        g_installed && g_cfg.cameraAnalog ? "accesa" : "spenta (leva destra = frecce)");
}

bool CameraAnalog() { return g_installed && g_cfg.cameraAnalog; }

void CameraSet(float vx, float vy)
{
    g_vx.store(static_cast<int>(vx * 10000.0f));
    g_vy.store(static_cast<int>(vy * 10000.0f));
    g_want.store(vx != 0.0f || vy != 0.0f);
}

void TickCursor(int bufX, int bufY)
{
    g_cursorX.store(bufX);
    g_cursorY.store(bufY);
}

bool TickHover() { return g_installed && g_hover.load(); }

int TickHeroHealth(int& handle)
{
    handle = g_heroHandle.load();
    return g_heroPct.load();
}

void TickSlowMotion(bool on) { g_slowWant.store(on); }
void TickSpeedStep(int dir) { g_speedStep.store(dir == 10 ? 10 : dir > 0 ? 1 : -1); }
int  TickSpeedResult() { return g_speedResult.exchange(0); }
