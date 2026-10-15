# Guitar Loop Pedal — Project Documentation

A guitar looper pedal built around a **Daisy Seed** (STM32H750 + AK4556 codec). Records a master loop from your guitar, layers multiple overdubs on top, and plays it all back continuously while you keep playing live. A 0.96" SPI OLED shows status, an RGB LED gives at-a-glance state, and a momentary footswitch drives all control.

This document is the source of truth for what is built. It is updated whenever firmware functionality is added, removed, or changed.

---

## 1. Hardware overview

### Bill of materials

| Part | Notes |
|---|---|
| Electrosmith Daisy Seed (STM32H750IB) | Brain + AK4556 stereo codec |
| HiLetgo 0.96" SPI OLED, SSD1306, 128×64, white panel | 7-pin SPI variant |
| 10 kΩ linear potentiometer | Loop-volume knob |
| DaierTek SPST momentary footswitch (NO, 2-pin) | "Soft Touch Stomp Box" type |
| Common-cathode RGB LED + 3× 220 Ω resistors | Status indicator |
| Op-amp output buffer (existing analog board) | DC bias + AC-coupled output stage feeding guitar amp |
| Misc.: breadboard, jumpers, USB-C cable | |

### Pin assignments

All references below use Daisy Seed labels (D0–D30, A0–A11). For convenience the underlying STM32 pin (e.g. PC10) and the breakout pin number on the Seed PCB are also shown.

| Function | Daisy pin | STM32 pin | Seed PCB pin |
|---|---|---|---|
| OLED `GND` | DGND | – | 40 |
| OLED `VCC` | 3V3 | – | 38 |
| OLED `SCL` (SPI1 SCK) | D8 | PG11 | 9 |
| OLED `SDA` (SPI1 MOSI) | D10 | PB5 | 11 |
| OLED `RES` | D5 | PD2 | 6 |
| OLED `DC` | D6 | PC12 | 7 |
| OLED `CS` (SPI1 NSS) | D7 | PG10 | 8 |
| Looper footswitch (one terminal) | D1 | PC11 | 2 |
| Looper footswitch (other terminal) | DGND | – | 40 |
| Quantization footswitch (one terminal) | D0 | PB12 | 1 |
| Quantization footswitch (other terminal) | DGND | – | 40 |
| Octavize footswitch (one terminal) | D9 | PB4 | 10 |
| Octavize footswitch (other terminal) | DGND | – | 40 |
| Mute footswitch (one terminal) | D11 | PB8 | 12 |
| Mute footswitch (other terminal) | DGND | – | 40 |
| Potentiometer wiper | A0 (= D15) | PC0 | 15 |
| Potentiometer outer leg 1 | 3V3 | – | 38 |
| Potentiometer outer leg 2 | AGND | – | 20 |
| RGB LED common cathode | DGND | – | 40 |
| RGB LED Red (via 220 Ω) | D2 | PC10 | 3 |
| RGB LED Green (via 220 Ω) | D3 | PC9 | 4 |
| RGB LED Blue (via 220 Ω) | D4 | PC8 | 5 |
| Audio in (guitar) | AUDIO_IN_1 | – | 21 |
| Audio out (to amp buffer) | AUDIO_OUT_1 | – | 18 |

### Output buffer (analog board, not on the Seed)

Between `AUDIO_OUT_1` (pin 18) and the guitar amp input:

1. DC-blocking capacitor → into the **+** input of an op-amp follower
2. The follower output also feeds back to the **−** input (unity gain)
3. 100 Ω series resistor at the output of the follower
4. 10 µF AC-coupling capacitor
5. Voltage divider: 100 kΩ series + 10 kΩ shunt to ground (≈11× attenuation, brings line-level down to instrument-level)
6. Output to the amp's instrument-level input jack

A ~2.5 V DC bias on the **+** input of the op-amp keeps the signal swing centred.

---

## 2. Building and flashing the firmware

### One-time toolchain install (macOS)

```bash
brew install --cask gcc-arm-embedded
brew install dfu-util
```

The Arm GCC toolchain installs to `/Applications/ArmGNUToolchain/<ver>/arm-none-eabi/bin`. Add that to your `PATH` in `~/.zshrc` (already done in this project setup).

### Build

From the project root:

```bash
make -C libDaisy        # one time only
make -C DaisySP         # one time only
make                    # build firmware -> build/GuitarLoopPedal.bin
```

### Flash via USB DFU

1. Plug the Seed into the Mac with USB-C.
2. Hold **BOOT**, tap **RESET**, release **BOOT**.
3. Confirm with `dfu-util -l` — you should see `STM32 BOOTLOADER`.
4. `make program-dfu`.
5. Tap **RESET** to boot the new firmware.

The harmless `Error during download get_status` after a successful flash is normal.

---

## 3. Instruction manual — using the looper

### Powering on

Plug in the Seed and tap **RESET**. The OLED shows `READY` and the RGB LED is off. The guitar always passes through to the amp regardless of state.

### Recording the master loop

| Action | Result |
|---|---|
| Single tap from `READY` | Starts recording layer 1. RGB → **red**. OLED shows `REC` and a running second counter. |
| Single tap from `REC` | Closes the loop. Whatever you recorded becomes the master loop and starts playing back immediately. RGB → **green**. OLED shows `PLAY`, the loop length, the `Lay:N` overdub counter, a position bar, and an output-level meter. |

If you keep recording past 30 seconds, the buffer auto-stops and starts looping at the maximum length.

### Adding overdub layers

| Action | Result |
|---|---|
| Single tap from `PLAY` | Enters overdub. Whatever you play is added to the loop on top of the existing audio, with a small per-sample decay so old layers gracefully fade as more are stacked. RGB → **orange**. OLED shows `DUB`. |
| Single tap from `DUB` | Stops overdubbing and goes back to plain playback. The overdub is committed and the `Lay:N` counter on the OLED increments. RGB → **green**. |

You can repeat this as many times as you like.

### Undo (one-level)

| Action | Result |
|---|---|
| **Hold the footswitch ~1 s** while in `PLAY` or `DUB` | Restores the loop to the state it was in *just before* your most recent overdub started. The `Lay:N` counter decrements. The RGB LED **blinks red twice**, then returns to green. |

A hold with nothing to undo (e.g. immediately after recording the master loop) does nothing. The blink fires only on a successful undo.

The undo buffer is one snapshot deep — it always restores the state from before the **most recent** entry into Overdub mode, so undo can only roll back one layer at a time.

### Wipe (full reset)

| Action | Result |
|---|---|
| **Tap, then tap-and-hold ~1 s** (a "double-tap-hold") | Wipes the master loop, all overdubs, and the undo snapshot. Returns to `READY`. RGB LED **blinks red three times**, then turns off. |

The first tap of the pair will briefly advance state once before the wipe overrides it (e.g. `PLAY` → `DUB` → wipe). This is harmless.

The footswitch must be released and re-pressed within 400 ms for the second press to count as the "second tap of a pair."

You can also clear by tapping **RESET** on the Seed (or power-cycling) — that's a fresh boot and the loop buffers are zeroed at startup.

### Loop-volume knob

The potentiometer on **A0** continuously sets the playback volume of the loop, from 0% to 100%. The live guitar signal is always at full volume regardless of the knob.

When the quantization settings menu is open, the same knob is hijacked for editing the active parameter. As soon as you exit the menu, the knob returns to controlling loop volume (the loop volume is *not* re-read until you actually move the knob, so it doesn't jump).

### Quantization

A second foot switch wired to **D0** controls quantization.

| Action | Result |
|---|---|
| **Single tap** on the quantization switch (outside the menu) | Toggles quantization on / off. The OLED status line shows `Q:on` or `Q:--`. |
| **Double tap** on the quantization switch (outside the menu) | Opens the quantization settings menu. |
| **Single tap** inside the menu | Confirms the current parameter and advances to the next field. After the last field it exits. |
| **Double tap** inside the menu | Saves all current values and exits the menu immediately. |

The quantization switch's single-tap action is **deferred ~300 ms** so the firmware can disambiguate it from a double tap. The looper switch does *not* have this delay — its single-tap action is instant, as before.

#### Settings menu fields, in order

1. **Time Signature** — pot scrolls between `4/4`, `3/4`, `6/8`. Default `4/4`.
2. **BPM** — pot ranges from 40 to 240.
   - In `4/4` and `3/4`, BPM counts **quarter notes**. Default `100`.
   - In `6/8`, BPM counts **dotted quarters**, so each measure is two beats and the metronome clicks twice per bar. Default `75`.
   - When you change the time signature in field 1, the BPM is automatically reset to that signature's default. You can override it on the BPM screen.
3. **Metronome** — pot toggles between `OFF` and `ON`. Default `OFF`. *This setting only controls whether the metronome continues during master-loop recording; it does **not** keep the metronome running during playback or overdubbing. See "Where the metronome plays" below.*
4. **Metronome Volume** — pot ranges from 0 % to 100 %. Default `50`.

Each field uses **pot pickup**: the value does not start tracking the pot until you've moved the pot noticeably from where it sat when the field was opened. This lets the field land on its default and stay there until you intentionally turn the knob.

#### Where the metronome plays

| Phase | Metronome audible? |
|---|---|
| `READY` (idle) | No. |
| `CNT` (2-measure count-in before recording) | **Always**, regardless of the menu setting. The count-in is the tempo reference and cannot be silenced. |
| `REC` (recording the master loop) | Only if **Metronome = ON** in the menu. |
| `PLAY` (looping playback) | No. |
| `DUB` (overdub) | No. |

So toggling **Metronome = ON** in the menu effectively means "keep the click going through master-loop recording so I can stay in time", not "click forever". After recording, the metronome falls silent.

#### What quantization does to recording

When quantization is **on**:

- Tapping the looper switch from `READY` enters a **2-measure count-in** before recording starts. The OLED shows `CNT`, the current beat (`Beat N/M`), and the text `count-in 2 bars`. The RGB LED pulses red on every click. **The count-in clicks always play whenever quantization is on**, regardless of the `Metronome` setting in the menu — they exist to give you a reliable tempo reference before you play.
- After the count-in finishes, the looper transitions to `REC` and starts recording immediately. The very first recorded sample is exactly on beat 1.
- The `Metronome` setting in the quantization menu controls **what happens during master-loop recording**. If it is `ON`, the metronome keeps clicking through the `REC` phase as a continued tempo reference. If it is `OFF`, the metronome goes silent the moment recording begins. Either way, the metronome stops once recording is closed and is **never** audible during `PLAY` or `DUB`.
- When you tap to stop recording, the recorded length is **rounded to the nearest whole measure** based on the current time signature and BPM. So if you record slightly under or over two measures, the loop is locked exactly at two measures, and your foot timing doesn't have to be precise.
- Tapping the looper switch a second time *during* the count-in cancels it and returns to `READY`.

When quantization is **off**, the looper behaves exactly as before — recording starts immediately on the first tap, loop length is whatever you recorded, and no metronome plays.

### Octavize

A third foot switch on **D9** is a toggle. Tap once to turn octavize on, tap again to turn it off.

When octavize is on, the **live guitar input** is pitch-shifted **one octave down** before being heard, recorded into the master loop, and added to overdubs. The previously recorded loop is *not* shifted. This lets you play bass lines with your guitar over a normal-pitched loop.

While octavize is on, the **blue channel of the RGB LED** lights up alongside the normal state color (so PLAY + octavize = cyan, REC + octavize = magenta, DUB + octavize = white-ish, etc.). The bottom of the OLED shows `O+` instead of `O-`.

The pitch shift is granular and adds a small amount of latency / artifacts characteristic of real-time pitch-shifting. Best results on monophonic playing (single notes); polyphonic chords will sound "warbly".

### Mute

A fourth foot switch on **D11** is a momentary hold. While you press and hold it, the loop playback (and any overdub-monitoring of it) is silenced. The **live guitar still passes through** to the amp at full volume, so you can keep playing while the loop is muted.

Releasing the switch restores the loop instantly. The OLED shows a centred `MUTED` overlay while the switch is held. Use this for breakdown sections, surprise drops, or to silence the loop while you tune up without losing the loop.

### What the OLED shows in each state

| State | OLED content |
|---|---|
| `READY` | "press to record" |
| `CNT` | "Beat N / M" countdown and "count-in 2 bars" hint (only with quant on) |
| `REC` | elapsed seconds and a buffer-fill progress bar |
| `PLAY` | loop length, `Lay:N` overdub counter, current position bar |
| `DUB` | same fields as `PLAY`, with `DUB` as the big state label |
| Menu open | "QUANT MENU" + current field name + value + tap/dtap legend |

A small status line at the bottom of the main screen always shows
`Q:on/--  <time-sig>  <bpm>  M:on/--`
so you know quantization and metronome state at a glance.

### What the RGB LED tells you at a glance

| Color | Meaning |
|---|---|
| Off | `READY` (no loop yet) |
| Pulsing red on each click | `CNT` (counting in before record) |
| Solid red | `REC` (recording the master loop) |
| Solid blue (added on top of any of the above) | Octavize is on. Combined colors: PLAY + octavize = cyan; REC + octavize = magenta; DUB + octavize = ~white. |
| Solid green | `PLAY` (looping) |
| Solid orange (R+G) | `DUB` (overdubbing) |
| 2× red blink, then back to green | Undo just happened |
| 3× red blink, then off | Wipe just happened |

---

## 4. Feature list

### Audio path
- Guitar passthrough on `AUDIO_IN_1` → `AUDIO_OUT_1` always active, unity gain.
- 30-second mono master loop in SDRAM.
- Loop and live guitar are summed and soft-clipped at the output.
- 21 ms linear crossfade at the loop boundary to eliminate the wrap-around click.

### Looper control
- Single-tap state machine: READY → REC → PLAY → DUB → PLAY (toggle).
- Long-hold (~1 s) undo of the most recent overdub layer.
- Double-tap-hold (~1 s) wipes everything and returns to READY.
- Pot on A0 sets loop playback volume in real time, with smoothing.

### Overdubbing
- Additive overdub with 0.99× per-sample decay so many layers can stack without saturating.
- One-level undo: a snapshot of the loop is captured the moment overdub begins; undo restores it. The OLED layer counter is rolled back accordingly.

### State persistence
- All loop state lives in zero-initialized RAM, so power-off / hardware reset always returns to a clean READY state with no loop.
- The SDRAM buffers are explicitly zero-cleared at boot for belt-and-suspenders safety against retained values.

### User feedback
- 0.96" SPI OLED with state label, loop length, overdub counter, position bar, and output level meter.
- RGB LED gives instant status without looking at the OLED.
- LED blinks twice red on undo, three times red on wipe.

### Reliability
- Both footswitches debounced at 1 kHz inside the audio callback (not the slow OLED loop), so single taps register reliably.
- Press-tracking distinguishes short tap, long hold, double-tap, and double-tap-hold so all four gestures coexist across the two switches.

### Quantization
- Loop length is rounded to the nearest whole measure based on time signature and BPM.
- Time signatures supported: 4/4, 3/4, 6/8. BPM range 40–240. In 4/4 and 3/4 the BPM counts quarter notes; in 6/8 the BPM counts dotted quarters (two beats per measure).
- Metronome generates a sine click on each beat with a higher-pitched accent on beat 1.
- Metronome only sounds when both quantization and the metronome flag are enabled.
- Single quant-switch tap toggles quantization on/off; double tap opens a 4-field settings menu (time sig, BPM, metro on, metro volume) navigated entirely with the pot + the same switch.

---

## 5. Known limitations and design notes

- Single-level undo only. Holding repeatedly does not peel back multiple layers — only the most recent overdub can be undone.
- 30-second maximum master loop length. Going past 30 s during initial recording auto-locks the loop at maximum length and starts playback.
- Mono only (left audio in / left audio out). Right channel is mirrored to the same signal at the output.
- Wipe shows a brief `DUB` flicker if invoked while in `PLAY` (because the first tap of the double-tap-hold advances the state before the wipe gesture is recognised).
- The HiLetgo OLED's SPI traffic is suspected to inject some ~350 Hz interference into the analog input on the breadboard. The current build keeps the OLED enabled because the visual feedback is essential; if hum becomes a problem in future, that is the first thing to investigate.

---

## 6. Source layout

```
GuitarLoopPedal/
├── Makefile                   # builds src/main.cpp against libDaisy + DaisySP
├── README.md                  # short build instructions
├── LOOPER.md                  # this file — full project documentation
├── src/main.cpp               # firmware
├── tools/grab_recording.py    # captures a 10 s recording over USB CDC for noise analysis
├── tools/analyze_noise.py     # analyzes the captured WAV for tonal noise sources
├── libDaisy/                  # vendored hardware library
└── DaisySP/                   # vendored DSP library
```

---

## 7. Changelog

- **v0.1** — basic passthrough, audio test tones, noise capture and analysis tooling.
- **v0.2** — single-tap looper (record → loop → clear), debounced footswitch, OLED status.
- **v0.3** — overdub mode, RGB LED status, one-level undo on long hold, loop-boundary crossfade, OLED layer counter, undo confirmation blink.
- **v0.4** — double-tap-hold gesture wipes the loop. RGB LED triple-blinks red on wipe. Documentation file (`LOOPER.md`) added.
- **v0.5** — second footswitch on D0 for quantization. Single tap toggles quant on/off; double tap opens a settings menu (time signature, BPM, metronome on/off, metronome volume) navigated with the pot and the quant switch. Loop length is rounded to the nearest measure when quantization is on. Built-in metronome with sine click and beat-1 accent.
- **v0.6** — when quantization is on, recording is preceded by a mandatory 2-measure count-in (always audible, regardless of the menu's metronome setting), so the user has a tempo reference before playing. The metronome on/off setting now only controls whether the metronome continues *after* the count-in. New `CNT` state with beat countdown on the OLED and a red-pulsing LED that flashes with each click.
- **v0.7** — Metronome rule narrowed: `Metronome = ON` only keeps clicks going through master-loop recording, never during playback or overdubbing. 6/8 now counts the BPM in dotted quarters (2 beats per measure) instead of eighth notes. Time-sig defaults: 4/4 = 100 BPM, 3/4 = 100 BPM, 6/8 = 75 BPM. Selecting a new time signature in the menu auto-resets the BPM to that default. Menu fields now use pot pickup, so the parameter doesn't snap to wherever the pot is when a field opens.
- **v0.8** — Two more foot switches: octavize (toggle on D9) drops the live guitar one octave so bass lines can be added into the loop and overdubs; mute (momentary hold on D11) silences the loop while letting the live guitar pass through. Blue RGB-LED channel now indicates octavize on. OLED status line shows `O+/-` and overlays a `MUTED` banner while mute is held.
