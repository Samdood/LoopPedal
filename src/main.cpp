// Looper state transitions, audio processing, and application entry point.
// See LOOPER.md for wiring and controls.
#include "controls.h"
#include "hardware.h"
#include "looper.h"
#include "metronome.h"
#include "ui.h"
#include <cstring>
#include <cmath>

namespace pedal {
using namespace daisy;

// =====================================================================
// Crossfaded loop read at boundary
// =====================================================================
static inline float ReadLoopSampleXfade(uint32_t idx, uint32_t len)
{
    if(len == 0) return 0.0f;
    float a = loop_buf[idx];
    if(len > 2 * kFadeSamples && idx >= len - kFadeSamples)
    {
        uint32_t pos_in = idx - (len - kFadeSamples);
        float    f_in   = static_cast<float>(pos_in) / kFadeSamples;
        float    f_out  = 1.0f - f_in;
        return a * f_out + loop_buf[pos_in] * f_in;
    }
    return a;
}

// =====================================================================
// Looper transitions
// =====================================================================
static uint32_t QuantizeLength(uint32_t raw_len)
{
    if(!quant_enabled || raw_len == 0) return raw_len;
    int   beats   = BeatsPerMeasure(time_sig);
    float spb     = static_cast<float>(kSampleRate) * 60.0f / bpm;
    uint32_t spm  = static_cast<uint32_t>(spb * beats);
    if(spm == 0) return raw_len;
    uint32_t measures = (raw_len + spm / 2) / spm;     // round to nearest measure
    if(measures < 1) measures = 1;
    uint32_t qlen = measures * spm;
    if(qlen > kMaxLoopSample)
        qlen = (kMaxLoopSample / spm) * spm;
    return qlen;
}

void DoShortTap()  // looper switch
{
    switch(state)
    {
        case State::Idle:
            write_idx = 0;
            if(quant_enabled)
            {
                // Two-measure count-in at the configured BPM/time signature.
                // The metronome ticks immediately and the very first sample
                // fires a downbeat click.
                metro.Reset();
                int   beats = BeatsPerMeasure(time_sig) * 2;
                float spb   = static_cast<float>(kSampleRate) * 60.0f / bpm;
                count_in_total_beats        = beats;
                count_in_samples_remaining  = static_cast<uint32_t>(beats * spb);
                state = State::CountIn;
            }
            else
            {
                state = State::Recording;
            }
            break;

        case State::CountIn:
            // Tapping again during the count-in cancels it.
            count_in_samples_remaining = 0;
            state = State::Idle;
            break;

        case State::Recording:
        {
            uint32_t qlen = QuantizeLength(write_idx);
            loop_length    = qlen;
            play_idx       = 0;
            overdub_count  = 0;
            state          = (loop_length > 0) ? State::Looping : State::Idle;
            break;
        }

        case State::Looping:
            if(pending == PendingOp::None)
                pending = PendingOp::Snapshot;
            snapshot_count = overdub_count;
            state          = State::Overdub;
            break;

        case State::Overdub:
            overdub_count++;
            state = State::Looping;
            break;
    }
}

void DoLongHoldUndo()
{
    if((state != State::Looping && state != State::Overdub) || !has_undo)
        return;
    state               = State::Looping;
    loop_muted          = true;
    overdub_count       = snapshot_count;
    if(pending == PendingOp::None)
        pending = PendingOp::Restore;
    undo_blink_start_ms = System::GetNow();
}

void DoWipe()
{
    state              = State::Idle;
    write_idx          = 0;
    play_idx           = 0;
    loop_length        = 0;
    overdub_count      = 0;
    snapshot_count     = 0;
    has_undo           = false;
    loop_muted         = false;
    pending            = PendingOp::None;
    wipe_blink_start_ms = System::GetNow();
}

// =====================================================================
// Audio callback
// =====================================================================
void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    PollLooperSwitch();
    PollQuantSwitch();

    // Octavize: latch toggle on rising edge.
    footsw_oct.Debounce();
    if(footsw_oct.RisingEdge())
        octavize_on = !octavize_on;

    // Mute: held while the switch is physically pressed.
    footsw_mute.Debounce();
    mute_held = footsw_mute.Pressed();

    float blk_out = 0.0f;

    for(size_t i = 0; i < size; i += 2)
    {
        float x = in[i];

        // 60 Hz mains-hum notch — applied at the input so it cleans the signal
        // for output, recording, overdub, and the octavize path.
        x = hum_notch.Process(x);

        // The "live" signal is the input, optionally shifted down an octave.
        // Octavize affects the audible live signal AND what gets recorded
        // into the master loop / overdub, so the user can lay down bass lines.
        float live = octavize_on ? pitch_down.Process(x) : x;

        float loop_sample = 0.0f;

        if(state == State::CountIn)
        {
            // Count-in: no recording, just count samples until done.
            if(count_in_samples_remaining > 0)
            {
                count_in_samples_remaining--;
            }
            else
            {
                state     = State::Recording;
                write_idx = 0;
                // Re-align the metronome so beat 1 of recording lines up
                // exactly with the first recorded sample.
                metro.Reset();
            }
        }
        else if(state == State::Recording)
        {
            if(write_idx < kMaxLoopSample)
            {
                loop_buf[write_idx++] = live;
            }
            else
            {
                loop_length    = QuantizeLength(write_idx);
                play_idx       = 0;
                overdub_count  = 0;
                state          = State::Looping;
            }
        }
        else if((state == State::Looping || state == State::Overdub)
                && loop_length > 0 && !loop_muted)
        {
            loop_sample = ReadLoopSampleXfade(play_idx, loop_length) * loop_gain;

            if(state == State::Overdub)
            {
                float prev = loop_buf[play_idx];
                loop_buf[play_idx] = prev * 0.99f + live;
            }
            play_idx = (play_idx + 1) % loop_length;
        }

        // Mute footswitch silences the loop playback, but live guitar still
        // passes through. Metronome clicks are unaffected.
        if(mute_held) loop_sample = 0.0f;

        // Metronome (silent if quant_enabled is false)
        float click = metro.Process();

        float y = live + loop_sample + click;
        if(y >  1.0f) y =  1.0f;
        if(y < -1.0f) y = -1.0f;

        out[i]     = y;
        out[i + 1] = y;

        float ay = fabsf(y);
        if(ay > blk_out) blk_out = ay;
    }

    peak_out = (blk_out > peak_out) ? blk_out : peak_out * 0.9f;
}


static void ServicePendingOperation()
{
    PendingOp op = pending;
    if(op == PendingOp::Snapshot)
    {
        std::memcpy(undo_buf, loop_buf, loop_length * sizeof(float));
        has_undo = true;
        pending  = PendingOp::None;
    }
    else if(op == PendingOp::Restore)
    {
        std::memcpy(loop_buf, undo_buf, loop_length * sizeof(float));
        has_undo   = false;
        loop_muted = false;
        pending    = PendingOp::None;
    }
}

} // namespace pedal

int main()
{
    using namespace pedal;
    InitHardware();
    metro.Reset();
    hw.StartAudio(AudioCallback);
    while(1)
    {
        ServicePendingOperation();
        UpdateUi();
    }
}
