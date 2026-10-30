// test_overlay.cpp - prova fuori dal gioco di Overlay.cpp (C++17, MSVC, Win32)
// Simula le copie di gbr.exe con SetDIBitsToDevice da un buffer a 16 bit 1920x1080 (RGB565 con maschere o RGB555 senza
// compressione, quello visto nel gioco) verso una DIB a 32 bit e controlla:
// pannelli disegnati, posizione con la coordinata verticale contata dal basso (come fa il gioco), copie parziali,
// buffer del gioco intatto dopo ogni copia, cancellazione del pannello quando sparisce. Salva le immagini in BMP.
// Compilazione: tools/test_overlay/build.bat (usa lo stesso Overlay.cpp della mod).

#include "../../ImperiumPad/ImperiumPad.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

void Log(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

int InputPatchImport(HMODULE mod, void* target, void* hook)
{
    auto base = reinterpret_cast<uint8_t*>(mod);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    int count = 0;
    for (auto imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp)
        for (auto t = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + imp->FirstThunk); t->u1.Function; ++t)
            if (reinterpret_cast<void*>(t->u1.Function) == target) {
                DWORD old;
                VirtualProtect(&t->u1.Function, 4, PAGE_READWRITE, &old);
                t->u1.Function = reinterpret_cast<DWORD>(hook);
                VirtualProtect(&t->u1.Function, 4, old, &old);
                ++count;
            }
    return count;
}

// Finti del resto della mod: nella prova non c'e' il gioco
Config g_cfg;
const char* GameDir() { return "."; }
const char* GlyphStyleName(GlyphStyle s) { return s == GlyphStyle::Xbox ? "Xbox" : s == GlyphStyle::PlayStation ? "PlayStation" : "Steam"; }
bool GameRead(GameState& s) { s = GameState{}; return false; }
int  GameUiControls(UiControl*, int, uint32_t* top) { *top = 0; return 0; }
int  GameBarButtons(RECT*, int*, int) { return 0; }
bool GameBarClick(int, char* name, int cap) { if (cap > 0) name[0] = 0; return false; }
bool GameCommandTitle(int, char* out, int cap) { if (cap > 0) out[0] = 0; return false; }
bool GameGroupCounts(int counts[10]) { const int c[10] = { 0, 12, 5, 0, 1, 0, 0, 30, 0, 0 }; for (int i = 0; i < 10; ++i) counts[i] = c[i]; return true; }

namespace {

constexpr int W = 1920, H = 1080;

struct Bmi16 { BITMAPINFOHEADER h; DWORD masks[3]; };

void SaveBmp(const char* path, const uint32_t* px)
{
    BITMAPFILEHEADER f{};
    BITMAPINFOHEADER h{};
    h.biSize = sizeof(h); h.biWidth = W; h.biHeight = -H; h.biPlanes = 1; h.biBitCount = 32;
    f.bfType = 0x4d42; f.bfOffBits = sizeof(f) + sizeof(h); f.bfSize = f.bfOffBits + W * H * 4;
    FILE* fp = nullptr;
    fopen_s(&fp, path, "wb");
    if (!fp) return;
    fwrite(&f, sizeof(f), 1, fp); fwrite(&h, sizeof(h), 1, fp); fwrite(px, 4, static_cast<size_t>(W) * H, fp);
    fclose(fp);
    printf("salvata %s\n", path);
}

int Run(bool topDown, bool rgb555, const char* outPrefix)
{
    // buffer del gioco: righe con sfumatura e una griglia, in RGB565
    std::vector<uint16_t> game(static_cast<size_t>(W) * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const int r = (x * 31) / W, g = (y * 63) / H, b = ((x / 64 + y / 64) & 1) ? 20 : 6;
            game[static_cast<size_t>(y) * W + x] = rgb555 ? static_cast<uint16_t>((r << 10) | ((g >> 1) << 5) | b)
                                                           : static_cast<uint16_t>((r << 11) | (g << 5) | b);
        }
    // in memoria: se dal basso, la riga 0 dall'alto sta in fondo
    std::vector<uint16_t> mem(game.size());
    for (int y = 0; y < H; ++y) memcpy(&mem[static_cast<size_t>(topDown ? y : H - 1 - y) * W], &game[static_cast<size_t>(y) * W], W * 2);
    const std::vector<uint16_t> clean = mem;

    Bmi16 bmi{};
    bmi.h.biSize = sizeof(bmi.h); bmi.h.biWidth = W; bmi.h.biHeight = topDown ? -H : H; bmi.h.biPlanes = 1;
    bmi.h.biBitCount = 16; bmi.h.biCompression = rgb555 ? BI_RGB : BI_BITFIELDS;
    if (!rgb555) { bmi.masks[0] = 0xf800; bmi.masks[1] = 0x7e0; bmi.masks[2] = 0x1f; }
    else { bmi.masks[0] = 0x00010000; bmi.masks[1] = 0x00030002; bmi.masks[2] = 0x00050004; }   // come nel gioco: tabella, non maschere

    // destinazione: DIB a 32 bit (come la finestra)
    BITMAPINFO di{};
    di.bmiHeader.biSize = sizeof(di.bmiHeader); di.bmiHeader.biWidth = W; di.bmiHeader.biHeight = -H;
    di.bmiHeader.biPlanes = 1; di.bmiHeader.biBitCount = 32;
    void* destBits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &di, DIB_RGB_COLORS, &destBits, nullptr, 0);
    SelectObject(dc, bmp);

    int errors = 0;
    auto gameBlit = [&](int x, int y, int w, int h) {
        // stessa chiamata di gbr.exe (0x677f2a): ySrc = H - h - y, righe 0..H
        SetDIBitsToDevice(dc, x, y, w, h, x, H - h - y, 0, H, mem.data(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
        GdiFlush();
        if (mem != clean) { printf("ERRORE: buffer del gioco modificato dopo la copia %d,%d %dx%d\n", x, y, w, h); ++errors; }
    };

    // 1) copia intera con riquadro e aiuto visibili
    gameBlit(0, 0, W, H);
    char path[MAX_PATH];
    sprintf_s(path, "%s_1_intera.bmp", outPrefix);
    SaveBmp(path, static_cast<uint32_t*>(destBits));

    // 1b) subito dopo, una copia parziale del gioco proprio sopra il centro dell'aiuto (nessun cambio dei pannelli):
    // a schermo l'aiuto deve restare, perche' il pezzo copiato dal gioco lo contiene
    gameBlit(W / 2 - 100, H / 2 - 20, 200, 40);
    {
        const uint32_t* d = static_cast<uint32_t*>(destBits);
        int clean = 0;
        for (int y = H / 2 - 20; y < H / 2 + 20; ++y)
            for (int x = W / 2 - 100; x < W / 2 + 100; ++x) {
                const uint16_t s = game[static_cast<size_t>(y) * W + x];
                const int sr = ((s >> (rgb555 ? 10 : 11)) & 31) * 255 / 31;
                if (std::abs(static_cast<int>((d[static_cast<size_t>(y) * W + x] >> 16) & 0xff) - sr) <= 9) ++clean;
            }
        // con il pannello scuro sopra quasi nessun pixel coincide col gioco
        if (clean > 200 * 40 / 4) { printf("ERRORE: dopo una copia parziale l'aiuto e' sparito dal centro (%d pixel puliti)\n", clean); ++errors; }
    }

    // 2) aiuto nascosto, solo una piccola copia parziale lontana: il pannello dell'aiuto deve essere cancellato
    OverlayHelp(false, L"", nullptr, L"");
    gameBlit(1700, 900, 100, 100);
    sprintf_s(path, "%s_2_aiuto_nascosto.bmp", outPrefix);
    SaveBmp(path, static_cast<uint32_t*>(destBits));

    // controllo: al centro (dove c'era l'aiuto) la destinazione deve essere uguale al buffer del gioco
    const uint32_t* d = static_cast<uint32_t*>(destBits);
    int diff = 0;
    for (int y = H / 2 - 50; y < H / 2 + 50; ++y)
        for (int x = W / 2 - 50; x < W / 2 + 50; ++x) {
            const uint16_t s = game[static_cast<size_t>(y) * W + x];
            const int sr = ((s >> (rgb555 ? 10 : 11)) & 31) * 255 / 31, sb = (s & 31) * 255 / 31;
            const uint32_t v = d[static_cast<size_t>(y) * W + x];
            if (std::abs(static_cast<int>((v >> 16) & 0xff) - sr) > 9 || std::abs(static_cast<int>(v & 0xff) - sb) > 9) ++diff;
        }
    if (diff) { printf("ERRORE: dopo aver nascosto l'aiuto restano %d pixel diversi al centro\n", diff); ++errors; }

    DeleteObject(bmp);
    DeleteDC(dc);
    return errors;
}

// Simboli del pad sulla barra (0.5.0): 14 pulsanti finti in basso, buffer RGB555 come nel gioco. Controlla che i primi
// dodici abbiano il simbolo nell'angolo in basso a destra, gli altri no, che il buffer resti pulito e che i simboli
// spariscano quando il pad si spegne. Salva la barra ingrandita per ogni stile.
int RunGlyphs(const char* out)
{
    std::vector<uint16_t> mem(static_cast<size_t>(W) * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) mem[static_cast<size_t>(y) * W + x] = static_cast<uint16_t>((((x / 8) & 31) << 10) | (12 << 5) | 6);
    const std::vector<uint16_t> clean = mem;
    Bmi16 bmi{};
    bmi.h.biSize = sizeof(bmi.h); bmi.h.biWidth = W; bmi.h.biHeight = -H; bmi.h.biPlanes = 1; bmi.h.biBitCount = 16;
    bmi.h.biCompression = BI_RGB;
    BITMAPINFO di{};
    di.bmiHeader.biSize = sizeof(di.bmiHeader); di.bmiHeader.biWidth = W; di.bmiHeader.biHeight = -H;
    di.bmiHeader.biPlanes = 1; di.bmiHeader.biBitCount = 32;
    void* destBits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &di, DIB_RGB_COLORS, &destBits, nullptr, 0);
    SelectObject(dc, bmp);
    const uint32_t* d = static_cast<uint32_t*>(destBits);
    int errors = 0;
    auto gameBlit = [&](int x, int y, int w, int h) {
        SetDIBitsToDevice(dc, x, y, w, h, x, H - h - y, 0, H, mem.data(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
        GdiFlush();
        if (mem != clean) { printf("ERRORE: buffer del gioco modificato (simboli)\n"); ++errors; }
    };
    // pixel della destinazione diversi dal gioco nel quadrato in basso a destra del pulsante
    auto inkIn = [&](const RECT& b) {
        int n = 0;
        for (int y = b.bottom - 12; y < b.bottom - 4; ++y)
            for (int x = b.right - 12; x < b.right - 4; ++x) {
                const uint16_t s = clean[static_cast<size_t>(y) * W + x];
                const int sr = ((s >> 10) & 31) * 255 / 31;
                if (std::abs(static_cast<int>((d[static_cast<size_t>(y) * W + x] >> 16) & 0xff) - sr) > 12) ++n;
            }
        return n;
    };
    std::vector<RECT> bar;
    for (int i = 0; i < 14; ++i) bar.push_back({ 400 + i * 56, 1018, 400 + i * 56 + 52, 1070 });
    OverlayBadge(L"", L"", 0);
    OverlayTestBar(&bar);
    const GlyphStyle styles[3] = { GlyphStyle::Steam, GlyphStyle::Xbox, GlyphStyle::PlayStation };
    const char* names[3] = { "steam", "xbox", "playstation" };
    for (int k = 0; k < 3; ++k) {
        OverlayPad(true, true, styles[k], k == 1, 0);
        Sleep(120);
        gameBlit(0, 0, W, H);
        for (int i = 0; i < 14; ++i) {
            const int n = inkIn(bar[i]);
            if (i < kSlotCount && n == 0) { printf("ERRORE: %s, pulsante %d senza simbolo\n", names[k], i + 1); ++errors; }
            if (i >= kSlotCount && n != 0) { printf("ERRORE: %s, pulsante %d con un simbolo (oltre il dodicesimo)\n", names[k], i + 1); ++errors; }
        }
        // ritaglio della barra per guardarla
        char path[MAX_PATH];
        sprintf_s(path, "%s\\simboli_%s.bmp", out, names[k]);
        const int cx = 396, cy = 1012, cw = 14 * 56 + 8, ch = 64;
        std::vector<uint32_t> crop(static_cast<size_t>(cw) * ch);
        for (int y = 0; y < ch; ++y) memcpy(&crop[static_cast<size_t>(y) * cw], &d[static_cast<size_t>(cy + y) * W + cx], cw * 4);
        BITMAPFILEHEADER f{}; BITMAPINFOHEADER h{};
        h.biSize = sizeof(h); h.biWidth = cw; h.biHeight = -ch; h.biPlanes = 1; h.biBitCount = 32;
        f.bfType = 0x4d42; f.bfOffBits = sizeof(f) + sizeof(h); f.bfSize = f.bfOffBits + cw * ch * 4;
        FILE* fp = nullptr;
        fopen_s(&fp, path, "wb");
        if (fp) { fwrite(&f, sizeof(f), 1, fp); fwrite(&h, sizeof(h), 1, fp); fwrite(crop.data(), 4, crop.size(), fp); fclose(fp); printf("salvata %s\n", path); }
    }
    // pad spento: una piccola copia lontana deve bastare a cancellare i simboli
    OverlayPad(false, true, GlyphStyle::Steam, false, 0);
    gameBlit(10, 10, 50, 50);
    for (int i = 0; i < kSlotCount; ++i)
        if (inkIn(bar[i])) { printf("ERRORE: simbolo %d rimasto dopo lo spegnimento del pad\n", i + 1); ++errors; break; }
    OverlayTestBar(nullptr);
    DeleteObject(bmp);
    DeleteDC(dc);
    return errors;
}

// Radiali (0.6.0): radiale dei comandi con 12 pulsanti finti (icone copiate dal buffer) e radiale dei gruppi (normale e
// in modo "assegna"); controlla che compaiano al centro, che il buffer resti pulito e che spariscano alla chiusura.
int RunRadials(const char* out)
{
    std::vector<uint16_t> mem(static_cast<size_t>(W) * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            uint16_t v = static_cast<uint16_t>((((x / 8) & 31) << 10) | (((y / 8) & 31) << 5) | 8);
            if (y >= 1018 && y < 1070 && x >= 400 && x < 400 + 12 * 56 && ((x - 400) % 56) < 52)   // "icone" colorate
                v = static_cast<uint16_t>((((x - 400) / 56 * 2) << 10) | (((y - 1018) / 2) << 5) | (31 - (x - 400) / 56 * 2));
            mem[static_cast<size_t>(y) * W + x] = v;
        }
    // sfondo vero facoltativo: una schermata del gioco gia' in RGB555 1920x1080 (sfondo555.raw nella cartella di uscita),
    // con la barra di un eroe a 12 pulsanti da 630,1027 con passo 55 (come nel log del gioco)
    bool real = false;
    {
        char path[MAX_PATH];
        sprintf_s(path, "%s\\sfondo555.raw", out);
        FILE* fp = nullptr;
        fopen_s(&fp, path, "rb");
        if (fp) { real = fread(mem.data(), 2, mem.size(), fp) == mem.size(); fclose(fp); }
    }
    const std::vector<uint16_t> clean = mem;
    Bmi16 bmi{};
    bmi.h.biSize = sizeof(bmi.h); bmi.h.biWidth = W; bmi.h.biHeight = -H; bmi.h.biPlanes = 1; bmi.h.biBitCount = 16;
    bmi.h.biCompression = BI_RGB;
    BITMAPINFO di{};
    di.bmiHeader.biSize = sizeof(di.bmiHeader); di.bmiHeader.biWidth = W; di.bmiHeader.biHeight = -H;
    di.bmiHeader.biPlanes = 1; di.bmiHeader.biBitCount = 32;
    void* destBits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &di, DIB_RGB_COLORS, &destBits, nullptr, 0);
    SelectObject(dc, bmp);
    const uint32_t* d = static_cast<uint32_t*>(destBits);
    int errors = 0;
    auto gameBlit = [&](int x, int y, int w, int h) {
        SetDIBitsToDevice(dc, x, y, w, h, x, H - h - y, 0, H, mem.data(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
        GdiFlush();
        if (mem != clean) { printf("ERRORE: buffer del gioco modificato (radiali)\n"); ++errors; }
    };
    auto centreChanged = [&]() {
        int n = 0;
        for (int y = H / 2 - 40; y < H / 2 + 40; ++y)
            for (int x = W / 2 - 40; x < W / 2 + 40; ++x) {
                const uint16_t sv = clean[static_cast<size_t>(y) * W + x];
                const int sr = ((sv >> 10) & 31) * 255 / 31;
                if (std::abs(static_cast<int>((d[static_cast<size_t>(y) * W + x] >> 16) & 0xff) - sr) > 12) ++n;
            }
        return n;
    };
    auto save = [&](const char* name) {
        char path[MAX_PATH];
        sprintf_s(path, "%s\\%s.bmp", out, name);
        const int cx = W / 2 - 360, cy = H / 2 - 350, cw = 720, ch = 700;
        std::vector<uint32_t> crop(static_cast<size_t>(cw) * ch);
        for (int y = 0; y < ch; ++y) memcpy(&crop[static_cast<size_t>(y) * cw], &d[static_cast<size_t>(cy + y) * W + cx], cw * 4);
        BITMAPFILEHEADER f{}; BITMAPINFOHEADER h{};
        h.biSize = sizeof(h); h.biWidth = cw; h.biHeight = -ch; h.biPlanes = 1; h.biBitCount = 32;
        f.bfType = 0x4d42; f.bfOffBits = sizeof(f) + sizeof(h); f.bfSize = f.bfOffBits + cw * ch * 4;
        FILE* fp = nullptr;
        fopen_s(&fp, path, "wb");
        if (fp) { fwrite(&f, sizeof(f), 1, fp); fwrite(&h, sizeof(h), 1, fp); fwrite(crop.data(), 4, crop.size(), fp); fclose(fp); printf("salvata %s\n", path); }
    };
    std::vector<RECT> bar;
    for (int i = 0; i < 12; ++i)
        bar.push_back(real ? RECT{ 630 + i * 55, 1027, 630 + i * 55 + 51, 1079 } : RECT{ 400 + i * 56, 1018, 400 + i * 56 + 52, 1070 });
    OverlayTestBar(&bar);
    OverlayPad(true, true, GlyphStyle::Xbox, false, 0);
    struct Case { int kind, hi; bool assign; const wchar_t* hint; const char* name; };
    const Case cases[] = {
        { 1, 2, false, L"Lascia RB: esegue   B: annulla", "radiale_comandi" },
        { 2, 1, false, L"Lascia LB: seleziona   tieni LT: assegna   B: annulla", "radiale_gruppi" },
        { 2, 7, true, L"Lascia LB: seleziona   tieni LT: assegna   B: annulla", "radiale_gruppi_assegna" },
    };
    for (const Case& c : cases) {
        OverlayRadial(c.kind, c.hi, c.assign, c.hint);
        Sleep(120);
        gameBlit(0, 0, W, H);      // legge le voci
        gameBlit(0, 0, W, H);      // disegna con le voci lette
        if (OverlayRadialCount() != (c.kind == 1 ? 12 : 10)) { printf("ERRORE: %s, voci %d\n", c.name, OverlayRadialCount()); ++errors; }
        if (!real && centreChanged() < 1000) { printf("ERRORE: %s non disegnato al centro\n", c.name); ++errors; }
        save(c.name);
    }
    OverlayRadial(0, -1, false, nullptr);
    gameBlit(10, 10, 40, 40);
    if (centreChanged() != 0) { printf("ERRORE: radiale rimasto dopo la chiusura (%d pixel)\n", centreChanged()); ++errors; }
    if (real) printf("radiali provati sulla schermata vera\n");
    OverlayPad(false, false, GlyphStyle::Steam, false, 0);
    OverlayTestBar(nullptr);
    DeleteObject(bmp);
    DeleteDC(dc);
    return errors;
}

// Lingue (0.7.0): aiuto e radiale dei gruppi in italiano, inglese e spagnolo, salvati per guardare testi e accenti.
int RunLanguages(const char* out)
{
    std::vector<uint16_t> mem(static_cast<size_t>(W) * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) mem[static_cast<size_t>(y) * W + x] = static_cast<uint16_t>((((x / 16) & 15) << 10) | (((y / 16) & 15) << 5) | 10);
    Bmi16 bmi{};
    bmi.h.biSize = sizeof(bmi.h); bmi.h.biWidth = W; bmi.h.biHeight = -H; bmi.h.biPlanes = 1; bmi.h.biBitCount = 16;
    bmi.h.biCompression = BI_RGB;
    BITMAPINFO di{};
    di.bmiHeader.biSize = sizeof(di.bmiHeader); di.bmiHeader.biWidth = W; di.bmiHeader.biHeight = -H;
    di.bmiHeader.biPlanes = 1; di.bmiHeader.biBitCount = 32;
    void* destBits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &di, DIB_RGB_COLORS, &destBits, nullptr, 0);
    SelectObject(dc, bmp);
    const char* langs[3] = { "Italian", "English", "Spanish" };
    for (int k = 0; k < 3; ++k) {
        g_cfg.language = langs[k];
        LangInit();
        // aiuto: qualche riga con le parole piu' lunghe e gli accenti
        std::vector<std::wstring> rows = {
            std::wstring(Tr(T_PadRightStick)) + L"\t" + Tr(T_KeyArrows) + L"\t" + Tr(T_ActCamera),
            std::wstring(Tr(T_PsCircle)) + L"\tK\t" + Tr(T_ActCancel),
            TrF(T_HelpHeldPlusKey, L"R2") + L"\t" + TrF(T_KeyPlusKey, L"P") + L"\t" + TrF(T_ActBarCommand, L"L2"),
            std::wstring(L"R1 / L1\tO / H\t") + Tr(T_ActHeroes),
            std::wstring(Tr(T_PadDown)) + L"\tV\t" + Tr(T_ActLastAlert),
            std::wstring(Tr(T_PadMenus)) + L"\tZ X C V\t" + Tr(T_ActMenuJump),
        };
        const std::wstring foot = std::wstring(Tr(T_FootLaptop)) + TrF(T_FootStyle, L"Create");
        OverlayHelp(true, TrF(T_HelpTitle, L"PlayStation").c_str(), &rows, foot.c_str());
        std::vector<RECT> bar;
        OverlayTestBar(&bar);
        OverlayPad(true, true, GlyphStyle::PlayStation, false, 0);
        std::wstring hint = TrF(T_HintRelease, L"L1") + Tr(T_HintSelect) + TrF(T_HintAssign, L"L2") + TrF(T_HintCancel, Tr(T_PsCircle));
        OverlayRadial(2, 3, false, hint.c_str());
        Sleep(120);
        for (int i = 0; i < 2; ++i) {
            SetDIBitsToDevice(dc, 0, 0, W, H, 0, 0, 0, H, mem.data(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
            GdiFlush();
        }
        char path[MAX_PATH];
        sprintf_s(path, "%s\\lingua_%s.bmp", out, langs[k]);
        SaveBmp(path, static_cast<uint32_t*>(destBits));
        OverlayRadial(0, -1, false, nullptr);
        OverlayHelp(false, L"", nullptr, L"");
        OverlayTestBar(nullptr);
        OverlayPad(false, false, GlyphStyle::Steam, false, 0);
    }
    g_cfg.language = "Auto";
    DeleteObject(bmp);
    DeleteDC(dc);
    return 0;
}

// Cursore disegnato (0.8.0): la freccia compare con la punta nel punto chiesto (coordinate della finestra 1280x720, buffer
// 1920x1080: deve essere portata in proporzione), segue gli spostamenti e sparisce quando si spegne.
int RunCursor(const char* out)
{
    std::vector<uint16_t> mem(static_cast<size_t>(W) * H, static_cast<uint16_t>((8 << 10) | (12 << 5) | 6));
    const std::vector<uint16_t> clean = mem;
    Bmi16 bmi{};
    bmi.h.biSize = sizeof(bmi.h); bmi.h.biWidth = W; bmi.h.biHeight = -H; bmi.h.biPlanes = 1; bmi.h.biBitCount = 16;
    bmi.h.biCompression = BI_RGB;
    BITMAPINFO di{};
    di.bmiHeader.biSize = sizeof(di.bmiHeader); di.bmiHeader.biWidth = W; di.bmiHeader.biHeight = -H;
    di.bmiHeader.biPlanes = 1; di.bmiHeader.biBitCount = 32;
    void* destBits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &di, DIB_RGB_COLORS, &destBits, nullptr, 0);
    SelectObject(dc, bmp);
    const uint32_t* d = static_cast<uint32_t*>(destBits);
    int errors = 0;
    auto blit = [&](int x, int y, int w, int h) {
        SetDIBitsToDevice(dc, x, y, w, h, x, H - h - y, 0, H, mem.data(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
        GdiFlush();
        if (mem != clean) { printf("ERRORE: buffer del gioco modificato (cursore)\n"); ++errors; }
    };
    auto white = [&](int x, int y) { const uint32_t v = d[static_cast<size_t>(y) * W + x]; return ((v >> 16) & 0xff) > 200 && ((v >> 8) & 0xff) > 200; };
    OverlayCursor(true, 640, 360, 1280, 720);       // -> 960, 540 nel buffer
    blit(0, 0, W, H);
    if (!white(964, 552)) { printf("ERRORE: freccia del cursore non disegnata sotto la punta\n"); ++errors; }
    if (white(950, 530)) { printf("ERRORE: freccia del cursore fuori posto\n"); ++errors; }
    SaveBmp((std::string(out) + "\\cursore.bmp").c_str(), static_cast<const uint32_t*>(destBits));
    OverlayCursor(true, 100, 100, 1280, 720);       // spostato: il posto vecchio va ripulito
    blit(0, 0, 10, 10);
    if (white(964, 552)) { printf("ERRORE: freccia rimasta nel posto vecchio\n"); ++errors; }
    if (!white(154, 162)) { printf("ERRORE: freccia non disegnata nel posto nuovo\n"); ++errors; }
    OverlayCursor(false, 0, 0, 0, 0);
    blit(0, 0, 10, 10);
    if (white(154, 162)) { printf("ERRORE: freccia rimasta dopo lo spegnimento\n"); ++errors; }
    DeleteObject(bmp);
    DeleteDC(dc);
    return errors;
}

} // namespace

int main(int argc, char** argv)
{
    const char* out = argc > 1 ? argv[1] : ".";
    OverlayInstall();
    OverlayBadge(L"ImperiumPad 0.4.0   (View tenuto: aiuto)", L"Pad simulato con le lettere: ATTIVO (Bloc Maiusc spento)", 60000);
    std::vector<std::wstring> rows = {
        L"Leva sinistra\tW A S D\tCursore (al bordo scorre la mappa)",
        L"A\tJ\tSeleziona; tenuto: riquadro; doppio: stesso tipo o esercito",
        L"Croce giu'\tV\tUltimo avviso",
    };
    char prefix[MAX_PATH];
    int errors = 0;
    const char* names[] = { "565_dall_alto", "565_dal_basso", "555_dall_alto" };
    for (int pass = 0; pass < 3; ++pass) {
        OverlayHelp(true, L"ImperiumPad: comandi del pad", &rows, L"Bloc Num o Bloc Maiusc: spegne e riaccende il pad");
        sprintf_s(prefix, "%s\\overlay_%s", out, names[pass]);
        errors += Run(pass != 1, pass == 2, prefix);
    }
    errors += RunGlyphs(out);
    errors += RunRadials(out);
    errors += RunLanguages(out);
    errors += RunCursor(out);
    printf(errors ? "PROVA FALLITA: %d errori\n" : "PROVA RIUSCITA\n", errors);
    return errors ? 1 : 0;
}
