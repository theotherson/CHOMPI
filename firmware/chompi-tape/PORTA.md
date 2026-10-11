# PORTA

A CHOMPI firmware built on TAPE 2.0 with two sides: making samples and building loops.
It borrows from Chase Bliss Blooper (layered loops you can manipulate and print) and the OP-1
(projects, a tape you build up). This is the design spec; it changes as decisions are made.

**Status:** groundwork done on the `porta` branch, not yet tested on hardware. No PORTA features yet.

Items marked **(open)** are undecided.

---

## The two pages

The **toggle switch** chooses the page:

| Toggle | Page | What it's for |
|---|---|---|
| one side | **Sampler page** | Record an input into the buffer, manipulate it, save it to a sample slot. Play the sample banks, with the sequencer/arp. |
| other side | **Loop page** | Build a project's loop from up to 4 layers, manipulate layers or the whole loop, print and save. |

This replaces the toggle's TAPE role (play vs record). Recording moves to buttons:
holding CHOMPI on the sampler page and the loop key on the loop page **(open: confirm)**.

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

| Key | Function |
|---|---|
| low C# | Current project (**open:** does it show the project's layers 1–4 on the white keys?) |
| low D# | Project slots 1–15 on the white keys |
| low F# | Slice mode for the active layer(s). Hold: slice the loop across the white keys (see Slicing) |
| low G# | Sequencer/arp on, pattern options, latch (shift+G#). **(open: exact gestures, see below)** |
| low A# | **(open)** |
| mid C# | FX pre/post toggle **(open: same on both pages?)** |
| mid D# | Loop length, all layers or each one **(open: same on both pages?)** |
| upper F# / G# / A# | Erase / copy / save, as in TAPE **(open: same on both pages?)** |

### Sampler page

| Key | Function |
|---|---|
| low C# | Chromatic banks: 15 samples, each played chromatically (TAPE's JAMMI) |
| low D# | 15 one-shots at fixed pitch (TAPE's CUBBI) |
| low F# | **(open)** |
| low G# | Sequencer/arp. Latch (shift+G#) holds the notes, or the arp if it's playing **(open: exact gestures)** |
| low A# | Input type: cycles mic → aux → resample. Key LED colour shows which |
| mid C#, mid D#, upper F#/G#/A# | As on the loop page **(open)** |

### Gestures (open)

Latch is **shift+G#** (Sam's choice). "Shift" is the CHOMPI key (MIXTAPE's docs call it SHIFT), and holding it
is also what makes G# a function key, so shift+G# can't be both "arp on" and "latch". Options:
1. Shift+hold G#: pick a pattern, which turns the arp on ("off" in the list turns it off). Shift+tap G#: latch.
2. Shift+tap G#: arp on/off. Shift+double-tap G#: latch. Shift+hold G#: patterns.

### Encoders, wheel, play/loop keys (open)

What all six encoders, the wheel and the play/loop keys do on each page. The start/end knobs set the
project's loop length.

---

## Projects and layers

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

After the groundwork (below): code about 56 KB free, data about 10 KB free.

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
