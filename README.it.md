<div align="center">

# real_delete

**Un piccolo tool in C che sovrascrive in modo sicuro il contenuto di un file prima di eliminarlo — senza usare `lseek()`.**

[🇬🇧 Read in English](README.md)

</div>

---

## Contesto

Questo è un piccolo esercizio scritto per il corso universitario di **Sistemi Operativi**, non un progetto vero e proprio — ma l'idea alla base (sovrascrittura allineata alle pagine tramite `mmap()` invece di `seek()` + `write()`) è una tecnica genuinamente utile.

---

## Cosa fa

Un semplice `rm` rimuove solo la entry della directory che punta al file — i blocchi dati restano sul disco finché non vengono sovrascritti da qualcos'altro, ed è proprio per questo che i file "eliminati" sono spesso recuperabili. `real_delete` sovrascrive il contenuto del file con zeri *prima* di eliminarlo, così è il dato stesso a sparire, non solo il riferimento ad esso.

```bash
./real_delete segreto.txt
```

---

## Perché `mmap()` invece di `seek()` + `write()`

Il modo più diretto per azzerare un file è un ciclo di chiamate `lseek()` + `write()`. Per file di grandi dimensioni, questo significa decine di migliaia di syscall — ognuna un context switch, e ognuna che usura un po' di più i dispositivi di storage flash.

`real_delete` invece mappa il file in memoria con `mmap()` e lo sovrascrive direttamente in **chunk da 4 KB**, allineati alla dimensione di pagina del sistema operativo:

- File ≤ 1 pagina: mappati e azzerati in un singolo passaggio `mmap()`/`memset()`/`munmap()`
- File più grandi: processati pagina per pagina, così l'uso di memoria resta costante indipendentemente dalla dimensione del file
- `msync(MS_SYNC)` forza ogni scrittura su disco prima di procedere, garantendo che la sovrascrittura avvenga davvero prima che il file venga eliminato

```c
char *file_map = mmap(NULL, to_map, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, offset);
memset(file_map, 0, to_map);
msync(file_map, to_map, MS_SYNC);
munmap(file_map, to_map);
```

---

## Compilazione ed esecuzione

```bash
gcc -Wall -Wextra -o real_delete real_delete.c
./real_delete percorso/al/file
```

Richiede un ambiente POSIX (`mmap`, `msync`, `unlink` — Linux/macOS/BSD). Non compila così com'è su Windows.

---

## Note sul comportamento

- **File vuoti** (0 byte) saltano completamente il passaggio di mapping e vengono semplicemente eliminati.
- **Le scritture su pagine mappate in memoria con `MAP_SHARED`** sono ciò che effettivamente raggiunge il disco — `msync` non è opzionale qui, è l'unica cosa che fa la differenza tra "sovrascritto" e "ancora in cache, ancora recuperabile".
- Il programma termina con errore (e un messaggio `perror()`) su qualsiasi fallimento di `open`, `fstat`, `mmap`, `msync` o `munmap` — non prosegue silenziosamente in caso di sovrascrittura parziale.

---

## Limitazioni note

- **Nessuna sovrascrittura multi-passata.** Un singolo passaggio di azzeramento è sufficiente contro i tool di undelete più comuni, ma non regge il confronto con un recupero forense su alcuni tipi di storage — questo è stato scritto come esercizio su `mmap()`, non come tool di cancellazione sicura production-ready (per quello vedi `shred(1)`).
- **Limite SSD/flash.** Il wear-leveling sugli SSD fa sì che il blocco fisico che contiene effettivamente i vecchi dati potrebbe non essere quello che il sistema operativo crede di sovrascrivere — nessun tool in userspace può garantire una cancellazione completa su storage flash senza supporto TRIM/secure-erase da parte del disco stesso.
- **Nessuna gestione di directory o symlink** — opera su un singolo percorso di file regolare.

---

## Licenza

MIT — vedi [LICENSE](LICENSE).
