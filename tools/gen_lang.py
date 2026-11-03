# Genera ImperiumPad/Lang.cpp e aggiorna l'elenco dei testi in ImperiumPad/ImperiumPad.h (nel C++ gli accenti
# diventano codici universali). Uso: python -I tools/gen_lang.py <cartella del repository>
import sys
root = sys.argv[1]

T = [
 # id, italiano, inglese, spagnolo
 ('StatusLaptopOn', 'Pad simulato con le lettere: ACCESO (Bloc Num o Bloc Maiusc per spegnerlo)',
  'Letter-key pad: ON (Num Lock or Caps Lock turns it off)',
  'Mando simulado con letras: ACTIVADO (Bloq Num o Bloq Mayús para apagarlo)'),
 ('StatusLaptopOff', 'Pad simulato SPENTO: lettere al gioco (Bloc Num o Bloc Maiusc per riaccenderlo)',
  'Letter-key pad OFF: letters go to the game (Num Lock or Caps Lock turns it back on)',
  'Mando simulado APAGADO: las letras van al juego (Bloq Num o Bloq Mayús para encenderlo)'),
 ('StatusNumpadOn', 'Pad simulato con il tastierino: ACCESO (Bloc Maiusc per spegnerlo)',
  'Numpad pad: ON (Caps Lock turns it off)',
  'Mando simulado con el teclado numérico: ACTIVADO (Bloq Mayús para apagarlo)'),
 ('StatusNumpadOff', 'Pad simulato SPENTO (Bloc Maiusc per riaccenderlo)',
  'Numpad pad OFF (Caps Lock turns it back on)',
  'Mando simulado APAGADO (Bloq Mayús para encenderlo)'),
 ('StatusController', 'Controller collegato', 'Controller connected', 'Mando conectado'),
 ('StatusNoController', 'Nessun controller: mouse e tastiera', 'No controller: mouse and keyboard', 'Sin mando: ratón y teclado'),
 ('BadgeHelp', '%s tenuto: aiuto', 'hold %s: help', 'mantén %s: ayuda'),
 ('PsCross', 'Croce', 'Cross', 'Cruz'),
 ('PsCircle', 'Cerchio', 'Circle', 'Círculo'),
 ('PsSquare', 'Quadrato', 'Square', 'Cuadrado'),
 ('PsTriangle', 'Triangolo', 'Triangle', 'Triángulo'),
 ('HelpTitle', 'ImperiumPad: comandi del pad (simboli %s)', 'ImperiumPad: pad controls (%s symbols)',
  'ImperiumPad: controles del mando (símbolos %s)'),
 ('HelpHeldPlusKey', '%s tenuto + tasto', 'hold %s + button', 'mantén %s + botón'),
 ('PadLeftStick', 'Leva sinistra', 'Left stick', 'Stick izquierdo'),
 ('PadRightStick', 'Leva destra', 'Right stick', 'Stick derecho'),
 ('PadUp', 'Freccia su', 'D-pad up', 'Cruceta arriba'),
 ('PadDown', 'Freccia giù', 'D-pad down', 'Cruceta abajo'),
 ('PadLeft', 'Freccia sinistra', 'D-pad left', 'Cruceta izquierda'),
 ('PadRight', 'Freccia destra', 'D-pad right', 'Cruceta derecha'),
 ('PadMenus', 'Frecce nei menu', 'D-pad in menus', 'Cruceta en menús'),
 ('KeyArrows', 'frecce', 'arrows', 'flechas'),
 ('KeyPlusKey', '%s + tasto', '%s + key', '%s + tecla'),
 ('KeyNumDot', 'punto', 'Num .', 'Supr'),
 ('KeyEnter', 'Invio', 'Enter', 'Intro'),
 ('KeyHome', 'Home', 'Home', 'Inicio'),
 ('KeyEnd', 'Fine', 'End', 'Fin'),
 ('KeyDel', 'Canc', 'Del', 'Supr'),
 ('KeyPgDn', 'PagGiù', 'PgDn', 'Av Pág'),
 ('KeyPgUp', 'PagSu', 'PgUp', 'Re Pág'),
 ('KeyIns', 'Insert', 'Insert', 'Insert'),
 ('ActCursor', 'Cursore (al bordo scorre la mappa)', 'Cursor (scrolls the map at the edge)',
  'Cursor (en el borde desplaza el mapa)'),
 ('ActCamera', 'Camera (più inclini, più veloce); nei menu: salta tra le caselle',
  'Camera (tilt more to go faster); in menus: jumps between controls',
  'Cámara (cuanto más inclinas, más rápido); en menús: salta entre opciones'),
 ('ActSelect', 'Seleziona; tenuto: riquadro; doppio: stesso tipo o esercito',
  'Select; hold: box select; double: same type or whole army',
  'Seleccionar; mantener: recuadro; doble: mismo tipo o ejército'),
 ('ActAction', 'Azione (clic destro); tenuto: orienta la formazione',
  'Action (right click); hold: face the formation', 'Acción (clic derecho); mantener: orientar la formación'),
 ('ActCancel', 'Annulla', 'Cancel', 'Cancelar'),
 ('ActShift', 'Maiusc: aggiunge alla selezione e mette in coda i comandi', 'Shift: adds to the selection, queues commands',
  'Mayús: añade a la selección y pone las órdenes en cola'),
 ('ActBarCommand', 'Comando della barra col simbolo di quel tasto; con %s anche: dal 13° pulsante',
  'Command bar button showing that symbol; with %s too: from the 13th button',
  'Orden de la barra con el símbolo de ese botón; con %s también: desde el botón 13'),
 ('ActHeroes', 'Eroe successivo / precedente; tenuti: radiale comandi / gruppi',
  'Next / previous hero; hold: commands / groups wheel', 'Héroe siguiente / anterior; mantener: rueda de órdenes / grupos'),
 ('ActHeroArmy', "Esercito dell'eroe", "Hero's army", 'Ejército del héroe'),
 ('ActLastAlert', 'Ultimo avviso', 'Last alert', 'Último aviso'),
 ('ActHealthBars', 'Barre della salute', 'Health bars', 'Barras de salud'),
 ('ActSeeThrough', 'Vedere attraverso, tenuto', 'See through (hold)', 'Ver a través (mantener)'),
 ('ActMenuJump', "Saltano da una casella all'altra", 'Jump from control to control', 'Saltan de una opción a otra'),
 ('ActBigMap', 'Mappa strategica', 'Minimap (whole map)', 'Mapa estratégico'),
 ('ActCentre', 'Centra sulla selezione', 'Center on the selection', 'Centrar en la selección'),
 ('ActGameMenu', 'Menu principale (nei menu: chiude)', 'Main menu (in menus: closes)', 'Menú principal (en menús: cierra)'),
 ('ActInfo', 'Pannello informazioni; tenuto: questo aiuto', 'Info panel; hold: this help', 'Panel de información; mantener: esta ayuda'),
 ('FootLaptop', 'Bloc Num o Bloc Maiusc: pad acceso/spento.   ', 'Num Lock or Caps Lock: pad on/off.   ',
  'Bloq Num o Bloq Mayús: mando encendido/apagado.   '),
 ('FootNumpad', 'Tastierino come pad.   ', 'Numpad as pad.   ', 'Teclado numérico como mando.   '),
 ('FootStyle', '%s tenuto + freccia sinistra/destra: simboli Steam, Xbox o PlayStation',
  'hold %s + d-pad left/right: Steam, Xbox or PlayStation symbols',
  'mantén %s + cruceta izquierda/derecha: símbolos Steam, Xbox o PlayStation'),
 ('HintRelease', 'Lascia %s', 'Release %s', 'Suelta %s'),
 ('HintRun', ': esegue', ': run', ': ejecutar'),
 ('HintSelect', ': seleziona', ': select', ': seleccionar'),
 # 0.8.4: nel radiale dei gruppi L2 passa da seleziona ad assegna e ritorno (prima andava tenuto fino al rilascio)
 ('HintToAssign', '   %s: assegna invece di selezionare', '   %s: assign instead of select',
  '   %s: asignar en lugar de seleccionar'),
 ('HintToSelect', '   %s: seleziona invece di assegnare', '   %s: select instead of assign',
  '   %s: seleccionar en lugar de asignar'),
 ('HintAssignRun', ': assegna', ': assign', ': asignar'),
 ('HintCancel', '   %s: annulla', '   %s: cancel', '   %s: cancelar'),
 ('RadNoCommands', 'Nessun comando per questa selezione', 'No commands for this selection', 'No hay órdenes para esta selección'),
 ('RadTiltCommand', 'Inclina la leva verso un comando', 'Tilt the stick towards a command', 'Inclina el stick hacia una orden'),
 ('RadUnnamed', '(comando senza nome)', '(unnamed command)', '(orden sin nombre)'),
 ('RadAssignChoose', 'Assegna la selezione: scegli il gruppo', 'Assign the selection: choose the group',
  'Asignar la selección: elige el grupo'),
 ('RadTiltGroup', 'Inclina la leva verso un gruppo', 'Tilt the stick towards a group', 'Inclina el stick hacia un grupo'),
 ('RadAssignTo', 'Assegna la selezione al gruppo %d', 'Assign the selection to group %d', 'Asignar la selección al grupo %d'),
 ('RadGroupOne', 'Gruppo %d: %d elemento', 'Group %d: %d unit', 'Grupo %d: %d unidad'),
 ('RadGroupMany', 'Gruppo %d: %d elementi', 'Group %d: %d units', 'Grupo %d: %d unidades'),
 ('RadGroupEmpty', 'Gruppo %d: vuoto', 'Group %d: empty', 'Grupo %d: vacío'),
 ('RadEmpty', 'vuoto', 'empty', 'vacío'),
 # 0.8.0: impostazioni nel gioco, avvisi, aiuto
 ('SetTitle', 'ImperiumPad: impostazioni', 'ImperiumPad: settings', 'ImperiumPad: opciones'),
 ('SetFoot', 'Croce su/giù: scegli   sinistra/destra o %s: cambia   %s o %s: chiudi (si salva da solo)',
  'D-pad up/down: choose   left/right or %s: change   %s or %s: close (saved automatically)',
  'Cruceta arriba/abajo: elegir   izquierda/derecha o %s: cambiar   %s o %s: cerrar (se guarda solo)'),
 ('SetLanguage', 'Lingua', 'Language', 'Idioma'),
 ('SetSymbols', 'Simboli dei tasti', 'Button symbols', 'Símbolos de los botones'),
 ('SetCursorSpeed', 'Velocità del cursore', 'Cursor speed', 'Velocidad del cursor'),
 ('SetCameraSpeed', 'Velocità della camera', 'Camera speed', 'Velocidad de la cámara'),
 ('SetAnalogCamera', 'Camera analogica', 'Analog camera', 'Cámara analógica'),
 ('SetMagnet', 'Calamita del cursore', 'Cursor magnet', 'Imán del cursor'),
 ('SetVibration', 'Vibrazione', 'Vibration', 'Vibración'),
 ('SetSlowMotion', 'Tempo rallentato nei radiali', 'Slow motion in wheels', 'Cámara lenta en las ruedas'),
 ('SetHideLetters', 'Nascondi le lettere della tastiera (Resurrected)', 'Hide keyboard letters (Resurrected)',
  'Ocultar letras del teclado (Resurrected)'),
 ('SetCursorMode', 'Cursore', 'Cursor', 'Cursor'),
 ('ValGame', 'come il gioco', 'same as the game', 'como el juego'),
 ('ValAuto', 'Automatico', 'Automatic', 'Automático'),
 ('ValOn', 'Acceso', 'On', 'Activado'),
 ('ValOff', 'Spento', 'Off', 'Desactivado'),
 ('ValLight', 'Leggera', 'Light', 'Suave'),
 ('ValMedium', 'Media', 'Medium', 'Media'),
 ('ValStrong', 'Forte', 'Strong', 'Fuerte'),
 ('ValCursorReal', 'di Windows', 'Windows', 'de Windows'),
 ('ValCursorDrawn', 'Disegnato', 'Drawn', 'Dibujado'),
 ('BadgeVirtualCursor', 'Il cursore di Windows non si sposta: uso il cursore disegnato',
  'The Windows cursor does not move: using the drawn cursor', 'El cursor de Windows no se mueve: uso el cursor dibujado'),
 ('BadgeNoKeyboard', 'Tastiera di Steam non disponibile (serve Steam in Big Picture o la Steam Machine)',
  'Steam keyboard not available (needs Steam Big Picture or a Steam Machine)',
  'Teclado de Steam no disponible (hace falta Steam en Big Picture o una Steam Machine)'),
 ('PadYMenus', '%s nei menu', '%s in menus', '%s en menús'),
 ('ActKeyboard', 'Tastiera di Steam per scrivere (prima %s sul campo)', 'Steam keyboard for typing (first %s on the field)',
  'Teclado de Steam para escribir (primero %s en el campo)'),
 ('PadSettings', '%s tenuto + %s', 'hold %s + %s', 'mantén %s + %s'),
 ('ActSettings', 'Impostazioni di ImperiumPad', 'ImperiumPad settings', 'Opciones de ImperiumPad'),
 # 0.8.3: simboli sulla barra con RT tenuto, menu senza selezione (nomi come nei suggerimenti del gioco, GAME.LOC.XML)
 ('SetBarSymbols', 'Simboli sulla barra', 'Symbols on the command bar', 'Símbolos en la barra de órdenes'),
 ('ValWhileHeld', 'Con %s tenuto', 'While holding %s', 'Manteniendo %s'),
 ('ValAlways', 'Sempre', 'Always', 'Siempre'),
 ('BadgeBarHint', 'Comandi della barra: tieni %s e premi il tasto col simbolo del pulsante',
  'Command bar: hold %s and press the button shown on the command',
  'Barra de órdenes: mantén %s y pulsa el botón que aparece en la orden'),
 ('EmptyHelp', 'Guida', 'Help', 'Ayuda'),
 ('EmptyDiplomacy', 'Diplomazia', 'Diplomacy', 'Diplomacia'),
 ('EmptyQuickSave', 'Salvataggio rapido', 'Quick save', 'Guardar rápidamente'),
 ('EmptyParty', 'Seleziona gruppo', 'Select party', 'Seleccionar grupo'),
 ('EmptyNotes', 'Obiettivi', 'Notes', 'Objetivos'),
 ('EmptyQuickLoad', 'Caricamento rapido', 'Quick load', 'Cargar rápidamente'),
 ('EmptyMainMenu', 'Menu principale', 'Main menu', 'Menú principal'),
 ('EmptyChat', 'Conversazione', 'Chat', 'Conversación'),
 ('EmptyMinimap', 'Mappa strategica', 'Minimap', 'Mapa estratégico'),
 # 0.8.4: radiale dei gruppi spiegato quando i gruppi sono vuoti o non c'e' nulla da assegnare
 ('RadNoGroups', 'Nessun gruppo ancora', 'No groups yet', 'Todavía no hay grupos'),
 ('HintNoGroups', 'Seleziona delle unità, poi tieni %s, premi %s, scegli un numero e lascia %s.   %s: chiudi',
  'Select some units, then hold %s, press %s, pick a number and release %s.   %s: close',
  'Selecciona unidades, luego mantén %s, pulsa %s, elige un número y suelta %s.   %s: cerrar'),
 ('HintNoSelection', 'Nessuna unità selezionata: chiudi con %s, seleziona e riprova',
  'No units selected: close with %s, select some and try again',
  'No hay unidades seleccionadas: cierra con %s, selecciona y vuelve a intentarlo'),
 ('BadgeNothingToAssign', 'Niente da assegnare: prima seleziona delle unità', 'Nothing to assign: select some units first',
  'Nada que asignar: primero selecciona unidades'),
]

def esc(s):
    out = ''
    for ch in s:
        o = ord(ch)
        if ch == '"': out += '\\"'
        elif ch == '\\': out += '\\\\'
        elif o < 128: out += ch
        else: out += '\\u%04x' % o
    return 'L"' + out + '"'

ids = ',\n    '.join('T_' + t[0] for t in T)
rows = '\n'.join('    { %s,\n      %s,\n      %s },' % (esc(t[1]), esc(t[2]), esc(t[3])) for t in T)

cpp = '''// Lang.cpp - testi mostrati a schermo in italiano, inglese e spagnolo (0.7.0)
// Linguaggio: C++17, MSVC, Win32 (x86).
//
// La lingua si sceglie una volta all'avvio: [General] Language dell'ini (Auto, Italian, English, Spanish); con Auto
// quella del gioco, [Language] Default di Settings.ini nella cartella del gioco (il gioco Steam ha italian.pak,
// english.pak e spanish.pak in local). Lingua sconosciuta: inglese. Il log resta in italiano.
// Le lettere accentate sono scritte come \\uXXXX, cosi' il file non dipende dalla codifica con cui lo legge il compilatore.
// File generato dallo script dei testi: per cambiare una frase va bene anche modificarlo a mano.

#include "ImperiumPad.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

struct Row { const wchar_t* it; const wchar_t* en; const wchar_t* es; };

const Row kRows[] = {
%s
};
static_assert(sizeof(kRows) / sizeof(kRows[0]) == T_Count, "una riga per ogni testo");

Lang g_lang = Lang::English;

} // namespace

void LangInit()
{
    char want[32] = {};
    _snprintf_s(want, sizeof(want), _TRUNCATE, "%%s", g_cfg.language.c_str());
    const bool automatic = !want[0] || _stricmp(want, "Auto") == 0;
    if (automatic) {
        char ini[MAX_PATH];
        _snprintf_s(ini, sizeof(ini), _TRUNCATE, "%%s\\\\Settings.ini", GameDir());
        GetPrivateProfileStringA("Language", "Default", "", want, sizeof(want), ini);
    }
    if (_stricmp(want, "Italian") == 0 || _stricmp(want, "Italiano") == 0 || _stricmp(want, "it") == 0) g_lang = Lang::Italian;
    else if (_stricmp(want, "Spanish") == 0 || _stricmp(want, "Espanol") == 0 || _stricmp(want, "es") == 0) g_lang = Lang::Spanish;
    else g_lang = Lang::English;
    Log("Lingua dei testi a schermo: %%s (%%s \\"%%s\\")", g_lang == Lang::Italian ? "italiano" : g_lang == Lang::Spanish ? "spagnolo" : "inglese",
        automatic ? "dal gioco, Settings.ini" : "da ImperiumPad.ini", want);
}

Lang CurrentLang() { return g_lang; }

const wchar_t* Tr(Txt id)
{
    if (id < 0 || id >= T_Count) return L"";
    const Row& r = kRows[id];
    return g_lang == Lang::Italian ? r.it : g_lang == Lang::Spanish ? r.es : r.en;
}

std::wstring TrF(Txt id, ...)
{
    wchar_t buf[512];
    va_list ap;
    va_start(ap, id);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, Tr(id), ap);
    va_end(ap);
    return buf;
}
''' % rows

open(root + '/ImperiumPad/Lang.cpp', 'w', encoding='utf-8', newline='\r\n').write(cpp)

hdr = '''// --- Lang.cpp: testi a schermo in italiano, inglese e spagnolo (0.7.0) ----------
enum class Lang { Italian, English, Spanish };
enum Txt {
    %s,
    T_Count
};
void           LangInit();                // sceglie la lingua (ini o Settings.ini del gioco)
Lang           CurrentLang();
const wchar_t* Tr(Txt id);                // testo nella lingua scelta
std::wstring   TrF(Txt id, ...);          // testo con %%s / %%d riempiti
''' % ids
h = open(root + '/ImperiumPad/ImperiumPad.h', encoding='utf-8').read()
a = h.index('// --- Lang.cpp:')
b = h.index('\n', h.index('std::wstring   TrF(Txt id, ...);', a)) + 1
open(root + '/ImperiumPad/ImperiumPad.h', 'w', encoding='utf-8').write(h[:a] + hdr + h[b:])
print(len(T), 'testi')
