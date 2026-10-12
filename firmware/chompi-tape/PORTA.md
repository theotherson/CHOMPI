# PORTA

A CHOMPI firmware built on TAPE 2.0 with two sides: making samples and building loops.
It borrows from Chase Bliss Blooper (layered loops you can manipulate and print) and the OP-1
(projects, a tape you build up). This is the design spec; it changes as decisions are made.

**Status:** groundwork on the `porta` branch. A first draft of the features (page switch, sampler-page keys,
four-layer looper) is on `porta-draft`. Neither has been tested on hardware yet.

Items marked **(open)** are undecided.

---

## The two pages

The **toggle switch** chooses the page:

| Toggle | Page | What it's for |
|---|---|---|
| one side | **Sampler page** | Record an input into the buffer, manipulate it, save it to a sample slot. Play the sample banks, with the sequencer/arp. |
| other side | **Loop page** | Build a project's loop from up to 4 layers, manipulate layers or the whole loop, print and save. |

Toggle down (TAPE's record position, input monitored) is the sampler page; up is the loop page.

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
| low C# / D# | **Sample picker:** chromatic / one-shot banks, again to scroll banks (as TAPE). Tap a slot to pick it and close (one-shots: any key); **CHOMPI + a slot auditions it** and keeps the picker open |
| low F# | Slice machine (later) |
| low G# | **Layer picker** (see Layer view) |
| low A# | Nothing: no input select on the loop page |
| mid C# | FX pre/post toggle |
| mid D#, others | Later: pattern length, quantisation… so beats can be locked in |
| upper F# / G# / A# | Erase / copy / save, as in TAPE |

### Sampler page

| Key | Function |
|---|---|
| low C# | Chromatic banks: 15 samples, each played chromatically (TAPE's JAMMI) |
| low D# | 15 one-shots at fixed pitch (TAPE's CUBBI) |
| low F# | **(open)** |
| low G# | Sequencer/arp. One-shots: tap toggles it on/off. CHOMPI + hold: patterns. CHOMPI + tap: latch (see Arp gestures) |
| low A# | Input type: cycles mic → aux → resample. Key LED colour shows which |
| mid C#, mid D#, upper F#/G#/A# | As on the loop page **(open)** |

### Arp gestures on G#

"Shift" is the CHOMPI key (MIXTAPE's docs call it SHIFT). In chromatic mode the black keys are notes,
so there the arp needs CHOMPI held.

| | Loop page, and one-shots on the sampler page | Sampler page, chromatic |
|---|---|---|
| Sequencer on/off | tap G# (sequences what's on the white keys: the layer, or the one-shots) | pick a pattern (key 1 = off) |
| Patterns on the white keys, key 1 = off | CHOMPI + hold G# | CHOMPI + hold G# |
| Latch | CHOMPI + tap G# | CHOMPI + tap G# |

- Patterns and latch use the same gestures everywhere. The plain G# tap works wherever G# isn't a note:
  the loop page and one-shot mode.
- Latch: notes when nothing plays, the arp when it does.
- Pattern keys (white keys while the patterns show):

  | Key | 1 | 2 | 3 | 4 | 5 | 6 | 7–11 |
  |---|---|---|---|---|---|---|---|
  | | off | sequence (order pressed) | up | down | ping-pong | random | TEMPO's rest patterns |

### Knobs

Both pages share TAPE 2.0's knobs, except:

| Knob | Sampler page | Loop page |
|---|---|---|
| 1 | Recording threshold (off fully left; LED white by level, red while the input is above it) | Pages: sample pitch, gain (TAPE), **overdub level** (red keeps nothing, green keeps it all) |
| 2 / 3 | Sample start / end (TAPE) | **Recorded loop's start / end**, all layers together |
| 4 | FX (TAPE) | FX (TAPE) |
| Wheel, volume | TAPE | TAPE |

The threshold is shared: the loop page's armed recording uses the value set on the sampler page.

### Recording

| Gesture | Sampler page | Loop page |
|---|---|---|
| Loop | Record a sample now (held, or latched with TAPE's latch option) | Looper record, as TAPE |
| CHOMPI + loop | **Arm:** recording starts when the input crosses the threshold | **Arm:** the first take or an overdub starts at the threshold |
| Loop, or CHOMPI + loop, while armed/recording | Stop (or cancel; the old sample stays) | Cancel the arm |

While armed the loop key blinks red. A sample recording keeps a ~6 ms pre-roll so the attack isn't lost
(from MIXTAPE); the looper doesn't yet. The first ~100 ms after arming is ignored (key noise).



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

### Layer view

CHOMPI + G# on the loop page opens it, and it **stays open** (the CHOMPI menu latched):
- black keys keep their menu functions; G# again closes it (so does leaving the loop page);
- white keys 1–4 are the layers, 5–15 do nothing;
- knobs, their clicks, play and loop work as normal unless CHOMPI is held.

| Gesture | Does |
|---|---|
| tap a layer key | turn it on / off (mute); off is nearly dark |
| CHOMPI + tap a layer key | audition it: only that layer plays (it pulses), until tapped again or the picker closes |
| CHOMPI + hold a layer key 1 s | bounce the other unmuted layers into it |
| hold a layer key 2 s | it blinks red and recording into it is armed; on release the picker closes, the loop key blinks, and the next note played starts the take |

LEDs: green = the layer recorded into, white = recorded, dim = empty, dim red = being cleared, blinking red =
armed, flashing white = being bounced into. Mutes aren't saved and reset with the looper.

### How the four layers work (draft)

- The first recording, on the selected layer, sets the loop length; that layer is the master.
- The other layers take the master's length and position and play in step with it, each from its own buffer.
- The loop key overdubs into the selected layer only; the output is all four summed.
- Speed, direction and scrub move all layers together.
- After a reset, the old layers are cleared in the background (well under a second). A layer being cleared is silent
  and can't be recorded into.
- Each layer takes unlimited overdubs, merged into that layer. Four is the number of *separate* layers
  (memory: 4 × 55 s stereo).
- **Bounce** (Portastudio style): in the layer picker, CHOMPI + hold the destination layer 1 s. Every other
  recorded, *unmuted* layer is mixed into it and emptied, ready to record again; muted layers stay, so muting
  picks what's bounced. What plays doesn't change; it takes about a third of a second, and the key flashes
  meanwhile. This replaces adding more layers.
- **Later:** selective bounce: hold the destination key and tap the layers to merge into it (1+2 into 2,
  3+4 into 4, …); holding without tapping bounces everything, as now.
- Not done yet: saving and loading the looper still covers layer 1 only; no projects yet.

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
