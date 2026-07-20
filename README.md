<div align="center">

# real_delete

**A small C utility that securely overwrites a file's contents before unlinking it — no `lseek()` involved.**

[🇮🇹 Leggi in italiano](README.it.md)

</div>

---

## Context

This is a short exercise written for a university **Operating Systems** course, not a full-blown project — but the underlying idea (page-aligned overwriting via `mmap()` instead of `seek()` + `write()`) is a genuinely useful technique, so it's included here as a portfolio piece. Written by hand, no AI-assisted code.

---

## What it does

A plain `rm` only removes a file's directory entry — the actual data blocks stay on disk until overwritten by something else, which is exactly why "deleted" files are so often recoverable. `real_delete` overwrites the file's content with zeros *before* unlinking it, so the data itself is gone, not just the reference to it.

```bash
./real_delete secret.txt
```

---

## Why `mmap()` instead of `seek()` + `write()`

The straightforward way to zero out a file is a loop of `lseek()` + `write()` calls. For large files, that means tens of thousands of syscalls — each one a context switch, and each one wearing down flash-based storage a little more.

Instead, `real_delete` maps the file into memory with `mmap()` and overwrites it directly in **4 KB chunks**, matching the OS page size:

- Files ≤ 1 page: mapped and zeroed in a single `mmap()`/`memset()`/`munmap()` pass
- Larger files: processed page by page, so memory usage stays flat regardless of file size
- `msync(MS_SYNC)` forces each write back to disk before moving on, guaranteeing the overwrite actually lands before the file is unlinked

```c
char *file_map = mmap(NULL, to_map, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, offset);
memset(file_map, 0, to_map);
msync(file_map, to_map, MS_SYNC);
munmap(file_map, to_map);
```

---

## Building & Running

```bash
gcc -Wall -Wextra -o real_delete real_delete.c
./real_delete path/to/file
```

Requires a POSIX environment (`mmap`, `msync`, `unlink` — Linux/macOS/BSD). Won't build as-is on Windows.

---

## Behavior notes

- **Empty files** (0 bytes) skip the mapping step entirely and are just unlinked.
- **Zero-byte writes to memory-mapped, `MAP_SHARED` pages** are what actually reach disk — `msync` isn't optional here, it's the only thing standing between "overwritten" and "still cached, still recoverable."
- The program exits with an error (and a `perror()` message) on any failed `open`, `fstat`, `mmap`, `msync`, or `munmap` call — it doesn't silently continue on a partial overwrite.

---

## Known limitations

- **No multiple-pass overwrite.** A single zero-fill pass is enough to defeat casual undelete tools, but not a match for forensic recovery on some storage media — this was written as an exercise in `mmap()`, not as a production-grade secure-erase tool (see `shred(1)` for that).
- **SSD/flash caveat.** Wear-leveling on SSDs means the physical block actually holding the old data may not be the one the OS thinks it's overwriting — no userspace tool can fully guarantee erasure on flash without TRIM/secure-erase support from the drive itself.
- **No directory or symlink handling** — it operates on a single regular file path.

---

## License

MIT — see [LICENSE](LICENSE).
