# Regole CSGOpen TDM v0.1

Preset: `config/csgopen/tdm.cfg`. I nomi `sv_*` agiscono sull'autorità server;
le corrispondenti variabili senza prefisso sono sincronizzate con i client.
I valori sono un punto di partenza nelle unità di Red Eclipse, non valori CS:GO.

## Modalità e partita

Dichiarazioni: `src/game/vars.h`; selezione in `server::changemode`,
`server::chooseteam`, `server::setupspawns` (`src/game/server.cpp`),
`m_team`, `m_teamspawn` (`src/game/gamemode.h`), squadre in `player.h`.
`G_DEATHMATCH = 2`, senza mutatori (`0`), è già TDM: Alpha e Omega.
Non occorre un mutatore "team". `config/setup.cfg` definisce davvero
`tdm -> teamdm -> start -> mode; map`.

| Variabile server | Originale → prototipo | Significato e limiti |
| --- | --- | --- |
| `defaultmode` | 2 → 2 | indice modalità, G_START..G_MAX-1 |
| `defaultmuts` | 0 → 0 | maschera mutatori, 0..G_M_ALL |
| `rotatemode` | 1 → 0 | booleano: niente cambio casuale di modalità |
| `rotatemuts` | 3 → 0 | 0..VAR_MAX: niente mutatori casuali |
| `modelockfilter` | G_LIMIT → 1<<G_DEATHMATCH (4) | maschera modalità consentite senza privilegi, 0..G_ALL |
| `mutslockfilter` | G_M_FILTER → 0 | maschera mutatori consentiti senza privilegi, 0..G_M_ALL |
| `mutslockforce` | 0 → 0 | nessun mutatore imposto, 0..G_M_ALL |
| `waitforplayers` | 2 → 0 | enum 0..2; non attendere l'uscita degli altri dallo spectator |
| `botbalance` | -1 → 2 | -1..VAR_MAX; minimo totale di partecipanti, umani inclusi, non due bot aggiuntivi |
| `balancemaps` | -1 → 0 | -1..3; disattiva inversioni di squadre delle mappe asimmetriche |
| `defaultmap` (launcher) | stringa vuota → maps/echo | mappa iniziale; nome alternativo accettato dallo script |

La normale assegnazione/bilanciamento delle squadre resta upstream
(`teambalance = 6`). Un umano può iniziare con un bot avversario; per provare
friendly fire usare due client oppure `sv_botbalance 4` (un umano e tre bot,
con un bot alleato). I privilegi amministrativi consentono ancora
cambi intenzionali di modalità: il preset non è un sistema antimanomissione.

## Salute, danno e respawn

Dichiarazioni: `src/game/player.h` tramite macro `APVAR` in `playerdef.h`,
`vars.h` per fattori globali. Usi: `servstate::gethealth`, `sendspawn`,
`dodamage`, `calcdamage` e ramo regen di `server::checkclients` in `server.cpp`;
`clientstate::spawnstate`, `gameent::gethealth` in `game.h`.

| Variabile | Originale → prototipo | Unità e limiti |
| --- | --- | --- |
| `playerhealth`, `bothealth` | 1000 → 100 | punti interni reali; 1..VAR_MAX |
| `healthscale` | 1 → 1 | fattore adimensionale; 0..FVAR_MAX |
| `maxhealth` | 1.5 → 1 | fattore sul valore di spawn; 0..FVAR_MAX, **non** punti assoluti |
| `playerabilities`, `botabilities` | A_A_PLAYER/A_A_BOT (7679) → 7383 | 0..A_A_ALL; rimuove REGEN, MELEE, SECONDARY; conserva MOVE, JUMP, CROUCH, PRIMARY, AMMO e le altre capacità ordinarie |
| `playerteamdamage`, `botteamdamage` | A_T_PLAYER (7), A_T_AI (6) → A_T_PLAYER (7) per entrambi | maschera tipi di bersaglio alleati, 0..A_T_ALL; umani e bot possono danneggiarsi reciprocamente |
| `damageteamscale` | 0.5 → 1 | moltiplicatore, 0..FVAR_MAX; nessuna riduzione del danno tra alleati |
| `playerspawndelay`, `botspawndelay` | 5000 → 3000 | millisecondi; DEATHMILLIS (300)..VAR_MAX |
| `damagescale` | 1 → 0.1 | moltiplicatore, 0..FVAR_MAX; segue riduzione salute 1000→100 |

La regen richiede esplicitamente `A_A_REGEN` sul server, qui assente. Non si usa
un ritardo enorme o una modifica soltanto all'HUD. `regendelay=5000`,
`regentime=1000`, `regenhealth=50` restano originali ma non vengono eseguiti.
`server::isghost` e `physics::isghost` applicano teamdamage a danni e collisioni
con proiettili. Il friendly fire è ora attivo su richiesta dell'utente:
la maschera A_T_PLAYER include umani, bot ed enemy actor, senza A_T_GHOST.
Il fattore globale 1 conserva la scala normale delle armi, inclusi i loro
modificatori specifici `damageteam`. Questo sostituisce il requisito iniziale
di friendly fire disattivato; non richiede modifiche C++.

`m_delay` seleziona spawndelay normale per DM senza mutatori; server e client
usano lo stesso valore. Dopo morte, click primario/salto richiede il respawn;
il server lo concede al termine dei 3 secondi. Non è un respawn automatico
senza input. La protezione originale di spawn (3000 ms, interrotta sparando)
resta presente e deve essere considerata nei test di danno.

L'HUD upstream mostra la salute come percentuale del valore di spawn, quindi
100 iniziali; `damagedivisor` client passa da 10 a 1 per le cifre dell'obituary.
Il limite maxhealth è un tetto nei normali percorsi di danno/cura: non viene
aggiunto un nuovo sistema di armatura o cura.

## Movimento

Dichiarazioni: `vars.h`, capacità actor in `player.h`; usi in
`gameent::canimpulse` (`game.h`), `physics::impulseplayer`, `modifyinput`,
`modifyvelocity`, `movevelocity` (`physics.cpp`).

| Variabile | Originale → prototipo | Unità e limiti |
| --- | --- | --- |
| `playerimpulse` | IM_T_ALL (4095) → 1<<IM_T_JUMP (1) | maschera capacità, 0..IM_T_ALL |
| `botimpulse` | IM_T_MVAI (4095) → 1 | stessa maschera per bot |
| `movespeed` | 1 → 0.55 | fattore della velocità target, FVAR_NONZERO..FVAR_MAX |
| `moverun` | 1.25 → 1 | fattore durante corsa, stessi limiti |
| `movesprint` | 1.5 → 1 | fattore sprint, stessi limiti |
| `movestraight` | 1.2 → 1 | fattore movimento avanti/non strafe, stessi limiti |
| `movestrafe` | 1.1 → 1 | fattore strafe, stessi limiti |
| `moveaccelscale` (nuovo C++) | 1 → 0.75 | moltiplicatore del tasso di risposta a terra; FVAR_NONZERO..FVAR_MAX |
| `movebrakescale` (nuovo C++) | 1 → 1.25 | moltiplicatore del tasso di risposta senza input; stessi limiti; ridotto da 1.5 dopo il primo test manuale |

`canimpulse` controlla la maschera prima dei costi/timer. Il normale salto a
terra richiede proprio IM_T_JUMP: azzerare l'intero sistema lo romperebbe.
Boost (incluso salto extra in aria), dash, slide, launch, melee impulse, kick
(walljump), grab, wallrun, vault e pound non sono consentiti. I bind restano
intatti. Resta la tolleranza originale di 125 ms quando si lascia il terreno;
non è un salto aggiuntivo dopo il primo. Salto e crouch non sono riscalati:
`impulsespeed=75`, `impulsejump=1.5`, `movecrawl=0.6` upstream.

`movevelocity` calcola velocità in unità mondo/secondo, non unità Source:
`actor.speed * movescale * movespeed`, più modificatori di posizione/armi.
`playerspeed=100`, SMG portato `modspeed=-5`: normalmente circa 52.25 unità/s
nel preset prima di altri modificatori. Restano riduzioni mentre si spara,
in aria (0.75), step-up/down, acqua, gravità e inerzia originali.
`gamespeed=100` è invariato.

La risposta originale usa lo stesso `floorcoast` per accelerazione e frenata:

```
r = pow(max(1 - 1/coast, 0), millis/20)
velocity = target*(1-r) + previous*r
```

L'unica modifica C++ al movimento moltiplica l'esponente per `moveaccelscale`
quando c'è input, oppure `movebrakescale` senza input, per actor a terra fuori
dai liquidi. Coefficienti 1 riproducono la formula precedente. Valori maggiori
convergono più rapidamente. Per coast=5, tempo per raggiungere il 90% della
velocità target: circa 206 ms originale, 275 ms con 0.75; frenata al 10% in
165 ms con 1.25 (prima 137 ms con 1.5). Le superfici possono modificare coast: i tempi non sono costanti
universali. Niente refactoring della collisione o dell'integrazione.

Le due variabili sono `GFVAR(IDF_GAMEMOD,...)`: entrano nel percorso di
sincronizzazione server/client esistente. Il movimento rimane simulato e
predetto dai client, incluso il proprietario dei bot; il dedicato upstream
non diventa autorità fisica né un nuovo anticheat.

## Armi

Dichiarazioni: `src/game/weapons.h` e macro `weapdef.h`; selezione/munizioni in
`clientstate::spawnstate`, `canuseweap`, `canreload` (`game.h`); restrizioni
server in `server::hasitem`, `chkloadweap`, eventi di uso/tiro (`server.cpp`).
La pistola è W_PISTOL=1; SMG W_SMG=4. W_RIFLE=8 è semiautomatico, non il
fucile automatico desiderato. Nessuna sostituzione di modelli o asset.

| Variabile | Originale → prototipo | Unità e limiti |
| --- | --- | --- |
| `playerweaponspawn`, `botweaponspawn` | W_PISTOL → W_PISTOL | indice arma, 0..W_MAX-1 |
| `playermaxcarry`, `botmaxcarry` | 2 → 1 | numero di armi loadout oltre alla pistola, 0..W_LOADOUT |
| `pistoldisabled`, `smgdisabled` | 0 → 0 | booleani 0..1 |
| `{claw,sword,shotgun,flamer,plasma,zapper,rifle,corroder,grenade,mine,rocket,minigun,jetsaw,eclipse,melee}disabled` | 0 → 1 | booleani 0..1, nessun pickup/loadout ammesso per queste armi |
| `{player,bot}spawngrenades`, `{player,bot}spawnmines` | 0 → 0 | enum 0..2, nessun extra alla comparsa |
| `spreeprize`, `spreemaxprize`, `spreebreakprize`, `revengeprize` | -1 → 0 | -1..W_PRIZES; nessun premio arena da serie di uccisioni |
| `janitorlimit` | 8 → 0 | numero AI janitor, 0..MAXAI; evita i loro premi arena |
| `kamikaze` | 1 → 0 | enum 0..3, nessuna esplosione di morte |
| `playerloadweap` (client) | stringa vuota → "4" | elenco di indici, parser `client::setloadweap` (`client.cpp`) |
| `showloadoutmenu` (client) | 0 → 0 | booleano 0..1, niente apertura automatica menu |
| `damagedivisor` (client) | 10 → 1 | divisore dell'obituary, FVAR_NONZERO..FVAR_MAX |

Il server non si fida della scelta del menu: tutte le armi loadout tranne SMG
sono disabilitate. `spawnstate` sostituisce scelte invalide con la sola arma
consentita. `playerloadweap` rende il vettore non vuoto, necessario per
`chkloadweap`; non è l'unica restrizione. Melee e fuoco SECONDARY sono rimossi
dalle capacità per evitare kick/claw e l'attacco alternativo SMG con proiettili adesivi esplosivi. La sola primary resta utilizzabile.

Pistol primary: semiauto, delayattack1=200 ms, proiettile a 2000 unità/s,
damage1=200 prima della scala (20 nel preset). SMG primary: fullauto1=1,
delayattack1=75 ms (~13.3 colpi/s), proiettile a 2500 unità/s, damage1=160
(16 nel preset), caricatore 40, ammospawn 120 (40+80), reload 1250 ms.
La pistola ha 10 colpi e ammostore=-1 (ricarica senza riserva finita).
L'SMG ha riserva finita per vita; la pistola resta disponibile e il respawn
rifornisce entrambe. Pickup SMG/pistola possono rifornire munizioni, ma non
sono necessari per avere l'equipaggiamento. Le altre armi non sono accessibili.

Rimangono dispersione, danni per parte del corpo, push/stun, traiettorie,
ricarica, animazioni, eventuale drill SMG e compensazione di latenza upstream.
Sono proiettili, **non hitscan**. Niente AK/M4, penetrazione Source, armatura
completa, rinculo fedele o nuovo sistema di rete in questa milestone.

### Precisione in movimento e crouch

Primary pistol/SMG: variabili `WPFVARM(IDF_GAMEMOD,...)` in `weapons.h`,
nomi prodotti da `weapdef.h`; tutte con limiti 0..FVAR_MAX e sincronizzazione
server/client. Il preset configura entrambi i tipi di actor tramite le stesse
proprietà delle armi, senza intervenire sul solo mirino.

| Suffisso variabile primary | Originale → preset | Significato |
| --- | --- | --- |
| `pistolspread1`, `smgspread1` | 0, 0 → 1, 2 | dispersione base nelle unità interne di offsetray |
| `{pistol,smg}spreadstill1` | 1 → 1 | moltiplicatore in piedi da fermo |
| `{pistol,smg}spreadrunning1` | 1 → 3 | moltiplicatore in corsa |
| `{pistol,smg}spreadsprinting1` | 1.2 → 3 | stesso valore anche per lo stato sprint upstream |
| `{pistol,smg}spreadmoving1` | 1 → 2 | camminata; in crouch viene moltiplicato anche per spreadcrouch |
| `{pistol,smg}spreadcrouch1` | 1 → 0.5 | crouch fermo; bonus applicato anche al movimento in crouch |
| `{pistol,smg}spreadinair1` | 0 → 2 | incremento additivo del moltiplicatore in aria, escluse scale |

La dispersione effettiva è `base * accmodspread`, limitata da `WSP`:
in piedi 1x, corsa 3x, crouch fermo 0.5x, crouch in movimento 1x. In aria
si aggiunge 2 al moltiplicatore. `spreadmin1=0`, `spreadmax1=0` rimangono
upstream (nessun limite aggiuntivo); `spreadz1` resta 2 per pistol, 1 per SMG.
Non sono gradi Source: `offsetray` usa `distanza * spread / 10000` per scalare
un offset casuale di raggio massimo 50, con ulteriore divisione verticale
per spreadz. Esempio: a distanza 1000 unità, SMG standing ha raggio massimo
10 unità, corsa 30, crouch fermo 5. Sono valori iniziali da collaudare.

Piccola correzione C++ in `weapons::accmodspread` (`weapons.cpp`):
`gameent::running()` (`game.h`) indica lo stile e risulta vero anche da fermo.
Il ramo di corsa ora richiede input move/strafe. Il ramo di movimento applica
inoltre il bonus crouch, invece di ignorarlo. I pesi upstream 1 conservano la
risposta originale di pistol/SMG nel profilo arena (base zero); il test della
funzione verifica anche equivalenza dei modificatori con i pesi originali.

`weapons::shoot` chiama questa funzione e `offsetray` prima di `projs::shootv`:
la traiettoria cambia realmente. L'HUD usa la stessa funzione per il feedback.
Il server sincronizza i parametri, valida stato/munizioni e inoltra le shot
positions ricevute (`N_SHOOT`, `shotevent::process`); non ricalcola il sorteggio
della dispersione. Resta il modello di fiducia upstream, senza nuovo anticheat.
La selezione usa input e stato crouch, non la velocità residua: rilasciare i
tasti ripristina subito la precisione da fermo anche durante la frenata.
Rinculo, danno, rate of fire e munizioni non vengono ritoccati.

### Dispersione progressiva della raffica

Richiesta del 1 ottobre 2026. Non esisteva un accumulo di dispersione basato
sui colpi: `weapshot` indica munizioni del singolo tiro e `weaptime` cambia
anche durante reload/switch. Aggiunto stato separato per arma in `clientstate`
(`game.h`): `weapbloom` e `weapbloomtime`, inizializzati nel costruttore e
azzerati da `weapreset`, inclusi morte e spawn. Nessuna modifica al protocollo.

| Variabile GFVAR/GVAR IDF_GAMEMOD (`vars.h`) | Default → preset | Unità e limiti |
| --- | --- | --- |
| `spreadburstadd` | 0 → 0.35 | incremento adimensionale per colpo primary, 0..FVAR_MAX; 0 disabilita l'accumulo nel profilo originale |
| `spreadburstmax` | 1.5 → 1.5 | limite dell'incremento del moltiplicatore, 0..FVAR_MAX |
| `spreadburstrecovery` | 1200 → 1200 | ms per recuperare dal limite a zero, 1..VAR_MAX |

`getweapbloom` calcola `max(0, valore - limite * tempo_trascorso / recovery)`
senza mutare lo stato durante il disegno dell'HUD. `addweapbloom` applica
prima il recupero, poi l'incremento, con clamp al limite. `accmodspread`
moltiplica il valore di postura/aria per `1 + accumulo`. In `weapons::shoot`,
l'incremento avviene dopo il calcolo della traiettoria del colpo, soltanto
per primary con dispersione base positiva e dopo i controlli canshoot/munizioni.
Primo colpo preciso; tentativi a vuoto o durante reload non accumulano.
Gli alt-fire non accumulano né usano bloom.

Il limite 1.5 consente fino a 2.5x la dispersione della postura corrente;
tra colpi interviene il recupero, perciò la raffica SMG a 75 ms raggiunge
un incremento effettivo pre-colpo di circa 1.406 (2.406x). Il bonus crouch
rimane moltiplicativo. A limite pieno, 600 ms recuperano metà accumulo e
1200 ms lo azzerano. Cambiare arma non cancella il suo storico; il tempo
continua a farlo decadere. La ricarica SMG di 1250 ms normalmente permette
il recupero completo, senza reset immediato quando si preme reload.

Le impostazioni sono sincronizzate e preservate da savevars. Anche i bot
gestiti dal client usano questo percorso di tiro. Lo stato bloom è calcolato
dal client proprietario; come già per spread/shot positions, il server non
ricalcola il cono né introduce nuovi controlli anticheat. Per diagnosi esiste
`getclientweapbloom <client> <indice-arma>` in `client.cpp`, che restituisce
l'accumulo decaduto, non il moltiplicatore finale.

## Applicazione e persistenza

`engine::rehash` (`src/engine/server.cpp`) esegue `localinit.cfg` per client
locale, `servinit.cfg` per dedicato, con EXEC_VERSION. I profili generati
eseguono lì `tdm.cfg`, prima delle configurazioni client e prima dei socket
del dedicato. `autoexec.cfg` applica invece solo `client.cfg` dopo i defaults.
Alla fine del preset `sv_savevars` salva i default server non IDF_MAP.
`cleanup/resetgamevars` ripristinano questi default di sessione, non quelli
arena; nessun cambiamento ai default binari originali.

Le regole del preset sono IDF_GAMEMOD, non IDF_MAP: i file di livello non le
possono sovrascrivere e N_GAMEINFO importa solo variabili IDF_MAP. Salute,
armi, impulse e i nuovi coefficienti restano validi a ogni `spawnstate`.
I parametri ambientali di mappa (gravità, coast, materiali, hurt) restano tali;
su una nuova mappa vanno verificati. Il preset non promette uguali percorsi
su mappe pensate per parkour. Le rotazioni automatiche non aggiungono mutatori.

Profili in `.csgopen/original`, `.csgopen/csgopen-client`, `.csgopen/server`.
Passare al gameplay originale significa uscire e usare `dev.sh original`;
non tentare di ripristinare la sessione TDM con `resetvars`, che mantiene
volutamente i suoi default salvati.

## Mappa e collegamento locale

`bath` non è presente. `echo` è nel submodule maps a
`a77d20d1b03820b4ae6242ec79d4d3fd79e6fba3`; `echo.txt` documenta la derivazione
da Cube 2 e `echo.cfg` descrive il cortile del laboratorio. Sono presenti
spawn per Alpha/Omega oltre ai neutrali, su più quote. Nessun asset modificato.
I percorsi ordinari a terra sono la zona di prova; la percorribilità completa
senza parkour deve ancora essere confermata manualmente.

Il server usa serverip=127.0.0.1, serverlanport=0, servermaster="",
masterserver=0 e httpserver=0 (il dedicato upstream abilita HTTP di default).
`serveroption`/`setupserversockets` in `engine/server.cpp` riconoscono
`-si127.0.0.1`, `-sm`, `-ss1`, `-sp28801`. La porta info è quella di gioco+1.
La configurazione viene letta prima dell'apertura dei socket.

`engine/client.cpp::connectserv` ora esclude solo il nome letterale
`127.0.0.1` dal prompt delle linee guida del master. `doc/guidelines.txt`
esclude già il gioco offline e server scollegati dal master. Non si imposta
`connectguidelines` e gli altri indirizzi conservano il controllo upstream.
Usare il literal indicato anche al posto di `localhost` per questa prova.

## Indicatore di dispersione

`clipspread 1` nel profilo client CSGOpen collega il raggio del cerchio delle
munizioni alla dispersione primaria corrente: stessa `accmodspread` e `WSP`
del tiro, inclusi postura, penalità in aria e accumulo decaduto della raffica.
La scala è `clamp(sqrt(spread / 2), 0.65, 2.5)` rispetto al raggio upstream: la
SMG ferma è il riferimento. È indicativa, non un confine dei punti di impatto.
Il numero di glifi resta quello delle munizioni, con dimensione leggibile e
animazioni upstream; anche le animazioni di ricarica usano il raggio corrente.
Default binario 0 e anteprime invariate.

### Accumulo della pistola

Il preset imposta `sv_pistolspreadburstscale 2`: incremento 0.7 per colpo
contro 0.35 della SMG, con stesso limite 1.5 e recupero 1200 ms. Alla cadenza
minima della pistola (200 ms) il precedente incremento recuperava già 0.25
tra i colpi; a 280 ms tornava completamente a zero. Il nuovo valore permette
accumulo anche a 300–400 ms, preservando primo colpo, crouch e SMG.
Default del moltiplicatore 1; spreadburstadd 0 mantiene disattivata la funzione.
