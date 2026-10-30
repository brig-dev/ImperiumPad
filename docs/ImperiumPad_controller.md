# ImperiumPad — progetto del supporto al controller

Progetto del supporto al controller (30/10/2026), aggiornato alla 0.8.1. Versioni: 0.2.0 e 0.3.0 cursore, clic, camera e tasti; 0.4.0 riquadro di stato e aiuto disegnati nel gioco; 0.5.0 croce a scatti tra le caselle dei menu e simboli del pad sui pulsanti della barra con RT + tasto, al posto del radiale dei comandi previsto all'inizio; 0.6.0 camera analogica, radiale dei comandi con RB tenuto e dei gruppi con LB tenuto; 0.7.0 testi in italiano, inglese e spagnolo; 0.8.0 calamita, vibrazione, tempo rallentato, cursore disegnato, impostazioni nel gioco, tasti riassegnabili. Si basa su una ricerca su come si giocano gli strategici in tempo reale col pad su console, sullo studio del codice del gioco e sulle verifiche riportate in fondo. I tasti hanno i nomi XInput per posizione: A sud (Croce sul DualSense), B est (Cerchio), X ovest (Quadrato), Y nord (Triangolo), LB/RB dorsali, LT/RT grilletti, View e Menu (Select e Start), L3/R3 pressione delle leve.

## 1. Principi

Il cursore c'e' sempre, come in Age of Empires su console: e' il modo naturale di giocare a Imperivm, che e' fatto di clic su unita', villaggi e pulsanti. Due tasti fanno quasi tutto: A e' il clic sinistro (seleziona), X e' il clic destro (il "comando predefinito" del gioco: muovi, attacca, cattura, entra, assegna all'eroe, raccogli, cura, a seconda di cosa c'e' sotto il cursore; il gioco cambia gia' la forma del cursore per dirlo). Quello che sta nella barra comandi in basso si raggiunge con un menu radiale costruito leggendo la barra stessa, quindi si adatta da solo a ogni unita', eroe, edificio, civilta' e mod, senza tabelle scritte da noi. Un tasto fa una cosa sola (lezione di Halo Wars), un dorsale tenuto apre un radiale, un grilletto tenuto fa da modificatore. Mouse e tastiera restano sempre attivi, e il cursore e' lo stesso: con lo Steam Controller si puo' usare il trackpad destro come mouse e i tasti del pad insieme. La mod non scrive mai nello stato del gioco: dà ordini solo come farebbe il giocatore (clic, tasti, funzioni della barra), cosi' non rompe le partite in rete, che sono sincronizzate "in lockstep".

## 2. Come funziona dentro (tecnica)

Cursore. La leva sinistra muove il cursore vero di Windows con `SetCursorPos`, entro la finestra del gioco. Cosi' Windows manda da solo al gioco i movimenti del mouse, il gioco mostra il cursore giusto (attacco, cattura, entra) e il suo scorrimento ai bordi funziona: spingendo il cursore contro il bordo dello schermo la mappa scorre, come su console. Velocita' con curva di risposta e accelerazione (valori di partenza presi da SacredPad: zona morta 0,20, curva 2, velocita' massima circa 0,9 altezze dello schermo al secondo). "Calamita": quando sotto il cursore c'e' un'unita' o un edificio (funzione del gioco `0x5e1cd0`, la stessa che usa il clic destro), il cursore rallenta, cosi' e' facile fermarsi sopra un soldato. Rischio da verificare per primo: sotto gamescope lo spostamento del cursore da programma potrebbe non funzionare; in quel caso si nasconde il cursore di sistema e se ne disegna uno nostro nell'overlay, con la forma del cursore del gioco.

Clic e tasti. A e X mandano alla finestra `WM_LBUTTONDOWN/UP` e `WM_RBUTTONDOWN/UP` con le coordinate del cursore (il gioco le prende dal messaggio, verificato), il doppio A manda `WM_LBUTTONDBLCLK`. I tasti del gioco (Home, virgola, F7, Q, Spazio, Esc, F10, cifre dei gruppi) si mandano con `WM_KEYDOWN/UP`. Maiusc, Ctrl e Alt "virtuali" si ottengono agganciando `GetKeyState` dentro user32, cosi' li vedono sia il gioco sia Imperivm Resurrected, che legge i modificatori per conto suo.

Camera. La leva destra muove la vista in modo analogico chiamando le funzioni di scorrimento del gioco (`0x409320` piu' `0x6265c0`), con velocita' proporzionale all'inclinazione; e' piu' comodo dello scorrimento del gioco, che ha una velocita' sola. Ripiego se qualcosa non va: simulare le frecce.

Overlay. Si aggancia `SetDIBitsToDevice` nella tabella di import di `gbr.exe`: prima di copiare l'immagine sullo schermo il gioco ci passa il buffer RGB565, dove disegniamo radiali, suggerimenti dei tasti e la schermata d'aiuto. Funziona anche con Resurrected, che prende l'immagine piu' a valle per Direct3D 11. Lo stesso aggancio gira nel thread del gioco: e' il posto giusto per chiamare le funzioni del gioco (barra comandi, camera, oggetto sotto il cursore) senza rischi di accessi contemporanei; il nostro thread del pad prepara le richieste e il thread del gioco le esegue al fotogramma successivo.

Radiale dei comandi. Il gestore della selezione `[0x9c0938]` tiene a +0x134 la barra comandi; la barra tiene a +0x98 l'elenco dei comandi disponibili per la selezione (indici del registro dei comandi `[0x9969dc]`, descrittore con titolo e tasto); il clic su un comando e' `0x6b3500(barra, indice)`, la stessa funzione del clic del giocatore, che passa dai controlli del gioco (costi, requisiti) e poi esegue o mette il comando "in attesa di bersaglio". Le icone si copiano dal buffer dello schermo, dove la barra le ha appena disegnate, e i nomi vengono dal descrittore (testi gia' in italiano).

Menu del gioco (principale, opzioni, salvataggi, diplomazia). Come in SacredPad: la croce salta tra i pulsanti visibili della finestra che ha il fuoco (radice dell'interfaccia `[0xa7abf0]`, pulsanti `CUIButton`), A clicca, B chiude; la leva sinistra muove comunque il cursore libero.

## 3. Schema dei comandi proposto

In partita, con la mappa a schermo:

| Comando | Azione | Cosa fa nel gioco |
|---|---|---|
| Leva sinistra | cursore | movimento del mouse; al bordo dello schermo scorre la mappa |
| Leva destra | camera | scorrimento analogico della vista |
| A | seleziona | clic sinistro |
| A tenuto + leva sinistra | selezione a riquadro | trascinamento sinistro |
| A doppio | stesso tipo / esercito intero | doppio clic: tutte le unita' dello stesso tipo sullo schermo, oppure tutto l'esercito se l'unita' e' di un eroe (verificato) |
| X | azione | clic destro: comando predefinito sul punto o sull'oggetto |
| X tenuto + leva sinistra | orienta la formazione | trascinamento destro con un eroe e il suo esercito selezionati (`hero_set_final_party_orientation`) |
| B | annulla | Esc, ma solo se c'e' un comando in attesa o una selezione: non apre mai il menu per sbaglio |
| RT tenuto + tasto | comando della barra | dalla 0.5.0: sui primi dodici pulsanti della barra c'e' il simbolo di un tasto (A, B, X, Y, LB, RB, croce su/destra/giu'/sinistra, L3, R3); RT tenuto + quel tasto esegue il comando (`0x6b3500`, come il clic). Il radiale con leva e rilascio resta un'idea per le barre piu' lunghe |
| RB tenuto | radiale dei comandi | dalla 0.6.0: i pulsanti della barra in cerchio con icona e titolo (per l'eroe formazioni e abilita'); leva sinistra sceglie, croce sinistra/destra passa alla vicina, rilascio esegue, B annulla |
| LB tenuto | radiale dei gruppi | dalla 0.6.0: gruppi 1-0 con il numero di elementi; rilascio seleziona; con LT tenuto assegna (Ctrl+cifra) |
| RB tocco / LB tocco | eroe successivo / precedente | virgola / Maiusc+virgola |
| LT tenuto | modificatore Maiusc | con A aggiunge o toglie dalla selezione; con un gruppo lo aggiunge |
| Croce su | esercito dell'eroe | F7 |
| Croce giu' | ultimo avviso | Q |
| Croce sinistra | barre della salute | accento grave |
| Croce destra tenuta | vedere attraverso alberi ed edifici | barra rovesciata |
| L3 | mappa grande | Spazio |
| R3 | centra sulla selezione | Home; due volte sul gruppo, come la cifra ripetuta |
| Menu (Start) | menu di gioco | F10 |
| View tocco / tenuto | pannello informazioni / aiuto del pad | I / schermata con il controller disegnato e i comandi |

Nei menu e nelle finestre (mappa senza fuoco): leva sinistra cursore libero, croce (e leva destra, dalla 0.5.0) da una casella all'altra (pulsanti, caselle di spunta, tendine, elenchi, cursori), A clic, B chiudi o indietro, LB/RB schede, View aiuto. Nella mappa grande: cursore sulla mappa, A seleziona o salta, X manda la selezione in quel punto (il gioco gia' lo fa con il clic destro sulla mappa grande, verificato), L3 o B chiude.

Tutti i tasti saranno riassegnabili in `ImperiumPad.ini`. Con Resurrected bisogna tenere il suo schema "Original" (predefinito): se si sceglie "Grid" o "Modern", i tasti mandati dal pad andrebbero tradotti, e per questo dove possibile si chiama direttamente la funzione del gioco invece di simulare il tasto.

Opzioni da provare: tempo rallentato mentre un radiale e' aperto, solo in partita singola (il gioco ha il comando di velocita' `CVXCmdSetSpeed`, che passa dalla coda dei comandi); vibrazione breve quando arriva un avviso (attacco a un insediamento) e quando l'eroe selezionato scende sotto un quarto della salute.

## 4. Esempi concreti

Inizio di una partita Strategia. Nel menu principale la croce sceglie "Strategia", A conferma; nella schermata delle opzioni la croce passa da un controllo all'altro e A apre gli elenchi; "Inizia" con A. In partita la camera e' sulla fortezza.

Reclutare un eroe e dei legionari. Il cursore sulla fortezza, A la seleziona. RT tenuto: il radiale mostra i comandi della barra della fortezza (recluta eroe, recluta legionari, arcieri, ricerche); la leva sceglie "Recluta eroe", si lascia RT. Di nuovo RT, "Recluta legionari", e cosi' via. Se un comando costa troppo, il gioco lo rifiuta come con il mouse.

Formare l'esercito. A tenuto e leva sinistra per tracciare un riquadro sui legionari appena usciti, si lascia A. Il cursore sull'eroe: diventa il cursore "assegna"; X. I legionari si uniscono all'eroe (il gioco: "Clicca con il pulsante destro per assegnare le unita' a questo eroe").

Marciare in formazione. RB tocco seleziona l'eroe, croce su seleziona eroe ed esercito. RB tenuto: radiale dell'eroe, "Formazione in linea", si lascia RB. Il cursore sul punto d'arrivo: X tenuto, la leva sinistra indica dove deve guardare il fronte (compare la freccia del gioco), si lascia X: l'esercito marcia e si schiera in linea con il fronte nella direzione scelta.

Battaglia. Con l'esercito selezionato, il cursore su un nemico diventa la spada: X attacca. Quando i nemici arrivano, RB tenuto, "Chiamata alla difesa" (costa 4 di energia, +20 difesa all'armata). Per salvare i feriti: LB tenuto, spicchio "tieni i piu' feriti" (PagGiu' del gioco), poi X verso la fortezza per mandarli a curarsi.

Catturare un villaggio. Esercito selezionato, cursore sul villaggio: il cursore diventa "cattura", X. Dopo la cattura, A sul villaggio e RT per i suoi comandi (rifornimenti, popolazione).

Gruppi. Selezionato l'esercito, LT e LB tenuti, spicchio 3: e' il gruppo 3. Piu' tardi LB tenuto, spicchio 3, rilascio: gruppo selezionato; R3 due volte: camera sul gruppo.

Mappa grande. L3 apre la mappa grande; il cursore si muove sulla mappa, X sul punto manda la selezione li', A su un'unita' la seleziona; L3 o B chiude.

Annullare. Dopo aver scelto nel radiale un comando che vuole un bersaglio (per esempio "Esplora area" di un druido), il cursore cambia forma: A sul punto lo esegue, B lo annulla. B di nuovo svuota la selezione.

## 5. Ordine di sviluppo (ogni passo provato sul PC e poi sulla Steam Machine)

0.2.0 cursore con la leva sinistra (`SetCursorPos`), A, X e B, doppio A, trascinamenti con A e X tenuti, Maiusc virtuale con LT; nessun overlay. E' il passo che dice se lo spostamento del cursore funziona sotto gamescope. 0.3.0 camera analogica con la leva destra, croce, RB/LB tocco, L3, R3, Menu, View tocco. 0.4.0 aggancio di `SetDIBitsToDevice`, scritta della versione e schermata d'aiuto del pad disegnate nell'immagine (prova anche con Resurrected). 0.5.0 (fatto) navigazione dei menu con la croce, simboli del pad sulla barra in stile Steam, Xbox o PlayStation, RT + tasto per i comandi. 0.6.0 (fatto) camera analogica, radiale dei comandi (RB) e dei gruppi (LB). 0.7.0 (fatto) testi in tre lingue. 0.8.0 (fatto) calamita, tempo rallentato, vibrazione, cursore disegnato, RT + LT per i pulsanti dal tredicesimo, tastiera di Steam, impostazioni nel gioco (View + Menu), tasti riassegnabili.

Impostazioni di Steam Input consigliate: layout "Gamepad" (il pad arriva come XInput); sullo Steam Controller il trackpad destro puo' restare "come mouse", perche' muove lo stesso cursore della leva.

## 6. Verifiche nel codice fatte il 30/10/2026 per questo progetto

Gruppi (`0x5e8ec0`): cifra = seleziona il gruppo (`0x5e8450(n, maiusc)`, con Maiusc lo aggiunge); stessa cifra entro un tempo breve = centra la camera sul gruppo; Ctrl+cifra = assegna (`0x5e7ce0(n)`). Ctrl premuto o rilasciato ricostruisce la barra comandi (`0x6b3d60`): alcuni comandi hanno una variante con Ctrl (campo +0x7c del descrittore: 0 senza Ctrl, 1 con Ctrl, 2 entrambi). Stesso tipo (`0x5e4370`): se l'unita' cliccata non e' di un eroe (`+0x170 == 0xffff`) seleziona nel rettangolo visibile tutte le unita' della stessa classe; se e' di un eroe seleziona tutto l'esercito dell'eroe. Eroe successivo (`0x5e88e0`, virgola e rotella): scorre gli eroi del giocatore locale; i giocatori stanno dentro la partita da +0x12cc con passo 800 byte. Oggetto sotto un punto dello schermo: `0x5e1cd0(punto)` restituisce l'oggetto dei copioni se visibile ed esplorato. Barra comandi: gestore della selezione +0x134 (controllato come `CVXCmdInterface`), elenco dei comandi a +0x98 (`[+0x98]+4` lista, voce +8 = indice), clic `0x6b3500(indice)` -> descrittore dal registro (metodo +0x10), controlli `0x6b3190` e `0x6b28d0`, oggetto comando `0x4e88c0` consegnato a `0x5e6bd0` del gestore della selezione; tasto rapido `0x6b3620` confronta il carattere con il tasto del descrittore (+0x1c8). Velocita': `CVXCmdSetSpeed` e' un comando registrato da `0x4ee870` (fabbrica `0x4e5740`), quindi passa dalla coda sincronizzata; velocita' iniziale da "VXTIME/GameSpeed" (`0x528d10`).
