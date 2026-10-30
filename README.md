# ImperiumPad

Native controller support for **Imperivm RTC HD – Great Battles of Rome** (Steam, app 752580), made for the couch, the Steam Deck and the Steam Machine. *Versione italiana più sotto.*

ImperiumPad is a small DLL (`wininet.dll`, 32 bit) that sits next to the game and works together with [Imperivm Resurrected](https://github.com/imperivm-resurrected/imperivm-resurrected) 1.0.3, which fixes music under Proton and brings Direct3D 11. It does not change any game file.

## What it does

- Left stick moves the cursor (with acceleration and a "magnet" that slows it over units, buildings and buttons); A = left click, X = right click (the game's default action), B = cancel, LT = Shift.
- Right stick scrolls the map, faster the more you tilt it, in any direction.
- Pad symbols on the command bar buttons (Steam, Xbox or PlayStation style): hold RT and press that button to run the command; RT + LT reaches buttons 13 to 24. Imperivm Resurrected's keyboard letters are hidden while the pad is in use.
- Hold RB: command wheel (icons and names of the bar commands; for a hero, formations and abilities). Hold LB: groups wheel 1-0 with unit counts (hold LT too to assign the selection). Time slows down while a wheel is open (single player only).
- D-pad and right stick jump between buttons, check boxes, lists and sliders in every menu. Y in menus opens the Steam on-screen keyboard (Big Picture, Steam Deck, Steam Machine).
- Hero selection, army, last alert, health bars, large map, centring: on the d-pad, bumpers and stick clicks.
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

- Hiding Resurrected's letters works only with Resurrected 1.0.3; with other versions they simply stay.
- Multiplayer: commands go through the game's own functions; slow motion is never used online.
- Built with Visual Studio (C++17, Win32); GitHub builds the zip at every change.
- ImperiumPad is not affiliated with the game's publishers or with Imperivm Resurrected. It contains no game files and no code from Resurrected.

## Licence

MIT: see [LICENSE](LICENSE). The game and Imperivm Resurrected have their own licences.

---

# ImperiumPad (italiano)

Supporto nativo al controller per **Imperivm RTC HD – Great Battles of Rome** (Steam, app 752580), pensato per il divano, lo Steam Deck e la Steam Machine.

ImperiumPad è una piccola DLL (`wininet.dll`, 32 bit) che sta accanto al gioco e lavora insieme a [Imperivm Resurrected](https://github.com/imperivm-resurrected/imperivm-resurrected) 1.0.3, che sistema la musica sotto Proton e porta Direct3D 11. Non modifica nessun file del gioco.

## Cosa fa

- Leva sinistra: cursore (con accelerazione e "calamita" che lo rallenta su unità, edifici e pulsanti); A = clic sinistro, X = clic destro (azione predefinita del gioco), B = annulla, LT = Maiusc.
- Leva destra: scorre la mappa, più veloce quanto più la inclini, in ogni direzione.
- Simboli del pad sui pulsanti della barra comandi (stile Steam, Xbox o PlayStation): RT tenuto + quel tasto esegue il comando; RT + LT arriva ai pulsanti dal 13 al 24. Le lettere della tastiera di Imperivm Resurrected spariscono mentre usi il pad.
- RB tenuto: radiale dei comandi (icone e nomi dei comandi della barra; per un eroe formazioni e abilità). LB tenuto: radiale dei gruppi 1-0 con il numero di unità (con LT tenuto assegni la selezione). Mentre un radiale è aperto il tempo rallenta (solo in partita singola).
- Croce e leva destra saltano tra pulsanti, caselle, elenchi e cursori in tutti i menu. Y nei menu apre la tastiera di Steam (Big Picture, Steam Deck, Steam Machine).
- Eroi, esercito, ultimo avviso, barre della salute, mappa grande, centra: su croce, dorsali e pressione delle leve.
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

- Nascondere le lettere di Resurrected funziona solo con Resurrected 1.0.3; con altre versioni restano.
- In rete i comandi passano dalle funzioni del gioco; il tempo rallentato non si usa mai in rete.
- ImperiumPad non e' legata agli editori del gioco ne' a Imperivm Resurrected. Non contiene file del gioco ne' codice di Resurrected.

## Licenza

MIT: vedi [LICENSE](LICENSE). Il gioco e Imperivm Resurrected hanno le loro licenze.
