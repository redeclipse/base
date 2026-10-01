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
