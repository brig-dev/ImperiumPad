// Config.cpp - lettura di ImperiumPad.ini (cartella del gioco)
// Linguaggio: C++17, MSVC, Win32 (x86).

#include "ImperiumPad.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

Config g_cfg;

namespace {

char g_ini[MAX_PATH];

int ReadInt(const char* sec, const char* key, int def) { return GetPrivateProfileIntA(sec, key, def, g_ini); }

float ReadFloat(const char* sec, const char* key, float def)
{
    char buf[32];
    GetPrivateProfileStringA(sec, key, "", buf, sizeof(buf), g_ini);
    if (!buf[0]) return def;
    for (char* c = buf; *c; ++c) if (*c == ',') *c = '.';   // accetta anche la virgola italiana
    return static_cast<float>(atof(buf));
}

bool ReadBool(const char* sec, const char* key, bool def)
{
    char buf[16];
    GetPrivateProfileStringA(sec, key, def ? "true" : "false", buf, sizeof(buf), g_ini);
    return _stricmp(buf, "true") == 0 || _stricmp(buf, "1") == 0 || _stricmp(buf, "yes") == 0 || _stricmp(buf, "si") == 0;
}

const char* SourceName(PadSource s)
{
    switch (s) {
    case PadSource::XInput:   return "XInput";
    case PadSource::Keyboard: return "Keyboard";
    case PadSource::Laptop:   return "Laptop";
    default:                  return "Auto";
    }
}

} // namespace

namespace {

// Tasti riassegnabili: nomi usati in [Buttons] (A=B vuol dire: il tasto fisico A fa quello che fa B).
struct ButtonName { const char* name; WORD mask; };
const ButtonName kButtons[] = {
    { "A", PAD_A }, { "B", PAD_B }, { "X", PAD_X }, { "Y", PAD_Y }, { "LB", PAD_LB }, { "RB", PAD_RB },
    { "View", PAD_BACK }, { "Menu", PAD_START }, { "L3", PAD_LS }, { "R3", PAD_RS },
    { "Up", PAD_DPAD_UP }, { "Down", PAD_DPAD_DOWN }, { "Left", PAD_DPAD_LEFT }, { "Right", PAD_DPAD_RIGHT },
};

WORD ButtonMask(const char* name)
{
    for (const ButtonName& b : kButtons) if (_stricmp(b.name, name) == 0) return b.mask;
    return 0;
}

int BitOf(WORD mask)
{
    for (int i = 0; i < 16; ++i) if (mask == (1u << i)) return i;
    return -1;
}

} // namespace

void ConfigWrite(const char* section, const char* key, const char* value)
{
    if (!WritePrivateProfileStringA(section, key, value, g_ini)) Log("ATTENZIONE: impossibile scrivere [%s] %s nell'ini", section, key);
}

WORD RemapButtons(WORD buttons)
{
    if (!g_cfg.remapOn) return buttons;
    WORD out = 0;
    for (int i = 0; i < 16; ++i) if (buttons & (1u << i)) out |= g_cfg.remap[i];
    return out;
}

const char* GlyphStyleName(GlyphStyle s)
{
    switch (s) {
    case GlyphStyle::Xbox:        return "Xbox";
    case GlyphStyle::PlayStation: return "PlayStation";
    default:                      return "Steam";
    }
}

void SaveGlyphStyle(GlyphStyle s)
{
    g_cfg.glyphs = s;
    const BOOL ok = WritePrivateProfileStringA("Pad", "Glyphs", GlyphStyleName(s), g_ini);
    Log("Simboli del pad: %s%s", GlyphStyleName(s), ok ? " (salvato in ImperiumPad.ini)" : " (ATTENZIONE: ini non scrivibile)");
}

void LoadConfig()
{
    _snprintf_s(g_ini, sizeof(g_ini), _TRUNCATE, "%s\\ImperiumPad.ini", GameDir());
    Config c;
    char lang[32];
    GetPrivateProfileStringA("General", "Language", "Auto", lang, sizeof(lang), g_ini);
    c.language = lang;
    char src[16];
    GetPrivateProfileStringA("Pad", "Source", "Auto", src, sizeof(src), g_ini);
    if (_stricmp(src, "XInput") == 0) c.padSource = PadSource::XInput;
    else if (_stricmp(src, "Keyboard") == 0) c.padSource = PadSource::Keyboard;
    else if (_stricmp(src, "Laptop") == 0) c.padSource = PadSource::Laptop;
    char glyphs[16];
    GetPrivateProfileStringA("Pad", "Glyphs", "Steam", glyphs, sizeof(glyphs), g_ini);
    if (_stricmp(glyphs, "Xbox") == 0) c.glyphs = GlyphStyle::Xbox;
    else if (_stricmp(glyphs, "PlayStation") == 0 || _stricmp(glyphs, "PS") == 0 || _stricmp(glyphs, "DualSense") == 0)
        c.glyphs = GlyphStyle::PlayStation;
    c.hideKeyLetters  = ReadBool("Pad", "HideKeyLetters", c.hideKeyLetters);
    char barSymbols[16];
    GetPrivateProfileStringA("Pad", "BarSymbols", "Hold", barSymbols, sizeof(barSymbols), g_ini);
    c.barSymbolsAlways = _stricmp(barSymbols, "Always") == 0;
    c.controllerIndex = ReadInt("Pad", "ControllerIndex", c.controllerIndex);
    c.tickMs          = ReadInt("Pad", "TickMs", c.tickMs);
    c.leftDeadzone    = ReadFloat("Pad", "LeftDeadzone", c.leftDeadzone);
    c.rightDeadzone   = ReadFloat("Pad", "RightDeadzone", c.rightDeadzone);
    c.tapMs           = ReadInt("Pad", "TapMs", c.tapMs);
    c.keyLog          = ReadBool("Pad", "KeyLog", c.keyLog);
    c.acceptInjected  = ReadBool("Pad", "AcceptInjected", c.acceptInjected);
    c.cursorSpeed     = ReadFloat("Cursor", "Speed", c.cursorSpeed);
    c.cursorCurve     = ReadFloat("Cursor", "Curve", c.cursorCurve);
    c.cursorRampMs    = ReadInt("Cursor", "RampMs", c.cursorRampMs);
    c.cursorNudge     = ReadFloat("Cursor", "Nudge", c.cursorNudge);
    char mode[16];
    GetPrivateProfileStringA("Cursor", "Mode", "Auto", mode, sizeof(mode), g_ini);
    if (_stricmp(mode, "Real") == 0) c.cursorMode = CursorMode::Real;
    else if (_stricmp(mode, "Virtual") == 0) c.cursorMode = CursorMode::Virtual;
    c.magnet          = ReadFloat("Cursor", "Magnet", c.magnet);
    c.vibration       = ReadFloat("Pad", "Vibration", c.vibration);
    c.slowMotion      = ReadFloat("Radial", "SlowMotion", c.slowMotion);
    // [Buttons]: tasto fisico = tasto logico; quelli non scritti restano se stessi
    for (int i = 0; i < 16; ++i) c.remap[i] = static_cast<WORD>(1u << i);
    for (const ButtonName& b : kButtons) {
        char v[16];
        GetPrivateProfileStringA("Buttons", b.name, "", v, sizeof(v), g_ini);
        const WORD target = v[0] ? ButtonMask(v) : 0;
        if (v[0] && !target) Log("ATTENZIONE: [Buttons] %s=%s non e' un tasto conosciuto", b.name, v);
        if (target && target != b.mask) {
            c.remap[BitOf(b.mask)] = target;
            c.remapOn = true;
            Log("Tasto riassegnato: %s fa %s", b.name, v);
        }
    }
    c.cameraOn        = ReadFloat("Camera", "Threshold", c.cameraOn);
    c.cameraAnalog    = ReadBool("Camera", "Analog", c.cameraAnalog);
    c.cameraSpeed     = ReadFloat("Camera", "Speed", c.cameraSpeed);
    c.cameraCurve     = ReadFloat("Camera", "Curve", c.cameraCurve);
    c.hookKeyState    = ReadBool("Input", "VirtualModifiers", c.hookKeyState);
    c.diagIntervalMs  = ReadInt("Diag", "IntervalMs", c.diagIntervalMs);
    c.memory          = ReadBool("Diag", "Memory", c.memory);

    // limiti di sicurezza
    if (c.tickMs < 2) c.tickMs = 2;
    if (c.tickMs > 50) c.tickMs = 50;
    if (c.cursorCurve < 0.5f) c.cursorCurve = 0.5f;
    if (c.cameraCurve < 0.5f) c.cameraCurve = 0.5f;
    if (c.cameraSpeed < 0.1f) c.cameraSpeed = 0.1f;
    if (c.cameraSpeed > 4.0f) c.cameraSpeed = 4.0f;
    if (c.magnet < 0.1f) c.magnet = 0.1f;
    if (c.magnet > 1.0f) c.magnet = 1.0f;
    if (c.vibration < 0.0f) c.vibration = 0.0f;
    if (c.vibration > 1.0f) c.vibration = 1.0f;
    if (c.slowMotion < 0.1f) c.slowMotion = 0.1f;
    if (c.slowMotion > 1.0f) c.slowMotion = 1.0f;
    if (c.diagIntervalMs < 100) c.diagIntervalMs = 100;
    g_cfg = c;

    Log("Impostazioni (%s): [General] Language=%s [Pad] Source=%s Glyphs=%s HideKeyLetters=%d BarSymbols=%s ControllerIndex=%d TickMs=%d LeftDeadzone=%.2f RightDeadzone=%.2f TapMs=%d "
        "KeyLog=%d AcceptInjected=%d; [Cursor] Speed=%.2f Curve=%.2f RampMs=%d Nudge=%.3f Mode=%d Magnet=%.2f; Vibration=%.2f SlowMotion=%.2f; [Camera] Threshold=%.2f Analog=%d Speed=%.2f Curve=%.2f; [Input] VirtualModifiers=%d; "
        "[Diag] IntervalMs=%d Memory=%d", g_ini, c.language.c_str(), SourceName(c.padSource), GlyphStyleName(c.glyphs), c.hideKeyLetters ? 1 : 0,
        c.barSymbolsAlways ? "Always" : "Hold", c.controllerIndex, c.tickMs, c.leftDeadzone,
        c.rightDeadzone, c.tapMs, c.keyLog ? 1 : 0, c.acceptInjected ? 1 : 0, c.cursorSpeed, c.cursorCurve, c.cursorRampMs, c.cursorNudge, static_cast<int>(c.cursorMode), c.magnet, c.vibration, c.slowMotion, c.cameraOn, c.cameraAnalog ? 1 : 0, c.cameraSpeed, c.cameraCurve,
        c.hookKeyState ? 1 : 0, c.diagIntervalMs, c.memory ? 1 : 0);
}
