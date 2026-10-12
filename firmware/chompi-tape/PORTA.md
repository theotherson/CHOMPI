# PORTA

A CHOMPI firmware built on TAPE 2.0 with two sides: making samples and building loops.
It borrows from Chase Bliss Blooper (layered loops you can manipulate and print) and the OP-1
(projects, a tape you build up). This is the design spec; it changes as decisions are made.

**Lineage:** Casio SK-1 and VSS-30, Boss SP-303: short samples, resampling as the way to build things up.

**Status:** groundwork on the `porta` branch. A first draft of the features (page switch, sampler-page keys,
four-layer looper) is on `porta-draft`. Neither has been tested on hardware yet.

Items marked **(open)** are undecided.

---

## The model (confirmed 2026-10-12)

```
 SAMPLER page                 4TRACK page (was "loop page")
 capture sounds        ──▶    PLAY them: C# chromatic, D# kits, F# loops, mid D# sequencer/arp, A# length, G# tracks/projects
  · a sample (≤ 10 s)         RECORD what's played onto a track = resampling, how loops are made
  · a kit (15 hits,           BOUNCE tracks together
    ≤ 1.5 s each)             SAVE a track as a loop → loop bank
 save to sample/kit bank      PROJECTS of 4 tracks
```

- A sample is one sound; a loop is samples played (freely or sequenced) and recorded onto a track, or a bounce.
- Chromatic and kit playing live on the 4TRACK page only; the sampler page captures **and edits**: load a
  sample (or record one), trim it, add FX (warble, drive…), save the result to a new slot.
- Sampler loop key (**built**, a one-layer looper on the 10 s sample buffer): empty: hold to record while
  held, tap to arm (knob 1's threshold; with it off, records latched); recording: tap to stop and keep
  playing; with a sample: tap to overdub (latched) or hold to overdub while held; armed: tap to cancel. Play:
  play / pause the sample; play + loop held 2 s: delete. No replace: delete, then record again. Keys play the
  sample at its own pitch. **Mid C# (FX pre / post) on the sampler page:** **pre** (yellow): the FX are on the **input** (mic / aux),
heard and recorded so, while the loop, keys and 4TRACK play dry. **Post** (teal): the FX are on the loop, live
(its input is recorded one block, 1 ms, late to allow it), and the input records dry. One FX chain: one or the
other.
**Overdubbing with the input on resample replaces** what's on the loop: it re-prints the loop as heard
(FX, window, speed); mic / aux overdubs layer on top.
- Kit capture: open the kit picker and arm; each threshold-crossing hit becomes a snapshot in the next slot
  (1…15) until loop is pressed; the filled slots blink, then save or delete the kit.
- Toggle **up** is the sampler page (TAPE's record position), **down** the 4TRACK page.

### Length limits (decided 2026-10-12)

| | Cap | Why |
|---|---|---|
| Samples | ~10 s | What makes RAM-loaded banks possible (a bank is 15 × the sample length); frees ~8.5 MB of the 55 s buffer; finer start/end trimming |
| Kit slots | ~1.5 s | Hits are short anyway; keeps kits small |
| Loops | a track's length (55 s) | A cap saves nothing: tracks reserve 55 s whatever they hold, loop files live on the SD card. Trim with knobs 2/3 |

## 4TRACK page v2: a loop player and four tracks you print onto (decided 2026-10-12)

Replaces the draft where layers recorded directly. **Not built yet**; the current build still has the
four-layer looper.

```
 LOOP PLAYER (the source)                      TRACKS 1–4
 · a recorded performance, or a loop           · a track "listens" (prints) in real time while you
   loaded from F#                                hold CHOMPI + tap the current track (it flashes
 · trimmed by its loop window, with              red), recording whatever is playing: the loop with
   speed, FX, knobs 1–4                          its FX, speed and window, plus live keys
 · never changed by printing                   · printing again adds on top (additive)
                                               · length: baked from the length view, or else as
                                                 long as it listened
                                               · can be saved as a new loop
```

- **Two windows:** a track's length, and the loop window inside it. The loop repeats within the track's
  length, cut off at the end if it doesn't fit. Loading a new loop never changes a track's length.
- **Length view (low A#):** each press steps through pages: number of bars, tempo, time signature. Hold length
  to show the tracks, then hold track(s) to bake the length into them (purple or blue while picking). Tracks can
  have different lengths, each looping on its own, lined up to the bar grid; baking several at once gives them
  the same length. Then a new length can be set and baked into other tracks.
- **Printing FX:** a one-shot bake (the FX print once, then the knobs reset, MIXTAPE-style) by default; a
  **toggle** prints FX continuously while overdubbing (Blooper-style, piles up; revisit if bakes sound tame).
  Printing with changed speed gives tape-style pitch feedback loops.
- **FX sends:** each track gets a send level into the one shared FX chain (separate FX per track won't fit);
  while a track prints its FX, the other tracks' sends mute.
- **Track picker, CHOMPI + tap:** the current track: print while CHOMPI is held; another track: select it.
  Auditioning is CHOMPI + hold (until CHOMPI is released), so the two don't clash.
- **Track LEDs (built):** empty dim white, with content pink, selected green, muted blinks dimly; being recorded
  into: red for the first take, flashing yellow for an overdub (as the loop key).
- **Arming a track with content** records another overdub on it (as now).
- **Deleting tracks (built):** in the track picker press erase, tap the tracks (they turn magenta, CHOMPI
  flashes magenta), then CHOMPI deletes them (CHOMPI flashes red). Deleting the last one empties the looper.
  TAPE's sample erase uses the same colours. Copy key: pearl white. Track menu key (low G#): green.
- **Save (planned, with projects):** opens the project slot menu: up to 15 slots of 4 tracks. Today it's
  still TAPE's: save the sample buffer into a slot of the current bank.

## The two pages

The **toggle switch** chooses the page:

| Toggle | Page | What it's for |
|---|---|---|
| one side | **Sampler page** | Record an input into the buffer, manipulate it, save it to a sample slot. Play the sample banks, with the sequencer/arp. |
| other side | **Loop page** | Build a project's loop from up to 4 layers, manipulate layers or the whole loop, print and save. |

Toggle up (TAPE's record position, input monitored) is the sampler page; down is the loop (4TRACK) page.

Recording: in TAPE, the CHOMPI key records in the record position and opens the black-key menu in the
play position. PORTA needs that menu on both pages, so CHOMPI is shift only and **the loop key records**:
a sample on the sampler page, a layer on the loop page. **Play** is the loop transport on both pages.
**(draft choice, open: confirm)**

### The shared buffer

Both pages use one buffer for manipulation:

```
 input (mic / aux / resample) ─┐               ┌─> save to a sample slot
                               ├──> BUFFER ────┤
 layer 1-4 of the project ─────┘  (manipulate) └─> print back to the layer
```

The manipulation tools are built once and work the same on both pages.

---

## Keys

On CHOMPI the black keys play notes. Their function roles below apply while the **CHOMPI key is held**,
as with TAPE's shift menu. White keys are keys 1–15.

### Loop page

The black keys open **pickers** on the white keys that stay open (the menu latched) until something is
picked, which goes straight back to the normal keyboard (chromatic notes across all keys). While a picker
is open, the knobs, play and loop work as normal unless CHOMPI is held.

| Key | Function |
|---|---|
| low C# | **Sample picker:** chromatic banks, again to scroll banks (as TAPE). Tap a slot to pick it and close; **CHOMPI + a slot auditions it** and keeps the picker open |
| low D# | **15-voice kits** (like TEMPO's second bank) **(planned; open: where the kit sounds come from)**. Today it's still TAPE's CUBBI banks, which move to F# |
| low F# | **Loop picker (planned):** TAPE's D# (CUBBI) banks, which are loops, move here. Tap F# to scroll banks: two for now, one full (the existing loops), one empty for new ones. Hold a loop, the layer picker appears, tap a layer to load it there **(open: where new loops come from: sampler recordings, tracks, or both)** |
| low G# | Tap: **layer picker** (see Layer view). Hold: **project picker (planned)** |
| low A# | **Length view (clock built, baking planned):** CHOMPI + A# opens it and it stays open (A# purple); A# again closes it. **White keys 1–14: key N = N bars** (lit up to the bars set; not used by recording yet). **Wheel: tempo** (40–240 BPM), its LEDs pulsing on the beat, brighter on the one, blue (slow) → green → red (fast). **Wheel click: tap tempo.** **CHOMPI + wheel: time signature**, shown on keys 1–5 while CHOMPI is held (4/4, 3/4, 6/8, 5/4, 7/4). Top key: metronome on/off (flashes on the beat). Later: hold + hold tracks to bake (see 4TRACK v2) |
| mid C# | FX pre/post toggle |
| mid D# | **Sequencer / arpeggiator (planned)** |
| upper F# / G# / A# | Erase / copy / save, as in TAPE |

### Sampler page

The **sample buffer** is one memory: the sampler page's loop path (play / loop keys) and the top white key
(TAPE's slot 15; every key on the sampler page) both play it. It **starts empty** and is silent until a slot is
held to load it or the input is recorded; a loaded or baked sample starts looping at once, as a recording does.

The menu is **always shown** on the sampler page, no CHOMPI needed: black keys are always their functions,
white keys are the bank's slots and the top one the buffer (see the table); save / copy / erase pick slots
with them as in TAPE. CHOMPI held
still shows the full menu. The loop key's LED: off while the buffer's empty; blinking red armed, red first take,
yellow overdub, dim white holding a sample.

| Key | Function |
|---|---|
| white keys 1–14 | **the current bank's slots:** tap to audition, **CHOMPI + tap to load** into the sample buffer (cut at 10 s; it starts looping); loading resets level, tuning, start/end and attack/release to their defaults (decided) |
| top white key | **the buffer:** plays from its top while held; pink when it holds something |
| low C# / D# | chromatic / loop banks: the other key switches mode, the same key steps banks (TAPE's way). Kit capture comes here later |
| low F# | **Bake (built):** plays the sample once from start to end (knobs 2/3), at the wheel's speed, alone through the FX, and replaces it with the result; then the FX knobs (and CHOMPI's resonance, warble, delay time) go neutral and the trim to the whole sample. Magenta with a sample, flashing while baking. Save stays separate: bake, listen, save |
| low G# | Free |
| low A# | Input type: cycles mic → aux → resample. Key LED colour shows which |
| mid D# | **Latch (built):** a key played while it's on keeps sounding after it's let go, to edit it (pitch, start, end, filter) while it plays; a new key replaces it, latch off stops it. White when on |
| mid C#, upper F#/G#/A# | As on the loop page **(open)** |

### Sequencer / arpeggiator (middle D#, 4TRACK page, planned)

From TEMPO's sequencer. "Shift" is the CHOMPI key. On the 4TRACK page the black keys show their menu
functions only with CHOMPI held, so:

| Gesture | Does |
|---|---|
| CHOMPI + tap middle D# | latch: holds the notes when nothing plays, the arp when it does |
| CHOMPI + hold middle D# | patterns on the white keys (below); picking one turns it on, key 1 turns it off |

**(open)** Exact on/off gesture, and where choosing sequence vs arpeggio goes now that middle D# is the
sequencer itself.

Pattern keys (white keys while the patterns show):

| Key | 1 | 2 | 3 | 4 | 5 | 6 | 7–11 |
|---|---|---|---|---|---|---|---|
| | off | sequence (order pressed) | up | down | ping-pong | random | TEMPO's rest patterns |

### Knobs

Each knob's click steps its pages; CHOMPI + a knob is its second function (the "shift" layer).

| Knob | Page | Knob alone | CHOMPI + knob |
|---|---|---|---|
| 1 | 1 | Level (TAPE's gain) | Overdub level (red keeps nothing, green keeps it all) |
| 1 | 2 | Recording threshold (off fully left; LED white by level, red while the input is above it) | Input level |
| 1 | 3 | **Tuning:** an octave down to an octave up around the sample's pitch (in the middle), forwards only; reverse and stop are the wheel's. LED blue down, green at pitch, red up | Stepped tuning: −octave, −fifth, pitch, +fifth, +octave |
| 1 | 4 | **LFO (planned, after the clock)** | Opens a white-key menu: shape off / triangle / square / random, targets pitch / filter / level |
| 2 / 3 | 1 | **Start / end:** the sample loop's on the sampler page (live, down to 10 ms, finer as they close in; bake uses them), the recorded loop's (all layers) on the 4TRACK page | Amp attack / release (saved per slot, as TAPE) |
| 2 / 3 | 2 | **Filter cutoff / resonance** (TAPE's DJ filter, moved from knob 4: low-pass left, high-pass right, open in the middle) | **Filter envelope attack / release** (1 ms to 4 s; both at zero = off) |
| 4 | 1 / 2 | FX: reverb / delay, lofi (TAPE; the filter page moved to knobs 2/3) | Delay time, warble |
| Wheel | | The sample loop's speed while it plays, scrub while stopped (sampler page); TAPE's looper (4TRACK) | **Always stepped** (octaves and fifths, through reverse), on both pages |
| Volume | | TAPE | TAPE |

- **Every key press retriggers the filter envelope**, which sweeps the filter from knob 2's setting towards
  open and back. With the cutoff in the middle (open) it has nothing to sweep.
- CHOMPI + click knob 1 resets the page shown. CHOMPI + click knob 2 / 3 (auto-loop / sustain) work on the
  4TRACK page only: on the sampler page keys audition and load rather than play notes.
- The threshold is shared: the 4TRACK page's armed recording uses the same value.
- Pan was dropped.

### Recording

| Gesture | Sampler page | Loop page |
|---|---|---|
| Loop | See the sampler loop key above | **Stopped:** arm the selected layer; the next note or loop press starts the take (loop key flashes; an open track picker closes so the note can be played). **Playing:** overdub, as TAPE |
| CHOMPI + loop | **Arm:** recording starts when the input crosses the threshold | **Arm:** the first take or an overdub starts at the threshold |
| Loop, or CHOMPI + loop, while armed/recording | Stop (or cancel; the old sample stays) | Cancel the arm |

While armed the loop key blinks red. A sample recording keeps a ~6 ms pre-roll so the attack isn't lost
(from MIXTAPE); the looper doesn't yet. The first ~100 ms after arming is ignored (key noise).



---

## Projects and layers

**Planned (2026-10-12):**
- Hold G# on the loop page: up to 15 projects on the white keys; projects with content glow dim pink. Loading
  one shows a loading animation (only the current project is in RAM).
- Switching away from a project with unsaved changes: the white keys flash showing full and empty project
  slots; tap one to save into, or the flashing erase key to discard.
- On the card: `/PORTA/PROJECTS/P01…P15/` (track1–4.wav + settings), `/PORTA/LOOPS/B1, B2/`.
- Loading a loop onto a track: into an empty project the loop sets the length; otherwise it's fitted (a
  shorter loop repeats to fill, a longer one is cut at the end). Knobs 2/3 trim the result.

- A **project** is a loop of up to **4 layers**. There are 15 project slots, one per white key.
- Each layer is up to **55 seconds**.
- Only the current project is in RAM. Switching projects loads its layers from the SD card: a few
  seconds for long layers, with a loading animation.
- **Layer lengths (open):** the suggestion is that each layer is a whole fraction or multiple of the
  project loop (½, 1, 2, 4…) so layers stay in time. Free lengths would drift and only suit generative
  textures, so that would be an explicit option.

### Manipulating and printing a layer

1. Select a layer. It goes into the buffer; the original stays untouched in its layer slot, so undo is instant.
2. Manipulate it in the buffer.
3. Print: the original is saved to the card (for example `/PORTA/<project>/layer2_orig.wav`),
   then the buffer is written over the layer.
4. "Remove manipulations" later reloads the original from the card (a second or two).

### Slicing (later stage)

Slice a layer across the white keys and play the slices in any order.
- There are 15 white keys, not 16. Suggestion: slice on the beat grid, so every key lands on a beat.
- Trigger slices quantised to the beat, so out-of-order playing stays tight.
- Record the slice performance back into a layer, which is resampling into the buffer.

Get recording, layers and printing working first, then slicing.

---

### Track view (built 2026-10-12, replaces the layer view)

The 4TRACK page is now a **loop buffer** plus **four tracks**:
- **Loop buffer:** the loop and play keys, as TAPE's looper (55 s). The loop key arms it when stopped (the next
  note or loop press starts the take), CHOMPI + loop arms it on the threshold. Knobs 2/3 set its window, the
  wheel its speed. Saving / loading the looper (slot 16) is the loop buffer.
- **Tracks:** print what's heard after the FX (the loop buffer, live keys and input; not the other tracks).
  The first print sets the tracks' length (one shared length for now; per-track lengths come with the length
  view's baking). Printing again adds on top. The tracks play while the loop buffer plays, and stop with it;
  with the loop buffer empty, the play key runs them.

CHOMPI + G# opens the track view, and it **stays open** (G# again closes it):
- white keys 1–4 are the tracks, 5–15 a **playhead** through the tracks' length, left to right, in the
  selected track's colour;
- knobs, their clicks, play and loop work as normal unless CHOMPI is held.

| Gesture | Does |
|---|---|
| hold a track ~0.5 s | **arm it, "listening":** it prints what plays (flashing red) until it's tapped again, or the loop buffer stops |
| tap a track | turn it on / off (mute); a printing track: stop printing |
| CHOMPI + tap a track | **select it** (even if it already is), ending any audition |
| CHOMPI + hold a track 2 s | audition it: only that track plays (it pulses) **until CHOMPI is released**. Stopped, it plays once from the top, then ends |
| hold copy (upper G#), tap the sources, tap the destination last, let go of copy | **bounce** the sources into the destination, **replacing** what's on it; CHOMPI held when letting go **merges** |
| erase, tap tracks, CHOMPI | delete those tracks |

LEDs: tracks with content in their own colour (1 pink, 2 blue, 3 orange, 4 purple), selected = green,
empty = dim white, being cleared = dim red, muted = blinking dimly, printing = flashing red, bounce picks =
blinking white, being bounced into = flashing white, picked for deleting = magenta. Mutes aren't saved.

Open: a "print one pass of the loop buffer" gesture (the track's length = the loop's), suggested as an
alternative to listening.

### How the four tracks work (draft)

- The first print, on the selected track, sets the length; that track is the master. The others take its
  length and position and play in step with it, each from its own buffer.
- After a delete, a track is cleared in the background (well under a second); being cleared it's silent and
  can't be printed into. Deleting the last track empties them, so the next print sets a new length.
- **Bounce** (Portastudio style): the sources are emptied, ready to print again; the destination comes out
  unmuted. It takes about a third of a second, and the destination flashes white meanwhile.
- Memory: 4 tracks + the loop buffer, 55 s stereo each (about 57 of 64 MB SDRAM in use).
- Not done yet: projects (saving and loading the four tracks).

---

## Sequencer and arp

From TEMPO (`ArpeggiatorSequencer.h`, about 4.7 KB of code), one component that does both:
- **Sequence:** notes play in the order pressed, one per clock step, snapped to the grid.
- **Arp:** up, down, ping-pong, random.
- **Rest patterns** and **latch**, with notes added and removed while it plays.
- Runs separately for the chromatic and one-shot banks, so both can sequence at once.

**(open)** Is press-order sequencing enough, or is a step grid (placing notes at positions) wanted too?

---

## Clock

PORTA uses a **single clock** that the loops, sequencer and slices all follow:

- **MIXTAPE's `Clock.h` as the source:** a sample-accurate beat and bar grid, metronome, time signatures
  (including 6/8), MIDI clock in with Start and Song Position.
- **Ported from TEMPO's `clockManager.h`:** tap tempo, MIDI clock out, and per-bank clock divisions.
- The sequencer takes its steps from this clock, not from TEMPO's separate timer.

**Built (first clock build):** MIXTAPE's clock, with its internal clock always running (40–240 BPM) unless
MIDI clock comes in (USB or TRS; Start or Song Position starts its grid). Tap tempo averages the last three
taps and puts the beat on the tap. MIDI clock out (24 ppq, USB and TRS) sends the internal clock only, not
one it follows; no Start/Stop out yet (they'll follow the 4TRACK transport). The metronome beeps on every
beat (higher on the one; 6/8 clicks its eighths), after the looper so it's never recorded. Recording doesn't
use the clock yet. Clock out is sent from the main loop, so SD copying can delay ticks.

---

## Memory

### SDRAM (64 MB)

| Use | Size |
|---|---|
| 4 layers × 55 s (48 kHz stereo, 16-bit) | 42.2 MB |
| Shared buffer, 55 s | 10.6 MB |
| Delay line | 0.4 MB |
| Sample banks loaded into RAM (TEMPO-style) | ~7.5 MB |
| **Total** | **~60.7 MB** |

**Sample banks in RAM** (planned) instead of streamed from the card as in TAPE:
- reliable, fast triggering for the sequencer;
- frees most of TAPE's 121 KB of SD streaming buffers for code.
- Costs: a 1–2 s load on bank change, and a cap of about 40 s of stereo across the 30 samples.
  **(open: is that cap acceptable?)** Streaming oversize samples could come later.

Sampler-page recordings go through the 55 s buffer, so they're capped at 55 s.

### Code and data (512 KB AXI SRAM)

As of 2026-10-12: code 239 of 320 KB, data 115 of 192 KB (about 80 KB free in each). The voices' SD
read-ahead (7 × 16 KB) lives in SDRAM; the sample buffer is 10 s. SDRAM: ~44.7 of 64 MB.

---

## Code to reuse

| From | What |
|---|---|
| TAPE 2.0 | Recording the input, saving WAVs to sample slots, FX chain (reverb, delay, filter, warble) |
| MIXTAPE | Looper with append, undo, bake FX and threshold recording; `RamBuffer` (play while loading); `Clock.h`; RAM slice playback from the OP-1 kit feature |
| TEMPO | `ArpeggiatorSequencer.h`; tap tempo and MIDI clock out from `clockManager.h`; RAM-loaded banks; possibly `SliceEngine` for slicing |

Things to fix while porting: TEMPO's sequencer stores notes in growable `std::vector`s (heap); fixed-size
arrays would be safer.

---

## Later ideas

### Kits as the slice machine (2026-10-12)

- **Low D# (kits)** is also the slice machine. Two ways to make a kit, as separate functions on the same
  button **(open: which gesture is which)**:
  1. threshold capture: each hit becomes the next slot (as above);
  2. record up to ~15 s, then slice it evenly onto the 15 white keys.
- The sequencer / arpeggiator (middle D#) plays the kit, or the chromatic sample, as a sequence or an arpeggio.
- Slicing wants up to ~15 s of
  source, more than the sampler's 10 s; a kit buffer of its own is fine on memory (~2.9 MB).

## Groundwork (done)

On the `porta` branch, on top of TAPE 2.0 (`tape-2.0` branch):

1. Own `/PORTA` folder on the card (falls back to the root), `boot_info` in backup SRAM.
2. libDaisy MIDI event queues 256 → 64 (frees 55 KB of data).
3. libDaisy built with `-Os` (frees 10.5 KB of code).
4. Heap: 8 KB in DTCM, bounds-checked.
5. Code/data split 280 KB / 232 KB (TAPE: 235.25 / 276.75; TEMPO already uses 282 / 230).

Result: code 229,992 bytes of 280 KB, data 227,324 of 232 KB.

**Hardware test pending:** boot with USB connected, play and record samples, MIDI in from the Mac
and hardware, clean audio.
