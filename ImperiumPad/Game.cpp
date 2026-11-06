// Game.cpp - riconoscimento di gbr.exe e letture in sola lettura della memoria del gioco
// Linguaggio: C++17, MSVC, Win32 (x86).
//
// Gli indirizzi vengono dallo studio di gbr.exe (originale Steam, senza ASLR: fissi a ogni avvio); [R] = tabelle
// di Imperivm Resurrected (src/addr). Si leggono solo se l'impronta dell'eseguibile e' quella conosciuta.

#include "ImperiumPad.h"
#include <cstring>
#include <cwchar>
#include <utility>
#include <vector>

namespace {

// FNV-1a a 64 bit del file, come Imperivm Resurrected (src/app/dllmain.cpp): l'impronta "normalizzata" salta il campo
// Characteristics (flag 4 GB, che Resurrected accende alla chiusura del gioco su Windows) e il checksum PE.
constexpr uint64_t kSteamExe           = 0x8063a6de24cf3ccaull;
constexpr uint64_t kSteamExeNormalized = 0x333183d57588d212ull;

bool g_knownExe = false;

// Indirizzi del gioco
constexpr uintptr_t kMapView    = 0x996ff0;   // vista della mappa [R kMapView]
constexpr uintptr_t kGame       = 0x996ff4;   // CVXGame, nullo fuori partita [R kGame]
constexpr uintptr_t kSelManager = 0x9c0938;   // gestore della selezione [R kSelectionManager]
constexpr uintptr_t kUIRoot     = 0xa7abf0;   // radice dell'interfaccia [R kUIRoot]
constexpr uintptr_t kEditorMode = 0xa8734c;   // editor aperto [R kEditorMode]
constexpr uint32_t  kRootFocus  = 0x368;      // finestra con il fuoco (metodo +0x74 della radice, 0x6326d0)

} // namespace

void GameCheckExecutable()
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        Log("Eseguibile: impossibile aprire %s (%lu)", path, GetLastError());
        return;
    }
    const auto image = reinterpret_cast<const uint8_t*>(GetModuleHandleA(nullptr));
    const DWORD pe = reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew;
    uint64_t exact = 1469598103934665603ull, norm = exact;
    std::vector<uint8_t> buf(65536);
    DWORD read = 0, total = 0;
    while (ReadFile(f, buf.data(), static_cast<DWORD>(buf.size()), &read, nullptr) && read) {
        for (DWORD i = 0; i < read; ++i) {
            const DWORD pos = total + i;
            const bool skip = (pos >= pe + 0x16 && pos < pe + 0x18) || (pos >= pe + 0x58 && pos < pe + 0x5c);
            exact = (exact ^ buf[i]) * 1099511628211ull;
            norm = (norm ^ (skip ? 0 : buf[i])) * 1099511628211ull;
        }
        total += read;
    }
    CloseHandle(f);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + pe);
    const bool laa = (nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0;
    g_knownExe = exact == kSteamExe || norm == kSteamExeNormalized;
    Log("Eseguibile: %lu byte, impronta %016llx (normalizzata %016llx), flag 4 GB %s -> %s", total, exact, norm,
        laa ? "si'" : "no", g_knownExe ? "gbr.exe originale Steam" : "SCONOSCIUTO: niente letture della memoria");
}

bool GameKnownExe() { return g_knownExe; }

// Funzione senza oggetti C++: __try non ammette distruttori nella stessa funzione.
bool GameRead(GameState& s)
{
    s = GameState{};
    if (!g_knownExe || !g_cfg.memory) return false;
    __try {
        s.game       = *reinterpret_cast<uint32_t*>(kGame);
        s.mapView    = *reinterpret_cast<uint32_t*>(kMapView);
        s.selManager = *reinterpret_cast<uint32_t*>(kSelManager);
        s.editor     = *reinterpret_cast<uint32_t*>(kEditorMode);
        if (const uint32_t root = *reinterpret_cast<uint32_t*>(kUIRoot)) s.focus = *reinterpret_cast<uint32_t*>(root + kRootFocus);
        if (s.game) {
            s.player = *reinterpret_cast<uint32_t*>(s.game + 0x12c8);   // giocatore locale
            s.paused = *reinterpret_cast<uint32_t*>(s.game + 0x1240);   // flag di pausa (0x51d2e0)
        }
        if (s.mapView) s.mapViewShown = (*reinterpret_cast<uint32_t*>(s.mapView + 0x28) & 2) != 0;
        if (s.selManager) {
            // oggetto selezione a +0xf0: +8 tipo, +0xc selezione del giocatore locale, +0x18 lista, +0x1c numero
            // (0x5b8760 e 0x5b8ae0, usate dal gioco per Esc e per i comandi)
            s.selKind    = *reinterpret_cast<uint32_t*>(s.selManager + 0xf0 + 0x8);
            s.selOwn     = *reinterpret_cast<uint32_t*>(s.selManager + 0xf0 + 0xc);
            s.selCount   = *reinterpret_cast<uint32_t*>(s.selManager + 0xf0 + 0x1c);
            s.pendingCmd = *reinterpret_cast<uint32_t*>(s.selManager + 0x11c);
        }
        // avvisi (0.8.0): gestore degli avvisi [0xa7ac04] (0x6b4a20), elenco a +0x34 e numero a +0x38; Q toglie l'ultimo
        // (0x6b6f10). Il numero che cresce = avviso nuovo.
        if (const uint32_t notes = *reinterpret_cast<uint32_t*>(0xa7ac04)) {
            const uint32_t n = *reinterpret_cast<uint32_t*>(notes + 0x38);
            s.alerts = n < 10000 ? n : 0;
        }
        s.ok = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        s.ok = false;
    }
    return s.ok;
}

// --- Finestre dell'interfaccia e barra comandi (0.5.0) -------------------------------------------------------------
// Finestra (CWindow, verificato in 0x631d60, 0x631e40, 0x632750): +0x0c genitore, +0x10 rettangolo in short x0 y0 x1 y1
// (coordinate del buffer, destra e basso compresi), +0x18 fratello successivo, +0x1c precedente, +0x20 primo figlio (quello
// in cima: 0x631d60 inserisce in testa), +0x24 ultimo, +0x28 flag (bit 2 = visibile). La radice tiene il fuoco a +0x368.
// Classe: metodo +4 del sotto-oggetto a obj + 4 + [[obj+4]+4], argomento il puntatore al nome (come 0x663170); restituisce
// il puntatore convertito o 0.
namespace {

constexpr uintptr_t kNameButton    = 0x829df4;   // puntatori ai nomi delle classi [R]
constexpr uintptr_t kNameCombo     = 0x829df8;
constexpr uintptr_t kNameImgButton = 0x829e14;
constexpr uintptr_t kNameSList     = 0x829e18;
constexpr uintptr_t kNameScroll    = 0x82a300;
constexpr uintptr_t kBarClick      = 0x6b3500;   // CUICmdBar: clic su un comando (thiscall, ECX = barra, indice) [R]

using IsKindOf_t = uint32_t(__fastcall*)(uint32_t self, uint32_t edx, const char* name);
using BarClick_t = void(__fastcall*)(uint32_t bar, uint32_t edx, int id);

uint32_t U32(uintptr_t a) { return *reinterpret_cast<const uint32_t*>(a); }

bool IsKind(uint32_t w, uintptr_t namePtr)
{
    const uint32_t sub = w + 4 + U32(U32(w + 4) + 4);
    const auto fn = reinterpret_cast<IsKindOf_t>(U32(U32(sub) + 4));
    return fn(sub, 0, reinterpret_cast<const char*>(U32(namePtr))) != 0;
}

int Classify(uint32_t w)
{
    if (IsKind(w, kNameButton) || IsKind(w, kNameImgButton)) return UiButton;
    if (IsKind(w, kNameCombo)) return UiCombo;
    if (IsKind(w, kNameSList)) return UiList;
    if (IsKind(w, kNameScroll)) return UiSlider;
    return -1;
}

// Visita le finestre visibili sotto 'top' (senza entrare nelle caselle trovate: i pulsanti interni di una lista o di una
// casella a tendina non contano) e raccoglie le caselle.
int Collect(uint32_t top, UiControl* out, int max)
{
    int n = 0, steps = 0;
    uint32_t w = U32(top + 0x20);
    while (w && ++steps < 4000) {
        bool descend = false;
        if (U32(w + 0x28) & 2) {
            const int kind = Classify(w);
            const short* r = reinterpret_cast<const short*>(w + 0x10);
            const int width = r[2] + 1 - r[0], height = r[3] + 1 - r[1];
            // le barre di scorrimento verticali (accanto alle liste) non sono caselle: si tengono solo i cursori orizzontali
            const bool useful = kind >= 0 && width > 4 && height > 4 && !(kind == UiSlider && width <= height);
            if (useful && n < max) {
                out[n].r = { r[0], r[1], r[2] + 1, r[3] + 1 };
                out[n].kind = kind;
                ++n;
            }
            descend = kind < 0;
        }
        if (descend && U32(w + 0x20)) { w = U32(w + 0x20); continue; }
        while (w && w != top && !U32(w + 0x18)) w = U32(w + 0x0c);
        if (!w || w == top) break;
        w = U32(w + 0x18);
    }
    return n;
}

uint32_t Bar()
{
    const uint32_t sel = U32(kSelManager);
    const uint32_t bar = sel ? U32(sel + 0x134) : 0;   // CUICmdBar creata da 0x5e2230 con 0x6b3ab0
    return bar && (U32(bar + 0x28) & 2) ? bar : 0;
}

} // namespace

int GameUiControls(UiControl* out, int max, uint32_t* top)
{
    *top = 0;
    if (!g_knownExe || !g_cfg.memory) return 0;
    __try {
        const uint32_t root = U32(kUIRoot);
        if (!root) return 0;
        // finestra principale di quella con il fuoco: si sale finche' il genitore e' la radice
        uint32_t w = U32(root + kRootFocus);
        for (int i = 0; w && i < 64; ++i) {
            const uint32_t parent = U32(w + 0x0c);
            if (parent == root) break;
            w = parent;
        }
        if (w && U32(w + 0x0c) == root && w != U32(kMapView) && (U32(w + 0x28) & 2)) {
            *top = w;
            return Collect(w, out, max);
        }
        // senza fuoco utile: tutte le finestre principali visibili tranne la vista della mappa
        int n = 0;
        for (uint32_t t = U32(root + 0x20), i = 0; t && i < 256 && n < max; t = U32(t + 0x18), ++i)
            if (t != U32(kMapView) && (U32(t + 0x28) & 2)) n += Collect(t, out + n, max - n);
        *top = root;
        return n;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// Pulsanti della barra: vettore a +0x78..+0x7c, per pulsante visibile bit 2 a +0x28, rettangolo a +0x10, indice del
// comando a +0x80 [R]. Lo stesso ordine lo usa Resurrected per i tasti a griglia.
int GameBarButtons(RECT* rects, int* ids, int max)
{
    if (!g_knownExe || !g_cfg.memory) return 0;
    int n = 0;
    __try {
        const uint32_t bar = Bar();
        if (!bar) return 0;
        const uint32_t begin = U32(bar + 0x78), end = U32(bar + 0x7c);
        if (!begin || end < begin || (end - begin) / 4 > 64) return 0;
        for (uint32_t p = begin; p < end && n < max; p += 4) {
            const uint32_t b = U32(p);
            if (!b || !(U32(b + 0x28) & 2)) continue;
            const short* r = reinterpret_cast<const short*>(b + 0x10);
            rects[n] = { r[0], r[1], r[2] + 1, r[3] + 1 };
            ids[n] = static_cast<int>(U32(b + 0x80));
            ++n;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return n;
}

// Menu "senza selezione" (0.8.3): quando non e' selezionato nulla, al posto della barra il gioco mostra la finestra a
// [gestore della selezione + 0x150] [R: EmptyMenu in controls.cpp di Resurrected], creata da 0x6cc5b0 con
// gameini/cmdbar/empty_<civilta'>.ini: una fila di nove pulsanti CUIButton, da sinistra Guida 0x1008, Diplomazia 0x1005,
// Salvataggio rapido 0x1003, Seleziona gruppo 0x1009, Obiettivi 0x1002, Caricamento rapido 0x1004, Menu principale 0x1001,
// Conversazione 0x1006, Mappa strategica 0x1007. Il figlio con un id si trova col metodo +0x7c della finestra (id, nome
// della classe), come fa Resurrected; i clic li gestisce il metodo +0x50 (0x6ccc80, messaggio 0x12345604).
int GameEmptyMenuButtons(RECT* rects, int* ids, int max)
{
    static const int kIds[9] = { 0x1008, 0x1005, 0x1003, 0x1009, 0x1002, 0x1004, 0x1001, 0x1006, 0x1007 };
    if (!g_knownExe || !g_cfg.memory) return 0;
    int n = 0;
    __try {
        const uint32_t sel = U32(kSelManager);
        const uint32_t menu = sel ? U32(sel + 0x150) : 0;
        if (!menu || !(U32(menu + 0x28) & 2)) return 0;
        const short* m = reinterpret_cast<const short*>(menu + 0x10);
        if (m[0] <= 0 && m[1] <= 0) return 0;   // non ancora sistemata sullo schermo
        using GetChild_t = uint32_t(__fastcall*)(uint32_t self, uint32_t edx, int id, const char* cls);
        const auto getChild = reinterpret_cast<GetChild_t>(U32(U32(menu) + 0x7c));
        for (int id : kIds) {
            if (n >= max) break;
            const uint32_t b = getChild(menu, 0, id, reinterpret_cast<const char*>(U32(kNameButton)));
            if (!b || !(U32(b + 0x28) & 2)) continue;
            const short* r = reinterpret_cast<const short*>(b + 0x10);
            if (r[2] <= r[0] || r[3] <= r[1]) continue;
            rects[n] = { r[0], r[1], r[2] + 1, r[3] + 1 };
            ids[n] = id;
            ++n;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    // da sinistra a destra, come i posti della barra (nei file del gioco sono gia' in quest'ordine)
    for (int i = 1; i < n; ++i)
        for (int j = i; j > 0 && rects[j].left < rects[j - 1].left; --j) {
            std::swap(rects[j], rects[j - 1]);
            std::swap(ids[j], ids[j - 1]);
        }
    return n;
}

// --- Trucchi di Imperivm Resurrected (0.9.0) -------------------------------------------------------------------------
// Il gioco non ha trucchi suoi. Resurrected 1.0.3 li offre in chat (/gold, /level...) e li manda a tutti come comando di
// chat del gioco con il testo "#CHEAT:verbo:valore:x,y"; ogni macchina lo riconosce (CheatsHandleCommand), controlla che
// la stanza li permetta ("Allow cheats") e lo esegue come script del gioco, quindi resta sincronizzato anche in rete.
// ImperiumPad mette in coda lo stesso comando, con codice suo: 0x4e9570 crea il messaggio di chat (40 byte, stringa a
// +0xc: testo a +4 della stringa, lunghezza +0x14, capacita' +0x18), 0x401ec0 le assegna il testo (thiscall, testo,
// lunghezza), il metodo 3 gli da' il giocatore locale [partita+0x12c8], 0x51d3c0 lo mette nella coda della partita
// (thiscall, ECX = partita) [R: QueueCheat in cheats.cpp]. Riferimento: la posizione (+0x24, +0x28) del primo oggetto
// selezionato, dalla tabella degli handle 0xa2a610 [R]. Sulle unita' selezionate: "#CHEAT:s<lettera>:<valore>:<handle>"
// (h cura, f viveri, l livello, k elimina), fino a 15 handle per comando, 3 caratteri ciascuno; Resurrected accetta solo
// testi fino a 64 caratteri. Tutto nel thread del gioco: la coda manda un comando ogni 130 ms, come fa Resurrected.
namespace {

constexpr uintptr_t kCmdChatCreate = 0x4e9570;
constexpr uintptr_t kStringAssign  = 0x401ec0;
constexpr uintptr_t kQueueCommand  = 0x51d3c0;
constexpr uintptr_t kHandleTable   = 0xa2a610;

std::vector<std::string> g_cheatQueue;
DWORD g_cheatLast = 0;

bool SendCheat(const std::string& text)
{
    if (text.size() > 64) return false;
    __try {
        const uint32_t game = U32(kGame);
        const uint32_t player = game ? U32(game + 0x12c8) : 0;
        if (!player) return false;
        using Create_t = uint32_t(__cdecl*)();
        using Assign_t = void(__fastcall*)(uint32_t self, uint32_t edx, const char* text, unsigned len);
        using This_t = void(__fastcall*)(uint32_t self, uint32_t edx, uint32_t arg);
        const uint32_t cmd = reinterpret_cast<Create_t>(kCmdChatCreate)();
        if (!cmd) return false;
        reinterpret_cast<Assign_t>(kStringAssign)(cmd + 0xc, 0, text.c_str(), static_cast<unsigned>(text.size()));
        reinterpret_cast<This_t>(U32(U32(cmd) + 3 * 4))(cmd, 0, player);
        reinterpret_cast<This_t>(kQueueCommand)(game, 0, cmd);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Oggetti selezionati: handle e posizione del primo (lista a +0xf0+0x18, nodo +8 = handle).
int ReadSelection(std::vector<uint16_t>& handles, int& x, int& y)
{
    handles.clear();
    x = y = -1;
    __try {
        const uint32_t sel = U32(kSelManager);
        if (!sel) return 0;
        const uint32_t head = U32(sel + 0xf0 + 0x18);
        const int count = static_cast<int>(U32(sel + 0xf0 + 0x1c));
        if (!head || count <= 0 || count > 2000) return 0;
        uint32_t node = U32(head);
        for (int guard = 0; node && node != head && guard < count + 8 && handles.size() < 300; ++guard) {
            const uint16_t h = *reinterpret_cast<const uint16_t*>(node + 8);
            const uint32_t obj = U32(kHandleTable + h * 4u);
            if (obj) {
                const int ox = static_cast<int>(U32(obj + 0x24)), oy = static_cast<int>(U32(obj + 0x28));
                if (ox >= 0 && oy >= 0 && ox < 4000000 && oy < 4000000) {
                    if (x < 0) { x = ox; y = oy; }
                    handles.push_back(h);
                }
            }
            node = U32(node);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        handles.clear();
    }
    return static_cast<int>(handles.size());
}

// Script del gioco eseguito subito, solo su questa macchina (0.10.1): metodo +0xa8 della partita (thiscall, testo), lo
// stesso con cui ogni macchina esegue i trucchi di Resurrected [R: RunScript in cheats.cpp]. Non passa dalla coda dei
// comandi, quindi in rete la partita si desincronizzerebbe: mai con la partita multigiocatore (vtable 0x7c2858).
bool RunScript(const std::string& text)
{
    __try {
        const uint32_t game = U32(kGame);
        if (!game || U32(game) == 0x7c2858) return false;
        using Run_t = void(__fastcall*)(uint32_t self, uint32_t edx, const char* text);
        reinterpret_cast<Run_t>(U32(U32(game) + 0xa8))(game, 0, text.c_str());
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Posizione dell'oggetto di un handle (+0x24, +0x28), se c'e'.
bool ObjectPos(uint16_t h, int& x, int& y)
{
    __try {
        const uint32_t obj = U32(kHandleTable + h * 4u);
        if (!obj) return false;
        x = static_cast<int>(U32(obj + 0x24));
        y = static_cast<int>(U32(obj + 0x28));
        return x >= 0 && y >= 0 && x < 4000000 && y < 4000000;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Trucchi classici di Imperivm (0.10.1), quelli della console del gioco, eseguiti come script: "@bonus" = AddBonus(200,
// 200, 60, 60, 20000) alle unita' selezionate (Unit::AddBonus, 0x5e0ce0; i valori sono quelli del trucco conosciuto,
// "quasi imbattibili"). Numeri: 1 attacco, 2 difesa (dalle avventure del gioco, 0.10.2); il binding mette il primo a +8
// della voce del bonus, il secondo a +0x14, il terzo a +0x18 e il quarto due volte (+0xc, +0x10: sembra la vita, nella
// mappa di LukkyFrost l'eroe invincibile ha 39990), il quinto non si vede usato. raggiunte come fa Resurrected con un cerchio di 6
// intorno alla posizione di ognuna; "@fog" = ToggleFog (nebbia di guerra accesa o spenta, funzione della console
// registrata da 0x6aa500). La velocita' 10x passa invece da GameTick (TickSpeedStep(10)).
int LocalCheat(const char* verb, int value, const std::vector<uint16_t>& handles)
{
    if (strcmp(verb, "@fog") == 0) {
        const bool ok = RunScript("ToggleFog();");
        Log("Trucco classico: ToggleFog %s", ok ? "eseguito" : "NON eseguito (partita in rete o errore)");
        return ok ? 1 : 0;
    }
    const bool atk = strcmp(verb, "@atk") == 0, def = strcmp(verb, "@def") == 0;
    if (strcmp(verb, "@bonus") == 0 || atk || def) {
        // 0.10.2: "@atk" / "@def" = solo attacco / solo difesa (primo / secondo numero, valore da 1 a 1000)
        const int v = (std::max)(1, (std::min)(1000, value));
        const std::string args = atk ? std::to_string(v) + ", 0, 0, 0, 0" : def ? "0, " + std::to_string(v) + ", 0, 0, 0" : "200, 200, 60, 60, 20000";
        std::string body;
        int done = 0, sent = 0;
        auto flush = [&]() {
            if (body.empty()) return;
            if (RunScript("ObjList ol; int k; " + body)) ++sent;
            body.clear();
        };
        for (uint16_t h : handles) {
            int x = 0, y = 0;
            if (!ObjectPos(h, x, y)) continue;
            body += "ol = ObjsInCircle(Point(" + std::to_string(x) + ", " + std::to_string(y) + "), 6, \"Unit\").GetObjList(); "
                    "for (k = 0; k < ol.count; k += 1) { if (ol[k].IsValid()) { ol[k].AsUnit().AddBonus(" + args + "); } } ";
            ++done;
            if (body.size() > 1400) flush();
        }
        flush();
        Log("Trucco classico: AddBonus(%s) a %d unita' selezionate (%d script)", args.c_str(), done, sent);
        return sent;
    }
    return 0;
}

} // namespace

int GameCheat(const char* verb, int value)
{
    if (!g_knownExe || !g_cfg.memory || !verb || !verb[0]) return 0;
    if (verb[0] == '@') {
        std::vector<uint16_t> sel;
        int sx = -1, sy = -1;
        ReadSelection(sel, sx, sy);
        return LocalCheat(verb, value, sel);
    }
    static const char kAlpha[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+-";
    std::vector<uint16_t> handles;
    int x = -1, y = -1;
    ReadSelection(handles, x, y);
    const std::string v = value > 0 ? std::to_string(value) : std::string();
    int queued = 0;
    if (verb[0] == 's' && verb[1] && !verb[2]) {
        // sulle unita' selezionate, a gruppi di 15 handle
        for (size_t i = 0; i < handles.size(); i += 15) {
            std::string text = std::string("#CHEAT:") + verb + ":" + v + ":";
            for (size_t j = i; j < handles.size() && j < i + 15; ++j) {
                const unsigned h = handles[j];
                text += kAlpha[(h >> 12) & 63];
                text += kAlpha[(h >> 6) & 63];
                text += kAlpha[h & 63];
            }
            g_cheatQueue.push_back(text);
            ++queued;
        }
    }
    else {
        std::string text = std::string("#CHEAT:") + verb + ":" + v;
        if (x >= 0) text += ":" + std::to_string(x) + "," + std::to_string(y);
        g_cheatQueue.push_back(text);
        ++queued;
    }
    Log("Trucco \"%s\" valore %d: %d comandi in coda (%u oggetti selezionati, riferimento %d,%d)", verb, value, queued,
        static_cast<unsigned>(handles.size()), x, y);
    return queued;
}

// "Crea truppe" (0.10.0): "#CHEAT:spawn:<classe>,<numero>:x,y"; Resurrected controlla che la classe esista e le fa uscire
// dall'edificio proprio piu' vicino al punto (quello selezionato), fino a 100 per volta.
int GameCheatSpawn(const char* cls, int count)
{
    if (!g_knownExe || !g_cfg.memory || !cls || !cls[0]) return 0;
    for (const char* c = cls; *c; ++c)
        if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || *c == '_' || *c == '-')) return 0;
    std::vector<uint16_t> handles;
    int x = -1, y = -1;
    ReadSelection(handles, x, y);
    if (x < 0) return 0;
    if (count < 1) count = 1;
    if (count > 100) count = 100;
    const std::string text = std::string("#CHEAT:spawn:") + cls + "," + std::to_string(count) + ":" + std::to_string(x) + "," + std::to_string(y);
    if (text.size() > 64) return 0;
    g_cheatQueue.push_back(text);
    Log("Trucco \"crea truppe\": %s x%d dall'edificio a %d,%d", cls, count, x, y);
    return 1;
}

// Prossimo insediamento (0.10.0): nella tabella degli handle si cercano gli oggetti del giocatore locale (proprietario
// a +0x70 [R: civai.cpp]) la cui classe (+0x3c) e' un "BaseTownhall" (metodo IsKindOf per nome 0x59c020 [R]); la camera
// va sul successivo con CenterOn 0x627300 (thiscall sul gestore della vista [0x9edd18], punto in coordinate della vista:
// x come nel mondo, y * 0xb5 / 256, senza l'altezza del terreno) [R: SetCamera in spectator.cpp, ProjectedRect].
namespace {
using IsKindName_t = int(__fastcall*)(uint32_t cls, uint32_t edx, const char* name);
using CenterOn_t = void(__fastcall*)(uint32_t view, uint32_t edx, int* pos, int x);
uint16_t g_lastTownhall = 0;

bool TownhallAt(uint32_t index, uint32_t player, int& x, int& y)
{
    __try {
        const uint32_t obj = U32(kHandleTable + index * 4u);
        if (!obj || U32(obj + 0x70) != player) return false;
        const uint32_t cls = U32(obj + 0x3c);
        if (!cls || !reinterpret_cast<IsKindName_t>(0x59c020)(cls, 0, "BaseTownhall")) return false;
        x = static_cast<int>(U32(obj + 0x24));
        y = static_cast<int>(U32(obj + 0x28));
        return x >= 0 && y >= 0 && x < 4000000 && y < 4000000;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
uint32_t LocalPlayer()
{
    __try {
        const uint32_t game = U32(kGame);
        return game ? U32(game + 0x12c8) : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool CenterCamera(int x, int y)
{
    int pos[2] = { x, static_cast<int>((static_cast<long long>(y) * 0xb5) >> 8) };
    __try {
        const uint32_t view = U32(0x9edd18);
        if (!view) return false;
        reinterpret_cast<CenterOn_t>(0x627300)(view, 0, pos, 0);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
} // namespace

int GameNextSettlement(int& index, int& total)
{
    index = total = 0;
    if (!g_knownExe || !g_cfg.memory) return 0;
    const uint32_t player = LocalPlayer();
    if (!player) return 0;
    struct Town { uint16_t h; int x, y; };
    std::vector<Town> towns;
    for (uint32_t i = 1; i < 65536; ++i) {
        int x, y;
        if (TownhallAt(i, player, x, y)) towns.push_back({ static_cast<uint16_t>(i), x, y });
    }
    total = static_cast<int>(towns.size());
    if (towns.empty()) { Log("Prossimo insediamento: nessun municipio del giocatore trovato"); return 0; }
    size_t pick = 0;
    for (size_t k = 0; k < towns.size(); ++k) if (towns[k].h > g_lastTownhall) { pick = k; break; }
    if (towns[pick].h <= g_lastTownhall) pick = 0;
    g_lastTownhall = towns[pick].h;
    index = static_cast<int>(pick) + 1;
    if (!CenterCamera(towns[pick].x, towns[pick].y)) {
        Log("Prossimo insediamento: ERRORE nello spostamento della camera");
        return 0;
    }
    Log("Prossimo insediamento: %d di %d (handle %u, %d,%d)", index, total, towns[pick].h, towns[pick].x, towns[pick].y);
    return 1;
}

void GameCheatTick()
{
    if (g_cheatQueue.empty()) return;
    const DWORD now = GetTickCount();
    if (now - g_cheatLast < 130) return;
    g_cheatLast = now;
    const std::string text = g_cheatQueue.front();
    g_cheatQueue.erase(g_cheatQueue.begin());
    const bool ok = SendCheat(text);
    Log("Trucco: comando \"%s\" %s", text.c_str(), ok ? "messo nella coda della partita" : "NON mandato");
    if (!ok) g_cheatQueue.clear();
}

// --- Lettere della tastiera di Imperivm Resurrected sulla barra (0.5.2) ------------------------------------------
// Resurrected (winmm.dll 1.0.3, SHA-256 1624740a...) tiene l'opzione "lettere sulla barra" in un byte a +0x43e87c
// (g_barLetters di src/game/controls.cpp): lo legge il calcolo delle lettere (+0x53907, cmp byte [g], 0), lo carica
// dall'ini con LettersOnBar (+0x5a6dd, setne) e lo salva (+0x5bad0); i cinque riferimenti nel codice sono tutti a quel
// byte. Il pad lo azzera mentre e' attivo e poi rimette il valore di prima: Resurrected toglie da solo le lettere
// dall'immagine (ControlsBarKeysChanged). Prima di scrivere si controlla che nel codice ci siano proprio le due istruzioni
// "cmp byte ptr [g], 0" a +0x53907 e +0x5bad0 con l'indirizzo giusto: con un'altra versione non si tocca nulla.
namespace {
uint8_t* g_letters = nullptr;     // byte di Resurrected, se riconosciuto
int      g_lettersState = 0;      // 0 da cercare, 1 trovato, -1 non trovato
bool     g_lettersHidden = false;
uint8_t  g_lettersSaved = 1;

bool CodeCmpByte(const uint8_t* base, uint32_t rva, uint32_t target)
{
    const uint8_t* p = base + rva;
    return p[0] == 0x80 && p[1] == 0x3d && *reinterpret_cast<const uint32_t*>(p + 2) == target && p[6] == 0x00;
}

uint8_t* FindLetters()
{
    HMODULE winmm = GetModuleHandleA("winmm.dll");
    char path[MAX_PATH] = {};
    if (!winmm || !GetModuleFileNameA(winmm, path, MAX_PATH) || _strnicmp(path, GameDir(), strlen(GameDir())) != 0) {
        Log("Lettere di Resurrected: winmm.dll di Resurrected non caricata, niente da nascondere");
        return nullptr;
    }
    const auto base = reinterpret_cast<const uint8_t*>(winmm);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const uint32_t size = nt->OptionalHeader.SizeOfImage, rva = 0x43e87c;
    const uint32_t target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base)) + rva;
    bool ok = false;
    __try { ok = size > rva && size > 0x5bad7 && CodeCmpByte(base, 0x53907, target) && CodeCmpByte(base, 0x5bad0, target); }
    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }
    if (!ok) {
        // 0.10.0: un'altra versione di Resurrected. L'opzione si carica dall'ini con il nome "LettersOnBar": nel codice c'e'
        // "push <indirizzo del nome>", la lettura, e poco dopo "setne byte ptr [opzione]" (0F 95 05 indirizzo), come nella
        // 1.0.3 a +0x5a6c5 / +0x5a6dd. Si cerca quello schema: se c'e' una volta sola e l'indirizzo sta dentro la DLL, e'
        // l'opzione; altrimenti le lettere restano.
        uint8_t* found = nullptr;
        int hits = 0;
        __try {
            const char name[] = "LettersOnBar";
            for (uint32_t i = 0x1000; i + sizeof(name) < size && !found; ++i) {
                if (memcmp(base + i, name, sizeof(name)) != 0) continue;
                const uint32_t va = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base)) + i;
                for (uint32_t c = 0x1000; c + 5 < size; ++c) {
                    if (base[c] != 0x68 || *reinterpret_cast<const uint32_t*>(base + c + 1) != va) continue;
                    for (uint32_t k = c + 5; k < c + 64 && k + 7 < size; ++k) {
                        if (base[k] != 0x0f || base[k + 1] != 0x95 || base[k + 2] != 0x05) continue;
                        const uint32_t addr = *reinterpret_cast<const uint32_t*>(base + k + 3);
                        const uint32_t lo = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base));
                        if (addr > lo && addr < lo + size) {
                            uint8_t* cand = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(addr));
                            if (found != cand) ++hits;
                            found = cand;
                        }
                        break;
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { found = nullptr; }
        if (found && hits == 1) {
            Log("Lettere di Resurrected: versione diversa dalla 1.0.3, opzione trovata dal codice che legge LettersOnBar "
                "(winmm.dll+0x%x, valore %u)", static_cast<unsigned>(found - base), *found);
            return found;
        }
        Log("Lettere di Resurrected: versione di winmm.dll diversa dalla 1.0.3 (%s) e opzione non riconosciuta (%d candidati), "
            "le lettere restano", path, hits);
        return nullptr;
    }
    Log("Lettere di Resurrected: opzione trovata in winmm.dll+0x%x (valore %u)", rva, base[rva]);
    return const_cast<uint8_t*>(base) + rva;
}
} // namespace

// 0.10.0: per i trucchi basta Resurrected di qualsiasi versione (la winmm.dll della cartella del gioco con il nome
// "Imperivm Resurrected" nelle informazioni di versione): il formato "#CHEAT:" non dipende dalla versione, e se cambiasse
// Resurrected scarterebbe il comando scrivendolo nel suo log.
bool GameResurrectedLoaded()
{
    static int state = 0;
    if (state) return state > 0;
    HMODULE winmm = GetModuleHandleA("winmm.dll");
    char path[MAX_PATH] = {};
    if (!winmm || !GetModuleFileNameA(winmm, path, MAX_PATH) || _strnicmp(path, GameDir(), strlen(GameDir())) != 0) { state = -1; return false; }
    bool named = false;
    DWORD dummy = 0;
    const DWORD sz = GetFileVersionInfoSizeA(path, &dummy);
    if (sz) {
        std::vector<char> data(sz);
        if (GetFileVersionInfoA(path, 0, sz, data.data())) {
            // cerca il nome del prodotto in tutto il blocco delle informazioni di versione (UTF-16)
            const wchar_t* want = L"Imperivm Resurrected";
            const size_t wl = wcslen(want) * 2;
            for (size_t i = 0; i + wl <= data.size() && !named; ++i) named = memcmp(data.data() + i, want, wl) == 0;
        }
    }
    state = named ? 1 : -1;
    Log("Resurrected per i trucchi: %s", named ? "si' (winmm.dll di Imperivm Resurrected)" : "no");
    return named;
}

void GameResurrectedLetters(bool hide)
{
    if (hide == g_lettersHidden) return;
    if (!g_lettersState) {
        g_letters = FindLetters();
        g_lettersState = g_letters ? 1 : -1;
    }
    if (g_lettersState < 0) { g_lettersHidden = hide; return; }
    __try {
        if (hide) {
            g_lettersSaved = *g_letters;
            *g_letters = 0;
        }
        else if (*g_letters == 0) {
            *g_letters = g_lettersSaved;   // se nel frattempo l'opzione e' stata cambiata da Resurrected, si lascia com'e'
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        g_lettersState = -1;
    }
    g_lettersHidden = hide;
    Log("Lettere della tastiera di Resurrected sulla barra: %s", hide ? "nascoste (pad attivo)" : "di nuovo visibili");
}

// 0x6b3500 controlla da se' che il comando sia nell'elenco della barra (+0x98), costi e requisiti, poi lo esegue o lo mette
// in attesa del bersaglio: e' la stessa strada del clic del giocatore (0x6b4360, messaggio 0x12345604).
// --- Comandi, gruppi e camera (0.6.0) -------------------------------------------------------------------------------
// Stringhe del gioco (std::string di MSVC 2005): testo a +4 (dentro se la capacita' a +0x18 e' < 16, altrimenti
// puntatore), lunghezza a +0x14 [R: RawString in controls.cpp di Resurrected; la 0.5.3 leggeva a +0/+0x10, sbagliato].
// Descrittore di un comando: metodo +0x10 del registro [0x9969dc] con l'indice (come 0x6b3500); nome interno a +0x08,
// titolo italiano con i marcatori a +0x80, tasto a +0x1c4 [R].
namespace {

int ReadGameString(uint32_t obj, char* out, int cap)
{
    const uint32_t size = U32(obj + 0x14), capacity = U32(obj + 0x18);
    if (size > capacity || size > 4096) return 0;
    const char* text = capacity < 16 ? reinterpret_cast<const char*>(obj + 4) : reinterpret_cast<const char*>(U32(obj + 4));
    int n = 0;
    for (; n < cap - 1 && n < static_cast<int>(size); ++n) out[n] = text[n];
    out[n] = 0;
    return n;
}

uint32_t Descriptor(int id)
{
    const uint32_t reg = U32(0x9969dc);
    if (!reg) return 0;
    using Lookup_t = uint32_t(__fastcall*)(uint32_t self, uint32_t edx, int id);
    return reinterpret_cast<Lookup_t>(U32(U32(reg) + 0x10))(reg, 0, id);
}

} // namespace

bool GameBarClick(int id, char* name, int cap)
{
    if (cap > 0) name[0] = 0;
    if (!g_knownExe || !g_cfg.memory) return false;
    __try {
        const uint32_t bar = Bar();
        if (!bar) return false;
        if (const uint32_t desc = Descriptor(id)) if (cap > 0) ReadGameString(desc + 0x08, name, cap);
        reinterpret_cast<BarClick_t>(kBarClick)(bar, 0, id);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Titolo del comando senza i marcatori (<b>, <imagetransp ...>): il testo fuori dai marcatori fino al primo marcatore
// dopo il testo, come TitleText di Resurrected. Testi del gioco in Windows-1252.
bool GameCommandTitle(int id, char* out, int cap)
{
    if (cap > 0) out[0] = 0;
    if (!g_knownExe || !g_cfg.memory || cap < 2) return false;
    char raw[512];
    __try {
        const uint32_t desc = Descriptor(id);
        if (!desc || !ReadGameString(desc + 0x80, raw, sizeof(raw))) return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    int n = 0;
    bool tag = false;
    for (const char* c = raw; *c && n < cap - 1; ++c) {
        if (*c == '<') { if (n) break; tag = true; }
        else if (*c == '>') tag = false;
        else if (!tag && !(n == 0 && *c == ' ')) out[n++] = *c;
    }
    while (n && out[n - 1] == ' ') --n;
    out[n] = 0;
    return n > 0;
}

// Nome interno (+0x08) e parametro (+0x40) del descrittore di un comando [R: kDescName, kDescParam in cmdcheck.cpp di
// Resurrected]; per i comandi di addestramento ("train...") il parametro e' la classe dell'unita' (0.10.0).
bool GameCommandParam(int id, char* out, int cap)
{
    if (cap > 0) out[0] = 0;
    if (!g_knownExe || !g_cfg.memory || cap < 2) return false;
    __try {
        const uint32_t desc = Descriptor(id);
        return desc && ReadGameString(desc + 0x40, out, cap) > 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GameCommandName(int id, char* out, int cap)
{
    if (cap > 0) out[0] = 0;
    if (!g_knownExe || !g_cfg.memory || cap < 2) return false;
    __try {
        const uint32_t desc = Descriptor(id);
        return desc && ReadGameString(desc + 0x08, out, cap) > 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Gruppi del giocatore locale: oggetti di selezione a [partita+0x12c8] + 0xac + n * 0x2c, n = cifra del tasto (0x5e8450,
// chiamata da 0x5e8ec0 con tasto - '0'); numero di elementi a +0x1c come nella selezione a +0xf0 [R].
bool GameGroupCounts(int counts[10])
{
    for (int i = 0; i < 10; ++i) counts[i] = 0;
    if (!g_knownExe || !g_cfg.memory) return false;
    __try {
        const uint32_t game = U32(kGame);
        const uint32_t player = game ? U32(game + 0x12c8) : 0;
        if (!player) return false;
        for (int i = 0; i < 10; ++i) {
            const uint32_t n = U32(player + 0xac + i * 0x2c + 0x1c);
            counts[i] = n < 10000 ? static_cast<int>(n) : 0;
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Contenuto dei gruppi e della selezione (0.10.1, radiale dei gruppi): per ogni oggetto della lista (come in
// GameSelectedHero) si contano gli eroi (classe CVXHero) e si somma la salute (+0xc0) e la salute massima (+0xc8).
namespace {

void ReadGroup(uint32_t list, GroupInfo& g)
{
    g = GroupInfo{};
    __try {
        const uint32_t n = U32(list + 0x1c), head = U32(list + 0x18);
        if (!head || !n || n > 2000) return;
        g.count = static_cast<int>(n);
        long long hp = 0, max = 0;
        uint32_t node = U32(head);
        for (uint32_t guard = 0; node && node != head && guard < n + 8; ++guard) {
            const uint16_t h = *reinterpret_cast<const uint16_t*>(node + 8);
            const uint32_t obj = U32(0xa2a610 + h * 4u);
            if (obj) {
                if (IsKind(obj, 0x821294)) ++g.heroes;
                const int a = static_cast<int>(U32(obj + 0xc0)), b = static_cast<int>(U32(obj + 0xc8));
                if (b > 0 && b < 1000000 && a >= 0 && a <= b) { hp += a; max += b; }
            }
            node = U32(node);
        }
        g.hp = max > 0 ? static_cast<int>(hp * 100 / max) : -1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        g = GroupInfo{};
    }
}

} // namespace

bool GameGroupInfo(GroupInfo groups[11])
{
    for (int i = 0; i < 11; ++i) groups[i] = GroupInfo{};
    if (!g_knownExe || !g_cfg.memory) return false;
    uint32_t player = 0, sel = 0;
    __try {
        const uint32_t game = U32(kGame);
        player = game ? U32(game + 0x12c8) : 0;
        sel = U32(kSelManager);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (!player) return false;
    for (int i = 0; i < 10; ++i) ReadGroup(player + 0xac + i * 0x2c, groups[i]);
    if (sel) ReadGroup(sel + 0xf0, groups[10]);
    return true;
}

// Camera: lo stesso passo del timer di scorrimento del gioco (0x5b8100, evento 0x19a87453), con lo spostamento deciso
// da noi. Il timer: velocita' = min + (max - min) * opzione / 100 (0x824688, 0x82468c, opzione 0x82ef7c), passo
// allineato a 0x8246a0 e al massimo 0x824690; rettangolo della vista con 0x627470 (vista [0x996ff0]), spostato, poi
// 0x6265c0 sul gestore della visibilita' [vista+0x30] con la superficie (metodo +0x40 della vista; se vero,
// [[[vista+8]+0x30]+0x20]); infine, se c'e', l'oggetto [0x97d430] riceve lo spostamento (metodo +0x9c). Il timer non
// lavora se [0x996ff8] e' acceso o se la mappa grande e' aperta ([0x9d2cc0] metodo +0x34): idem qui.
// SOLO nel thread del gioco. dx, dy in pixel, gia' allineati. Restituisce false se la camera ora non si puo' muovere.
bool GameCameraMove(int dx, int dy)
{
    if (!g_knownExe || !g_cfg.memory) return false;
    __try {
        const uint32_t view = U32(kMapView);
        if (!view || U32(0x996ff8)) return false;
        if (const uint32_t zoom = U32(0x9d2cc0)) {
            using Open_t = int(__fastcall*)(uint32_t self, uint32_t edx);
            if (reinterpret_cast<Open_t>(U32(U32(zoom) + 0x34))(zoom, 0)) return false;
        }
        if (!dx && !dy) return true;
        int rect[4];
        using GetRect_t = void(__fastcall*)(uint32_t self, uint32_t edx, int* rect);
        reinterpret_cast<GetRect_t>(0x627470)(view, 0, rect);
        rect[0] += dx; rect[1] += dy; rect[2] += dx; rect[3] += dy;
        using Flag_t = int(__fastcall*)(uint32_t self, uint32_t edx, int arg);
        const int flag = reinterpret_cast<Flag_t>(U32(U32(view) + 0x40))(view, 0, 0);
        const uint32_t surface = flag ? U32(U32(U32(view + 8) + 0x30) + 0x20) : 0;
        using SetRect_t = void(__fastcall*)(uint32_t self, uint32_t edx, int* rect, uint32_t surface);
        reinterpret_cast<SetRect_t>(0x6265c0)(U32(view + 0x30), 0, rect, surface);
        if (const uint32_t other = U32(0x97d430)) {
            int delta[2] = { dx, dy };
            using Notify_t = void(__fastcall*)(uint32_t self, uint32_t edx, int* delta);
            reinterpret_cast<Notify_t>(U32(U32(other) + 0x9c))(other, 0, delta);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Velocita' dello scorrimento con le frecce (pixel al secondo), allineamento e passo massimo del gioco.
bool GameScrollParams(int& speed, int& align, int& stepMax)
{
    if (!g_knownExe || !g_cfg.memory) return false;
    __try {
        const int mn = static_cast<int>(U32(0x824688)), mx = static_cast<int>(U32(0x82468c)), opt = static_cast<int>(U32(0x82ef7c));
        speed = mn + (mx - mn) * opt / 100;
        align = static_cast<int>(U32(0x8246a0));
        stepMax = static_cast<int>(U32(0x824690));
        if (speed < 100 || speed > 20000) speed = 1280;
        if (align < 1 || align > 64) align = 16;
        if (stepMax < align || stepMax > 512) stepMax = 64;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// --- Oggetto sotto il cursore, eroe selezionato, velocita' della partita (0.8.0; SOLO nel thread del gioco) ----------
// Oggetto sotto un punto: lo stesso calcolo del gestore della selezione per i clic (0x5ea5fe): punto della finestra del
// gioco + angolo della vista ([0x9edd18]+0x618 e +0x61c letti come short, 0x623f30), poi 0x5e1cd0(&punto), cdecl, che
// restituisce l'oggetto dei copioni se e' visibile ed esplorato (0x5e1c10) oppure 0. Sola lettura.
bool GameObjectAt(int x, int y)
{
    if (!g_knownExe || !g_cfg.memory) return false;
    __try {
        const uint32_t vis = U32(0x9edd18);
        if (!vis) return false;
        int pt[2] = { x + *reinterpret_cast<const short*>(vis + 0x618), y + *reinterpret_cast<const short*>(vis + 0x61c) };
        using At_t = uint32_t(__cdecl*)(int* point);
        return reinterpret_cast<At_t>(0x5e1cd0)(pt) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Primo elemento della selezione, se e' un eroe del giocatore locale: salute e salute massima (+0xc0, +0xc8, interi).
// Selezione: [gestore+0xf0] con la lista a +0x18 (nodo: +0 successivo, +8 handle a 16 bit) e il numero a +0x1c;
// oggetto dalla tabella degli handle [0xa2a610 + h * 4]; eroe = classe CVXHero (nome a 0x821294, come 0x59de81);
// proprietario a +0x70 uguale a [partita+0x12c8].
bool GameSelectedHero(int& handle, int& hp, int& maxHp)
{
    handle = -1; hp = maxHp = 0;
    if (!g_knownExe || !g_cfg.memory) return false;
    __try {
        const uint32_t game = U32(kGame), sel = U32(kSelManager);
        if (!game || !sel || !U32(sel + 0xf0 + 0x1c)) return false;
        const uint32_t head = U32(sel + 0xf0 + 0x18);
        const uint32_t node = head ? U32(head) : 0;
        if (!node || node == head) return false;
        const uint16_t h = *reinterpret_cast<const uint16_t*>(node + 8);
        const uint32_t obj = U32(0xa2a610 + h * 4u);
        if (!obj || U32(obj + 0x70) != U32(game + 0x12c8) || !IsKind(obj, 0x821294)) return false;
        handle = h;
        hp = static_cast<int>(U32(obj + 0xc0));
        maxHp = static_cast<int>(U32(obj + 0xc8));
        return maxHp > 0 && maxHp < 1000000 && hp >= 0 && hp <= maxHp;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Velocita' della partita: [partita+0x1244] in millesimi (1000 = normale), scritta dal comando CVXCmdSetSpeed (0x4e67c0,
// tra 1 e 100000). Partita in rete: la partita e' la classe multigiocatore, vtable 0x7c2858 [R: GameIsMultiplayer].
bool GameSpeed(int& speed, bool& multiplayer)
{
    speed = 0; multiplayer = false;
    if (!g_knownExe || !g_cfg.memory) return false;
    __try {
        const uint32_t game = U32(kGame);
        if (!game) return false;
        multiplayer = U32(game) == 0x7c2858;
        speed = static_cast<int>(U32(game + 0x1244));
        return speed >= 1 && speed <= 100000;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GameSetSpeed(int speed)
{
    if (!g_knownExe || !g_cfg.memory || speed < 1 || speed > 100000) return false;
    __try {
        const uint32_t game = U32(kGame);
        if (!game || U32(game) == 0x7c2858) return false;   // mai in rete: la velocita' passa dalla coda sincronizzata
        *reinterpret_cast<uint32_t*>(game + 0x1244) = static_cast<uint32_t>(speed);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
