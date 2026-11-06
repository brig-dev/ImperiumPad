# ImperiumPad

Native controller support for **Imperivm RTC HD – Great Battles of Rome** (Steam, app 752580), made for the couch, the Steam Deck and the Steam Machine. *Versione italiana più sotto.*

ImperiumPad is a small DLL (`wininet.dll`, 32 bit) that sits next to the game and works together with [Imperivm Resurrected](https://github.com/imperivm-resurrected/imperivm-resurrected) 1.0.3, which fixes music under Proton and brings Direct3D 11. It does not change any game file.

Download: [GitHub releases](https://github.com/brig-dev/ImperiumPad/releases) or [ModDB](https://www.moddb.com/mods/imperiumpad).

## What it does

- Left stick moves the cursor (with acceleration and a "magnet" that slows it over units, buildings and buttons); A = left click, X = right click (the game's default action), B = cancel, LT = Shift (add to the selection, queue commands).
- Right stick scrolls the map, faster the more you tilt it, in any direction.
- Pad symbols on the command bar buttons (Steam, Xbox or PlayStation style): hold RT, the symbols appear on the buttons, press one to run that command; RT + LT reaches buttons 13 to 24. With nothing selected the same works on the bottom menu (help, diplomacy, quick save and load, minimap...). Imperivm Resurrected's keyboard letters are hidden while the pad is in use.
- Hold RB: command wheel (icons and names of the bar commands; for a hero, formations and abilities). Hold LB: groups wheel 1-0 with unit counts; release on a group to select it, or press LT while the wheel is open to assign the current selection to the group instead. Each group shows its health and a star when it holds a hero. Time slows down while a wheel is open (single player only).
- Hold Y: cheats wheel, with Imperivm Resurrected's cheats for what you selected (building: gold and food up to +100,000 at a time, population, repair, capture; units: heal, food, level 10 / 50 / max, remove; nothing: map, vision, peace, technologies, experience). Only when the room allows cheats ("Allow cheats"). With a building selected, "Spawn troops..." opens a second wheel with the units that building trains: d-pad up/down picks how many (1 to 100), A spawns them. In single player the wheel also has the classic Imperivm console cheats, run by ImperiumPad even without Resurrected: nearly invincible units, fog of war on/off, speed 10x.
- Quick selections: hold LT and press the d-pad for half of the selection with more / less health (up / down) or more / less experience (right / left). Hold View and press d-pad up/down to change the game speed (single player). Hold R3 to jump to your next settlement.
- Settings in the game (hold View, press Menu): size of symbols and texts (80% to 150%) and a page to remap every controller button.
- D-pad and right stick jump between buttons, check boxes, lists and sliders in every menu. Y in menus opens the Steam on-screen keyboard (Big Picture, Steam Deck, Steam Machine).
- Next/previous hero, hero's army, last alert, health bars, minimap, center on selection: on the d-pad, bumpers and stick clicks.
- Vibration on new alerts and when the selected hero drops below a quarter of its health.
- If the Windows cursor cannot be moved (gamescope), a drawn cursor takes over automatically.
- On-screen texts in English, Italian and Spanish (the game's language by default).
- Hold View for the full button list; hold View and press Menu for the settings (saved automatically).

## Install

1. Install Imperivm Resurrected 1.0.3 first (follow its instructions).
2. Copy `wininet.dll` and `ImperiumPad.ini` into the game folder, next to `gbr.exe` (Steam: right click the game, Manage, Browse local files).
3. Steam Deck / Steam Machine / Linux only: set the launch options to `WINEDLLOVERRIDES="winmm=n,b;wininet=n,b" %command%`.
4. Use the controller in Steam Input's "Gamepad" mode (XInput).

To uninstall, delete `wininet.dll` and `ImperiumPad.ini` (and the launch option). The mod writes `ImperiumPad.log` in the game folder: attach it when reporting a problem.

## Notes

- Hiding Resurrected's letters is tested with Resurrected 1.0.3; newer versions are recognised by searching their code, and if that fails the letters simply stay.
- Multiplayer: commands go through the game's own functions; slow motion is never used online.
- Built with Visual Studio (C++17, Win32); GitHub builds the zip at every change.
- Made by brig-dev with the help of Claude (Anthropic) for the analysis and the code; the ModDB logo was made with an AI image generator.
- ImperiumPad is not affiliated with the game's publishers or with Imperivm Resurrected. It contains no game files and no code from Resurrected.

## Licence

MIT: see [LICENSE](LICENSE). The game and Imperivm Resurrected have their own licences.

---

# ImperiumPad (italiano)

Supporto nativo al controller per **Imperivm RTC HD – Great Battles of Rome** (Steam, app 752580), pensato per il divano, lo Steam Deck e la Steam Machine.

ImperiumPad è una piccola DLL (`wininet.dll`, 32 bit) che sta accanto al gioco e lavora insieme a [Imperivm Resurrected](https://github.com/imperivm-resurrected/imperivm-resurrected) 1.0.3, che sistema la musica sotto Proton e porta Direct3D 11. Non modifica nessun file del gioco.

Download: [release su GitHub](https://github.com/brig-dev/ImperiumPad/releases) oppure [ModDB](https://www.moddb.com/mods/imperiumpad).

## Cosa fa

- Leva sinistra: cursore (con accelerazione e "calamita" che lo rallenta su unità, edifici e pulsanti); A = clic sinistro, X = clic destro (azione predefinita del gioco), B = annulla, LT = Maiusc (aggiunge alla selezione, mette in coda i comandi).
- Leva destra: scorre la mappa, più veloce quanto più la inclini, in ogni direzione.
- Simboli del pad sui pulsanti della barra comandi (stile Steam, Xbox o PlayStation): tieni RT, sui pulsanti compaiono i simboli, premi quel tasto per eseguire il comando; RT + LT arriva ai pulsanti dal 13 al 24. Senza selezione vale lo stesso per il menu in basso (guida, diplomazia, salvataggio e caricamento rapido, mappa strategica...). Le lettere della tastiera di Imperivm Resurrected spariscono mentre usi il pad.
- RB tenuto: radiale dei comandi (icone e nomi dei comandi della barra; per un eroe formazioni e abilità). LB tenuto: radiale dei gruppi 1-0 con il numero di unità; lasciando LB su un gruppo lo selezioni, premendo LT a radiale aperto assegni invece la selezione a quel gruppo. Ogni gruppo mostra la salute e una stella se contiene un eroe. Mentre un radiale è aperto il tempo rallenta (solo in partita singola).
- Y tenuto: radiale dei trucchi di Imperivm Resurrected per quello che hai selezionato (edificio: oro e cibo fino a +100.000 per volta, popolazione, riparazione, cattura; unità: cura, viveri, livello 10 / 50 / massimo, elimina; niente: mappa, visione, pace, tecnologie, esperienza). Solo se la stanza permette i trucchi ("Allow cheats"). Con un edificio selezionato, "Crea truppe..." apre un secondo radiale con le unità che quell'edificio addestra: croce su/giù sceglie quante (da 1 a 100), A le crea. In partita singola la ruota ha anche i trucchi classici della console di Imperivm, eseguiti da ImperiumPad anche senza Resurrected: unità quasi invincibili, nebbia di guerra sì/no, velocità 10x.
- Selezioni rapide: tieni LT e premi la croce per la metà della selezione con più / meno salute (su / giù) o più / meno esperienza (destra / sinistra). View tenuto + croce su/giù cambia la velocità della partita (partita singola). R3 tenuto salta al tuo prossimo insediamento.
- Impostazioni nel gioco (View tenuto + Menu): dimensione di simboli e testi (dall'80% al 150%) e una pagina per riassegnare ogni tasto del controller.
- Croce e leva destra saltano tra pulsanti, caselle, elenchi e cursori in tutti i menu. Y nei menu apre la tastiera di Steam (Big Picture, Steam Deck, Steam Machine).
- Eroe successivo e precedente, esercito dell'eroe, ultimo avviso, barre della salute, mappa strategica, centra sulla selezione: su croce, dorsali e pressione delle leve.
- Vibrazione agli avvisi nuovi e quando l'eroe selezionato scende sotto un quarto della salute.
- Se il cursore di Windows non si può spostare (gamescope), passa da solo a un cursore disegnato.
- Testi a schermo in italiano, inglese e spagnolo (di base la lingua del gioco).
- View tenuto: tutti i comandi; View tenuto + Menu: impostazioni (si salvano da sole).

## Installazione

1. Installa prima Imperivm Resurrected 1.0.3 (segui le sue istruzioni).
2. Copia `wininet.dll` e `ImperiumPad.ini` nella cartella del gioco, accanto a `gbr.exe` (Steam: tasto destro sul gioco, Gestisci, Sfoglia i file locali).
3. Solo Steam Deck / Steam Machine / Linux: nelle opzioni di avvio scrivi `WINEDLLOVERRIDES="winmm=n,b;wininet=n,b" %command%`.
4. Usa il controller con Steam Input in modalità "Gamepad" (XInput).

Per togliere la mod basta cancellare `wininet.dll` e `ImperiumPad.ini` (e l'opzione di avvio). La mod scrive `ImperiumPad.log` nella cartella del gioco: allegalo se segnali un problema.

## Note

- Nascondere le lettere di Resurrected è provato con Resurrected 1.0.3; le versioni nuove si riconoscono cercando nel loro codice, e se non si trova nulla le lettere restano.
- In rete i comandi passano dalle funzioni del gioco; il tempo rallentato non si usa mai in rete.
- Fatta da brig-dev con l'aiuto di Claude (Anthropic) per l'analisi e il codice; il logo di ModDB e' fatto con un generatore di immagini con l'IA.
- ImperiumPad non e' legata agli editori del gioco ne' a Imperivm Resurrected. Non contiene file del gioco ne' codice di Resurrected.

## Licenza

MIT: vedi [LICENSE](LICENSE). Il gioco e Imperivm Resurrected hanno le loro licenze.
