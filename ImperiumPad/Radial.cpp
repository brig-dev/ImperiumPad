// Radial.cpp - disegno dei radiali dei comandi e dei gruppi (diviso da Overlay.cpp nella 0.8.0)
// Linguaggio: C++17, MSVC, Win32 (x86).

#include "OverlayDraw.h"
#include <algorithm>
#include <cmath>
#include <cwchar>

namespace ovl {

std::wstring Widen(const char* s)
{
    wchar_t w[256];
    const int n = MultiByteToWideChar(1252, 0, s, -1, w, 256);
    return n > 0 ? std::wstring(w) : std::wstring();
}

// Copia dal buffer del gioco il rettangolo (x0, y0, w, h) come immagine opaca in out (w * h pixel).
bool CopyIcon(const Surface& s, int x0, int y0, int w, int h, uint32_t* out)
{
    __try {
        for (int y = 0; y < h; ++y) {
            const uint8_t* row = s.Row(y0 + y);
            for (int x = 0; x < w; ++x) {
                int R, G, B;
                if (s.fmt == Fmt::Rgb565) {
                    const uint16_t v = reinterpret_cast<const uint16_t*>(row)[x0 + x];
                    R = ((v >> 11) & 31) * 255 / 31; G = ((v >> 5) & 63) * 255 / 63; B = (v & 31) * 255 / 31;
                }
                else if (s.fmt == Fmt::Rgb555) {
                    const uint16_t v = reinterpret_cast<const uint16_t*>(row)[x0 + x];
                    R = ((v >> 10) & 31) * 255 / 31; G = ((v >> 5) & 31) * 255 / 31; B = (v & 31) * 255 / 31;
                }
                else {
                    const uint8_t* q = row + (x0 + x) * 4;
                    R = q[2]; G = q[1]; B = q[0];
                }
                out[static_cast<size_t>(y) * w + x] = 0xff000000u | (R << 16) | (G << 8) | B;
            }
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void TakeIcon(const Surface& s, const RECT& r, RadialItem& it)
{
    const int x0 = (std::max)(0, static_cast<int>(r.left)), y0 = (std::max)(0, static_cast<int>(r.top));
    const int x1 = (std::min)(s.w, static_cast<int>(r.right)), y1 = (std::min)(s.h, static_cast<int>(r.bottom));
    it.iw = x1 - x0;
    it.ih = y1 - y0;
    it.icon.clear();
    if (it.iw <= 0 || it.ih <= 0 || it.iw > 128 || it.ih > 128) { it.iw = it.ih = 0; return; }
    it.icon.resize(static_cast<size_t>(it.iw) * it.ih);
    if (!CopyIcon(s, x0, y0, it.iw, it.ih, it.icon.data())) { it.icon.clear(); it.iw = it.ih = 0; }
}

// Pixel premoltiplicato: sovrappone il colore c con opacita' a (0..1).
void Put(std::vector<float>& acc, int S, int x, int y, COLORREF c, float a)
{
    if (a <= 0 || x < 0 || y < 0 || x >= S || y >= S) return;
    if (a > 1) a = 1;
    float* p = &acc[(static_cast<size_t>(y) * S + x) * 4];
    p[0] = GetRValue(c) / 255.0f * a + p[0] * (1 - a);
    p[1] = GetGValue(c) / 255.0f * a + p[1] * (1 - a);
    p[2] = GetBValue(c) / 255.0f * a + p[2] * (1 - a);
    p[3] = a + p[3] * (1 - a);
}

// Disco pieno (ring = 0) o anello largo ring, centrato in (cx, cy), raggio r, con l'antialiasing.
void Disc(std::vector<float>& acc, int S, float cx, float cy, float r, float ring, COLORREF c, float alpha)
{
    const int x0 = static_cast<int>(cx - r - 2), x1 = static_cast<int>(cx + r + 2);
    const int y0 = static_cast<int>(cy - r - 2), y1 = static_cast<int>(cy + r + 2);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            float cov = ring > 0 ? ring / 2 - std::fabs(d - (r - ring / 2)) + 0.5f : r - d + 0.5f;
            if (cov <= 0) continue;
            Put(acc, S, x, y, c, (cov > 1 ? 1 : cov) * alpha);
        }
}

// Alone morbido attorno a un cerchio: opacita' che va da alpha (sul bordo r) a zero (a r + width).
void Glow(std::vector<float>& acc, int S, float cx, float cy, float r, float width, COLORREF c, float alpha)
{
    const float R = r + width;
    for (int y = static_cast<int>(cy - R - 1); y <= static_cast<int>(cy + R + 1); ++y)
        for (int x = static_cast<int>(cx - R - 1); x <= static_cast<int>(cx + R + 1); ++x) {
            const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
            if (d < r || d > R) continue;
            const float t = 1.0f - (d - r) / width;
            Put(acc, S, x, y, c, alpha * t * t);
        }
}

// Spicchio del cerchio centrato sull'angolo a0 (0 in alto, senso orario), largo span, tra i raggi r0 e r1; l'opacita'
// cala verso l'esterno. Bordi con l'antialiasing.
void Wedge(std::vector<float>& acc, int S, float c, float a0, float span, float r0, float r1, COLORREF col, float alpha)
{
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            const float dx = x + 0.5f - c, dy = y + 0.5f - c;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d < r0 - 1 || d > r1 + 1) continue;
            float a = std::atan2(dx, -dy) - a0;
            while (a > 3.14159265f) a -= 6.2831853f;
            while (a < -3.14159265f) a += 6.2831853f;
            const float edge = (span / 2 - std::fabs(a)) * d;           // distanza in pixel dal lato dello spicchio
            float cov = edge + 0.5f;
            cov = (std::min)(cov, d - r0 + 0.5f);
            cov = (std::min)(cov, r1 - d + 0.5f);
            if (cov <= 0) continue;
            const float t = (d - r0) / (r1 - r0);
            Put(acc, S, x, y, col, (cov > 1 ? 1 : cov) * alpha * (1.0f - 0.55f * t));
        }
}

// Icona del comando ridimensionata (bilineare) a larghezza w, centrata in (cx, cy), con una cornice scura e dorata.
void PutIcon(std::vector<float>& acc, int S, const RadialItem& it, float cx, float cy, int w)
{
    if (it.icon.empty() || it.iw <= 0 || it.ih <= 0) return;
    const int h = w * it.ih / it.iw;
    const int ox = static_cast<int>(cx - w / 2.0f), oy = static_cast<int>(cy - h / 2.0f);
    auto px = [&](int x, int y) {
        x = (std::max)(0, (std::min)(it.iw - 1, x));
        y = (std::max)(0, (std::min)(it.ih - 1, y));
        return it.icon[static_cast<size_t>(y) * it.iw + x];
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float sx = (x + 0.5f) * it.iw / w - 0.5f, sy = (y + 0.5f) * it.ih / h - 0.5f;
            const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
            const float fx = sx - x0, fy = sy - y0;
            const uint32_t p00 = px(x0, y0), p10 = px(x0 + 1, y0), p01 = px(x0, y0 + 1), p11 = px(x0 + 1, y0 + 1);
            auto ch = [&](int shift) {
                const float a = ((p00 >> shift) & 0xff) * (1 - fx) + ((p10 >> shift) & 0xff) * fx;
                const float b = ((p01 >> shift) & 0xff) * (1 - fx) + ((p11 >> shift) & 0xff) * fx;
                return static_cast<int>(a * (1 - fy) + b * fy + 0.5f);
            };
            Put(acc, S, ox + x, oy + y, RGB(ch(16), ch(8), ch(0)), 1.0f);
        }
    for (int x = -2; x < w + 2; ++x) {
        Put(acc, S, ox + x, oy - 2, RGB(10, 8, 5), 1.0f); Put(acc, S, ox + x, oy + h + 1, RGB(10, 8, 5), 1.0f);
        Put(acc, S, ox + x, oy - 1, RGB(196, 158, 80), 0.9f); Put(acc, S, ox + x, oy + h, RGB(196, 158, 80), 0.9f);
    }
    for (int y = -2; y < h + 2; ++y) {
        Put(acc, S, ox - 2, oy + y, RGB(10, 8, 5), 1.0f); Put(acc, S, ox + w + 1, oy + y, RGB(10, 8, 5), 1.0f);
        if (y >= -1 && y <= h) { Put(acc, S, ox - 1, oy + y, RGB(196, 158, 80), 0.9f); Put(acc, S, ox + w, oy + y, RGB(196, 158, 80), 0.9f); }
    }
}

// --- Icone del radiale dei trucchi (0.9.1) --------------------------------------------------------------------------
// Disegnate con GDI in bianco su nero a quattro volte la dimensione (come i simboli del pad), ridotte a copertura; il nero
// dentro una figura fa da taglio (fette del pane, occhi del teschio...). Colore e ombra si mettono quando si posa l'icona.
namespace {

void Poly(HDC dc, std::initializer_list<POINT> pts)
{
    std::vector<POINT> v(pts);
    Polygon(dc, v.data(), static_cast<int>(v.size()));
}

std::vector<uint8_t> CheatIconMask(int icon, int size)
{
    Mask m;
    if (size < 8 || !MaskOpen(m, size * 4)) return {};
    const int S = m.S;
    auto P = [S](float f) { return static_cast<int>(f * S); };
    HDC dc = m.dc;
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(WHITE_BRUSH));
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    HPEN black = CreatePen(PS_SOLID, (std::max)(2, S / 28), RGB(0, 0, 0));
    HPEN white = CreatePen(PS_SOLID, (std::max)(2, S / 14), RGB(255, 255, 255));
    auto cut = [&](int x0, int y0, int x1, int y1) {
        SelectObject(dc, black); MoveToEx(dc, x0, y0, nullptr); LineTo(dc, x1, y1); SelectObject(dc, GetStockObject(NULL_PEN));
    };
    switch (icon) {
    case CI_Coins:   // pila di tre monete viste di lato, ognuna col bordo nero
        SelectObject(dc, black);
        for (int k = 0; k < 3; ++k) Ellipse(dc, P(0.14f), P(0.52f - k * 0.16f), P(0.86f), P(0.84f - k * 0.16f));
        SelectObject(dc, GetStockObject(NULL_PEN));
        break;
    case CI_Loaf:    // pagnotta con tre tagli
        Ellipse(dc, P(0.08f), P(0.30f), P(0.92f), P(0.80f));
        cut(P(0.30f), P(0.40f), P(0.40f), P(0.66f)); cut(P(0.47f), P(0.36f), P(0.57f), P(0.68f)); cut(P(0.64f), P(0.40f), P(0.74f), P(0.66f));
        break;
    case CI_House:   // casa: tetto e muro con la porta
        Poly(dc, { { P(0.08f), P(0.50f) }, { P(0.50f), P(0.12f) }, { P(0.92f), P(0.50f) } });
        Rectangle(dc, P(0.20f), P(0.48f), P(0.80f), P(0.88f));
        SelectObject(dc, GetStockObject(BLACK_BRUSH)); Rectangle(dc, P(0.42f), P(0.62f), P(0.58f), P(0.88f)); SelectObject(dc, GetStockObject(WHITE_BRUSH));
        break;
    case CI_People:  // due persone, quella davanti col bordo
        Ellipse(dc, P(0.18f), P(0.14f), P(0.42f), P(0.38f)); Ellipse(dc, P(0.06f), P(0.42f), P(0.54f), P(0.96f));
        SelectObject(dc, black);
        Ellipse(dc, P(0.50f), P(0.20f), P(0.76f), P(0.46f)); Ellipse(dc, P(0.36f), P(0.50f), P(0.90f), P(1.08f));
        SelectObject(dc, GetStockObject(NULL_PEN));
        SelectObject(dc, GetStockObject(BLACK_BRUSH)); Rectangle(dc, 0, P(0.88f), S, S); SelectObject(dc, GetStockObject(WHITE_BRUSH));
        break;
    case CI_Hammer:  // martello in diagonale
        Poly(dc, { { P(0.20f), P(0.86f) }, { P(0.12f), P(0.78f) }, { P(0.56f), P(0.34f) }, { P(0.64f), P(0.42f) } });
        Poly(dc, { { P(0.38f), P(0.30f) }, { P(0.58f), P(0.10f) }, { P(0.90f), P(0.42f) }, { P(0.70f), P(0.62f) } });
        break;
    case CI_Flag:
    case CI_WhiteFlag:   // asta e bandiera mossa
        Rectangle(dc, P(0.16f), P(0.08f), P(0.24f), P(0.94f));
        Poly(dc, { { P(0.24f), P(0.12f) }, { P(0.50f), P(0.20f) }, { P(0.88f), P(0.12f) }, { P(0.88f), P(0.52f) }, { P(0.50f), P(0.60f) }, { P(0.24f), P(0.52f) } });
        break;
    case CI_Heart:
        Ellipse(dc, P(0.08f), P(0.16f), P(0.54f), P(0.60f)); Ellipse(dc, P(0.46f), P(0.16f), P(0.92f), P(0.60f));
        Poly(dc, { { P(0.10f), P(0.46f) }, { P(0.90f), P(0.46f) }, { P(0.50f), P(0.90f) } });
        break;
    case CI_Star: {  // stella a cinque punte
        POINT pts[10];
        for (int i = 0; i < 10; ++i) {
            const float a = -1.5707963f + i * 0.6283185f, r = (i % 2 ? 0.20f : 0.46f);
            pts[i] = { P(0.5f + r * std::cos(a)), P(0.54f + r * std::sin(a)) };
        }
        Polygon(dc, pts, 10);
        break;
    }
    case CI_Skull:   // cranio, mascella, occhi, naso e denti neri
        Ellipse(dc, P(0.16f), P(0.08f), P(0.84f), P(0.70f)); Rectangle(dc, P(0.30f), P(0.56f), P(0.70f), P(0.88f));
        SelectObject(dc, GetStockObject(BLACK_BRUSH));
        Ellipse(dc, P(0.27f), P(0.34f), P(0.45f), P(0.52f)); Ellipse(dc, P(0.55f), P(0.34f), P(0.73f), P(0.52f));
        Poly(dc, { { P(0.50f), P(0.55f) }, { P(0.44f), P(0.65f) }, { P(0.56f), P(0.65f) } });
        SelectObject(dc, GetStockObject(WHITE_BRUSH));
        cut(P(0.42f), P(0.74f), P(0.42f), P(0.88f)); cut(P(0.50f), P(0.74f), P(0.50f), P(0.88f)); cut(P(0.58f), P(0.74f), P(0.58f), P(0.88f));
        break;
    case CI_Map:     // mappa piegata in tre, con un percorso
        Poly(dc, { { P(0.08f), P(0.22f) }, { P(0.36f), P(0.14f) }, { P(0.64f), P(0.22f) }, { P(0.92f), P(0.14f) },
                   { P(0.92f), P(0.78f) }, { P(0.64f), P(0.86f) }, { P(0.36f), P(0.78f) }, { P(0.08f), P(0.86f) } });
        cut(P(0.36f), P(0.14f), P(0.36f), P(0.78f)); cut(P(0.64f), P(0.22f), P(0.64f), P(0.86f));
        cut(P(0.18f), P(0.66f), P(0.30f), P(0.44f)); cut(P(0.30f), P(0.44f), P(0.50f), P(0.56f)); cut(P(0.50f), P(0.56f), P(0.78f), P(0.32f));
        break;
    case CI_Eye:     // occhio: mandorla bianca, iride nera, pupilla bianca
        Ellipse(dc, P(0.04f), P(0.28f), P(0.96f), P(0.74f));
        SelectObject(dc, GetStockObject(BLACK_BRUSH)); Ellipse(dc, P(0.32f), P(0.30f), P(0.68f), P(0.72f));
        SelectObject(dc, GetStockObject(WHITE_BRUSH)); Ellipse(dc, P(0.44f), P(0.42f), P(0.56f), P(0.54f));
        break;
    case CI_Swords: {   // due spade incrociate con l'elsa
        for (int k = 0; k < 2; ++k) {
            const float s = k ? -1.0f : 1.0f;   // diagonale e controdiagonale
            auto X = [&](float u) { return k ? 1.0f - u : u; };
            Poly(dc, { { P(X(0.14f)), P(0.10f) }, { P(X(0.22f)), P(0.08f) }, { P(X(0.78f)), P(0.64f) }, { P(X(0.70f)), P(0.72f) } });
            Poly(dc, { { P(X(0.62f)), P(0.80f) }, { P(X(0.80f)), P(0.62f) }, { P(X(0.86f)), P(0.68f) }, { P(X(0.68f)), P(0.86f) } });
            Ellipse(dc, P(X(0.78f)) - P(0.06f), P(0.78f), P(X(0.78f)) + P(0.06f), P(0.90f));
            (void)s;
        }
        break;
    }
    case CI_Shield:  // scudo con la croce (0.10.1, quasi invincibili)
        Poly(dc, { { P(0.16f), P(0.10f) }, { P(0.84f), P(0.10f) }, { P(0.82f), P(0.50f) }, { P(0.50f), P(0.92f) }, { P(0.18f), P(0.50f) } });
        cut(P(0.50f), P(0.18f), P(0.50f), P(0.78f));
        cut(P(0.26f), P(0.40f), P(0.74f), P(0.40f));
        break;
    case CI_Hourglass:   // clessidra (0.10.1, velocita' x10)
        Poly(dc, { { P(0.16f), P(0.06f) }, { P(0.84f), P(0.06f) }, { P(0.84f), P(0.14f) }, { P(0.16f), P(0.14f) } });
        Poly(dc, { { P(0.16f), P(0.86f) }, { P(0.84f), P(0.86f) }, { P(0.84f), P(0.94f) }, { P(0.16f), P(0.94f) } });
        Poly(dc, { { P(0.24f), P(0.14f) }, { P(0.76f), P(0.14f) }, { P(0.54f), P(0.50f) }, { P(0.76f), P(0.86f) }, { P(0.24f), P(0.86f) }, { P(0.46f), P(0.50f) } });
        cut(P(0.34f), P(0.24f), P(0.66f), P(0.24f));
        break;
    case CI_Cloud:   // nuvola (0.10.1, nebbia di guerra)
        Ellipse(dc, P(0.06f), P(0.42f), P(0.44f), P(0.80f));
        Ellipse(dc, P(0.26f), P(0.22f), P(0.72f), P(0.68f));
        Ellipse(dc, P(0.54f), P(0.38f), P(0.94f), P(0.78f));
        Poly(dc, { { P(0.24f), P(0.58f) }, { P(0.76f), P(0.58f) }, { P(0.76f), P(0.80f) }, { P(0.24f), P(0.80f) } });
        break;
    case CI_Book:    // libro aperto con le righe
        Poly(dc, { { P(0.06f), P(0.22f) }, { P(0.46f), P(0.28f) }, { P(0.46f), P(0.86f) }, { P(0.06f), P(0.80f) } });
        Poly(dc, { { P(0.54f), P(0.28f) }, { P(0.94f), P(0.22f) }, { P(0.94f), P(0.80f) }, { P(0.54f), P(0.86f) } });
        for (int k = 0; k < 3; ++k) { cut(P(0.14f), P(0.40f + k * 0.13f), P(0.38f), P(0.43f + k * 0.13f)); cut(P(0.62f), P(0.43f + k * 0.13f), P(0.86f), P(0.40f + k * 0.13f)); }
        break;
    default:
        break;
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(black);
    DeleteObject(white);
    std::vector<uint8_t> cov = MaskTake(m, size);
    MaskClose(m);
    return cov;
}

COLORREF CheatIconColor(int icon)
{
    switch (icon) {
    case CI_Coins: case CI_Star: return RGB(244, 204, 92);
    case CI_Loaf: return RGB(226, 178, 108);
    case CI_House: case CI_People: return RGB(214, 204, 184);
    case CI_Hammer: case CI_Swords: case CI_Shield: return RGB(196, 198, 210);
    case CI_Hourglass: return RGB(244, 204, 92);
    case CI_Cloud: return RGB(204, 210, 222);
    case CI_Flag: return RGB(222, 76, 62);
    case CI_WhiteFlag: return RGB(244, 242, 236);
    case CI_Heart: return RGB(232, 74, 86);
    case CI_Skull: return RGB(238, 232, 218);
    case CI_Map: return RGB(220, 196, 146);
    case CI_Eye: return RGB(132, 184, 244);
    case CI_Book: return RGB(206, 166, 116);
    default: return RGB(240, 236, 224);
    }
}

// Bordo del disco secondo la categoria della voce.
COLORREF CheatRim(int color)
{
    switch (color) {
    case 1: return RGB(232, 190, 92);
    case 2: return RGB(124, 192, 92);
    case 3: return RGB(212, 86, 72);
    case 4: return RGB(104, 156, 232);
    default: return RGB(160, 128, 72);
    }
}

} // namespace

// Radiale (0.6.1, ridisegnato): fondo con una sfumatura dal centro, doppia cornice dorata, spicchio luminoso verso la
// voce scelta, voce scelta piu' grande e con un alone, icone ingrandite con cornice, disco centrale per il titolo.
// Arco intorno a un disco (0.10.1, salute del gruppo): da mezzogiorno in senso orario per frac del giro, tra rIn e rOut.
void Arc(std::vector<float>& acc, int S, float cx, float cy, float rIn, float rOut, float frac, COLORREF col, float alpha)
{
    if (frac <= 0) return;
    const int x0 = (std::max)(0, static_cast<int>(cx - rOut - 1)), x1 = (std::min)(S - 1, static_cast<int>(cx + rOut + 1));
    const int y0 = (std::max)(0, static_cast<int>(cy - rOut - 1)), y1 = (std::min)(S - 1, static_cast<int>(cy + rOut + 1));
    const float lim = (std::min)(1.0f, frac) * 6.2831853f;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const float dx = x + 0.5f - cx, dy = y + 0.5f - cy, d = std::sqrt(dx * dx + dy * dy);
            float cov = (std::min)(d - rIn + 0.5f, rOut - d + 0.5f);
            if (cov <= 0) continue;
            float a = std::atan2(dx, -dy);
            if (a < 0) a += 6.2831853f;
            if (a > lim) continue;
            Put(acc, S, x, y, col, (cov > 1 ? 1 : cov) * alpha);
        }
}

// Colore della salute (0.10.1): verde, oro, rosso; bronzo se non si sa.
COLORREF HealthColor(int hp)
{
    if (hp < 0) return RGB(160, 128, 72);
    if (hp >= 67) return RGB(124, 200, 92);
    if (hp >= 34) return RGB(236, 192, 88);
    return RGB(220, 84, 70);
}

Panel BuildRadial(int kind, const std::vector<RadialItem>& items, int hi, bool assign, const std::wstring& hint, int bufH,
                  const std::wstring& detail)
{
    const int S = (std::max)(240, bufH * 62 / 100);
    const float c = S / 2.0f, rOut = S / 2.0f - 3, ring = S * 0.355f, ri = S * 0.07f;
    const int n = static_cast<int>(items.size());
    const float step = n ? 6.2831853f / n : 6.2831853f;
    const float rc = ring - ri * 1.2f - S * 0.03f;          // disco centrale con il titolo
    constexpr COLORREF kRim = RGB(212, 172, 86), kRimDark = RGB(120, 92, 44), kHalo = RGB(255, 200, 90);
    std::vector<float> acc(static_cast<size_t>(S) * S * 4, 0.0f);

    // fondo: dal bruno caldo al centro al quasi nero sul bordo
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            const float d = std::sqrt((x + 0.5f - c) * (x + 0.5f - c) + (y + 0.5f - c) * (y + 0.5f - c));
            const float cov = rOut - d + 0.5f;
            if (cov <= 0) continue;
            const float t = d / rOut;
            const COLORREF col = RGB(static_cast<int>(40 - 26 * t), static_cast<int>(30 - 20 * t), static_cast<int>(18 - 12 * t));
            Put(acc, S, x, y, col, (cov > 1 ? 1 : cov) * (0.92f - 0.12f * t));
        }
    // spicchio verso la voce scelta
    if (hi >= 0 && hi < n) Wedge(acc, S, c, hi * step, step * 0.9f, rc + 3, rOut - 8, RGB(200, 150, 60), 0.32f);
    // cornici: esterna piena, interna sottile; disco centrale con il suo bordo
    Disc(acc, S, c, c, rOut, 3.0f, kRim, 1.0f);
    Disc(acc, S, c, c, rOut - 3, 1.5f, RGB(20, 14, 8), 0.9f);
    Disc(acc, S, c, c, rOut - 8, 1.2f, kRimDark, 0.9f);
    Disc(acc, S, c, c, rc, 0, RGB(8, 6, 4), 0.55f);
    Disc(acc, S, c, c, rc, 1.5f, kRimDark, 0.9f);
    if (hi >= 0 && hi < n)   // tacca sul bordo del disco centrale, verso la voce scelta
        Disc(acc, S, c + rc * std::sin(hi * step), c - rc * std::cos(hi * step), 5.0f, 0, kRim, 1.0f);

    std::vector<POINT> at(n);
    for (int i = 0; i < n; ++i) {
        const double a = i * step;
        at[i] = { static_cast<LONG>(c + ring * std::sin(a)), static_cast<LONG>(c - ring * std::cos(a)) };
        const bool sel = i == hi, empty = kind == 2 && items[i].count == 0;
        const float r = sel ? ri * 1.18f : ri, x = static_cast<float>(at[i].x), y = static_cast<float>(at[i].y);
        if (sel) Glow(acc, S, x, y, r, ri * 0.45f, kHalo, 0.55f);
        Disc(acc, S, x, y, r + 2, 0, RGB(8, 6, 4), 0.6f);                                   // ombra
        Disc(acc, S, x, y, r, 0, sel ? RGB(92, 66, 24) : RGB(30, 23, 14), empty && !sel ? 0.7f : 0.97f);
        COLORREF rim = kind == 3 ? CheatRim(items[i].count) : (empty ? RGB(96, 82, 60) : RGB(160, 128, 72));
        if (kind == 2 && !empty) rim = HealthColor(items[i].hp);
        Disc(acc, S, x, y, r, sel ? 3.5f : (kind == 3 || kind == 2 ? 2.5f : 2.0f), sel ? RGB(255, 214, 120) : rim, 1.0f);
        if (kind == 2 && !empty && items[i].hp >= 0) {
            // 0.10.1: salute del gruppo come arco fuori dal disco (fondo scuro, poi la parte piena nel colore della salute)
            const float a0 = r + 3.0f, a1 = r + 3.0f + (std::max)(3.0f, ri * 0.11f);
            Arc(acc, S, x, y, a0, a1, 1.0f, RGB(20, 14, 8), 0.75f);
            Arc(acc, S, x, y, a0, a1, items[i].hp / 100.0f, HealthColor(items[i].hp), 1.0f);
        }
        if (kind == 2 && items[i].heroes > 0) {
            // 0.10.1: stella dorata in alto a destra se nel gruppo c'e' un eroe
            const int isz = (std::max)(10, static_cast<int>(r * 0.62f));
            const std::vector<uint8_t> cov = CheatIconMask(CI_Star, isz);
            const int ox = static_cast<int>(x + r * 0.70f) - isz / 2, oy = static_cast<int>(y - r * 0.70f) - isz / 2;
            Disc(acc, S, ox + isz / 2.0f, oy + isz / 2.0f, isz * 0.58f, 0, RGB(30, 22, 10), 0.95f);
            Disc(acc, S, ox + isz / 2.0f, oy + isz / 2.0f, isz * 0.58f, 1.5f, RGB(212, 172, 86), 1.0f);
            for (int yy = 0; yy < isz && !cov.empty(); ++yy)
                for (int xx = 0; xx < isz; ++xx) {
                    const uint8_t c = cov[static_cast<size_t>(yy) * isz + xx];
                    if (c) Put(acc, S, ox + xx, oy + yy, RGB(250, 210, 96), c / 255.0f);
                }
        }
        if (kind == 1 || kind == 4) PutIcon(acc, S, items[i], x, y, static_cast<int>(r * 1.3f));
        if (kind == 3 && items[i].cheatIcon) {
            // icona dei trucchi (0.9.1): piu' in alto se sotto c'e' la quantita', con un'ombra
            const int isz = static_cast<int>(r * (items[i].label.empty() ? 1.15f : 0.95f));
            const std::vector<uint8_t> cov = CheatIconMask(items[i].cheatIcon, isz);
            if (!cov.empty()) {
                const int ox = static_cast<int>(x) - isz / 2;
                const int oy = static_cast<int>(y - (items[i].label.empty() ? 0.0f : r * 0.22f)) - isz / 2;
                const COLORREF ic = CheatIconColor(items[i].cheatIcon);
                for (int yy = 0; yy < isz; ++yy)
                    for (int xx = 0; xx < isz; ++xx) {
                        const uint8_t c = cov[static_cast<size_t>(yy) * isz + xx];
                        if (c) Put(acc, S, ox + xx + 2, oy + yy + 2, RGB(0, 0, 0), c / 255.0f * 0.55f);
                    }
                for (int yy = 0; yy < isz; ++yy)
                    for (int xx = 0; xx < isz; ++xx) {
                        const uint8_t c = cov[static_cast<size_t>(yy) * isz + xx];
                        if (c) Put(acc, S, ox + xx, oy + yy, ic, c / 255.0f);
                    }
            }
        }
    }

    // testi: bianco = normale, rosso puro = dorato, verde puro = grigio (poi si ricolorano)
    Mask m;
    if (MaskOpen(m, S)) {
        const int titlePx = (std::max)(15, bufH / 32), smallPx = (std::max)(11, bufH / 60), digitPx = static_cast<int>(ri * 1.05f);
        HFONT title = MakeFont(titlePx, true), small = MakeFont(smallPx, false), digit = MakeFont(digitPx, true), digitBig = MakeFont(digitPx * 6 / 5, true);
        std::wstring head;
        if (kind == 1 || kind == 4) {   // 4 = crea truppe (0.10.0): come i comandi, con altri testi
            if (n == 0) head = Tr(kind == 4 ? T_SpawnNone : T_RadNoCommands);
            else if (hi < 0) head = Tr(kind == 4 ? T_SpawnTilt : T_RadTiltCommand);
            else head = items[hi].title.empty() ? Tr(T_RadUnnamed) : items[hi].title;
        }
        else if (kind == 3) {   // trucchi (0.9.0)
            if (hi < 0 || hi >= n) head = Tr(T_RadTiltCheat);
            else head = items[hi].title;
        }
        else {
            // 0.8.4: con tutti i gruppi vuoti si dice che non ce ne sono (la riga sotto spiega come crearli)
            bool anyGroup = false;
            for (const RadialItem& it : items) if (it.count) { anyGroup = true; break; }
            if (hi < 0 && !assign && !anyGroup) head = Tr(T_RadNoGroups);
            else if (hi < 0) head = Tr(assign ? T_RadAssignChoose : T_RadTiltGroup);
            else {
                const int g = items[hi].id;
                if (assign) head = TrF(T_RadAssignTo, g);
                else if (items[hi].count) head = TrF(items[hi].count == 1 ? T_RadGroupOne : T_RadGroupMany, g, items[hi].count);
                else head = TrF(T_RadGroupEmpty, g);
            }
        }
        const int box = static_cast<int>(rc * 0.86f);
        RECT tr{ static_cast<LONG>(c - box), 0, static_cast<LONG>(c + box), 0 };
        HGDIOBJ old = SelectObject(m.dc, title);
        RECT t1 = tr; t1.bottom = S;
        DrawTextW(m.dc, head.c_str(), -1, &t1, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
        SelectObject(m.dc, small);
        RECT t2 = tr; t2.bottom = S;
        DrawTextW(m.dc, hint.c_str(), -1, &t2, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
        HFONT det = MakeFont((std::max)(12, smallPx * 6 / 5), false);
        SelectObject(m.dc, det);
        RECT t3 = tr; t3.bottom = S;
        if (!detail.empty()) DrawTextW(m.dc, detail.c_str(), -1, &t3, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
        const int h1 = t1.bottom - t1.top, h2 = t2.bottom - t2.top, h3 = detail.empty() ? 0 : t3.bottom - t3.top, gap = smallPx;
        const int g3 = detail.empty() ? 0 : gap / 2;
        const int top = static_cast<int>(c) - (h1 + g3 + h3 + gap + h2) / 2;
        SelectObject(m.dc, title);
        SetTextColor(m.dc, RGB(255, 0, 0));
        RECT d1{ tr.left, top, tr.right, top + h1 };
        DrawTextW(m.dc, head.c_str(), -1, &d1, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        if (!detail.empty()) {   // 0.10.1: cosa c'e' nel gruppo, in bianco sotto il titolo
            SelectObject(m.dc, det);
            SetTextColor(m.dc, RGB(255, 255, 255));
            RECT d3{ tr.left, top + h1 + g3, tr.right, top + h1 + g3 + h3 };
            DrawTextW(m.dc, detail.c_str(), -1, &d3, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        }
        SelectObject(m.dc, small);
        SetTextColor(m.dc, RGB(0, 255, 0));
        RECT d2{ tr.left, top + h1 + g3 + h3 + gap, tr.right, top + h1 + g3 + h3 + gap + h2 };
        DrawTextW(m.dc, hint.c_str(), -1, &d2, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        if (kind == 2) {
            for (int i = 0; i < n; ++i) {
                const bool sel = i == hi;
                const float r = sel ? ri * 1.18f : ri;
                wchar_t d[8];
                swprintf_s(d, L"%d", items[i].id);
                SelectObject(m.dc, sel ? digitBig : digit);
                SetTextColor(m.dc, sel ? RGB(255, 0, 0) : (items[i].count ? RGB(255, 255, 255) : RGB(0, 255, 0)));
                RECT dr{ at[i].x - static_cast<LONG>(r), at[i].y - static_cast<LONG>(r * 0.9f), at[i].x + static_cast<LONG>(r), at[i].y + static_cast<LONG>(r * 0.38f) };
                DrawTextW(m.dc, d, -1, &dr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(m.dc, small);
                SetTextColor(m.dc, RGB(0, 255, 0));
                wchar_t cnt[16];
                if (items[i].count) swprintf_s(cnt, L"%d", items[i].count); else wcscpy_s(cnt, Tr(T_RadEmpty));
                RECT cr{ at[i].x - static_cast<LONG>(r), at[i].y + static_cast<LONG>(r * 0.28f), at[i].x + static_cast<LONG>(r), at[i].y + static_cast<LONG>(r * 0.92f) };
                DrawTextW(m.dc, cnt, -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
        if (kind == 3) {
            // scritta corta nel disco; il colore passa dai canali della maschera: rosso = oro, blu = verde del cibo
            HFONT lab = MakeFont((std::max)(9, static_cast<int>(ri * 0.40f)), true);
            SelectObject(m.dc, lab);
            for (int i = 0; i < n; ++i) {
                if (items[i].label.empty()) continue;
                const float r = i == hi ? ri * 1.18f : ri;
                SetTextColor(m.dc, items[i].count == 1 ? RGB(255, 0, 0) : items[i].count == 2 ? RGB(0, 0, 255) : RGB(255, 255, 255));
                // sotto l'icona (0.9.1; senza icona al centro del disco)
                const LONG top = items[i].cheatIcon ? at[i].y + static_cast<LONG>(r * 0.28f) : at[i].y - static_cast<LONG>(r);
                RECT lr{ at[i].x - static_cast<LONG>(r), top, at[i].x + static_cast<LONG>(r),
                         items[i].cheatIcon ? at[i].y + static_cast<LONG>(r * 0.86f) : at[i].y + static_cast<LONG>(r) };
                DrawTextW(m.dc, items[i].label.c_str(), -1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            SelectObject(m.dc, small);
            DeleteObject(lab);
        }
        SelectObject(m.dc, old);
        DeleteObject(title);
        DeleteObject(small);
        DeleteObject(digit);
        DeleteObject(digitBig);
        DeleteObject(det);
        GdiFlush();
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x) {
                const uint32_t v = m.bits[static_cast<size_t>(y) * S + x];
                const int r = (v >> 16) & 0xff, g = (v >> 8) & 0xff, b = v & 0xff;
                if (!r && !g && !b) continue;
                COLORREF col = kWhite;
                int ink = (std::max)(r, (std::max)(g, b));
                if (!g && !b) col = RGB(240, 200, 104);            // dorato
                else if (!r && !b) col = RGB(176, 168, 150);       // grigio
                else if (!r && !g) col = RGB(140, 212, 110);       // verde (cibo, radiale dei trucchi)
                Put(acc, S, x + 1, y + 1, RGB(0, 0, 0), ink / 255.0f * 0.7f);   // ombra del testo
                Put(acc, S, x, y, col, ink / 255.0f);
            }
        MaskClose(m);
    }

    return FinishPanel(acc, S);
}

// Immagine premoltiplicata (r, g, b, a in 0..1) -> pannello ARGB con l'opacita' separata.
Panel FinishPanel(const std::vector<float>& acc, int S)
{
    Panel p;
    p.w = p.h = S;
    p.argb.resize(static_cast<size_t>(S) * S);
    for (size_t i = 0; i < p.argb.size(); ++i) {
        const float* q = &acc[i * 4];
        const float a = q[3];
        int R = 0, G = 0, B = 0;
        if (a > 0.001f) {
            R = (std::min)(255, static_cast<int>(q[0] / a * 255 + 0.5f));
            G = (std::min)(255, static_cast<int>(q[1] / a * 255 + 0.5f));
            B = (std::min)(255, static_cast<int>(q[2] / a * 255 + 0.5f));
        }
        p.argb[i] = (static_cast<uint32_t>((std::min)(1.0f, a) * 255 + 0.5f) << 24) | (R << 16) | (G << 8) | B;
    }
    return p;
}

} // namespace ovl
