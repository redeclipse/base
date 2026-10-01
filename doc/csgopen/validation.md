# Verifica della milestone

Data: 30 settembre 2026. Implementazione e verifiche automatiche completate;
collaudo interattivo ancora necessario. Nessun commit o push automatico;
commit di snapshot successivamente richiesto dall'utente, senza push.

## Provenienza e ambiente

- Fork: `https://github.com/agea/csgopen-base.git`, remote `origin`.
- Upstream aggiunto: `https://github.com/redeclipse/base.git`.
- Commit iniziale: `faf378d12558addc700d0e464e7e8c3a39fbceee`, branch originario `master`.
- Branch di lavoro: `feat/tdm-prototype`. Nessun lavoro locale preesistente;
  nessun commit prima della richiesta esplicita di snapshot, nessun push.
- `sysctl -n hw.model`: Mac14,10; `uname -m`: arm64. macOS 27.0, build 26A428.
  Renderer rilevato dal client: Apple M2 Pro.
- Xcode: `/Applications/Xcode.app/Contents/Developer`; Apple clang 21.0.0,
  target arm64-apple-darwin27.0.0. GNU Make 3.81.
- pkg-config: pkgconf 2.5.1. SDL ABI 2.32.72 fornita da sdl2-compat (SDL3),
  SDL2_image 2.8.12, OpenAL Soft 1.25.2, libsndfile 1.2.2, zlib SDK 1.2.12.
- Homebrew ha segnalato macOS 27 come versione prerelease non supportata.

## Baseline senza modifiche al gameplay

1. `git clone --recurse-submodules https://github.com/agea/csgopen-base.git csgopen-base`.
   Rete del sandbox non disponibile; rieseguito con autorizzazione dell'ambiente.
2. `git remote add upstream https://github.com/redeclipse/base.git` e
   `git switch -c feat/tdm-prototype`.
3. Analisi del Makefile: Darwin finiva nel ramo pkg-config X11/GL Linux.
   Analisi launcher: Darwin non riconosciuto. `console.cpp` usava X11 su tutti
   i sistemi non Windows. Correzioni isolate in questi tre file.
4. `HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 brew install sdl2 sdl2_image openal-soft libsndfile`.
   Installate le dipendenze mancanti. Homebrew ha anche aggiornato dipendenze
   transitive dei pacchetti richiesti; non è stato eseguito `brew upgrade` globale.
5. `PKG_CONFIG_PATH="$(brew --prefix openal-soft)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" make -C src -j4 client server`.
   **PASS**: entrambi i binari Mach-O arm64. Log:
   `.csgopen/logs/baseline-build.log`. Avvisi upstream su funzioni non usate,
   precedenze/logica e conversioni; nessun errore.
6. `file` e `otool -L`: architettura arm64; OpenGL.framework nativo, SDL/Cocoa,
   OpenAL Soft e libsndfile. `lipo -verify_arch` sulle quattro librerie: **PASS**.
7. Primo avvio anticipato durante il clone: **FAIL**, contenuto `maps/readme.txt`
   ancora assente. Processo chiuso; non considerato una verifica grafica.
8. `bash -n scripts/csgopen/dev.sh` e `scripts/csgopen/dev.sh check`: **PASS**.

I log completi e la copia del diff di piattaforma sono in `.csgopen/logs/` e
non sono tracciati in Git. L'esito finale e la checklist seguono sotto.

## Verifiche eseguite

### Raffica: aggiornamento del 1 ottobre 2026

Aggiunto accumulo di dispersione per arma, con primo colpo invariato,
incremento 0.35, limite 1.5 e recupero dal limite in 1200 ms. Default
`spreadburstadd=0` conserva il profilo originale. Test delle funzioni reali
estratte da `game.h` e `weapons.cpp` nel harness C++ con dipendenze simulate:
**PASS** per raffica SMG, saturazione, recupero a 600/1200 ms, indipendenza
tra armi, bonus crouch, alt-fire escluso, accumulo disabilitato, indice invalido,
limite zero e actor nuovo. Mantiene i test della postura e delle 256
combinazioni originali. Log `.csgopen/logs/burst-unit.log`, sorgente locale
`.csgopen/burst-unit.cpp`. Non equivale a una prova di tiro con input reale.

Build client e dedicato: **PASS**, `.csgopen/logs/burst-build-stdout.log`.
Smoke aggiornato: **PASS**, `SMOKE_DONE FAILURES 0`, nessun timeout/comando
sconosciuto, respawn 2997 ms. Parametri burst confermati prima/dopo morte e
cambio Echo→Dutility; getter dell'accumulo zero agli spawn. Log:
`.csgopen/logs/burst-smoke.log`. Il test non produce una raffica fisica;
l'aumento nonzero e il recupero sono verificati nel harness, mentre rosata,
rinculo combinato e sensazione richiedono la prova manuale su muro.

### Taratura del movimento: primo feedback

L'utente ha giudicato l'arresto leggermente brusco nella prova su Echo senza
bot. `sv_movebrakescale` passa da 1.5 a 1.25; velocità 0.55 e accelerazione
0.75 restano quelle della prova. Aggiornato il valore atteso nello smoke.
La risposta a terra converge più lentamente; l'utente ha successivamente
approvato il movimento con frenata 1.25. Le esecuzioni storiche sotto usavano
frenata 1.5 e non costituiscono una verifica del nuovo valore.
Avvio locale rieseguito: personaggio vivo, speed 0.55, accel 0.75, brake 1.25,
impulse 1 e nessun bot, senza comandi sconosciuti. Log:
`.csgopen/logs/movement-brake-125.log`. Smoke completo non rieseguito per
questa taratura al momento del primo avvio.

### Conferme manuali e precisione delle armi

L'utente conferma salute senza regen, respawn con ripristino dell'equipaggiamento
e persistenza delle regole dopo cambio mappa/nuova partita, compreso friendly
fire attivo. Il log `.csgopen/logs/ff-manual.log` contiene anche uccisioni del
bot alleato. Movimento approvato con frenata 1.25. Questi sono risultati
manuali riferiti dall'utente, non misure strumentali della velocità o del danno.

L'utente non ha notato problemi di precisione nel preset precedente, poi ha
richiesto esplicitamente meno precisione correndo e più precisione in crouch.
La nuova taratura deve essere provata manualmente: non estendere a essa
l'approvazione delle armi precedenti.

Test della funzione reale `accmodspread` estratta da `weapons.cpp` e compilata
in un harness C++ con actor/ladder/lookup delle variabili simulati: **PASS**.
Pesi letti dal preset: standing 1, running/sprinting 3, crouch 0.5,
crouch moving 1, walking 2, running airborne 5, ladder senza penalità aria.
Con pesi originali, confronto con la funzione del commit HEAD su 256
combinazioni stato/arma/zoom: **PASS**, stesso risultato. Il test non simula
input fisico, collisioni o traiettorie dei proiettili. Sorgente e log locali:
`.csgopen/accuracy-unit.cpp`, `.csgopen/logs/accuracy-unit.log`.

`dev.sh build`: **PASS**, client ricompilato e dedicato già aggiornato,
binari arm64. Smoke completo con frenata 1.25 e tutti i nuovi parametri di
dispersione: **PASS**, `SMOKE_DONE FAILURES 0`, nessun timeout/comando
sconosciuto, respawn misurato 2996 ms. Parametri confermati prima/dopo morte
e dopo cambio Echo→Dutility. Log: `.csgopen/logs/accuracy-build-stdout.log`
e `.csgopen/logs/accuracy-smoke.log`. Il confronto della rosata con colpi
reali in piedi/corsa/crouch resta da eseguire manualmente.

### Aggiornamento: friendly fire attivo

Su richiesta successiva dell'utente, il preset abilita ora il friendly fire
per umani e bot: `playerteamdamage=7`, `botteamdamage=7`,
`damageteamscale=1`. Sostituisce l'obiettivo iniziale di disabilitarlo.
Smoke aggiornato rieseguito contro il dedicato loopback: **PASS**,
`SMOKE_DONE FAILURES 0`, respawn misurato 2999 ms, valori conservati dopo
respawn e cambio Echo→Dutility. Log: `.csgopen/logs/ff-smoke.log`.
Le prove della prima milestone riportate sotto sono storiche.

L'utente ha riferito che la prova di equipaggiamento (punto 5) sembra corretta;
non ha confermato separatamente tutte le varianti dopo respawn/cambio mappa.
Ha inoltre verificato che il preset precedente bloccava i danni al bot alleato,
con simbolo di divieto: quel risultato riguarda il comportamento ora sostituito.
Il danno reale tra alleati è stato poi confermato nella sessione manuale,
aspettando la fine della protezione di spawn. Una sessione con `sv_botbalance 4`
fornisce un bot alleato e due avversari; non serve un secondo client per questa
prima verifica umano→bot.

### Baseline avviata prima del gameplay

```sh
scripts/csgopen/dev.sh original '-xtdm echo; sleep 20000 [quit]'
```

**PASS avvio/log, non collaudo visivo**: exit 0, contesto OpenGL 4.1 Metal
(-91.7), GLSL 4.10, OpenAL Soft con uscita stereo 48 kHz, caricamento Echo
CRC `4f8346be`, scambio game info e inizio partita con bot. Copia preservata:
`.csgopen/logs/baseline-original.log` e `baseline-original-stdout.log`.
Non è stato necessario cambiare versione GL o disabilitare il rendering.

Il controllo UI non ha individuato l'eseguibile non impacchettato come app
controllabile; la selezione diretta del suo percorso è fallita. Non ho
osservato/interagito con la finestra del gioco. Il preview incluso della
mappa non costituisce prova del rendering di questo client.

### Prototipo e script

| Prova | Esito ed evidenza |
| --- | --- |
| Build client e dedicato dopo patch gameplay e loopback | PASS; `prototype-build.log`, `script-build-stdout.log`; binari arm64 |
| `bash -n scripts/csgopen/dev.sh`, `sh -n redeclipse.sh` | PASS; non è stato eseguito ShellCheck |
| `dev.sh check` | PASS; librerie native e SDK presenti |
| check/build da `/private/tmp` attraverso symlink con spazi nel percorso | PASS; `paths-and-errors.log`; nessuna dipendenza dal cwd |
| `CSGOPEN_JOBS=0` e nome mappa `invalid/path` | PASS: exit 1 e messaggi comprensibili, prima dell'avvio |
| Primo client TDM locale, uscita da spectator | PASS; stato vivo, salute reale 100, clip pistola 10 e SMG 40 |
| Dedicato e socket | PASS; solo UDP 127.0.0.1:28801 e 127.0.0.1:28802, `server-sockets.txt` |
| Reset default server (`sv_resetvars 1`) | PASS: salute 100 e speed 0.55 conservati, `server-stdout.log` |
| Smoke client→dedicato, morte e nuovo spawn | PASS; `final-smoke.log`, zero assert falliti, salute/equipaggiamento ripristinati |
| Cambio Echo→Dutility sul dedicato | PASS; CRC Dutility `7232560a`, game info, nuovo match, regole condivise ancora attive |
| Comandi preset e sincronizzazione | PASS nei log: nessun comando sconosciuto/errori di configurazione; health 100, impulse 1, abilities 7383, accel 0.75, brake 1.5 |
| Profilo originale dopo il prototipo | PASS avvio e default arena conservati; `final-original.log` |
| `git diff --check`, submodule ai commit registrati, stato asset | PASS; 40 submodule, nessun asset modificato, `submodules.txt`, `asset-status.txt` |

Il dedicato è stato avviato con:

```sh
scripts/csgopen/dev.sh server '-xecho CSGOPEN_SERVER_READY; echo (concat HEALTH $sv_playerhealth IMPULSE $sv_playerimpulse REGEN $sv_playerabilities ACCEL $sv_moveaccelscale BRAKE $sv_movebrakescale); sv_resetvars 1; echo (concat RESET_HEALTH $sv_playerhealth RESET_SPEED $sv_movespeed)'
```

Socket ispezionati con `lsof -a -p <PID-del-test> -nP -i`. HTTP, LAN discovery
e master disabilitati prima di aprire socket; nessun intervento sul router.
Il processo di test viene chiuso alla fine delle verifiche.

Smoke ripetibile del repository, dedicato sulla porta di default già attivo:

```sh
scripts/csgopen/dev.sh tdm '-xexec "config/csgopen/smoke.cfg"'
rg 'CHECK_FAIL|RESPAWN_ELAPSED|SMOKE_DONE|SMOKE_TIMEOUT' .csgopen/logs/tdm-client.log
```

Esito finale: `SMOKE_DONE FAILURES 0`, nessun timeout. Misura finale respawn:
2998 ms; altra esecuzione 3005 ms. La misura sottrae timestamp client di
messaggi distinti, non misura direttamente il clock del server: la verifica
usa tolleranza 50 ms e conferma che a 2500 ms il personaggio non sia vivo.
Il ritardo server resta precisamente `playerspawndelay=3000` ed è controllato
in `m_delay` e nella coda di spawn. Lo smoke verifica valori sincronizzati,
salute/armi reali e assenza delle altre 15 armi possedute, prima e dopo morte.
Richiede il nuovo spawn con spectator/rientro dopo il suicidio: **non prova
il click fisico di respawn**. Il cambio mappa verifica regole, CRC e match,
non la percorribilità di Dutility.

Controllo finale del profilo originale con lo stesso binario:

```sh
scripts/csgopen/dev.sh original '-xtdm echo; sleep 8000 [echo (concat ORIGINAL_DEFAULTS HEALTH $playerhealth SPEED $movespeed IMPULSE $playerimpulse ACCEL $moveaccelscale BRAKE $movebrakescale); quit]'
```

Atteso/osservato: salute actor 1000, speed 1, impulse 4095, nuovi coefficienti
entrambi 1. Log originale separato da quello CSGOpen.

### Prove fallite e correzioni

- Avvio senza asset completi: fallimento preservato nella baseline; risolto
  attendendo l'inizializzazione dei submodule.
- Prima connessione dedicata: prompt upstream delle linee guida pubbliche,
  con errori UI `p_label_align`. Copia `remote-smoke-guidelines-blocked.log`.
  Il documento upstream esclude uso offline/server senza master. Applicata
  eccezione limitata al literal 127.0.0.1 in `connectserv`; nessun consenso
  impostato. Connessione reale rieseguita con successo.
- Primo script respawn: `primary 1; primary 0` non genera un tasto premuto
  (`D` in `command.cpp` usa `keypressed`); personaggio rimasto morto. Sostituito
  con spectator/rientro, senza patch alla logica di respawn.
- Primo assert strettamente `>=3000` sui timestamp client: misura 2994 ms,
  fallimento del test. Introdotta tolleranza dichiarata, non abbassato il
  ritardo di gioco. Riesecuzione finale passata.

## Verificato soltanto nel codice

- TDM già nativo con Alpha/Omega, nessun mutatore team necessario.
- Regen subordinata alla capacità A_A_REGEN, rimossa per umani e bot.
- Friendly fire attivo: teamdamage A_T_PLAYER per umani e bot, fattore danno
  alleati 1 nei percorsi server/client (richiesta successiva al preset iniziale).
- Solo IM_T_JUMP conserva il salto normale; le altre capacità impulse non
  sono consentite. Crouch e le capacità MOVE/JUMP/CROUCH restano presenti.
- I nuovi coefficienti usano GFVAR e la sincronizzazione esistente; default 1
  conserva la precedente formula. Nessuna modifica al tempo globale.
- Server valida loadout/pickup e armi disabilitate; SMG è fullauto, Rifle no.
- Regole IDF_GAMEMOD, separate da IDF_MAP; configurazione letta prima dello
  spawn/socket, default salvati per cleanup. Nessun aggiornamento degli asset.
- Rami Linux/Windows preservati nella patch di piattaforma: non compilati qui.

Riferimenti a simboli, limiti e valori in [gameplay.md](gameplay.md).

## Procedure manuali e controlli residui

Vedere gli esiti confermati sopra per movimento, salute, respawn e persistenza.
Restano prove complete con due umani, percorribilità completa di Echo,
diagnosi visiva degli avvisi e taratura della nuova dispersione delle armi.

1. Avviare `original`: verificare finestra, testi, HUD, modelli e shader,
   audio, mouse/tastiera, ingresso in partita e assenza di artefatti grafici.
2. Avviare `tdm`, scegliere nome, `/spectate 0`; confermare squadre e spawn.
   Percorrere cortile e accessi di Echo senza parkour. Annotare spawn isolati
   o percorsi che richiedono movimenti vietati: Echo resta candidata finché
   questo controllo non passa.
3. Provare corsa, strafe, accelerazione, arresto; confronto con `original`.
   Annotare sensazione e velocità senza modificare `gamespeed`.
4. Saltare da fermo/in corsa, accovacciarsi. Provare ripetutamente salto in
   aria, dash, boost, wallrun/walljump, slide e vault: non devono attivarsi.
5. Alla comparsa verificare HP 100, pistola/SMG; sparo semiauto/automatico e
   ricarica. Passare sui pickup delle altre armi e tentare altri loadout:
   devono restare inutilizzabili. Controllare anche dopo respawn/cambio mappa.
6. Subire danno senza morire (dopo la protezione spawn), allontanarsi e
   attendere almeno 10 s: HP non deve risalire. Confermare danno reale e HUD.
7. Con un bot alleato (`sv_botbalance 4`) o due umani nella stessa squadra,
   sparare al compagno dopo protezione spawn e confermare danno e possibile
   morte; anche contro un avversario il danno deve esserci.
   Per due client sullo stesso Mac servono home/log separati; non usare due
   istanze del wrapper sul medesimo profilo. Un secondo computer non può
   collegarsi al bind loopback della milestone.
8. Morire e richiedere respawn con click/salto dopo l'animazione di morte;
   verificare limite di circa 3 s, armi/HP e capacità conservate. Variare
   spawndelay per umano e bot nel preset, riavviare e confrontare.
9. Cambiare mappa/ripartire partita e poi tornare a `original`: verificare
   isolamento delle regole anche con interazione completa.

## Problemi aperti e prossimo passo

Nessun blocco di compilazione o caricamento GL osservato. Restano avvisi
upstream: alcune animazioni IQM mancanti (zapper/claw/sword-idle2), profili
PNG iCCP con CRC errato e segnalazione driver Apple di sampler all'avvio.
Non sono stati corretti alterando gli asset né nascosti con feature toggle.
Dutility ha un rail fuori mappa e serve soltanto a provare il cambio livello.
Non è stato confermato visivamente se gli avvisi producano artefatti.

La milestone non è interamente collaudata finché la checklist non passa.
Prossimo intervento consigliato: chiudere il collaudo di Echo e misurare
velocità/tempi di accelerazione e arresto su un percorso a terra ripetibile;
tarare i tre coefficienti prima di introdurre un'arma o un rinculo nuovo.

## Cerchio munizioni e dispersione — 1 ottobre 2026

Build nativa client/dedicato completata e `git diff --check` superato.
Il client aggiornato è stato avviato su Echo senza bot; il log
`.csgopen/logs/ring-manual.log` conferma `RING_TEST_READY STATE 0 RING 1 ADD 0.35`.
Ispezione del codice: raggio calcolato con lo stesso spread del tiro primario,
scala limitata, glifi delle munizioni e animazioni conservati; default originale
0 e anteprime escluse. Verifica visiva di leggibilità, corsa, crouch, raffica
e recupero ancora da effettuare dall'utente.

## Accumulo pistola — 1 ottobre 2026

Incremento per colpo della sola pistola portato da 0.35 a 0.7 tramite
`pistolspreadburstscale 2`; SMG invariata. Build client/dedicato e diff check
superati. Harness locale delle funzioni reali get/addweapbloom: accumulo
crescente a intervalli 200/300/400 ms, limite, recupero e indipendenza arma
superati. Smoke dedicato `.csgopen/logs/pistol-smoke-retry.log`: FAILURES 0,
respawn 2995 ms, parametro sincronizzato e conservato dopo cambio mappa.
Il primo tentativo è andato in timeout durante l'avvio tardivo del server;
il secondo ha avuto un segfault nel caricamento grafico delle texture mixer.
Il terzo ha completato il collaudo. Causa del crash non determinata.
Valutazione della sensazione e visibilità della pistola demandata all'utente.

## Riferimento Desert Eagle / PP-Bizon — 1 ottobre 2026

Prima fase solo configurazione, nessuna modifica binaria o agli asset.
Fonte gid=0 esportata in CSV e righe selezionate conservate in JSON.
Smoke dedicato `.csgopen/logs/reference-smoke.log`: SMOKE_DONE FAILURES 0,
respawn 2998 ms. Controllati danni e moltiplicatori configurati, cadenza,
fullauto, caricatori e riserve reali (7+21 / 64+128) anche dopo respawn;
parametri preservati dopo cambio mappa. `git diff --check` superato.
Nessuna verifica automatica di colpi fisici/headshot: conteggi 2/4 torso e
1 testa sono previsioni dal codice danni, da confermare in gioco.
Dispersione/recoil, armatura, falloff e mobilità Source non riprodotti in
questa fase; limiti documentati nel README e gameplay.

## Arsenale esteso — 1 ottobre 2026

Build native client/dedicato superate. Primo test arsenale fallito su alcune
scelte perché rientro spettatore richiesto entro DEATHMILLIS; test corretto
con attesa di 1100 ms. Menu loadout esponeva anche un errore upstream di
alias p_label_align: aggiunto il default mancante al widget decortext.
Secondo test `.csgopen/logs/arsenal-smoke-retry.log`: FAILURES 0, respawn
2999 ms. Sei primarie effettivamente assegnate con caricatore/riserva e
pistola; canshoot primaria consentita per tutte, secondaria consentita solo
Rifle. Splash/residual primari disattivati. Non sono stati simulati input
fisici: uso del menu, zoom AWP e sensazione di tiro richiedono prova manuale.

Test finale `.csgopen/logs/arsenal-final-smoke.log`: SMOKE_DONE FAILURES 0,
respawn 2997 ms. Verificati anche danni, moltiplicatori testa, rays, cadenza,
fullauto, munizioni e collisione dei cinque nuovi slot dopo cambio mappa.
Nomi finali e sidearm fissa documentati; nessun commit o push automatico.

## Correzione salvataggio loadout — 1 ottobre 2026

Segnalazione utente: scelta AWP nel menu non visibile dopo suicidio.
Ispezione: validazione upstream dipendeva dal secondo slot nascosto e poteva
rifiutare la primaria con filtro casuale vuoto. Modificata validazione nel
preset, scelta primaria salvata immediatamente e callback UI literal.
`.csgopen/logs/loadout-menu-smoke.log`: SMOKE_DONE FAILURES 0, tutte le sei
primarie assegnate passando da gameui_player_loadout_set/validate/set, con
filtro vuoto. Rendering e click fisici del menu restano verifica manuale.

Regressione locale aggiuntiva `.csgopen/logs/loadout-suicide-manual.log`:
MENU_AWP_SELECTED 8 VALID 1; dopo suicidio e respawn STATE 0 WEAPON 8
CLIP 5 RESERVE 10. Sessione aperta senza bot con menu loadout.

## Effetti proiettile e assenza rimbalzi — 1 ottobre 2026

Build nativa riuscita, diff check superato. Test dedicato
`.csgopen/logs/bullet-smoke.log`: SMOKE_DONE FAILURES 0, respawn 3004 ms.
Verificati collide1=241 e FX muzzle/trail/power su tutte le sette armi,
anche scoped AWP, dopo respawn e cambio mappa. Ispezione dei flag collisione:
nessun BOUNCE/DRILL/STICK, impatto su geometria/player/shots.
Audio degli slot energetici rimappato al tiro primario SMG solo nel preset;
transit e loop energetici Zapper esclusi. Nessuna modifica agli asset.
Verifica visiva/sonora e traiettorie da input fisico ancora manuale.

Ripristinati in configurazione gli effetti convenzionali originali di
Shotgun/Minigun su richiesta utente; valori confrontati con weapons.h e
aspettative smoke aggiornate. Nessuna modifica a danno/cadenza/collisione.
Diff check superato; nuova verifica visiva da fare al successivo avvio.

## HE e cook — 1 ottobre 2026

Build native client/dedicato superate, strumentazione temporanea del motore
rimossa prima della build finale. `.csgopen/logs/he-fixture.log` verifica
armamento, blocco cambio/drop, consumo della singola granata e rilascio dopo
1500 ms con LIFE 1500. Overcook: LIFE 1, SPEED 0, CENTER_DISTANCE 0;
esplosione effettiva sul proprietario da 100 HP a -20 e morte.
La fixture invocava le funzioni gameplay, senza simulare input fisici.

Smoke su server dedicato `.csgopen/logs/he-smoke.log`:
SMOKE_DONE FAILURES 0, respawn 3000 ms. Verificati parametri HE, inventario
1+0, selezione diretta, rifornimento al respawn e persistenza cambio mappa.
`git diff --check` superato. Cook da mouse, rimbalzi, danno ad altri player
ed esplosione alla morte durante cook richiedono ancora verifica manuale.

HE potenziata su richiesta: danno base 120 → 180 e raggio 48 → 72 (+50%
entrambi), miccia invariata. Solo configurazione e documentazione.
`.csgopen/logs/he-tuning-smoke.log`: SMOKE_DONE FAILURES 0; valori
sincronizzati e persistenti al cambio mappa, diff check superato.
Bilanciamento e sensazione del nuovo raggio da verificare in gioco.

HE colpibile: collide1 784 → 920 aggiunge COLLIDE_PROJ e IMPACT_SHOTS.
Ispezione projs.cpp: registrazione in collideprojs; hiteffect sui proiettili
chiama projpush, che distrugge il bersaglio locale o notifica il proprietario
remoto. Usa il percorso di esplosione nativo, senza nuove modifiche C++.
`.csgopen/logs/he-shootable-smoke.log`: SMOKE_DONE FAILURES 0, collisione
sincronizzata verificata anche dopo cambio mappa. Diff check superato.
Colpo effettivo su HE in volo/a terra da verificare manualmente.

## Smoke fumogena — 1 ottobre 2026

Build native client/dedicato superate. Fixture temporanea projs.cpp rimossa
prima della build finale, nessun comando di test distribuito. La fixture
crea un proiettile Mine sintetico e usa update/destroy reali del motore:
`.csgopen/logs/smoke-grenade-fixture-retry.log` mostra BLOCKED 1, CLEAR 1,
OPACITY 1 e HP 100; successivamente COUNT 0, DURATION 18000.
Il primo tentativo usava un ID locale non registrato nel server (sync error
atteso dalla fixture); il retry evita la notifica sintetica. I tempi di
CubeScript sono wall-clock mentre le nubi usano lastmillis di simulazione;
la scadenza viene verificata nello stato del motore, non dal solo timestamp.
Nessun input fisico simulato e nessuna verifica multiplayer della nube qui.

Build finale `.csgopen/logs/smoke-grenade-final-build.log` superata.
Smoke dedicato `.csgopen/logs/smoke-grenade-smoke.log`:
SMOKE_DONE FAILURES 0, respawn 2999 ms. Verificati parametri sincronizzati,
Mine abilitata come smoke, clip 1/reserve 0 al respawn, niente damage/radial,
HE conservata e persistenza al cambio mappa. Diff check superato.
Rendering esterno/interno, cook da mouse, memoria di tiro dei bot e nube
su due client richiedono prova manuale. Late join non ricostruisce nubi
esistenti; nube sferica senza clipping ai muri, limiti nel README.

## Densità esterna smoke e bot — 1 ottobre 2026

Utente conferma resa interna adeguata, segnala esterno troppo trasparente.
Ispezione renderer: PART_SMOKE usa compositing additivo; passaggio a
PART_SMOKE_LERP (PT_LERP) per coprire le sagome. Tre strati da 16 particelle
più centro, vita 600 ms invece di 350, stessa emissione ogni 100 ms.
Raggio 56 → 68 (+21.4%); overlay interno, durata e miccia invariati.
Build `.csgopen/logs/smoke-density-build.log` superata. Test dedicato
`.csgopen/logs/smoke-density-smoke.log`: SMOKE_DONE FAILURES 0,
respawn 3000 ms; nuovo raggio persistente dopo cambio mappa.
Diff check superato. Opacità esterna e prestazioni richiedono prova manuale.
Sessione di prova con botbalance 4 (utente più tre bot), skill 20–25,
adattamento skill disabilitato solo per questa sessione.

## Sagome attraverso smoke/muri — 1 ottobre 2026

Screenshot utente: halo colorati visibili attraverso fumo e geometria.
Preset client playerhalos/playerhalodamage 0; guardia CSGOpen nel pass HALO
impedisce comunque la silhouette di altri player. Ispezione renderer:
renderplayer (modello e attachment) e rendercheck (effetti status) saltati
quando la linea camera-centro attraversa smoke densa, per entrambe le squadre.
Label/overlay e radar applicano smoke + raycubelos, senza bypass per compagni.
Controllo discreto sull’intero modello, possibili transizioni ai bordi.

Build `.csgopen/logs/smoke-visibility-build.log` superata. Test dedicato
`.csgopen/logs/smoke-visibility-smoke.log`: SMOKE_DONE FAILURES 0,
compresi no_player_halos/no_damage_halos dopo respawn e cambio mappa.
Diff check superato. I test automatici verificano impostazioni e ciclo di gioco;
la scomparsa visiva di modelli/indicatori richiede nuova prova manuale con bot.

## Etichette solo compagni — 1 ottobre 2026

Preset client entityitemui/entityprojui -1: niente etichette su pickup e loot.
Guardia player/playeroverlay richiede stessa squadra non neutrale, oltre
alla visibilità già verificata con smoke e raycubelos; nessuna etichetta nemico.
Build `.csgopen/logs/labels-build.log` superata; test dedicato
`.csgopen/logs/labels-smoke.log`: SMOKE_DONE FAILURES 0, compresi
no_pickup_labels/no_loot_labels dopo respawn e cambio mappa.
Diff check superato. Comportamento grafico delle etichette compagni da
verificare manualmente nella sessione con bot.

## Corroder smoke e mina circolare — 1 ottobre 2026

Smoke migrata a Corroder con inventario spawn condiviso umano/bot, modello,
animazioni, icona, suoni e fisica da Grenade; colori HE arancione/smoke grigio.
Mine torna una mina circolare separata; Rocket resta disabilitato e riservato.
H smoke, J mina, G HE. Nessun asset o protocollo modificato.

Fixture temporanea rimossa prima della build finale. Primo test positivo
usava un centro target coincidente; retry con target definito a due unità:
`.csgopen/logs/mine-fixture-retry.log` OWNER 0 ALLY 0 ENEMY 1 UNARMED 0
DISTANT 0 DEAD 0 WALL_FOUND 1 WALL 0, ARM 1500 RANGE 32 AGE 1600.
SMOKE_MODEL weapons/grenade/hwep THROWN 1; proiettile sintetico Corroder
attraverso update/destroy reali: COUNT 1 OPACITY 1 HP 100. Questo verifica
predicato di innesco e nube; non è una prova di lancio da input fisico né
un colpo contro una mina su due client.

Build `.csgopen/logs/mine-final-build.log` superata, nessun comando fixture
nel sorgente finale. Test dedicato `.csgopen/logs/mine-smoke.log`:
SMOKE_DONE FAILURES 0, mine e smoke 1+0 indipendenti dopo respawn;
verificati armamento, raggio, collisione, danno, colori, Rocket disabilitato,
e persistenza al cambio mappa. Diff check superato.
Placement, detonazione effettiva su nemico, shot-down, resa dei colori e
sincronizzazione visiva restano verifiche manuali nella sessione con bot.

## Lanciagranate HE — 1 ottobre 2026

Rocket ora è un lanciagranate disponibile allo spawn: un colpo caricato e sei
in riserva, ricarica singola da 1800 ms, velocità 650 contro 250 della HE a
mano. Stessa miccia di 3000 ms, danno, raggio e collisioni della HE, senza
cook o guida. K seleziona l'arma; tiro secondario escluso dal preset.

Build finale `.csgopen/logs/launcher-final-build.log` superata. Fixture nativa
con doshot/weapreload reali: primo tentativo interrotto da auto-danno di una
HE rimbalzata; retry con auto-danno disattivato solo nel test ha sparato sette
colpi, consumato le sei riserve e rifiutato tiro/ricarica finali. Log
`.csgopen/logs/launcher-fixture-retry.log`: SHOTS 7, RESERVE 0, CAN_FIRE 0;
modello weapons/grenade/proj, LIFE 3000, COLLIDE 920, velocità effettiva 357.5
(dopo movespeed 0.55). Fixture rimossa prima della build finale.

Test sul server dedicato `.csgopen/logs/launcher-smoke.log`:
SMOKE_DONE FAILURES 0, incluse regole HE e inventario 1+6, respawn e cambio
mappa. Diff check superato. Gittata e resa visiva da verificare manualmente;
nessuna simulazione di input fisico o prova visiva automatica effettuata.

### Cook, impatto e rinculo del lanciagranate

Cook LIFEN 8/3000 ms; stesso blocco cambio/drop/pickup della HE. A fine cook
shootv crea la granata al centro del giocatore con lifetime1 e velocità zero.
La morte mentre si cucina il Rocket usa quel proiettile e consuma la sua
munizione, senza consumare la HE separata. Kickpush ridotto da 300 a 5,
rinculo verticale 0.1–0.2 e orizzontale zero.

Contatto diretto aggiunge HIT_PROJ|HIT_FULL e registra il client colpito per
non danneggiarlo nuovamente con la stessa granata. Calcolo client/server:
25 HP nel preset con danno HE esplosivo invariato; auto-danno/friendly fire
seguono i moltiplicatori esistenti. Nessun nuovo messaggio di protocollo.

Fixture temporanea di calcdamage/shootv rimossa prima della build finale:
`.csgopen/logs/launcher-cook-fixture.log`, DIRECT25 BLAST180, lifetime
3000/1500/1 a cook0/0.5/1, velocità zero a cook completo. I tiri sintetici
condividono il giocatore (la spinta cambia la velocità ereditata fra i tiri);
il flag cooked non scala la velocità del lancio. Questa verifica non è un
contatto fisico su un bot né un test di impatto su due client.
Build finale `.csgopen/logs/launcher-cook-final-build.log` superata.
Test dedicato `.csgopen/logs/launcher-cook-smoke.log`: SMOKE_DONE FAILURES 0,
incluse impostazioni cook/rinculo, inventario, respawn e cambio mappa.
Diff check superato. Resta la prova manuale di contatto con un bot, cook da
input fisico e sensazione del rinculo.

## Loadout con SMG oppure cinque utility — 1 ottobre 2026

Una primaria (ora comprende Rocket), Deagle fissa e scelta esclusiva tra
Bizon/MP9 secondaria e cinque slot HE/smoke/Mine. Slot ordinati, duplicati e
vuoti conservati nel preset; validazione condivisa spawnstate e parser
originale invariato con csgopenweapons0. Clip utility cap5/store0, nessuna
ricarica fra lanci; pickup utility/armi nuove bloccati per evitare bypass.
Menu (,), salvataggio immediato e applicazione al respawn.

Build finale `.csgopen/logs/loadout-final-build.log` superata, fixture rimossa.
Test dedicato `.csgopen/logs/loadout-final-cases.log`: LOADOUT_DONE FAILURES 0.
Dieci combinazioni con inventario ricevuto dal server: mix2HE/2smoke/1mine,
cinque HE, launcher+SMG con utility richieste ma negate, AK+MP9, slot vuoti,
primaria/secondaria/utility non valide, doppia SMG identica, slot oltre il
limite, legacy M249, cinque smoke. Verificati Deagle, clip/riserve primarie,
callback del menu, esclusione reciproca, attesa del respawn e persistenza al
cambio mappa. Menu aperto tramite comando nativo, nessun errore di script;
controllo visivo non effettuato: il client non appare tra le app disponibili
al tool di computer use.

Fixture temporanea nativa in weapons.cpp con doshot reale (cook forzato a
1 ms, auto-danno disattivato solo nel test):
`.csgopen/logs/loadout-utility-fixture.log`, cinque lanci, clip4/3/2/1/0,
sesto tiro FIRED0, CAN_FIRE0/CAN_RELOAD0, reserve0. Pickup HE e nuova arma
negati. Il test verifica consumo senza reload, non input fisico o resa visiva.
Build finale ripristina danno e codice senza comandi fixture.

Test completi dedicati `.csgopen/logs/loadout-smoke.log` e
`.csgopen/logs/loadout-arsenal.log`: SMOKE_DONE FAILURES 0 in entrambi;
verificate tutte le primarie precedenti, permessi, respawn e cambio mappa.
Diff check superato. Original Red Eclipse verificato per ispezione delle
condizioni di preset; non rieseguito come sessione di gameplay.
Resta la prova manuale di disposizione del menu, click sui selettori e
contatori durante l'uso delle tre utility. Sessione con bot avviata per questo.

### Correzione larghezza menu loadout

Screenshot manuale dell'utente: pannello destro tagliato, slot5 e testi fuori
area. Il contenitore è fisso a0.5; cinque selettori0.12 più quattro gap0.01
richiedevano0.64. Nel preset selettori ridotti a0.08 (totale0.44), icone0.065;
pulsanti accessorio larghi0.2 ciascuno, titoli abbreviati e note su due righe
con wrap0.46. Dimensioni originali mantenute fuori dal preset. Nessuna modifica
alle callback o all'inventario; diff check superato. Prova visiva del menu
corretto richiesta nella nuova sessione, senza dichiararla automatizzata.

### Quattro slot granate

Menu limitato a quattro selettori; parser e spawnstate leggono sei posizioni
(primaria, secondaria, quattro utility). Capacità HE/smoke/Mine4; munizioni
Rocket sempre1+6. Profili da sette posizioni migrati conservando primaria,
secondaria e primi quattro slot. README aggiornati in inglese.
Build `.csgopen/logs/four-slots-build.log` e diff check superati.

Primo test matrice interrotto prima di DONE, con primary_clip AWP fallito;
non conteggiato come successo. Retry mirato `.csgopen/logs/four-slots-retry.log`:
inventari e limiti superati (mix2HE/1smoke/1mine, richiesta5HE limitata a4,
SMG esclude utility, richiesta5smoke limitata a4 anche dopo cambio mappa),
ma tre assert primary_selected falliti: LOADOUT_DONE FAILURES3. Il test non
è dichiarato interamente superato; causa dei cambi di arma selezionata non
stabilita. Questi assert non riguardano quantità o tipi assegnati.
Test generale `.csgopen/logs/four-slots-smoke.log` interrotto da SIGSEGV nel
caricamento della mappa, senza conclusione. Riavviato il server locale:
`.csgopen/logs/four-slots-smoke-retry.log` SMOKE_DONE FAILURES0, comprese
capacità HE/smoke/Mine4 dopo spawn, respawn e cambio mappa. Il crash iniziale
non è stato diagnosticato né dichiarato risolto dal cambio slot.
Anche primo avvio manuale SIGSEGV durante composizione texture mixer;
riavvio identico `.csgopen/logs/four-slots-manual-retry.log` riuscito:
FOUR_SLOTS_READY PRIMARY13 HE4, menu aperto e partita con bot attiva.
Rimane un crash intermittente di avvio da diagnosticare; non sono stati
modificati renderer o asset per attribuirgli una soluzione non verificata.

## Eclipse Recoil splash and icon — 1 October 2026

The supplied PNGs were copied unchanged into `data/csgopen/branding/`;
SHA-256 hashes match their source files. No upstream asset submodule was edited.
The TDM launcher applies branding before SDL initialization. The renderer fits
the complete splash, bypasses animated/map backgrounds, and hides upstream
loading logos and the central information panel while retaining loading status.

Executed checks:

- Native client/server build passed: `.csgopen/logs/branding-build.log`.
- Runtime loaded the splash as 3344 × 1882 and selected the supplied icon:
  `.csgopen/logs/branding-launch.log` and `branding-smoke.log`.
- Inspected native renderer screenshots in 16:9 and 4:3. The clean 4:3 capture
  `.csgopen/branding-check/splash-4x3-clean.png` shows the complete artwork with
  black margins. These captures use `forcenoview` after startup; they verify
  layout, rather than capturing every transient startup frame.
- The repository smoke test passed: `SMOKE_DONE FAILURES 0`, including respawn
  and map change, in `.csgopen/logs/branding-smoke.log`. The test used separate
  profiles and loopback port 28931; only the copied test's connection port changed.
- A fresh original profile launched with empty `splashtex` and
  `windowicontex = textures/icon`: `.csgopen/logs/branding-original.log`.
- `bash -n scripts/csgopen/dev.sh` and `git diff --check` passed.

The initial sandboxed launch failed because SDL could not access any display;
the graphical checks above ran successfully outside that restriction. Icon
selection is verified by runtime configuration and the existing
`SDL_SetWindowIcon` call; its appearance in the macOS Dock still needs a manual
visual check. No `.app` bundle or platform icon conversion was needed for this
native SDL launcher. Linux and Windows were not compiled in this check.

### Menu logo replacement

The supplied 2048 × 768 RGBA `logo.png` is copied unchanged into the branding
directory (matching SHA-256). Both `logotex` and `logocroptex` point to it.
Main-menu and welcome-screen images derive their height from the texture aspect
instead of stretching to the old 2:1 frame. The upstream logo is 1024 × 512,
so its original profile still receives the same 2:1 dimensions.

Inspected native screenshots `.csgopen/branding-check/logo-main.png` and
`logo-welcome.png`: both show the full Eclipse Recoil logo at the available
header width, without distortion. Runtime `.csgopen/logs/branding-logo.log`
confirms 2048 × 768 and both new paths. The test deliberately restored the old
logo variables before executing `client.cfg`; branding correctly reapplied.
No new binary build was needed: these changes only affect assets and CubeScript.
Existing smoke test on loopback port 28931 passed with
`SMOKE_DONE FAILURES 0`: `.csgopen/logs/branding-logo-smoke.log`, including
respawn and map change. `git diff --check` passed. Test sessions were closed.

## Client release workflow — 1 October 2026

Added `.github/workflows/release.yml` for pushes to `master` and manual runs.
It builds only the client target on five native hosts: macOS ARM64/Intel,
Linux x86_64/ARM64 and Windows UCRT64 x86_64. The publication job requires every
build to succeed, verifies all target checksum manifests, uploads into a draft
release, then publishes. Manual runs on other branches retain Actions artifacts
without publishing. Linux, Intel macOS and Windows builds have not been run
locally or on GitHub yet.

Executed locally on the ARM64 development Mac:

- `actionlint` 1.7.11 accepted the workflow without diagnostics. The downloaded
  tool's SHA-256 matched its official release checksum.
- Eight packaging regression tests passed: transitive Linux/Windows dependency
  closure, missing-library rejection, conflicting library names, inherited
  macOS rpaths, exact multipart reconstruction, download checksums and a
  single-file archive below the size limit.
- The native Makefile build check passed; no client/server recompilation was
  needed for the opt-in Windows Makefile changes. Log:
  `.csgopen/logs/release-build.log`.
- The full macOS ARM64 package was built from the local working tree, including
  all recorded assets, branding, runtime libraries, dependency notices, an ICNS
  icon and an ad-hoc signed `.app`. Log:
  `.csgopen/logs/release-package-final.log`.
- The initial archive exceeded GitHub's 2 GiB limit. Multipart packaging kept
  the full content in two parts (1500 MiB and approximately 700 MiB). The
  generated extraction helper verified all checksums and reconstructed/extracted
  the app successfully: `.csgopen/logs/release-extract-macos.log`.
- `codesign --verify --deep --strict` accepted the extracted app. Dependency
  inspection found SDL2's dynamically loaded SDL3 requirement, which is now
  included explicitly with a relocatable library name.
- The extracted app's launcher started successfully and completed the repository
  smoke test with **`SMOKE_DONE FAILURES 0`**, including respawn and map change:
  `.csgopen/logs/release-smoke-game-verified.log`. The test used an isolated
  profile copied from the previously verified smoke profile and a server bound
  to loopback port 28931. Only the copied fixture's connection port changed.
  Both test processes exited.
- Python syntax, shell launcher/extraction syntax and `git diff --check` passed.
  Windows ICO conversion was also exercised locally with Pillow.

Earlier package checks caught missing SDL3 before game initialization, and a
first network attempt reported a respawn timestamp 2 ms below the fixture's
tolerance before it ended without the final marker. A subsequent attempt was
interrupted during this conversation. Neither was counted as a passing smoke
test; the final completed run above supplies the passing evidence.

No GitHub run, release publication, commit or push was performed. The first CI
run must still establish native build/package compatibility on the other four
hosts. macOS downloads are ad-hoc signed, not Developer ID signed or notarized;
quarantine approval on a separately downloaded app remains a manual check.

### Windows CI fixture path correction — 1 October 2026

The supplied `windows-job-logs.txt` reports failure in
`test_linux_keeps_audio_closure_but_uses_host_glibc_and_gpu`. The job stops in
the Python regression suite before icon generation, client compilation or
Windows packaging. The simulated `ldd` output interpolated the Windows host's
temporary paths; the Linux dependency parser correctly expects absolute POSIX
paths, so neither fixture library was discovered.

The fixture now uses fixed Linux paths and maps them to real host-local files
at the filesystem boundary. Actual library copying and transitive dependency
checks remain exercised on every host. A regression case explicitly supplies
Windows-style fixture paths: the previous test reproduces the attached failure,
and the corrected suite passes all nine tests locally. `actionlint` and
`git diff --check` also pass. Production packaging, compiler flags and workflow
targets are unchanged.

The user reports successful builds on both macOS and both Linux targets.
Those outcomes are user-reported; only the attached Windows log was inspected
for this correction. Native Windows compilation and packaging still require
the next CI run, since the failed run did not reach those steps.

### Release checksum line endings — 1 October 2026

The supplied `win-job-logs.txt` shows nine passing tests and successful upload
of the Windows client package. `publish-job-logs.txt` shows all five packages
downloaded and the macOS/Linux checksums accepted. Publication then fails on
the Windows checksum manifest: Python's default text output on Windows adds
CRLF, and GNU `sha256sum` interprets the CR as part of each filename. The job
stops before creating or publishing the release.

Checksum manifests now use explicit UTF-8 bytes with LF on every host. Archive
parts and extraction scripts retain their content; their exact bytes are still
hashed. A regression simulates Windows text translation, fails before the fix,
and passes afterward. All ten packaging tests pass locally. A small multipart
Windows fixture generated under that simulation passes GNU coreutils 9.10
`sha256sum --check`. Converting its manifest to CRLF reproduces the missing-file
failure with the Mac's native `/sbin/sha256sum`; the newer local GNU version
accepts CRLF, unlike the Ubuntu runner in the supplied log.
`actionlint` and `git diff --check` also pass. No native rebuild was needed for
this Python-only correction. Full GitHub release publication remains pending
the next run with the corrected manifest generator.
