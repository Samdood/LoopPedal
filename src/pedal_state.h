#pragma once

#include "daisy_seed.h"

namespace pedal {

// =====================================================================
// Loop buffers (SDRAM)
// =====================================================================
constexpr uint32_t kSampleRate    = 48000;
constexpr uint32_t kMaxLoopSecs   = 30;
constexpr uint32_t kMaxLoopSample = kSampleRate * kMaxLoopSecs;
extern float loop_buf[kMaxLoopSample];
extern float undo_buf[kMaxLoopSample];

constexpr uint32_t kFadeSamples = 1024;   // ~21 ms loop-boundary crossfade

// =====================================================================
// Looper state
// =====================================================================
enum class State : uint8_t { Idle, CountIn, Recording, Looping, Overdub };
extern volatile State    state;
extern volatile uint32_t write_idx;
extern volatile uint32_t count_in_samples_remaining;
extern volatile int      count_in_total_beats;
extern volatile uint32_t metro_last_click_ms;
extern volatile uint32_t play_idx;
extern volatile uint32_t loop_length;
extern volatile bool     has_undo;
extern volatile int      overdub_count;
extern volatile int      snapshot_count;

enum class PendingOp : uint8_t { None, Snapshot, Restore };
extern volatile PendingOp pending;
extern volatile bool      loop_muted;

extern volatile float loop_gain;   // 0..1, set by pot when not in menu
extern volatile float peak_out;

// =====================================================================
// LED feedback blinks
// =====================================================================
constexpr uint32_t kBlinkPhaseMs     = 150;
constexpr uint32_t kUndoBlinkTotalMs = kBlinkPhaseMs * 4;   // 2 blinks
constexpr uint32_t kWipeBlinkTotalMs = kBlinkPhaseMs * 6;   // 3 blinks
extern volatile uint32_t undo_blink_start_ms;
extern volatile uint32_t wipe_blink_start_ms;

// =====================================================================
// Quantization params + menu
// =====================================================================
enum class TimeSig : uint8_t { S44, S34, S68 };
extern volatile bool    quant_enabled;
extern volatile TimeSig time_sig;
extern volatile float   bpm;   // default for 4/4 and 3/4
extern volatile bool    metro_enabled;
extern volatile int     metro_volume;       // 0..100

enum class MenuField : uint8_t { None, TimeSig, Bpm, MetroOn, MetroVol };
extern volatile MenuField menu_field;

extern volatile bool octavize_on;
extern volatile bool mute_held;

} // namespace pedal
