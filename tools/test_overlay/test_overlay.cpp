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
bool g_testNoGroups = false;   // 0.8.4: radiale con tutti i gruppi vuoti
bool GameGroupCounts(int counts[10]) { const int c[10] = { 0, 12, 5, 0, 1, 0, 0, 30, 0, 0 }; for (int i = 0; i < 10; ++i) counts[i] = g_testNoGroups ? 0 : c[i]; return true; }
bool GameGroupInfo(GroupInfo g[11])
{
    // 0.10.1: gruppo 1 eroe e 24 unita', 2 dodici soldati feriti, 4 un eroe quasi morto, 7 trenta soldati; selezione 18 con eroe
    const int c[11] = { 0, 25, 12, 0, 1, 0, 0, 30, 0, 0, 18 }, h[11] = { 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 },
              hp[11] = { -1, 82, 45, -1, 20, -1, -1, 95, -1, -1, 70 };
    for (int i = 0; i < 11; ++i) { g[i].count = g_testNoGroups && i < 10 ? 0 : c[i]; g[i].heroes = g[i].count ? h[i] : 0; g[i].hp = g[i].count ? hp[i] : -1; }
    return true;
}
int  GameEmptyMenuButtons(RECT*, int*, int) { return 0; }
int  GameCheat(const char*, int) { return 0; }
int  GameCheatSpawn(const char*, int) { return 1; }
bool GameCommandParam(int, char* out, int cap) { if (cap > 0) out[0] = 0; return false; }
bool GameCommandName(int, char* out, int cap) { if (cap > 0) out[0] = 0; return false; }
int  GameNextSettlement(int& index, int& total) { index = 2; total = 3; return 1; }
void GameCheatTick() {}
void InputClickAt(int, int) {}
std::atomic<HWND> g_gameHwnd{ nullptr };

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
    // 0.8.3: Steam con BarSymbols=Hold e RT lasciato (niente simboli sui pulsanti), Xbox con RT tenuto, PlayStation con
    // BarSymbols=Always e RT lasciato; in tutti i casi il segno di RT a sinistra del primo pulsante
    const GlyphStyle styles[3] = { GlyphStyle::Steam, GlyphStyle::Xbox, GlyphStyle::PlayStation };
    const char* names[3] = { "steam_senza_rt", "xbox_rt_tenuto", "playstation_sempre" };
    const RECT hint{ 400 - 32 - 6 + 8, 1018 + 10 + 8, 400 - 6 - 8, 1018 + 10 + 32 - 8 };   // centro del segno di RT
    for (int k = 0; k < 3; ++k) {
        g_cfg.barSymbolsAlways = k == 2;
        OverlayPad(true, true, styles[k], k == 1, 0);
        Sleep(120);
        gameBlit(0, 0, W, H);
        const bool expect = k != 0;
        for (int i = 0; i < 14; ++i) {
            const int n = inkIn(bar[i]);
            if (i < kSlotCount && expect && n == 0) { printf("ERRORE: %s, pulsante %d senza simbolo\n", names[k], i + 1); ++errors; }
            if (i < kSlotCount && !expect && n != 0) { printf("ERRORE: %s, pulsante %d con un simbolo senza RT\n", names[k], i + 1); ++errors; }
            if (i >= kSlotCount && n != 0) { printf("ERRORE: %s, pulsante %d con un simbolo (oltre il dodicesimo)\n", names[k], i + 1); ++errors; }
        }
        if (!inkIn(hint)) { printf("ERRORE: %s, manca il segno di RT accanto alla barra\n", names[k]); ++errors; }
        // ritaglio della barra per guardarla
        char path[MAX_PATH];
        sprintf_s(path, "%s\\simboli_%s.bmp", out, names[k]);
        const int cx = 356, cy = 1012, cw = 14 * 56 + 48, ch = 64;
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
    if (inkIn(hint)) { printf("ERRORE: segno di RT rimasto dopo lo spegnimento del pad\n"); ++errors; }
    g_cfg.barSymbolsAlways = false;
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
    struct Case { int kind, hi; bool assign, noGroups; const wchar_t* hint; const char* name; };
    const Case cases[] = {
        { 1, 2, false, false, L"Lascia RB: esegue   B: annulla", "radiale_comandi" },
        { 2, 1, false, false, L"Lascia LB: seleziona   LT: assegna invece di selezionare   B: annulla", "radiale_gruppi" },
        { 2, 7, true, false, L"Lascia LB: assegna   LT: seleziona invece di assegnare   B: annulla", "radiale_gruppi_assegna" },
        { 2, -1, false, true, L"Seleziona delle unit\u00e0, poi tieni LB, premi LT, scegli un numero e lascia LB.   B: chiudi",
          "radiale_gruppi_vuoti" },
        { 3, 2, false, false, L"Lascia RB: usa   B: annulla", "radiale_trucchi" },
    };
    // 0.9.0: voci del radiale dei trucchi per un edificio (come Core.cpp), dieci come i gruppi
    {
        std::vector<CheatEntry> ch = {
            { L"+1K", L"Oro +1000 all'insediamento", 1, CI_Coins }, { L"+10K", L"Oro +10000 all'insediamento", 1, CI_Coins },
            { L"MAX", L"Oro +100000 all'insediamento (il massimo per volta)", 1, CI_Coins }, { L"+1K", L"Cibo +1000", 2, CI_Loaf },
            { L"+10K", L"Cibo +10000", 2, CI_Loaf }, { L"MAX", L"Cibo +100000", 2, CI_Loaf }, { L"+50", L"Popolazione massima +50", 0, CI_House },
            { L"+100", L"Abitanti +100", 0, CI_People }, { L"", L"Ripara l'edificio", 0, CI_Hammer }, { L"", L"Cattura l'insediamento", 3, CI_Flag } };
        OverlayCheatItems(ch);
    }
    for (const Case& c : cases) {
        g_testNoGroups = c.noGroups;
        OverlayRadial(c.kind, c.hi, c.assign, c.hint);
        Sleep(120);
        gameBlit(0, 0, W, H);      // legge le voci
        gameBlit(0, 0, W, H);      // disegna con le voci lette
        if (OverlayRadialCount() != (c.kind == 1 ? 12 : 10)) { printf("ERRORE: %s, voci %d\n", c.name, OverlayRadialCount()); ++errors; }
        if (!real && centreChanged() < 1000) { printf("ERRORE: %s non disegnato al centro\n", c.name); ++errors; }
        save(c.name);
    }
    g_testNoGroups = false;
    // 0.9.1: le icone degli altri due radiali dei trucchi (unita' e niente selezionato), una sola immagine ciascuno
    {
        const std::vector<CheatEntry> units = { { L"", L"Cura", 3, CI_Heart }, { L"", L"Viveri", 2, CI_Loaf }, { L"10", L"Livello 10", 1, CI_Star },
            { L"50", L"Livello 50", 1, CI_Star }, { L"MAX", L"Livello massimo", 1, CI_Star }, { L"", L"Elimina", 3, CI_Skull } };
        const std::vector<CheatEntry> none = { { L"", L"Mappa", 4, CI_Map }, { L"", L"Visione", 4, CI_Eye }, { L"", L"Pace", 0, CI_WhiteFlag },
            { L"", L"Tecnologie", 0, CI_Book }, { L"EROI", L"Esperienza eroi", 1, CI_Star }, { L"SOLDATI", L"Esperienza soldati", 0, CI_Star } };
        const std::vector<CheatEntry>* sets[2] = { &units, &none };
        const char* names2[2] = { "radiale_trucchi_unita", "radiale_trucchi_niente" };
        for (int k = 0; k < 2; ++k) {
            OverlayCheatItems(*sets[k]);
            OverlayRadial(3, 1, false, L"Lascia RB: usa   B: annulla");
            Sleep(120);
            gameBlit(0, 0, W, H);
            gameBlit(0, 0, W, H);
            save(names2[k]);
        }
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
        std::wstring hint = TrF(T_HintRelease, L"L1") + Tr(T_HintSelect) + TrF(T_HintToAssign, L"L2") + TrF(T_HintCancel, Tr(T_PsCircle));
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

// Vetrina (0.10.0): le novita' disegnate sopra schermate vere del gioco (sfondi RGB555 1920x1080 dall'alto, preparati
// a parte: vetrina_fortezza.raw, vetrina_menu.raw nella cartella di uscita), per guardarle come saranno nel gioco.
int RunShowcase(const char* out)
{
    auto load = [&](const char* name, std::vector<uint16_t>& mem) {
        char path[MAX_PATH];
        sprintf_s(path, "%s\\%s", out, name);
        FILE* fp = nullptr;
        fopen_s(&fp, path, "rb");
        if (!fp) return false;
        const bool ok = fread(mem.data(), 2, mem.size(), fp) == mem.size();
        fclose(fp);
        return ok;
    };
    std::vector<uint16_t> fort(static_cast<size_t>(W) * H), menu(static_cast<size_t>(W) * H);
    if (!load("vetrina_fortezza.raw", fort) || !load("vetrina_menu.raw", menu)) { printf("vetrina: sfondi mancanti, saltata\n"); return 0; }
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
    auto shot = [&](std::vector<uint16_t>& mem, const char* name) {
        Sleep(130);
        for (int i = 0; i < 3; ++i) {
            SetDIBitsToDevice(dc, 0, 0, W, H, 0, 0, 0, H, mem.data(), reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS);
            GdiFlush();
            Sleep(110);
        }
        char path[MAX_PATH];
        sprintf_s(path, "%s\\%s.bmp", out, name);
        SaveBmp(path, static_cast<uint32_t*>(destBits));
    };
    g_cfg.language = "Italian";
    LangInit();
    OverlayBadge(L"", L"", 0);
    // 1) impostazioni, riga scelta "Dimensione di simboli e testi"
    {
        const Txt labels[] = { T_SetLanguage, T_SetSymbols, T_SetBarSymbols, T_SetUiScale, T_SetCursorSpeed, T_SetCameraSpeed,
                               T_SetAnalogCamera, T_SetMagnet, T_SetVibration, T_SetSlowMotion, T_SetHideLetters, T_SetCursorMode, T_SetButtons };
        const std::wstring values[] = { L"Automatico (come il gioco)", L"Steam", TrF(T_ValWhileHeld, L"R2"), L"Grande (125%)", L"1.3", L"1.0",
                                        Tr(T_ValOn), Tr(T_ValMedium), Tr(T_ValStrong), Tr(T_ValOn), Tr(T_ValOn), Tr(T_ValAuto), Tr(T_ValOpen) };
        std::vector<std::wstring> rows;
        for (int i = 0; i < 13; ++i) {
            const bool sel = i == 3;
            rows.push_back(std::wstring(sel ? L"\u25b6 " : L"    ") + Tr(labels[i]) + L"\t\t" + (sel ? L"\u25c0  " + values[i] + L"  \u25b6" : values[i]));
        }
        OverlayHelp(true, Tr(T_SetTitle), &rows, TrF(T_SetFoot, L"A", L"B", L"Menu").c_str());
        shot(fort, "vetrina_1_impostazioni");
    }
    // 2) pagina dei tasti, A e B scambiati, riga scelta B
    {
        const wchar_t* names[] = { L"A", L"B", L"X", L"Y", L"L1", L"R1", L"View", L"Menu", L"L3", L"R3", Tr(T_PadUp), Tr(T_PadDown), Tr(T_PadLeft), Tr(T_PadRight) };
        std::vector<std::wstring> rows;
        for (int i = 0; i < 14; ++i) {
            const bool sel = i == 1;
            std::wstring value = i == 0 ? L"B" : i == 1 ? L"A" : std::wstring(names[i]) + L"  " + Tr(T_ButtonsSame);
            rows.push_back(std::wstring(sel ? L"\u25b6 " : L"    ") + names[i] + L"\t\t" + (sel ? L"\u25c0  " + value + L"  \u25b6" : value));
        }
        OverlayHelp(true, Tr(T_ButtonsTitle), &rows, TrF(T_ButtonsFoot, L"X", L"B").c_str());
        shot(fort, "vetrina_2_tasti");
    }
    // 3) aiuto con le righe nuove (controller vero: due colonne)
    {
        const std::wstring rows[] = {
            std::wstring(Tr(T_PadLeftStick)) + L"\t\t" + Tr(T_ActCursor), std::wstring(Tr(T_PadRightStick)) + L"\t\t" + Tr(T_ActCamera),
            std::wstring(L"A\t\t") + Tr(T_ActSelect), std::wstring(L"X\t\t") + Tr(T_ActAction), std::wstring(L"B\t\t") + Tr(T_ActCancel),
            std::wstring(L"L2\t\t") + Tr(T_ActShift), TrF(T_HelpHeldPlusKey, L"R2") + L"\t\t" + TrF(T_ActBarCommand, L"L2"),
            std::wstring(L"R1 / L1\t\t") + Tr(T_ActHeroes), std::wstring(Tr(T_PadUp)) + L"\t\t" + Tr(T_ActHeroArmy),
            std::wstring(Tr(T_PadDown)) + L"\t\t" + Tr(T_ActLastAlert), std::wstring(Tr(T_PadLeft)) + L"\t\t" + Tr(T_ActHealthBars),
            std::wstring(Tr(T_PadRight)) + L"\t\t" + Tr(T_ActSeeThrough), std::wstring(Tr(T_PadMenus)) + L"\t\t" + Tr(T_ActMenuJump),
            std::wstring(L"L3\t\t") + Tr(T_ActBigMap), std::wstring(L"R3\t\t") + Tr(T_ActCentre), TrF(T_PadHeld, L"R3") + L"\t\t" + Tr(T_ActNextSettlement),
            TrF(T_PadPlusDpad, L"L2") + L"\t\t" + Tr(T_ActHalfSelect), TrF(T_PadPlusUpDown, L"View") + L"\t\t" + Tr(T_ActSpeed),
            std::wstring(L"Menu\t\t") + Tr(T_ActGameMenu), std::wstring(L"View\t\t") + Tr(T_ActInfo), TrF(T_PadSettings, L"View", L"Menu") + L"\t\t" + Tr(T_ActSettings),
            TrF(T_PadHeld, L"Y") + L"\t\t" + Tr(T_ActCheats), TrF(T_PadYMenus, L"Y") + L"\t\t" + TrF(T_ActKeyboard, L"A") };
        std::vector<std::wstring> v(std::begin(rows), std::end(rows));
        OverlayHelp(true, TrF(T_HelpTitle, L"Steam").c_str(), &v, TrF(T_FootStyle, L"View").c_str());
        shot(fort, "vetrina_3_aiuto");
        OverlayHelp(false, L"", nullptr, L"");
    }
    // 4) radiale dei trucchi con un edificio, con "Crea truppe..." (spade incrociate)
    std::vector<RECT> fortBar;
    for (int i = 0; i < 4; ++i) fortBar.push_back({ 850 + i * 55, 1027, 850 + i * 55 + 51, 1079 });
    OverlayTestBar(&fortBar);
    OverlayPad(true, true, GlyphStyle::Steam, false, 0);
    {
        std::vector<CheatEntry> ch = {
            { L"+1K", TrF(T_ChGold, 1000), 1, CI_Coins }, { L"+10K", TrF(T_ChGold, 10000), 1, CI_Coins },
            { L"MAX", TrF(T_ChGold, 100000) + Tr(T_ChMaxOnce), 1, CI_Coins }, { L"+1K", TrF(T_ChFood, 1000), 2, CI_Loaf },
            { L"+10K", TrF(T_ChFood, 10000), 2, CI_Loaf }, { L"MAX", TrF(T_ChFood, 100000) + Tr(T_ChMaxOnce), 2, CI_Loaf },
            { L"+50", TrF(T_ChMaxPop, 50), 0, CI_House }, { L"+100", TrF(T_ChPopulation, 100), 0, CI_People },
            { L"", Tr(T_ChRepair), 0, CI_Hammer }, { L"", Tr(T_ChCapture), 3, CI_Flag }, { L"", Tr(T_ChSpawnMenu), 3, CI_Swords } };
        OverlayCheatItems(ch);
        OverlayRadial(3, 10, false, (TrF(T_HintRelease, L"Y") + Tr(T_HintUse) + TrF(T_HintCancel, L"B")).c_str());
        shot(fort, "vetrina_4_trucchi_edificio");
    }
    // 5) radiale "crea truppe" con le icone vere della barra della fortezza
    {
        OverlayTestTitles({ L"Assolda un eroe", L"Addestra miliziani", L"Addestra frombolieri", L"Addestra cavalieri" });
        OverlayRadial(4, 1, false, TrF(T_SpawnHint, L"A", 20, L"B").c_str());
        shot(fort, "vetrina_5_crea_truppe");
        OverlayTestTitles({});
        OverlayRadial(0, -1, false, nullptr);
    }
    // 6) riquadri: velocita', insediamento, meta' della selezione
    {
        OverlayBadge(L"ImperiumPad 0.10.0", TrF(T_BadgeSpeed, 125).c_str(), 60000);
        shot(fort, "vetrina_6a_velocita");
        OverlayBadge(L"ImperiumPad 0.10.0", TrF(T_BadgeSettlement, 2, 3).c_str(), 60000);
        shot(fort, "vetrina_6b_insediamento");
        OverlayBadge(L"ImperiumPad 0.10.0", Tr(T_BadgeSelHealthUp), 60000);
        shot(fort, "vetrina_6c_selezione");
        OverlayBadge(L"", L"", 0);
    }
    // 8) 0.10.1: radiale dei gruppi con il contenuto (gruppo 1 scelto; poi "assegna" con la selezione)
    {
        const std::wstring hint = TrF(T_HintRelease, L"L1") + Tr(T_HintSelect) + TrF(T_HintToAssign, L"L2") + TrF(T_HintCancel, L"B");
        OverlayRadial(2, 0, false, hint.c_str());
        shot(fort, "vetrina_8a_gruppi");
        OverlayRadial(2, 2, true, hint.c_str());
        shot(fort, "vetrina_8b_gruppi_assegna");
        OverlayRadial(0, -1, false, nullptr);
    }
    // 9) 0.10.1: trucchi classici nei radiali delle unita' e senza selezione
    {
        std::vector<CheatEntry> ch = {
            { L"", Tr(T_ChHeal), 3, CI_Heart }, { L"", Tr(T_ChRation), 2, CI_Loaf }, { L"10", TrF(T_ChLevel, 10), 1, CI_Star },
            { L"50", TrF(T_ChLevel, 50), 1, CI_Star }, { L"MAX", TrF(T_ChLevelMax, 999), 1, CI_Star }, { L"", Tr(T_ChKill), 3, CI_Skull },
            { L"+20", TrF(T_ChAttack, 20), 3, CI_Swords }, { L"+100", TrF(T_ChAttack, 100), 3, CI_Swords },
            { L"+20", TrF(T_ChDefense, 20), 4, CI_Shield }, { L"+100", TrF(T_ChDefense, 100), 4, CI_Shield },
            { L"MAX", Tr(T_ChBonus), 1, CI_Shield } };
        OverlayCheatItems(ch);
        OverlayRadial(3, 7, false, (TrF(T_HintRelease, L"Y") + Tr(T_HintUse) + TrF(T_HintCancel, L"B")).c_str());
        shot(fort, "vetrina_9a_trucchi_unita");
        std::vector<CheatEntry> ch2 = {
            { L"", Tr(T_ChMap), 4, CI_Map }, { L"", Tr(T_ChVision), 4, CI_Eye }, { L"", Tr(T_ChPeace), 0, CI_WhiteFlag },
            { L"", Tr(T_ChTech), 0, CI_Book }, { Tr(T_ChShortHeroes), TrF(T_ChExpHeroes, 1000), 1, CI_Star },
            { Tr(T_ChShortTroops), TrF(T_ChExpTroops, 1000), 0, CI_Star }, { L"", Tr(T_ChFog), 4, CI_Cloud },
            { L"x10", Tr(T_ChSpeed10), 1, CI_Hourglass } };
        OverlayCheatItems(ch2);
        OverlayRadial(3, 7, false, (TrF(T_HintRelease, L"Y") + Tr(T_HintUse) + TrF(T_HintCancel, L"B")).c_str());
        shot(fort, "vetrina_9b_trucchi_niente");
        OverlayRadial(0, -1, false, nullptr);
    }
    // 7) dimensioni: simboli e segno di R2 sulla barra del menu senza selezione, con R2 tenuto, a 80%, 100%, 150%
    {
        std::vector<RECT> mbar;
        for (int i = 0; i < 9; ++i) mbar.push_back({ 654 + i * 71, 1027, 654 + i * 71 + 51, 1078 });
        OverlayTestBar(&mbar);
        const float scales[3] = { 0.8f, 1.0f, 1.5f };
        const char* names[3] = { "vetrina_7a_dimensione_80", "vetrina_7b_dimensione_100", "vetrina_7c_dimensione_150" };
        for (int k = 0; k < 3; ++k) {
            g_cfg.uiScale = scales[k];
            OverlayPad(true, true, GlyphStyle::Steam, true, 0);
            shot(menu, names[k]);
        }
        g_cfg.uiScale = 1.0f;
    }
    OverlayTestBar(nullptr);
    OverlayPad(false, false, GlyphStyle::Steam, false, 0);
    g_cfg.language = "Auto";
    DeleteObject(bmp);
    DeleteDC(dc);
    printf("vetrina salvata\n");
    return 0;
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
    errors += RunShowcase(out);
    printf(errors ? "PROVA FALLITA: %d errori\n" : "PROVA RIUSCITA\n", errors);
    return errors ? 1 : 0;
}
