#include "pedal_state.h"

namespace pedal {

float DSY_SDRAM_BSS loop_buf[kMaxLoopSample];
float DSY_SDRAM_BSS undo_buf[kMaxLoopSample];
volatile State    state                       = State::Idle;
volatile uint32_t write_idx                   = 0;
volatile uint32_t count_in_samples_remaining  = 0;
volatile int      count_in_total_beats        = 0;
volatile uint32_t metro_last_click_ms         = 0;
volatile uint32_t play_idx       = 0;
volatile uint32_t loop_length    = 0;
volatile bool     has_undo       = false;
volatile int      overdub_count  = 0;
volatile int      snapshot_count = 0;
volatile PendingOp pending      = PendingOp::None;
volatile bool      loop_muted   = false;
volatile float loop_gain = 0.0f;   // 0..1, set by pot when not in menu
volatile float peak_out  = 0.0f;
volatile uint32_t undo_blink_start_ms = 0;
volatile uint32_t wipe_blink_start_ms = 0;
volatile bool    quant_enabled = false;
volatile TimeSig time_sig      = TimeSig::S44;
volatile float   bpm           = 100.0f;   // default for 4/4 and 3/4
volatile bool    metro_enabled = false;
volatile int     metro_volume  = 50;       // 0..100
volatile MenuField menu_field = MenuField::None;

volatile bool octavize_on = false;
volatile bool mute_held = false;

} // namespace pedal
