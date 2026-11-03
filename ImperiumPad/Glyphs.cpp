// Glyphs.cpp - simboli del pad sui pulsanti della barra (diviso da Overlay.cpp nella 0.8.0)
// Linguaggio: C++17, MSVC, Win32 (x86).

#include "OverlayDraw.h"
#include <algorithm>
#include <cstring>

namespace ovl {

// --- Simboli del pad (0.5.0) ---------------------------------------------------------------------------------------
// Ogni simbolo si disegna con GDI in bianco su nero a quattro volte la dimensione, a strati (fondo, bordo, segno); ogni
// strato ridotto alla dimensione vera da' la copertura con l'antialiasing, e gli strati colorati si sovrappongono.
bool MaskOpen(Mask& m, int S)
{
    HDC screen = GetDC(nullptr);
    m.dc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = S;
    bi.bmiHeader.biHeight = -S;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    m.bmp = CreateDIBSection(m.dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!m.bmp || !bits) { DeleteDC(m.dc); m.dc = nullptr; return false; }
    m.old = SelectObject(m.dc, m.bmp);
    m.bits = static_cast<uint32_t*>(bits);
    m.S = S;
    SetBkMode(m.dc, TRANSPARENT);
    SetTextColor(m.dc, RGB(255, 255, 255));
    return true;
}

void MaskClose(Mask& m)
{
    if (!m.dc) return;
    SelectObject(m.dc, m.old);
    DeleteObject(m.bmp);
    DeleteDC(m.dc);
    m.dc = nullptr;
}

// Riduce la maschera grande (S = 4 * size) a una copertura 0..255 per pixel.
std::vector<uint8_t> MaskTake(Mask& m, int size)
{
    GdiFlush();
    std::vector<uint8_t> cov(static_cast<size_t>(size) * size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            int sum = 0;
            for (int yy = 0; yy < 4; ++yy)
                for (int xx = 0; xx < 4; ++xx) {
                    const uint32_t v = m.bits[static_cast<size_t>(y * 4 + yy) * m.S + x * 4 + xx];
                    int i = static_cast<int>((v >> 16) & 0xff);
                    i = (std::max)(i, static_cast<int>((v >> 8) & 0xff));
                    i = (std::max)(i, static_cast<int>(v & 0xff));
                    sum += i;
                }
            cov[static_cast<size_t>(y) * size + x] = static_cast<uint8_t>(sum / 16);
        }
    memset(m.bits, 0, static_cast<size_t>(m.S) * m.S * 4);
    return cov;
}

enum class Shape { Circle, Shoulder, Dpad };

// Figura piena (ring = false) o solo il bordo spesso t.
void DrawShape(Mask& m, Shape sh, bool ring, int t)
{
    const int S = m.S, e = S / 16;
    HPEN pen = ring ? CreatePen(PS_SOLID, t, RGB(255, 255, 255)) : nullptr;
    HGDIOBJ oldPen = SelectObject(m.dc, ring ? static_cast<HGDIOBJ>(pen) : GetStockObject(NULL_PEN));
    HGDIOBJ oldBrush = SelectObject(m.dc, GetStockObject(ring ? NULL_BRUSH : WHITE_BRUSH));
    const int in = ring ? t / 2 : 0;
    if (sh == Shape::Circle) {
        Ellipse(m.dc, e + in, e + in, S - e - in + 1, S - e - in + 1);
    }
    else if (sh == Shape::Shoulder) {
        RoundRect(m.dc, e + in, S * 18 / 100 + in, S - e - in + 1, S * 82 / 100 - in + 1, S / 3, S / 3);
    }
    else {
        const int c = S / 2, a = S * 21 / 100, lo = e + in, hi = S - e - in;   // croce: bracci larghi 2a
        const POINT pts[12] = { { c - a + in, lo }, { c + a - in, lo }, { c + a - in, c - a + in }, { hi, c - a + in },
                                { hi, c + a - in }, { c + a - in, c + a - in }, { c + a - in, hi }, { c - a + in, hi },
                                { c - a + in, c + a - in }, { lo, c + a - in }, { lo, c - a + in }, { c - a + in, c - a + in } };
        Polygon(m.dc, pts, 12);
    }
    SelectObject(m.dc, oldBrush);
    SelectObject(m.dc, oldPen);
    if (pen) DeleteObject(pen);
}

void DrawLabel(Mask& m, const wchar_t* text, int px)
{
    HFONT f = MakeFont(px, true);
    HGDIOBJ old = SelectObject(m.dc, f);
    RECT r{ 0, 0, m.S, m.S };
    DrawTextW(m.dc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(m.dc, old);
    DeleteObject(f);
}

// Segni PlayStation: 0 croce, 1 cerchio, 2 quadrato, 3 triangolo (contorni spessi).
void DrawPsSymbol(Mask& m, int which)
{
    const int S = m.S, c = S / 2, t = S / 10;
    HPEN pen = CreatePen(PS_SOLID, t, RGB(255, 255, 255));
    HGDIOBJ oldPen = SelectObject(m.dc, pen);
    HGDIOBJ oldBrush = SelectObject(m.dc, GetStockObject(NULL_BRUSH));
    if (which == 0) {
        const int d = S * 19 / 100;
        MoveToEx(m.dc, c - d, c - d, nullptr); LineTo(m.dc, c + d, c + d);
        MoveToEx(m.dc, c - d, c + d, nullptr); LineTo(m.dc, c + d, c - d);
    }
    else if (which == 1) {
        const int d = S * 21 / 100;
        Ellipse(m.dc, c - d, c - d, c + d + 1, c + d + 1);
    }
    else if (which == 2) {
        const int d = S * 18 / 100;
        Rectangle(m.dc, c - d, c - d, c + d + 1, c + d + 1);
    }
    else {
        const int d = S * 23 / 100;
        const POINT pts[3] = { { c, c - d }, { c + d, c + d * 7 / 10 }, { c - d, c + d * 7 / 10 } };
        Polygon(m.dc, pts, 3);
    }
    SelectObject(m.dc, oldBrush);
    SelectObject(m.dc, oldPen);
    DeleteObject(pen);
}

// Braccio della croce direzionale da evidenziare: 0 su, 1 destra, 2 giu', 3 sinistra.
void DrawDpadArm(Mask& m, int dir)
{
    // tutto il braccio, dalla punta al centro, dentro il bordo
    const int S = m.S, c = S / 2, a = S * 21 / 100, e = S / 16 + (std::max)(4, S / 12), n = (std::max)(4, S / 12);
    RECT r{};
    if (dir == 0) r = { c - a + n, e, c + a - n, c + a - n };
    else if (dir == 1) r = { c - a + n, c - a + n, S - e, c + a - n };
    else if (dir == 2) r = { c - a + n, c - a + n, c + a - n, S - e };
    else r = { e, c - a + n, c + a - n, c + a - n };
    FillRect(m.dc, &r, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
}

// Sovrappone uno strato colorato (copertura cov, opacita' alpha) all'immagine premoltiplicata acc (r, g, b, a in 0..1).
void Layer(std::vector<float>& acc, const std::vector<uint8_t>& cov, COLORREF c, int alpha)
{
    const float cr = GetRValue(c) / 255.0f, cg = GetGValue(c) / 255.0f, cb = GetBValue(c) / 255.0f;
    for (size_t i = 0; i < cov.size(); ++i) {
        const float a = cov[i] / 255.0f * alpha / 255.0f;
        float* p = &acc[i * 4];
        p[0] = cr * a + p[0] * (1 - a);
        p[1] = cg * a + p[1] * (1 - a);
        p[2] = cb * a + p[2] * (1 - a);
        p[3] = a + p[3] * (1 - a);
    }
}

Panel BuildGlyph(GlyphStyle style, int slot, int size)
{
    Panel p;
    Mask m;
    if (!MaskOpen(m, size * 4)) return p;
    const int S = m.S, ring = (std::max)(4, S / 12);
    constexpr COLORREF kFill = RGB(22, 22, 26), kRing = RGB(190, 190, 194), kInk = RGB(246, 246, 246);
    // A, B, X, Y nei colori Xbox; segni PlayStation nei colori classici (croce, cerchio, quadrato, triangolo)
    static const COLORREF kXbox[4] = { RGB(112, 192, 72), RGB(232, 72, 60), RGB(72, 142, 236), RGB(246, 202, 52) };
    static const COLORREF kPs[4] = { RGB(142, 162, 236), RGB(236, 92, 102), RGB(226, 132, 202), RGB(72, 202, 172) };
    static const wchar_t* kFace[4] = { L"A", L"B", L"X", L"Y" };

    // posti 0..11 come kSlotMask; 12 e 13 (0.8.3) = RT e LT, il segno accanto alla barra
    Shape shape = Shape::Circle;
    if (slot == 4 || slot == 5 || slot >= 12) shape = Shape::Shoulder;
    else if (slot >= 6 && slot <= 9) shape = Shape::Dpad;
    COLORREF ringColor = kRing, inkColor = kInk;
    if (slot < 4 && style == GlyphStyle::Xbox) ringColor = inkColor = kXbox[slot];
    if (slot < 4 && style == GlyphStyle::PlayStation) inkColor = kPs[slot];
    if (slot >= 12) ringColor = kGold;   // il segno di RT accanto alla barra si deve notare

    DrawShape(m, shape, false, 0);
    const std::vector<uint8_t> fill = MaskTake(m, size);
    DrawShape(m, shape, true, ring);
    const std::vector<uint8_t> edge = MaskTake(m, size);
    if (slot < 4) {
        if (style == GlyphStyle::PlayStation) DrawPsSymbol(m, slot);
        else DrawLabel(m, kFace[slot], S * 62 / 100);
    }
    else if (slot == 4 || slot == 5) {
        const wchar_t* t = style == GlyphStyle::Xbox ? (slot == 4 ? L"LB" : L"RB") : (slot == 4 ? L"L1" : L"R1");
        DrawLabel(m, t, S * 46 / 100);
    }
    else if (slot <= 9) {
        DrawDpadArm(m, slot - 6);
    }
    else if (slot >= 12) {
        const wchar_t* t = style == GlyphStyle::Xbox ? (slot == 12 ? L"RT" : L"LT") : (slot == 12 ? L"R2" : L"L2");
        DrawLabel(m, t, S * 46 / 100);
    }
    else {
        const wchar_t* t = style == GlyphStyle::Xbox ? (slot == 10 ? L"LS" : L"RS") : (slot == 10 ? L"L3" : L"R3");
        DrawLabel(m, t, S * 46 / 100);
    }
    const std::vector<uint8_t> ink = MaskTake(m, size);
    MaskClose(m);

    std::vector<float> acc(static_cast<size_t>(size) * size * 4, 0.0f);
    Layer(acc, fill, kFill, 225);
    Layer(acc, edge, ringColor, 255);
    Layer(acc, ink, inkColor, 255);
    p.w = p.h = size;
    p.argb.resize(static_cast<size_t>(size) * size);
    for (size_t i = 0; i < p.argb.size(); ++i) {
        const float* q = &acc[i * 4];
        const float a = q[3];
        const int A = static_cast<int>(a * 255 + 0.5f);
        int R = 0, G = 0, B = 0;
        if (a > 0.001f) {
            R = (std::min)(255, static_cast<int>(q[0] / a * 255 + 0.5f));
            G = (std::min)(255, static_cast<int>(q[1] / a * 255 + 0.5f));
            B = (std::min)(255, static_cast<int>(q[2] / a * 255 + 0.5f));
        }
        p.argb[i] = (static_cast<uint32_t>(A) << 24) | (R << 16) | (G << 8) | B;
    }
    return p;
}

} // namespace ovl
