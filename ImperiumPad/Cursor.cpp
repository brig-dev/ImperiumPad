// Cursor.cpp - freccia del cursore disegnato (0.8.0)
// Linguaggio: C++17, MSVC, Win32 (x86).
//
// Serve solo quando il cursore di Windows non si puo' spostare (Input.cpp, cursore disegnato): una freccia bianca con il
// bordo nero, la punta in alto a sinistra (e' il punto del clic), con lo stesso metodo a strati dei simboli.

#include "OverlayDraw.h"
#include <algorithm>

namespace ovl {

Panel BuildCursor(int height)
{
    Panel p;
    const int size = (std::max)(16, height);
    Mask m;
    if (!MaskOpen(m, size * 4)) return p;
    const float k = m.S / 22.0f;   // freccia disegnata su una griglia 22 x 22
    const POINT arrow[7] = { { static_cast<LONG>(1 * k), static_cast<LONG>(1 * k) }, { static_cast<LONG>(1 * k), static_cast<LONG>(18 * k) },
                             { static_cast<LONG>(5.5f * k), static_cast<LONG>(14 * k) }, { static_cast<LONG>(9 * k), static_cast<LONG>(21 * k) },
                             { static_cast<LONG>(12 * k), static_cast<LONG>(19.5f * k) }, { static_cast<LONG>(8.5f * k), static_cast<LONG>(13 * k) },
                             { static_cast<LONG>(14 * k), static_cast<LONG>(13 * k) } };
    HGDIOBJ oldPen = SelectObject(m.dc, GetStockObject(NULL_PEN));
    HGDIOBJ oldBrush = SelectObject(m.dc, GetStockObject(WHITE_BRUSH));
    Polygon(m.dc, arrow, 7);
    const std::vector<uint8_t> fill = MaskTake(m, size);
    HPEN pen = CreatePen(PS_SOLID, (std::max)(4, m.S / 14), RGB(255, 255, 255));
    SelectObject(m.dc, pen);
    SelectObject(m.dc, GetStockObject(NULL_BRUSH));
    Polygon(m.dc, arrow, 7);
    const std::vector<uint8_t> edge = MaskTake(m, size);
    SelectObject(m.dc, oldBrush);
    SelectObject(m.dc, oldPen);
    DeleteObject(pen);
    MaskClose(m);
    std::vector<float> acc(static_cast<size_t>(size) * size * 4, 0.0f);
    Layer(acc, fill, RGB(250, 246, 236), 255);
    Layer(acc, edge, RGB(12, 10, 8), 255);
    return FinishPanel(acc, size);
}

} // namespace ovl
