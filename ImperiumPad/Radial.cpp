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

// Radiale (0.6.1, ridisegnato): fondo con una sfumatura dal centro, doppia cornice dorata, spicchio luminoso verso la
// voce scelta, voce scelta piu' grande e con un alone, icone ingrandite con cornice, disco centrale per il titolo.
Panel BuildRadial(int kind, const std::vector<RadialItem>& items, int hi, bool assign, const std::wstring& hint, int bufH)
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
        Disc(acc, S, x, y, r, sel ? 3.5f : 2.0f, sel ? RGB(255, 214, 120) : (empty ? RGB(96, 82, 60) : RGB(160, 128, 72)), 1.0f);
        if (kind == 1) PutIcon(acc, S, items[i], x, y, static_cast<int>(r * 1.3f));
    }

    // testi: bianco = normale, rosso puro = dorato, verde puro = grigio (poi si ricolorano)
    Mask m;
    if (MaskOpen(m, S)) {
        const int titlePx = (std::max)(15, bufH / 32), smallPx = (std::max)(11, bufH / 60), digitPx = static_cast<int>(ri * 1.05f);
        HFONT title = MakeFont(titlePx, true), small = MakeFont(smallPx, false), digit = MakeFont(digitPx, true), digitBig = MakeFont(digitPx * 6 / 5, true);
        std::wstring head;
        if (kind == 1) {
            if (n == 0) head = Tr(T_RadNoCommands);
            else if (hi < 0) head = Tr(T_RadTiltCommand);
            else head = items[hi].title.empty() ? Tr(T_RadUnnamed) : items[hi].title;
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
        const int h1 = t1.bottom - t1.top, h2 = t2.bottom - t2.top, gap = smallPx;
        const int top = static_cast<int>(c) - (h1 + gap + h2) / 2;
        SelectObject(m.dc, title);
        SetTextColor(m.dc, RGB(255, 0, 0));
        RECT d1{ tr.left, top, tr.right, top + h1 };
        DrawTextW(m.dc, head.c_str(), -1, &d1, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(m.dc, small);
        SetTextColor(m.dc, RGB(0, 255, 0));
        RECT d2{ tr.left, top + h1 + gap, tr.right, top + h1 + gap + h2 };
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
        SelectObject(m.dc, old);
        DeleteObject(title);
        DeleteObject(small);
        DeleteObject(digit);
        DeleteObject(digitBig);
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
