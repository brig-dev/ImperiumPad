// Overlay.cpp - disegno di ImperiumPad dentro l'immagine del gioco
// Linguaggio: C++17, MSVC, Win32 (x86).
//
// gbr.exe disegna in software in un buffer proprio e lo copia nella finestra con un'unica chiamata a
// SetDIBitsToDevice (0x677c80, chiamata a 0x677f2a), un rettangolo alla volta (aggiornamenti parziali), sempre dallo
// stesso buffer intero: SetDIBitsToDevice(hdc, xDest, yDest, w, h, xSrc, H - h - ySrcAlto, 0, H, bits, bmi, uso), con
// BITMAPINFO a 0xa0a1d0. Formati: 16 bit RGB565 con maschere (superficie di tipo 6), 16 bit senza compressione cioe'
// RGB555 (quello visto sul PC con Resurrected alla prova della 0.4.0), 8 bit con palette (non gestito).
// Si aggancia quella chiamata nella tabella di import di gbr.exe. A ogni copia del gioco si disegna nel buffer solo la
// parte dei pannelli che cade nel rettangolo copiato (salvando i pixel coperti), si lascia passare la copia, poi si
// rimettono i pixel originali: il buffer del gioco resta pulito e il gioco a schermo ha sempre i pannelli sopra. Solo
// quando un pannello compare, cambia, si sposta o sparisce (o ogni mezzo secondo, per sicurezza) si copiano a parte i
// rettangoli interi dei pannelli e quelli vecchi da cancellare (0.4.2: prima lo si faceva a ogni copia, e Resurrected a
// ogni copia esegue i suoi compiti di fotogramma). Se il gioco non copia nulla (schermo fermo) e un pannello deve
// cambiare, OverlayTick chiede a Windows di ridisegnare la finestra: WM_PAINT del gioco (0x678a10) passa dalla stessa
// funzione di copia (posto 18 della vtable 0x7dbd88), come fa Resurrected con InvalidateRect. Imperivm Resurrected intercetta SetDIBitsToDevice dentro gdi32 per portare
// l'immagine in Direct3D 11: il nostro aggancio sta a monte, quindi i pannelli arrivano anche nella sua immagine.
// I testi si scrivono con GDI (antialiasing) in una DIB a 32 bit e si fondono nel buffer del gioco.
//
// 0.5.0: l'aggancio gira nel thread del gioco, quindi e' anche il posto dove leggere le finestre dei menu e la barra
// comandi (metodi virtuali e liste che solo quel thread modifica) e dove eseguire i comandi chiesti con RT + tasto.
// Oltre al riquadro e all'aiuto si disegnano i simboli del pad (stile Steam, Xbox o PlayStation) nell'angolo in basso a
// destra dei pulsanti della barra (Resurrected scrive le sue lettere in alto a sinistra).
// 0.6.0: radiali dei comandi della barra (RB tenuto) e dei gruppi (LB tenuto), costruiti qui nel thread del gioco.

#include "ImperiumPad.h"
#include "OverlayDraw.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstring>
#include <string>
#include <vector>

namespace ovl {

HFONT MakeFont(int px, bool bold)
{
    return CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

} // namespace ovl

namespace {

using namespace ovl;

using SetDIBitsToDevice_t = int (WINAPI*)(HDC, int, int, DWORD, DWORD, int, int, UINT, UINT, const VOID*, const BITMAPINFO*, UINT);
SetDIBitsToDevice_t g_realBlit = nullptr;

// --- Stato chiesto dal thread del pad -----------------------------------------------------------------------
CRITICAL_SECTION g_lock;
bool             g_lockReady = false;
std::wstring     g_badge1, g_badge2;   // riquadro informativo in alto a sinistra
DWORD            g_badgeUntil = 0;
bool             g_helpOn = false;
std::vector<std::wstring> g_helpRows;  // righe dell'aiuto: "colonna1\tcolonna2\tcolonna3"
std::wstring     g_helpTitle, g_helpFoot;
unsigned         g_version = 1;        // cambia quando cambia il contenuto: i pannelli si ricostruiscono
bool             g_padActive = false, g_padInMap = false, g_padLayer = false;
int              g_padPage = 0;        // RT + LT: simboli sui pulsanti dal tredicesimo (0.8.0)
bool             g_cursorOn = false;   // cursore disegnato (0.8.0): posizione e dimensioni dell'area client
int              g_cursorX = 0, g_cursorY = 0, g_clientW = 0, g_clientH = 0;
std::vector<RECT> g_barPad;            // copia dei pulsanti della barra per il thread del pad (calamita)
std::atomic<int> g_bufW{ 0 }, g_bufH{ 0 };       // dimensioni del buffer del gioco all'ultima copia
std::atomic<int> g_drawnCursor{ -1 };            // posizione del cursore disegnato all'ultima copia (x << 16 | y)
GlyphStyle       g_padStyle = GlyphStyle::Steam;
int              g_cmdSlot = -1;       // comando chiesto con RT + tasto, in attesa del thread del gioco
DWORD            g_cmdAt = 0;
bool             g_testBarOn = false;  // solo per tools/test_overlay
std::vector<RECT> g_testBar;
int              g_radKind = 0;        // radiale aperto: 0 nessuno, 1 comandi della barra, 2 gruppi
int              g_radHi = -1;         // voce scelta
bool             g_radAssign = false;  // gruppi: LT tenuto, si assegna invece di selezionare
std::wstring     g_radHint;            // riga di aiuto sotto il titolo (nomi dei tasti dello stile scelto)
int              g_radCount = 0;       // voci del radiale aperto lette dal thread del gioco
// caselle dei menu lette nel thread del gioco, per il thread del pad
std::vector<UiControl> g_ui;
int              g_uiW = 0, g_uiH = 0;
DWORD            g_uiTick = 0;
bool             g_uiMenu = false;     // elenco letto nei menu con il pad attivo (in mappa resta vuoto)
bool             g_uiPending = false;  // l'ultima copia non e' stata letta (lettura ogni 100 ms): serve un'altra copia

std::atomic<DWORD> g_lastHook{ 0 };    // ultima copia del gioco vista (GetTickCount)
std::atomic<int>   g_drawnState{ 0 };  // visibili all'ultima copia: bit 1 riquadro, bit 2 aiuto, bit 4 simboli
std::atomic<unsigned> g_drawnVersion{ 0 };
std::atomic<unsigned> g_hookCalls{ 0 };

// --- Pannelli pronti (solo nel thread del gioco) -----------------------------------------------------------
Panel    g_badgePanel, g_helpPanel;
unsigned g_builtVersion = 0;
int      g_builtHeight = 0;
Panel    g_glyph[kSlotCount];         // simboli del pad, uno per posto
Panel    g_cursorPanel;               // freccia del cursore disegnato
GlyphStyle g_glyphStyle = GlyphStyle::Steam;
int      g_glyphSize = 0;             // 0 = da costruire
unsigned g_glyphBuilds = 0;

struct Rect { int x = 0, y = 0, w = 0, h = 0; bool Empty() const { return w <= 0 || h <= 0; } };

// --- Costruzione dei pannelli con GDI ------------------------------------------------------------------------
struct Segment { std::wstring text; COLORREF color; int x; bool bold; };
struct Line { std::vector<Segment> segs; int height; };



// Disegna le righe in una DIB a 32 bit: testo bianco su nero, poi il colore di ogni segmento e lo sfondo
// semitrasparente diventano un'immagine ARGB.
Panel BuildPanel(const std::vector<Line>& lines, int width, int pad, int fontPx)
{
    Panel p;
    int height = pad * 2;
    for (const Line& l : lines) height += l.height;
    p.w = width;
    p.h = height;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) { DeleteDC(dc); p.w = p.h = 0; return p; }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    memset(bits, 0, static_cast<size_t>(width) * height * 4);
    std::vector<COLORREF> colorMap(static_cast<size_t>(width) * height, kWhite);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    HFONT normal = MakeFont(fontPx, false), bold = MakeFont(fontPx, true);
    int y = pad;
    for (const Line& l : lines) {
        for (const Segment& s : l.segs) {
            HGDIOBJ old = SelectObject(dc, s.bold ? bold : normal);
            RECT r{ s.x, y, width - pad, y + l.height };
            DrawTextW(dc, s.text.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT calc = r;
            DrawTextW(dc, s.text.c_str(), -1, &calc, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_SINGLELINE | DT_CALCRECT);
            for (int yy = r.top; yy < r.bottom && yy < height; ++yy)
                for (int xx = r.left; xx < calc.right + 2 && xx < width; ++xx) colorMap[static_cast<size_t>(yy) * width + xx] = s.color;
            SelectObject(dc, old);
        }
        y += l.height;
    }
    GdiFlush();
    p.argb.resize(static_cast<size_t>(width) * height);
    const auto* src = static_cast<const uint32_t*>(bits);
    for (int i = 0; i < width * height; ++i) {
        const uint32_t v = src[i];
        const int cr = static_cast<int>((v >> 16) & 0xff), cg = static_cast<int>((v >> 8) & 0xff), cb = static_cast<int>(v & 0xff);
        int ink = cr > cg ? cr : cg;          // intensita' del testo (bianco antialiasing su nero)
        if (cb > ink) ink = cb;
        const COLORREF c = colorMap[i];
        const int r = (GetRValue(kBack) * (255 - ink) + GetRValue(c) * ink) / 255;
        const int g = (GetGValue(kBack) * (255 - ink) + GetGValue(c) * ink) / 255;
        const int b = (GetBValue(kBack) * (255 - ink) + GetBValue(c) * ink) / 255;
        const int a = kBackAlpha + (255 - kBackAlpha) * ink / 255;
        p.argb[i] = (static_cast<uint32_t>(a) << 24) | (r << 16) | (g << 8) | b;
    }
    // bordo dorato sottile
    for (int x = 0; x < width; ++x) { p.argb[x] = 0xffb08c3c; p.argb[static_cast<size_t>(height - 1) * width + x] = 0xffb08c3c; }
    for (int yy = 0; yy < height; ++yy) { p.argb[static_cast<size_t>(yy) * width] = 0xffb08c3c; p.argb[static_cast<size_t>(yy) * width + width - 1] = 0xffb08c3c; }
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteObject(normal);
    DeleteObject(bold);
    DeleteDC(dc);
    return p;
}

int TextWidth(const std::wstring& s, int fontPx, bool boldFont)
{
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    HFONT f = MakeFont(fontPx, boldFont);
    HGDIOBJ old = SelectObject(dc, f);
    SIZE sz{};
    GetTextExtentPoint32W(dc, s.c_str(), static_cast<int>(s.size()), &sz);
    SelectObject(dc, old);
    DeleteObject(f);
    DeleteDC(dc);
    return sz.cx;
}

std::vector<std::wstring> SplitTabs(const std::wstring& s)
{
    std::vector<std::wstring> out;
    size_t start = 0;
    for (;;) {
        const size_t t = s.find(L'\t', start);
        out.push_back(s.substr(start, t == std::wstring::npos ? std::wstring::npos : t - start));
        if (t == std::wstring::npos) break;
        start = t + 1;
    }
    return out;
}

void BuildPanels(int bufferHeight)
{
    std::wstring b1, b2, title, foot;
    std::vector<std::wstring> rows;
    EnterCriticalSection(&g_lock);
    b1 = g_badge1; b2 = g_badge2; title = g_helpTitle; foot = g_helpFoot; rows = g_helpRows;
    LeaveCriticalSection(&g_lock);

    const int font = bufferHeight / 42 > 12 ? bufferHeight / 42 : 12;    // 25 px a 1080, 51 a 2160
    const int lineH = font * 13 / 10, pad = font * 2 / 3;

    // riquadro informativo
    std::vector<Line> badge;
    badge.push_back({ { { b1, kGold, pad, true } }, lineH });
    if (!b2.empty()) badge.push_back({ { { b2, kWhite, pad, false } }, lineH });
    const int bw = (std::max)(TextWidth(b1, font, true), TextWidth(b2, font, false)) + pad * 2 + 4;
    g_badgePanel = BuildPanel(badge, bw, pad, font);

    // aiuto: tre colonne (tasto del pad, tasto del simulatore, azione)
    int c1 = 0, c2 = 0, c3 = 0;
    std::vector<std::vector<std::wstring>> cells;
    for (const std::wstring& r : rows) {
        cells.push_back(SplitTabs(r));
        const auto& c = cells.back();
        if (c.size() > 0) c1 = (std::max)(c1, TextWidth(c[0], font, true));
        if (c.size() > 1) c2 = (std::max)(c2, TextWidth(c[1], font, false));
        if (c.size() > 2) c3 = (std::max)(c3, TextWidth(c[2], font, false));
    }
    const int gap = font;
    const int x1 = pad, x2 = x1 + c1 + gap, x3 = x2 + (c2 ? c2 + gap : 0);
    int hw = x3 + c3 + pad + 4;
    hw = (std::max)(hw, TextWidth(title, font, true) + pad * 2 + 4);
    hw = (std::max)(hw, TextWidth(foot, font, false) + pad * 2 + 4);
    std::vector<Line> help;
    help.push_back({ { { title, kGold, pad, true } }, lineH * 3 / 2 });
    for (const auto& c : cells) {
        Line l{ {}, lineH };
        if (c.size() > 0) l.segs.push_back({ c[0], kGold, x1, true });
        if (c.size() > 1 && c2) l.segs.push_back({ c[1], kGrey, x2, false });
        if (c.size() > 2) l.segs.push_back({ c[2], kWhite, x3, false });
        help.push_back(l);
    }
    if (!foot.empty()) {
        help.push_back({ {}, lineH / 2 });
        help.push_back({ { { foot, kGrey, pad, false } }, lineH });
    }
    g_helpPanel = BuildPanel(help, hw, pad, font);
    Log("Overlay: pannelli preparati (carattere %d px; riquadro %dx%d, aiuto %dx%d)", font, g_badgePanel.w, g_badgePanel.h,
        g_helpPanel.w, g_helpPanel.h);
}

void BuildGlyphs(GlyphStyle style, int size)
{
    for (int i = 0; i < kSlotCount; ++i) g_glyph[i] = BuildGlyph(style, i, size);
    g_glyphStyle = style;
    g_glyphSize = size;
    ++g_glyphBuilds;
    Log("Overlay: simboli del pad %s preparati (%d px)", GlyphStyleName(style), size);
}

// --- Fusione nel buffer del gioco ---------------------------------------------------------------------------------

// Fonde nel buffer la regione r (coordinate del buffer, dall'alto) del pannello che sta in 'at': il pixel (x, y) del
// buffer prende il pixel (x - at.x, y - at.y) del pannello, con l'opacita' moltiplicata per mul/255. 'saved' riceve i
// pixel coperti.
void Blend(const Surface& s, const Panel& p, const Rect& at, const Rect& r, int mul, std::vector<uint8_t>& saved)
{
    const int bytes = s.bpp / 8;   // saved e' gia' della dimensione giusta (SafeBlend)
    for (int yy = 0; yy < r.h; ++yy) {
        uint8_t* row = s.Row(r.y + yy) + static_cast<ptrdiff_t>(r.x) * bytes;
        memcpy(&saved[static_cast<size_t>(yy) * r.w * bytes], row, static_cast<size_t>(r.w) * bytes);
        const uint32_t* src = &p.argb[static_cast<size_t>(r.y + yy - at.y) * p.w + (r.x - at.x)];
        for (int xx = 0; xx < r.w; ++xx) {
            const uint32_t c = src[xx];
            const int a = static_cast<int>(c >> 24) * mul / 255;
            if (!a) continue;
            const int sr = (c >> 16) & 0xff, sg = (c >> 8) & 0xff, sb = c & 0xff;
            if (s.fmt == Fmt::Rgb565) {
                uint16_t& d = reinterpret_cast<uint16_t*>(row)[xx];
                const int dr = ((d >> 11) & 31) * 255 / 31, dg = ((d >> 5) & 63) * 255 / 63, db = (d & 31) * 255 / 31;
                const int nr = (sr * a + dr * (255 - a)) / 255, ng = (sg * a + dg * (255 - a)) / 255, nb = (sb * a + db * (255 - a)) / 255;
                d = static_cast<uint16_t>(((nr * 31 / 255) << 11) | ((ng * 63 / 255) << 5) | (nb * 31 / 255));
            }
            else if (s.fmt == Fmt::Rgb555) {
                uint16_t& d = reinterpret_cast<uint16_t*>(row)[xx];
                const int dr = ((d >> 10) & 31) * 255 / 31, dg = ((d >> 5) & 31) * 255 / 31, db = (d & 31) * 255 / 31;
                const int nr = (sr * a + dr * (255 - a)) / 255, ng = (sg * a + dg * (255 - a)) / 255, nb = (sb * a + db * (255 - a)) / 255;
                d = static_cast<uint16_t>(((nr * 31 / 255) << 10) | ((ng * 31 / 255) << 5) | (nb * 31 / 255));
            }
            else {
                uint8_t* d = row + xx * 4;   // BGRX
                d[0] = static_cast<uint8_t>((sb * a + d[0] * (255 - a)) / 255);
                d[1] = static_cast<uint8_t>((sg * a + d[1] * (255 - a)) / 255);
                d[2] = static_cast<uint8_t>((sr * a + d[2] * (255 - a)) / 255);
            }
        }
    }
}

void Restore(const Surface& s, const Rect& r, const std::vector<uint8_t>& saved)
{
    const int bytes = s.bpp / 8;
    for (int yy = 0; yy < r.h; ++yy)
        memcpy(s.Row(r.y + yy) + static_cast<ptrdiff_t>(r.x) * bytes, &saved[static_cast<size_t>(yy) * r.w * bytes],
               static_cast<size_t>(r.w) * bytes);
}

// Fusione e ripristino protetti: un errore di memoria spegne l'overlay invece di far chiudere il gioco.
bool g_failed = false;

bool SafeBlend(const Surface& s, const Panel& p, const Rect& at, const Rect& r, int mul, std::vector<uint8_t>& saved)
{
    saved.resize(static_cast<size_t>(r.w) * r.h * (s.bpp / 8));
    __try { Blend(s, p, at, r, mul, saved); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_failed = true; return false; }
}

bool SafeRestore(const Surface& s, const Rect& r, const std::vector<uint8_t>& saved)
{
    __try { Restore(s, r, saved); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { g_failed = true; return false; }
}

Rect Intersect(const Rect& a, const Rect& b)
{
    const int x0 = (std::max)(a.x, b.x), y0 = (std::max)(a.y, b.y);
    const int x1 = (std::min)(a.x + a.w, b.x + b.w), y1 = (std::min)(a.y + a.h, b.y + b.h);
    return Rect{ x0, y0, x1 - x0, y1 - y0 };
}

bool Same(const Rect& a, const Rect& b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

Rect Clip(Rect r, int w, int h)
{
    if (r.x < 0) { r.w += r.x; r.x = 0; }
    if (r.y < 0) { r.h += r.y; r.y = 0; }
    if (r.x + r.w > w) r.w = w - r.x;
    if (r.y + r.h > h) r.h = h - r.y;
    return r;
}


// --- Radiali (0.6.0; disegno in Radial.cpp) ----------------------------------------------------------------------
std::vector<RadialItem> g_radItems;    // voci lette nel thread del gioco
Panel    g_radialPanel;
std::wstring g_radKey;                 // contenuto del pannello costruito (per ricostruirlo solo se cambia)

// Cosa si disegna a una copia: pannello, posto nel buffer, opacita'.
struct Item {
    const Panel* panel = nullptr;
    Rect at;
    int mul = 255;
};
std::vector<Item> g_prev;              // disegnati alla copia precedente

bool SameItem(const Item& a, const Item& b) { return a.panel == b.panel && a.mul == b.mul && Same(a.at, b.at); }

// --- Menu e barra comandi letti nel thread del gioco (0.5.0) -------------------------------------------------------
std::vector<RECT> g_barRects;          // pulsanti visibili della barra all'ultima lettura
std::vector<int>  g_barIds;            // indici dei loro comandi
DWORD g_uiNext = 0;

void ReadGame(const Surface& s, DWORD now, bool active, bool inMap)
{
    if (!active || !inMap) { g_barRects.clear(); g_barIds.clear(); }   // pad spento o fuori mappa: i simboli spariscono subito
    if (static_cast<int>(now - g_uiNext) < 0) {
        // 0.5.1: nei menu il gioco copia solo quando qualcosa cambia; se questa copia non si legge, l'elenco delle caselle
        // resterebbe quello di prima finche' il gioco non copia di nuovo. OverlayTick chiede allora un'altra copia.
        if (active && !inMap) {
            EnterCriticalSection(&g_lock);
            g_uiPending = true;
            LeaveCriticalSection(&g_lock);
        }
        return;
    }
    g_uiNext = now + 100;
    static uint32_t lastTop = 0;
    static int lastCount = -1, lastBar = -1;

    // caselle dei menu (solo fuori mappa e con il pad attivo)
    UiControl ctl[96];
    uint32_t top = 0;
    const int n = active && !inMap ? GameUiControls(ctl, 96, &top) : 0;
    if (active && !inMap && (top != lastTop || n != lastCount)) {
        int kinds[4] = {};
        for (int i = 0; i < n; ++i) ++kinds[ctl[i].kind];
        Log("Menu: finestra %08x, %d caselle (pulsanti %d, tendine %d, elenchi %d, cursori %d)", top, n, kinds[0], kinds[1],
            kinds[2], kinds[3]);
        lastTop = top;
        lastCount = n;
    }
    if (!active || inMap) { lastTop = 0; lastCount = -1; }
    EnterCriticalSection(&g_lock);
    g_ui.assign(ctl, ctl + n);
    g_uiW = s.w;
    g_uiH = s.h;
    g_uiTick = now;
    g_uiMenu = active && !inMap;
    g_uiPending = false;
    const bool testBar = g_testBarOn;
    const std::vector<RECT> fake = g_testBar;
    LeaveCriticalSection(&g_lock);

    // pulsanti della barra (in mappa con il pad attivo)
    g_barRects.clear();
    g_barIds.clear();
    if (active && inMap) {
        if (testBar) { g_barRects = fake; g_barIds.assign(fake.size(), 0); }
        else {
            RECT rects[64];
            int ids[64];
            const int nb = GameBarButtons(rects, ids, 64);
            g_barRects.assign(rects, rects + nb);
            g_barIds.assign(ids, ids + nb);
        }
    }
    EnterCriticalSection(&g_lock);
    g_barPad = g_barRects;
    LeaveCriticalSection(&g_lock);

    // voci del radiale aperto (0.6.0): comandi della barra con icona e titolo, oppure gruppi con il numero di unita'
    int kind;
    EnterCriticalSection(&g_lock);
    kind = active && inMap ? g_radKind : 0;
    LeaveCriticalSection(&g_lock);
    std::vector<RadialItem> items;
    if (kind == 1) {
        const size_t n = (std::min)(g_barRects.size(), static_cast<size_t>(16));
        for (size_t i = 0; i < n; ++i) {
            RadialItem it;
            it.id = g_barIds[i];
            char t[160];
            if (!testBar && GameCommandTitle(it.id, t, sizeof(t))) it.title = Widen(t);
            if (testBar) { wchar_t b[32]; swprintf_s(b, L"Comando di prova %d", static_cast<int>(i + 1)); it.title = b; }
            TakeIcon(s, g_barRects[i], it);
            items.push_back(it);
        }
    }
    else if (kind == 2) {
        int counts[10];
        GameGroupCounts(counts);
        for (int i = 0; i < 10; ++i) {
            RadialItem it;
            it.id = (i + 1) % 10;
            it.count = counts[it.id];
            items.push_back(it);
        }
    }
    g_radItems = items;
    EnterCriticalSection(&g_lock);
    g_radCount = static_cast<int>(items.size());
    LeaveCriticalSection(&g_lock);
    const int nb = static_cast<int>(g_barRects.size());
    if (nb != lastBar && (active && inMap)) {
        if (nb) Log("Barra comandi: %d pulsanti visibili, primo %ld,%ld-%ld,%ld%s", nb, g_barRects[0].left, g_barRects[0].top,
                    g_barRects[0].right, g_barRects[0].bottom, nb > kSlotCount ? " (oltre il dodicesimo senza simbolo)" : "");
        else Log("Barra comandi: nessun pulsante visibile");
    }
    lastBar = active && inMap ? nb : -1;
}

// Comando chiesto con RT + tasto: si rilegge la barra e si clicca il pulsante in quel posto.
void RunCommand(DWORD now)
{
    int slot;
    DWORD at;
    EnterCriticalSection(&g_lock);
    slot = g_cmdSlot;
    at = g_cmdAt;
    g_cmdSlot = -1;
    LeaveCriticalSection(&g_lock);
    if (slot < 0) return;
    if (now - at > 1000) { Log("Comando del posto %d scaduto (nessuna copia del gioco per 1 s)", slot + 1); return; }
    GameState gs;
    if (!GameRead(gs) || !gs.InMap()) { Log("Comando del posto %d non eseguito: non si e' in mappa", slot + 1); return; }
    RECT rects[64];
    int ids[64];
    const int n = GameBarButtons(rects, ids, 64);
    if (slot >= n) { Log("Comando del posto %d: la barra ha solo %d pulsanti", slot + 1, n); return; }
    char name[72];
    const bool ok = GameBarClick(ids[slot], name, sizeof(name));
    Log("Comando del posto %d \"%s\" (indice %d, pulsante %ld,%ld): %s", slot + 1, name, ids[slot], rects[slot].left,
        rects[slot].top, ok ? "passato al gioco come un clic (costi e requisiti li controlla il gioco)" : "ERRORE nella chiamata");
}

int WINAPI HookSetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc, int ySrc, UINT startScan,
                                 UINT lines, const VOID* bits, const BITMAPINFO* bmi, UINT use)
{
    static bool loggedFormat = false, loggedSkip = false;
    if (!bits || !bmi) return g_realBlit(hdc, xDest, yDest, w, h, xSrc, ySrc, startScan, lines, bits, bmi, use);

    Surface s;
    s.bits = static_cast<uint8_t*>(const_cast<VOID*>(bits));
    s.w = bmi->bmiHeader.biWidth;
    s.h = bmi->bmiHeader.biHeight < 0 ? -bmi->bmiHeader.biHeight : bmi->bmiHeader.biHeight;
    s.topDown = bmi->bmiHeader.biHeight < 0;
    s.bpp = bmi->bmiHeader.biBitCount;
    s.stride = ((s.w * s.bpp + 31) / 32) * 4;
    if (!loggedFormat) {
        loggedFormat = true;
        const DWORD* masks = reinterpret_cast<const DWORD*>(bmi->bmiColors);
        Log("Overlay: primo disegno del gioco %dx%d, %d bit, compressione %lu%s, %s, maschere %08lx %08lx %08lx, righe %u-%u, "
            "thread %lu", s.w, s.h, s.bpp, bmi->bmiHeader.biCompression,
            bmi->bmiHeader.biCompression == BI_BITFIELDS ? " (maschere)" : "", s.topDown ? "dall'alto" : "dal basso",
            masks[0], masks[1], masks[2], startScan, lines, GetCurrentThreadId());
    }
    static Fmt loggedFmt = Fmt::None;
    const DWORD comp = bmi->bmiHeader.biCompression;
    const DWORD greenMask = reinterpret_cast<const DWORD*>(bmi->bmiColors)[1];
    if (s.bpp == 16 && comp == BI_BITFIELDS && greenMask == 0x7e0) s.fmt = Fmt::Rgb565;
    else if (s.bpp == 16 && (comp == BI_RGB || (comp == BI_BITFIELDS && greenMask == 0x3e0))) s.fmt = Fmt::Rgb555;
    else if (s.bpp == 32 && comp == BI_RGB) s.fmt = Fmt::Bgrx32;
    const bool supported = s.fmt != Fmt::None;
    if (g_failed) return g_realBlit(hdc, xDest, yDest, w, h, xSrc, ySrc, startScan, lines, bits, bmi, use);
    if (!supported || startScan != 0 || static_cast<int>(lines) != s.h || s.w <= 0 || s.h <= 0) {
        if (!loggedSkip) {
            loggedSkip = true;
            Log("Overlay: formato non gestito (%d bit, compressione %lu, righe %u-%u): niente disegno", s.bpp,
                bmi->bmiHeader.biCompression, startScan, lines);
        }
        return g_realBlit(hdc, xDest, yDest, w, h, xSrc, ySrc, startScan, lines, bits, bmi, use);
    }

    if (s.fmt != loggedFmt) {
        loggedFmt = s.fmt;
        Log("Overlay: formato del buffer %s", s.fmt == Fmt::Rgb565 ? "RGB565" : s.fmt == Fmt::Rgb555 ? "RGB555" : "32 bit BGRX");
    }

    // stato richiesto
    const DWORD now = GetTickCount();
    g_lastHook.store(now);
    g_hookCalls.fetch_add(1);
    g_bufW.store(s.w);
    g_bufH.store(s.h);
    bool badgeOn, helpOn, active, inMap, layer, radAssign, cursorOn;
    int radKind, radHi, page, curX, curY, cliW, cliH;
    std::wstring radHint;
    GlyphStyle style;
    unsigned version;
    EnterCriticalSection(&g_lock);
    badgeOn = g_badgeUntil && static_cast<int>(g_badgeUntil - now) > 0;
    helpOn = g_helpOn;
    version = g_version;
    active = g_padActive;
    inMap = g_padInMap;
    layer = g_padLayer;
    page = g_padPage;
    style = g_padStyle;
    cursorOn = g_cursorOn;
    curX = g_cursorX; curY = g_cursorY; cliW = g_clientW; cliH = g_clientH;
    radKind = g_radKind;
    radHi = g_radHi;
    radAssign = g_radAssign;
    radHint = g_radHint;
    LeaveCriticalSection(&g_lock);
    if ((badgeOn || helpOn) && (version != g_builtVersion || s.h != g_builtHeight)) {
        BuildPanels(s.h);
        g_builtVersion = version;
        g_builtHeight = s.h;
    }
    ReadGame(s, now, active, inMap);

    // cose da disegnare, in coordinate del buffer (dall'alto), dal basso verso l'alto: simboli della barra, riquadro, aiuto.
    // I simboli stanno nell'angolo in basso a destra dei pulsanti, larghi il 42% dell'altezza del pulsante; con RT tenuto
    // sono pieni, altrimenti un po' trasparenti. Con RT + LT (0.8.0) gli stessi simboli passano ai pulsanti dal tredicesimo.
    std::vector<Item> want;
    const int first = page ? kSlotCount : 0;
    const int nb = (std::max)(0, (std::min)(static_cast<int>(g_barRects.size()) - first, kSlotCount));
    if (nb) {
        const RECT& b0 = g_barRects[first];
        const int size = (std::max)(12, (std::min)(64, static_cast<int>((b0.bottom - b0.top) * 42 / 100)));
        if (size != g_glyphSize || style != g_glyphStyle) BuildGlyphs(style, size);
        for (int i = 0; i < nb; ++i) {
            const RECT& b = g_barRects[first + i];
            if (!g_glyph[i].w) continue;
            Item it{ &g_glyph[i], Clip({ b.right - g_glyph[i].w - 1, b.bottom - g_glyph[i].h - 1, g_glyph[i].w, g_glyph[i].h }, s.w, s.h),
                     layer ? 255 : 200 };
            if (!it.at.Empty()) want.push_back(it);
        }
    }
    // radiale al centro, sopra tutto tranne l'aiuto; si ricostruisce solo se cambia il contenuto
    bool radialOn = false;
    if (radKind && active && inMap) {
        if (radHi >= static_cast<int>(g_radItems.size())) radHi = -1;
        std::wstring key = std::to_wstring(radKind) + L"|" + std::to_wstring(radHi) + L"|" + (radAssign ? L"1" : L"0") + L"|" +
                           radHint + L"|" + std::to_wstring(s.h) + L"|" + std::to_wstring(g_radItems.size());
        for (const RadialItem& it : g_radItems) key += L"|" + std::to_wstring(it.id) + L":" + std::to_wstring(it.count) + L":" + it.title;
        if (key != g_radKey) {
            g_radialPanel = BuildRadial(radKind, g_radItems, radHi, radAssign, radHint, s.h);
            g_radKey = key;
            ++g_glyphBuilds;   // forza la copia completa
        }
        if (g_radialPanel.w) {
            const int x = (std::max)(0, (s.w - g_radialPanel.w) / 2), y = (std::max)(0, (s.h - g_radialPanel.h) / 2);
            want.push_back({ &g_radialPanel, Clip({ x, y, g_radialPanel.w, g_radialPanel.h }, s.w, s.h) });
            radialOn = true;
        }
    }
    else if (!g_radKey.empty()) {
        g_radKey.clear();
    }
    // Il riquadro sta sotto la barra in alto del gioco; i pannelli partono sempre da x, y >= 0 e se fossero piu' grandi
    // dello schermo si tagliano a destra e in basso.
    if (badgeOn && g_badgePanel.w) want.push_back({ &g_badgePanel, Clip({ s.h / 54, s.h * 7 / 100, g_badgePanel.w, g_badgePanel.h }, s.w, s.h) });
    if (helpOn && g_helpPanel.w) {
        const int x = (std::max)(0, (s.w - g_helpPanel.w) / 2), y = (std::max)(0, (s.h - g_helpPanel.h) / 2);
        want.push_back({ &g_helpPanel, Clip({ x, y, g_helpPanel.w, g_helpPanel.h }, s.w, s.h) });
    }
    // cursore disegnato (0.8.0), sopra tutto: la punta della freccia sta nel punto del cursore, portato dalle coordinate
    // della finestra a quelle del buffer
    int cursorAt = -1;
    if (cursorOn && cliW > 0 && cliH > 0) {
        const int size = (std::max)(16, s.h / 34);
        if (g_cursorPanel.w != size) g_cursorPanel = BuildCursor(size);
        const int bx = curX * s.w / cliW, by = curY * s.h / cliH;
        const Rect at = Clip({ bx - size / 22, by - size / 22, g_cursorPanel.w, g_cursorPanel.h }, s.w, s.h);
        if (g_cursorPanel.w && !at.Empty()) {
            want.push_back({ &g_cursorPanel, at });
            cursorAt = (bx << 16) | (by & 0xffff);
        }
    }

    // copia completa dei pannelli solo quando cambiano (o ogni mezzo secondo, per sicurezza)
    static DWORD lastFull = 0;
    static unsigned lastFullVersion = 0, lastFullGlyphs = 0;
    bool changed = version != lastFullVersion || g_glyphBuilds != lastFullGlyphs || now - lastFull > 500 ||
                   want.size() != g_prev.size();
    for (size_t i = 0; !changed && i < want.size(); ++i) if (!SameItem(want[i], g_prev[i])) changed = true;

    // rettangolo copiato dal gioco, in coordinate del buffer (ySrc e' contato dal basso: alto = H - h - ySrc)
    const Rect game{ xSrc, s.h - static_cast<int>(h) - ySrc, static_cast<int>(w), static_cast<int>(h) };
    const int dx = xDest - xSrc, dy = yDest - game.y;

    const size_t count = want.size();
    std::vector<Rect> region(count);
    std::vector<std::vector<uint8_t>> saved(count);
    std::vector<char> drawn(count, 0);
    for (size_t i = 0; i < count; ++i) {
        region[i] = changed ? want[i].at : Intersect(want[i].at, game);
        if (region[i].Empty()) continue;
        drawn[i] = SafeBlend(s, *want[i].panel, want[i].at, region[i], want[i].mul, saved[i]);
        if (g_failed) break;
    }
    if (g_failed) Log("Overlay: ERRORE durante il disegno, overlay spento per questa sessione");
    const int result = g_realBlit(hdc, xDest, yDest, w, h, xSrc, ySrc, startScan, lines, bits, bmi, use);
    auto blit = [&](const Rect& r) {
        if (r.Empty()) return;
        g_realBlit(hdc, r.x + dx, r.y + dy, r.w, r.h, r.x, s.h - r.h - r.y, startScan, lines, bits, bmi, use);
    };
    if (changed) {
        // prima si ricopiano i posti dei pannelli spariti o spostati: nel buffer ci sono gia' i pannelli nuovi interi, quindi
        // dove un posto vecchio tocca un pannello nuovo resta il pannello nuovo
        for (const Item& old : g_prev) {
            bool still = false;
            for (const Item& it : want) if (Same(it.at, old.at)) { still = true; break; }
            if (!still) blit(old.at);
        }
        for (size_t i = 0; i < count; ++i) if (drawn[i]) blit(want[i].at);
        lastFull = now;
        lastFullVersion = version;
        lastFullGlyphs = g_glyphBuilds;
    }
    // ripristino al contrario: dove due pannelli si coprono, il secondo ha salvato anche il disegno del primo
    for (size_t i = count; i-- > 0;) if (drawn[i]) SafeRestore(s, region[i], saved[i]);
    g_prev = want;
    g_drawnState.store((badgeOn && g_badgePanel.w ? 1 : 0) | (helpOn && g_helpPanel.w ? 2 : 0) | (nb ? 4 : 0) | (radialOn ? 8 : 0));
    g_drawnCursor.store(cursorAt);
    g_drawnVersion.store(version);

    RunCommand(now);
    return result;
}

} // namespace

void OverlayInstall()
{
    InitializeCriticalSection(&g_lock);
    g_lockReady = true;
    g_realBlit = reinterpret_cast<SetDIBitsToDevice_t>(GetProcAddress(GetModuleHandleA("gdi32.dll"), "SetDIBitsToDevice"));
    const int n = InputPatchImport(GetModuleHandleA(nullptr), reinterpret_cast<void*>(g_realBlit),
                                   reinterpret_cast<void*>(&HookSetDIBitsToDevice));
    Log("Overlay: SetDIBitsToDevice agganciato in gbr.exe (%d voci)%s", n, n ? "" : " ATTENZIONE: nessuna voce, niente disegno");
}

void OverlayBadge(const wchar_t* line1, const wchar_t* line2, DWORD ms)
{
    if (!g_lockReady) return;
    EnterCriticalSection(&g_lock);
    if (g_badge1 != line1 || g_badge2 != line2) { g_badge1 = line1; g_badge2 = line2; ++g_version; }
    g_badgeUntil = GetTickCount() + ms;
    if (!g_badgeUntil) g_badgeUntil = 1;
    LeaveCriticalSection(&g_lock);
}

void OverlayHelp(bool on, const wchar_t* title, const std::vector<std::wstring>* rows, const wchar_t* foot)
{
    if (!g_lockReady) return;
    EnterCriticalSection(&g_lock);
    if (rows && (*rows != g_helpRows || g_helpTitle != title || g_helpFoot != foot)) {
        g_helpRows = *rows;
        g_helpTitle = title;
        g_helpFoot = foot;
        ++g_version;
    }
    g_helpOn = on;
    LeaveCriticalSection(&g_lock);
}

void OverlayPad(bool active, bool inMap, GlyphStyle style, bool layer, int page)
{
    if (!g_lockReady) return;
    EnterCriticalSection(&g_lock);
    g_padActive = active;
    g_padInMap = inMap;
    g_padStyle = style;
    g_padLayer = layer;
    g_padPage = page;
    LeaveCriticalSection(&g_lock);
}

void OverlayCursor(bool on, int x, int y, int clientW, int clientH)
{
    if (!g_lockReady) return;
    EnterCriticalSection(&g_lock);
    g_cursorOn = on;
    g_cursorX = x; g_cursorY = y; g_clientW = clientW; g_clientH = clientH;
    LeaveCriticalSection(&g_lock);
}

bool OverlayBufferSize(int& w, int& h)
{
    w = g_bufW.load();
    h = g_bufH.load();
    return w > 0 && h > 0;
}

void OverlayBarRects(std::vector<RECT>& out)
{
    out.clear();
    if (!g_lockReady) return;
    EnterCriticalSection(&g_lock);
    out = g_barPad;
    LeaveCriticalSection(&g_lock);
}

bool OverlayUiControls(std::vector<UiControl>& out, int& bufW, int& bufH)
{
    out.clear();
    if (!g_lockReady) return false;
    // 0.5.1: niente limite di eta' (prima 500 ms): nei menu fermi il gioco non copia e quindi non cambia nulla a schermo;
    // l'elenco resta quello dell'ultima copia letta, e OverlayTick fa leggere anche l'ultima copia saltata
    EnterCriticalSection(&g_lock);
    const bool valid = g_uiTick && g_uiMenu && g_uiW > 0 && g_uiH > 0;
    if (valid) { out = g_ui; bufW = g_uiW; bufH = g_uiH; }
    LeaveCriticalSection(&g_lock);
    return valid;
}

void OverlayRadial(int kind, int highlight, bool assign, const wchar_t* hint)
{
    if (!g_lockReady) return;
    EnterCriticalSection(&g_lock);
    g_radKind = kind;
    g_radHi = highlight;
    g_radAssign = assign;
    if (hint && g_radHint != hint) g_radHint = hint;
    if (!kind) g_radCount = 0;
    LeaveCriticalSection(&g_lock);
}

int OverlayRadialCount()
{
    if (!g_lockReady) return 0;
    EnterCriticalSection(&g_lock);
    const int n = g_radKind ? g_radCount : 0;
    LeaveCriticalSection(&g_lock);
    return n;
}

void OverlayBarCommand(int slot)
{
    if (!g_lockReady || slot < 0 || slot >= 64) return;
    EnterCriticalSection(&g_lock);
    g_cmdSlot = slot;
    g_cmdAt = GetTickCount();
    LeaveCriticalSection(&g_lock);
}

void OverlayTestBar(const std::vector<RECT>* rects)
{
    if (!g_lockReady) return;
    EnterCriticalSection(&g_lock);
    g_testBarOn = rects != nullptr;
    if (rects) g_testBar = *rects; else g_testBar.clear();
    LeaveCriticalSection(&g_lock);
}

// Chiamata dal thread del pad a ogni giro: se un pannello deve comparire, cambiare o sparire (o un comando aspetta il
// thread del gioco) e il gioco non copia nulla da un po' (schermo fermo), chiede a Windows di ridisegnare la finestra, al
// massimo ogni 200 ms. Ogni 30 s scrive nel log quante copie ha fatto il gioco (per capire quanto spesso disegna).
void OverlayTick(HWND hwnd)
{
    if (!g_lockReady || !hwnd) return;
    const DWORD now = GetTickCount();
    bool badgeOn, helpOn, glyphs, command, menuRead, radial, cursorOn;
    int cursorWant = -1;
    unsigned version;
    EnterCriticalSection(&g_lock);
    badgeOn = g_badgeUntil && static_cast<int>(g_badgeUntil - now) > 0;
    helpOn = g_helpOn;
    version = g_version;
    glyphs = g_padActive && g_padInMap;
    command = g_cmdSlot >= 0;
    // nei menu con il pad attivo: copia saltata dalla lettura, oppure elenco mai letto o letto in mappa
    menuRead = g_padActive && !g_padInMap && (g_uiPending || !g_uiTick || !g_uiMenu);
    radial = g_radKind && g_padActive && g_padInMap;
    cursorOn = g_cursorOn && g_clientW > 0 && g_clientH > 0;
    if (cursorOn) {
        const int bw = g_bufW.load(), bh = g_bufH.load();
        if (bw > 0 && bh > 0) cursorWant = ((g_cursorX * bw / g_clientW) << 16) | ((g_cursorY * bh / g_clientH) & 0xffff);
    }
    LeaveCriticalSection(&g_lock);
    // cursore disegnato spostato con lo schermo fermo (menu): ridisegno subito, al massimo ogni 15 ms, se no si muoverebbe
    // a scatti di 200 ms
    static DWORD lastCursorAsk = 0;
    if (cursorWant != g_drawnCursor.load() && (cursorOn || g_drawnCursor.load() >= 0) && now - g_lastHook.load() > 15 &&
        now - lastCursorAsk >= 15) {
        lastCursorAsk = now;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    const int want = (badgeOn ? 1 : 0) | (helpOn ? 2 : 0);
    const int drawn = g_drawnState.load();
    static DWORD lastAsk = 0, lastCount = 0;
    const bool differs = want != (drawn & 3) || (want && version != g_drawnVersion.load()) || ((drawn & 4) && !glyphs) || command || menuRead ||
                         ((drawn & 8) != 0) != radial;
    if (differs && now - g_lastHook.load() > 100 && now - lastAsk > 200) {
        lastAsk = now;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    if (!lastCount) lastCount = now;
    if (now - lastCount >= 30000) {
        Log("Overlay: %u copie del gioco negli ultimi %lu s", g_hookCalls.exchange(0), (now - lastCount) / 1000);
        lastCount = now;
    }
}
