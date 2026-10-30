// ImperiumPad.cpp - proxy di wininet.dll, log, DllMain
// Linguaggio: C++17. Compilatore: MSVC (Visual Studio Community 2026).
// Piattaforma: Win32 (x86), Release, runtime statico /MT.
//
// Questo file contiene solo la parte "proxy" (ripresa da SacredPad): gbr.exe importa wininet.dll,
// quindi Windows (o Proton con wininet=n,b) carica questa DLL dalla cartella del gioco. Tutte le 327
// esportazioni della wininet.dll di Windows, con gli stessi nomi e ordinali, saltano alla DLL vera
// di sistema; l'elenco e gli stub sono generati da tools/gen_proxy.py in ProxyExports.inc e wininet.def.
// Poi si apre ImperiumPad.log e parte il thread principale (Core.cpp).

#include "ImperiumPad.h"
#include <intrin.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

namespace {

HMODULE          g_self = nullptr;
HMODULE          g_real = nullptr;
HANDLE           g_log  = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_logLock;
char             g_dir[MAX_PATH] = {};

// Archivio dei log, come in SacredPad: prima di essere riscritto, ImperiumPad.log si copia nella cartella
// ImperiumPad_log con la data e l'ora dell'ultima scrittura nel nome; si tengono gli ultimi kLogKeep file.
constexpr int kLogKeep = 60;

void ArchiveLog(const char* path)
{
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &a) || (a.nFileSizeHigh == 0 && a.nFileSizeLow == 0)) return;
    char dir[MAX_PATH];
    _snprintf_s(dir, sizeof(dir), _TRUNCATE, "%s\\ImperiumPad_log", g_dir);
    CreateDirectoryA(dir, nullptr);
    FILETIME lt{}; SYSTEMTIME st{};
    FileTimeToLocalFileTime(&a.ftLastWriteTime, &lt);
    FileTimeToSystemTime(&lt, &st);
    char dst[MAX_PATH];
    _snprintf_s(dst, sizeof(dst), _TRUNCATE, "%s\\ImperiumPad_%04u%02u%02u_%02u%02u%02u.log", dir, st.wYear, st.wMonth,
                st.wDay, st.wHour, st.wMinute, st.wSecond);
    CopyFileA(path, dst, FALSE);
    // pulizia: i nomi hanno la data, quindi l'ordine alfabetico e' quello cronologico
    char pat[MAX_PATH];
    _snprintf_s(pat, sizeof(pat), _TRUNCATE, "%s\\ImperiumPad_*.log", dir);
    std::vector<std::string> names;
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) names.push_back(fd.cFileName); } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (static_cast<int>(names.size()) <= kLogKeep) return;
    std::sort(names.begin(), names.end());
    for (size_t i = 0; i + kLogKeep < names.size(); ++i) {
        char f[MAX_PATH];
        _snprintf_s(f, sizeof(f), _TRUNCATE, "%s\\%s", dir, names[i].c_str());
        DeleteFileA(f);
    }
}

void OpenLog()
{
    InitializeCriticalSection(&g_logLock);
    GetModuleFileNameA(g_self, g_dir, MAX_PATH);
    if (char* slash = strrchr(g_dir, '\\')) *slash = '\0';

    char path[MAX_PATH], prev[MAX_PATH];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\ImperiumPad.log", g_dir);
    _snprintf_s(prev, sizeof(prev), _TRUNCATE, "%s\\ImperiumPad_precedente.log", g_dir);
    ArchiveLog(path);
    MoveFileExA(path, prev, MOVEFILE_REPLACE_EXISTING);
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

} // namespace

void Log(const char* fmt, ...)
{
    if (g_log == INVALID_HANDLE_VALUE) return;

    char line[1024];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "[%02u:%02u:%02u.%03u][T%05lu] ",
                        t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, GetCurrentThreadId());
    if (n < 0) n = 0;

    va_list ap;
    va_start(ap, fmt);
    int m = _vsnprintf_s(line + n, sizeof(line) - n - 2, _TRUNCATE, fmt, ap);
    va_end(ap);
    if (m < 0) m = static_cast<int>(strlen(line + n));
    n += m;
    line[n++] = '\r';
    line[n++] = '\n';

    EnterCriticalSection(&g_logLock);
    DWORD written = 0;
    WriteFile(g_log, line, static_cast<DWORD>(n), &written, nullptr);
    LeaveCriticalSection(&g_logLock);
}

const char* GameDir()    { return g_dir; }
HMODULE     SelfModule() { return g_self; }

// ---------------------------------------------------------------------------
// Proxy: le esportazioni di wininet.dll
// ---------------------------------------------------------------------------
namespace {

struct ProxyEntry { const char* name; WORD ordinal; };

const ProxyEntry kEntries[] = {
#define PROXY_ENTRY(idx, name, ord) { name, ord },
#include "ProxyExports.inc"
#undef PROXY_ENTRY
};
constexpr int kCount = sizeof(kEntries) / sizeof(kEntries[0]);

// Ripiego per le funzioni che la DLL di sistema non ha (sotto Wine ne mancano alcune): non si conosce
// il numero di argomenti, quindi la funzione non puo' ripulire lo stack come quella vera. Il gioco usa
// solo 6 funzioni di wininet, tutte presenti anche in Wine; se il ripiego viene chiamato, lo dice il log.
DWORD WINAPI MissingFunction()
{
    Log("ERRORE: chiamata una funzione di wininet assente nella DLL di sistema (indirizzo di ritorno %p)", _ReturnAddress());
    SetLastError(ERROR_PROC_NOT_FOUND);
    return 0;
}

} // namespace

extern "C" FARPROC g_fn[kCount] = {};

// Ogni stub e' un salto all'indirizzo vero: parametri e risultato passano intatti.
#define PROXY_STUB(idx) \
    extern "C" __declspec(naked) void ProxyStub##idx() { __asm { jmp dword ptr [g_fn + (idx) * 4] } }
#include "ProxyExports.inc"
#undef PROXY_STUB

namespace {

bool LoadRealDll()
{
    char sysdir[MAX_PATH];
    UINT len = GetSystemDirectoryA(sysdir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        Log("ERRORE: GetSystemDirectoryA fallita (%lu)", GetLastError());
        return false;
    }
    char path[MAX_PATH];
    _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\wininet.dll", sysdir);

    g_real = LoadLibraryA(path);
    if (!g_real || g_real == g_self) {
        Log("ERRORE: impossibile caricare la wininet.dll vera da %s (%lu)", path, GetLastError());
        return false;
    }
    char realPath[MAX_PATH] = {};
    GetModuleFileNameA(g_real, realPath, MAX_PATH);
    Log("wininet.dll vera caricata: %s (base %p)", realPath, g_real);

    int missing = 0;
    std::string list;
    for (int i = 0; i < kCount; ++i) {
        const char* key = kEntries[i].name ? kEntries[i].name : MAKEINTRESOURCEA(kEntries[i].ordinal);
        g_fn[i] = GetProcAddress(g_real, key);
        if (!g_fn[i]) {
            g_fn[i] = reinterpret_cast<FARPROC>(&MissingFunction);
            ++missing;
            if (list.size() < 700) {
                char one[64];
                if (kEntries[i].name) _snprintf_s(one, sizeof(one), _TRUNCATE, " %s", kEntries[i].name);
                else _snprintf_s(one, sizeof(one), _TRUNCATE, " #%u", kEntries[i].ordinal);
                list += one;
            }
        }
    }
    Log("Esportazioni: %d inoltrate, %d assenti nella DLL di sistema%s%s", kCount - missing, missing, missing ? ":" : "",
        list.c_str());
    return true;
}

} // namespace

// DllMain: solo il minimo indispensabile, siamo sotto il loader lock.
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = hModule;
        DisableThreadLibraryCalls(hModule);
        OpenLog();

        char exe[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        Log("ImperiumPad %s caricato. Processo: %s (PID %lu)", IMPERIUMPAD_VERSION, exe, GetCurrentProcessId());
        Log("Cartella: %s", g_dir);

        if (!LoadRealDll()) return FALSE;

        // Il thread parte davvero solo dopo che il loader ha finito.
        if (HANDLE th = CreateThread(nullptr, 0, CoreThread, nullptr, 0, nullptr))
            CloseHandle(th);
        else
            Log("ERRORE: CreateThread fallita (%lu)", GetLastError());
    }
    else if (reason == DLL_PROCESS_DETACH) {
        Log("ImperiumPad scaricato");
    }
    return TRUE;
}
