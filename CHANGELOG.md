# Changelog

Changes against upstream Haeleth's **PONScripter fullsrc 20111009**
(<https://sekaitranslates.files.wordpress.com/2011/11/ponscripter-20111009-fullsrc1.tar.bz2>).

| | |
|---|---|
| Upstream | PONScripter fullsrc 20111009 (tarball; the tree carries `.hg_archival.txt`, node `3e17f878`) |
| Fork | https://gitlab.com/matchaxnb/ponscripter (git remote `origin`) |
| Branch | `master` |

There is no upstream git remote: the base is a source tarball, so the fork has
its own root commit (an orphan history) and `git merge-base` does not apply.
It is consumed by the [vnovel](https://gitlab.com/matchaxnb/vnovel) libretro
core as one of its engines.


## User font selection (2026-10-26)

- The core's font options reach the engine through the bridge: a face the user
  chose is opened in place of the game's own (falling back to it, logged, when
  the face is unreadable), the size the script asks for is scaled, letter and
  word spacing ride through the same per-character fold the layout and the
  total-fit planner share, and the tint is applied where a glyph is blended.
  At the defaults nothing changes.
- The scale is re-derived from the script's own geometry rather than multiplied
  in place, so it is idempotent: a changed size can be re-applied at the next
  sentence boundary, and turned back down again.

## libretro integration

- `__LIBRETRO__` guard with `libretro_script_error_longjmp()` to catch `exit()`
  via setjmp/longjmp instead of terminating the process.
- `executeLabel()` made public, plus `eventLoop`/`getWidth`/`getHeight`/
  `isAutoMode`/`isSkipMode` getters for the C ABI wrapper. The core must call
  `eventLoop()`, not `executeLabel()`: PONScripter's `executeLabel` returns on
  `RET_WAIT`, so calling it directly leaves the engine thread with no
  rendering.
- `snapshotSave` / `dumpState` commands for the automated tests.
- Right-click skip during a `!w` sleep wait (skip to the next clickwait).
- Mangled SJIS comments replaced with `\xNN` escapes; comments in UTF-8.

## Crash fixes

- **SIGSEGV on skip**: `shiftCursorOnButton` held a `std::map::iterator` into
  `buttons`; `enter`/`leaveSystemCall` swap/clear that map and invalidate it,
  so the next UP/DOWN press decremented a freed node. Replaced with an `int`
  button key re-found each step (as reference ONScripter-EN does), reset to 0
  across system-call boundaries.
- **SIGABRT on reset**: `DirPaths::get_all_paths()` computed `paths.size()-1`
  on an unsigned size; an empty deque underflowed to `UINT_MAX` and
  `paths.at(0)` threw, terminating the process. The empty case is guarded.

## Audio

- `mp3callback` posted `ONS_SOUND_EVENT` whenever `SMPEG_playAudio()` returned
  0, which the engine reads as "track finished" (stop + restart). The bundled
  SMPEG legitimately returns 0 for the first ~2 buffers while a stream primes,
  with `SMPEG_status()` still `SMPEG_PLAYING` — so BGM was restarted before it
  produced audio (silent BGM, runaway stop/play loop at ~200 % CPU, eventually
  a SIGSEGV from deleting a live sample). Now a 0 counts as end-of-track only
  once `SMPEG_status() != SMPEG_PLAYING`.

## Text window and portability

- **The plane is published separately from the frame.** The sentence text is
  published through a text-plane callback, into a surface of its own rather than
  only inside the picture, so text can be restyled, reflowed or shown a page at
  a time independently of the artwork. The engine keeps its own copy, so a run
  with no host is unaffected.
- The text-window backdrop honours the core's opacity knob (100 = unchanged,
  0 = invisible, 200 = twice as opaque), for both colour backdrops (scaled in
  `shadowTextDisplay`) and image backdrops (multiplier baked into the stored
  alpha in `setupAnimationInfo`, since blending caps its multiplier at 256).
- Two sites reading `SDL_SysWMinfo.window` — SDL 2's struct layout used inside
  SDL 1.2 window code, which never compiled — are guarded with
  `!defined(__LIBRETRO__)`. Found building the core for mingw; no behaviour
  change for existing targets.

## Diagnostics

- Debug mode logs the full script line including its arguments.
- The AVI decoder's stream dumps are gated on the core's debug mask (the
  `pons:avi` bit) instead of an argument passed `false` at its only call site,
  which made them unreachable. A standalone build reads the bit as off.

## License

GPLv2, inherited from upstream — see `COPYING`.
