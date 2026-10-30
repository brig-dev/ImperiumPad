// OverlayDraw.h - parti comuni del disegno dell'overlay (Overlay.cpp, Glyphs.cpp, Radial.cpp, Cursor.cpp)
// Linguaggio: C++17, MSVC, Win32 (x86). Solo per i file dell'overlay; tutto gira nel thread del gioco.
#pragma once

#include "ImperiumPad.h"

namespace ovl {

struct Panel {
    int w = 0, h = 0;
    std::vector<uint32_t> argb;   // pixel 0xAARRGGBB, opacita' in A
};

constexpr COLORREF kWhite = RGB(240, 236, 224), kGold = RGB(232, 190, 92), kGrey = RGB(170, 165, 150);
constexpr COLORREF kBack = RGB(24, 18, 12);
constexpr int      kBackAlpha = 200;   // 0..255

HFONT MakeFont(int px, bool bold);           // Segoe UI con l'antialiasing

// Buffer del gioco passato a SetDIBitsToDevice
enum class Fmt { None, Rgb565, Rgb555, Bgrx32 };

struct Surface {
    uint8_t* bits = nullptr;
    int w = 0, h = 0, stride = 0, bpp = 0;
    Fmt fmt = Fmt::None;
    bool topDown = false;
    uint8_t* Row(int y) const { return bits + static_cast<ptrdiff_t>(topDown ? y : h - 1 - y) * stride; }
};

// Disegno a strati: maschera GDI bianca su nero a quattro volte la dimensione, ridotta a copertura 0..255 (Glyphs.cpp)
struct Mask {
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ old = nullptr;
    uint32_t* bits = nullptr;
    int S = 0;
};
bool MaskOpen(Mask& m, int S);
void MaskClose(Mask& m);
std::vector<uint8_t> MaskTake(Mask& m, int size);
void Layer(std::vector<float>& acc, const std::vector<uint8_t>& cov, COLORREF c, int alpha);
Panel BuildGlyph(GlyphStyle style, int slot, int size);

// Pixel premoltiplicati e dischi con l'antialiasing (Radial.cpp)
void Put(std::vector<float>& acc, int S, int x, int y, COLORREF c, float a);
void Disc(std::vector<float>& acc, int S, float cx, float cy, float r, float ring, COLORREF c, float alpha);
Panel FinishPanel(const std::vector<float>& acc, int S);   // premoltiplicato -> ARGB

// --- Radiali (0.6.0) -----------------------------------------------------------------------------------------------
// Cerchio al centro dello schermo con le voci intorno (la prima in alto, poi in senso orario) e al centro il titolo della
// voce scelta. Radiale 1: i pulsanti visibili della barra comandi, con l'icona copiata dal buffer del gioco (dove la barra
// e' appena stata disegnata) e il titolo italiano del comando; per un eroe sono formazioni e abilita'. Radiale 2: i
// gruppi 1..9, 0 con il numero di unita'. Si costruisce nel thread del gioco quando cambia qualcosa (voce scelta,
// contenuto, modo), con lo stesso metodo dei simboli: strati di copertura calcolati e testo GDI.
struct RadialItem {
    int id = 0;                        // comando (radiale 1) o numero del gruppo (radiale 2)
    std::wstring title;
    int count = 0;
    int iw = 0, ih = 0;
    std::vector<uint32_t> icon;        // 0xAARRGGBB
};
std::wstring Widen(const char* s);           // Windows-1252 -> UTF-16
void TakeIcon(const Surface& s, const RECT& r, RadialItem& it);
Panel BuildRadial(int kind, const std::vector<RadialItem>& items, int hi, bool assign, const std::wstring& hint, int bufH);

// Cursore disegnato (Cursor.cpp, 0.8.0)
Panel BuildCursor(int height);

} // namespace ovl
