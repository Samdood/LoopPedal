#pragma once

#include "timing.h"
#include <cmath>

namespace pedal {

using daisy::System;

// =====================================================================
// Metronome
// =====================================================================
struct Metronome
{
    float beat_phase       = 0.0f;
    int   beat_in_measure  = 0;
    float click_phase      = 0.0f;
    float click_freq       = 2000.0f;
    float click_amp        = 0.0f;

    void Reset()
    {
        // Set up so the very next sample wraps -> beat 0 fires a downbeat.
        int bpm_ = BeatsPerMeasure(time_sig);
        beat_phase      = 1.0f;
        beat_in_measure = bpm_ - 1;
        click_amp       = 0.0f;
    }

    void Trigger(bool downbeat)
    {
        click_amp  = 1.0f;
        click_phase = 0.0f;
        click_freq = downbeat ? 2400.0f : 1500.0f;
        metro_last_click_ms = System::GetNow();
    }

    float Process()
    {
        // The metronome only runs during the count-in (always, to give a
        // tempo reference) and during the master-loop recording (only if the
        // user enabled the metronome in the menu). It is silent in all other
        // states — Idle, Looping, Overdub.
        bool active =
              (state == State::CountIn)
           || (state == State::Recording && metro_enabled);

        if(!quant_enabled || !active)
        {
            click_amp = 0.0f;
            return 0.0f;
        }

        // Phase: bpm beats per minute -> increments by (bpm/60)/sr per sample
        float advance = (bpm / 60.0f) / static_cast<float>(kSampleRate);
        beat_phase += advance;
        if(beat_phase >= 1.0f)
        {
            beat_phase -= 1.0f;
            int bpm_ = BeatsPerMeasure(time_sig);
            beat_in_measure = (beat_in_measure + 1) % bpm_;
            Trigger(beat_in_measure == 0);
        }

        if(click_amp > 0.001f)
        {
            float s = sinf(click_phase) * click_amp;
            click_phase += 2.0f * static_cast<float>(M_PI) * click_freq
                           / static_cast<float>(kSampleRate);
            if(click_phase > 2.0f * static_cast<float>(M_PI))
                click_phase -= 2.0f * static_cast<float>(M_PI);
            click_amp *= 0.9985f;
            return s * (metro_volume * 0.01f);
        }
        return 0.0f;
    }
};

extern Metronome metro;

} // namespace pedal
