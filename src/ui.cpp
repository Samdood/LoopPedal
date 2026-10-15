#include "ui.h"
#include "hardware.h"
#include "timing.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace pedal {
using namespace daisy;

// =====================================================================
// LED + OLED helpers
// =====================================================================
static void UpdateLed()
{
    uint32_t now = System::GetNow();

    if(wipe_blink_start_ms != 0)
    {
        uint32_t since = now - wipe_blink_start_ms;
        if(since < kWipeBlinkTotalMs)
        {
            uint32_t phase = since / kBlinkPhaseMs;
            bool     on    = (phase % 2 == 0);
            SetLedRGB(on ? 1 : 0, 0, 0);
            return;
        }
    }
    if(undo_blink_start_ms != 0)
    {
        uint32_t since = now - undo_blink_start_ms;
        if(since < kUndoBlinkTotalMs)
        {
            uint32_t phase = since / kBlinkPhaseMs;
            bool     on    = (phase % 2 == 0);
            SetLedRGB(on ? 1 : 0, 0, 0);
            return;
        }
    }

    bool r = false, g = false, b = false;
    switch(state)
    {
        case State::Idle:      break;
        case State::Recording: r = true; break;
        case State::Looping:   g = true; break;
        case State::Overdub:   r = true; g = true; break;
        case State::CountIn:
            // Flash red for ~80 ms after each metronome click so the LED
            // pulses with the count-in beat.
            r = (now - metro_last_click_ms) < 80;
            break;
    }

    // Octavize lights the blue channel on top of the state color.
    if(octavize_on) b = true;

    SetLedRGB(r, g, b);
}

static void DrawCentered(int y, const char* s, FontDef font)
{
    int len  = static_cast<int>(strlen(s));
    int w    = len * font.FontWidth;
    int x    = (128 - w) / 2;
    if(x < 0) x = 0;
    display.SetCursor(x, y);
    display.WriteString(s, font, true);
}

static void DrawMenu()
{
    char buf[32];

    DrawCentered(0, "QUANT MENU", Font_7x10);

    // Field name + value
    const char* field_name = "?";
    const char* value      = "?";
    char        valbuf[16];

    switch(menu_field)
    {
        case MenuField::TimeSig:
            field_name = "Time Signature";
            value      = TimeSigLabel(time_sig);
            break;
        case MenuField::Bpm:
            field_name = "BPM";
            snprintf(valbuf, sizeof(valbuf), "%d", static_cast<int>(bpm));
            value = valbuf;
            break;
        case MenuField::MetroOn:
            field_name = "Metronome";
            value      = metro_enabled ? "ON" : "OFF";
            break;
        case MenuField::MetroVol:
            field_name = "Metro Vol";
            snprintf(valbuf, sizeof(valbuf), "%d%%", metro_volume);
            value = valbuf;
            break;
        default: break;
    }

    DrawCentered(14, field_name, Font_7x10);

    // Big value, surrounded by < > to suggest pot scrolling
    snprintf(buf, sizeof(buf), "< %s >", value);
    DrawCentered(28, buf, Font_11x18);

    DrawCentered(50, "Tap=Next  DTap=Save", Font_6x8);
}

static void DrawMain()
{
    char buf[32];

    State s = state;

    display.SetCursor(0, 0);
    display.WriteString("Looper", Font_7x10, true);

    const char* lbl = "READY";
    switch(s)
    {
        case State::Idle:      lbl = "READY"; break;
        case State::CountIn:   lbl = "CNT";   break;
        case State::Recording: lbl = "REC";   break;
        case State::Looping:   lbl = "PLAY";  break;
        case State::Overdub:   lbl = "DUB";   break;
    }
    display.SetCursor(60, 0);
    display.WriteString(lbl, Font_11x18, true);

    if(s == State::CountIn)
    {
        // How many beats of the count-in have elapsed
        float spb         = static_cast<float>(kSampleRate) * 60.0f / bpm;
        int   beats_left  = static_cast<int>(count_in_samples_remaining / spb) + 1;
        int   beats_done  = count_in_total_beats - beats_left + 1;
        if(beats_done < 1) beats_done = 1;
        if(beats_done > count_in_total_beats) beats_done = count_in_total_beats;
        snprintf(buf, sizeof(buf), "Beat %d/%d",
                 beats_done, count_in_total_beats);
        display.SetCursor(0, 22);
        display.WriteString(buf, Font_7x10, true);

        display.SetCursor(0, 35);
        display.WriteString("count-in 2 bars", Font_7x10, true);
    }
    else if(s == State::Recording)
    {
        float secs = static_cast<float>(write_idx) / kSampleRate;
        snprintf(buf, sizeof(buf), "rec %4.1fs", static_cast<double>(secs));
        display.SetCursor(0, 22);
        display.WriteString(buf, Font_7x10, true);
    }
    else if(s == State::Looping || s == State::Overdub)
    {
        float lensec = static_cast<float>(loop_length) / kSampleRate;
        snprintf(buf, sizeof(buf), "len %4.1fs", static_cast<double>(lensec));
        display.SetCursor(0, 22);
        display.WriteString(buf, Font_7x10, true);

        snprintf(buf, sizeof(buf), "Lay:%d", overdub_count);
        display.SetCursor(74, 22);
        display.WriteString(buf, Font_7x10, true);

        // Loop position bar
        int w    = 128;
        int fill = (loop_length > 0)
                       ? static_cast<int>(
                             static_cast<float>(play_idx) / loop_length
                             * (w - 2))
                       : 0;
        display.DrawRect(0, 35, w - 1, 41, true);
        if(fill > 0)
            display.DrawRect(1, 36, 1 + fill, 40, true, true);
    }
    else // Idle
    {
        display.SetCursor(0, 22);
        display.WriteString("press to record", Font_7x10, true);
    }

    // Bottom status line, abbreviated to fit:
    //   "<sig> <bpm> Q+ M+ O+"   (+ = on, - = off)
    snprintf(buf,
             sizeof(buf),
             "%s %d Q%s M%s O%s",
             TimeSigLabel(time_sig),
             static_cast<int>(bpm),
             quant_enabled ? "+" : "-",
             metro_enabled ? "+" : "-",
             octavize_on   ? "+" : "-");
    display.SetCursor(0, 56);
    display.WriteString(buf, Font_6x8, true);

    // While the mute switch is held, paint a clear "MUTED" indicator on top.
    if(mute_held)
    {
        // Black box + bold label across the loop-position area.
        display.DrawRect(0, 33, 127, 47, false, true);   // clear strip
        const char* m = "MUTED";
        int w = static_cast<int>(strlen(m)) * Font_11x18.FontWidth;
        int x = (128 - w) / 2; if(x < 0) x = 0;
        display.SetCursor(x, 30);
        display.WriteString(m, Font_11x18, true);
    }
}

void UpdateUi()
{
    UpdateLed();
    // Pot: loop volume normally; menu parameter when in menu.
    // Inside the menu we use a "pot pickup" so the parameter doesn't snap
    // to wherever the pot happens to be — the user must move the pot a
    // bit before it starts updating the value.
    static MenuField prev_menu_field = MenuField::None;
    static float     pot_entry       = 0.0f;
    static bool      pot_engaged     = false;

    float pot = hw.adc.GetFloat(0);

    if(menu_field != prev_menu_field)
    {
        prev_menu_field = menu_field;
        pot_entry       = pot;
        pot_engaged     = false;
    }

    if(menu_field == MenuField::None)
    {
        loop_gain = loop_gain * 0.9f + pot * 0.1f;
    }
    else
    {
        // Engage the pot once the user has moved it noticeably from where
        // it sat when this field was opened. Until then, the value stays
        // at whatever default it was set to.
        if(!pot_engaged && fabsf(pot - pot_entry) > 0.03f)
            pot_engaged = true;

        if(pot_engaged)
        {
            switch(menu_field)
            {
                case MenuField::TimeSig:
                {
                    TimeSig new_sig = TimeSig::S44;
                    if(pot < 0.333f)      new_sig = TimeSig::S44;
                    else if(pot < 0.666f) new_sig = TimeSig::S34;
                    else                  new_sig = TimeSig::S68;
                    if(new_sig != time_sig)
                    {
                        time_sig = new_sig;
                        // BPM default depends on the time signature.
                        bpm = DefaultBpmFor(new_sig);
                    }
                    break;
                }
                case MenuField::Bpm:
                    bpm = roundf(40.0f + pot * 200.0f);   // 40..240
                    break;
                case MenuField::MetroOn:
                    metro_enabled = (pot >= 0.5f);
                    break;
                case MenuField::MetroVol:
                    metro_volume = static_cast<int>(pot * 100.0f);
                    if(metro_volume < 0)   metro_volume = 0;
                    if(metro_volume > 100) metro_volume = 100;
                    break;
                default: break;
            }
        }
    }

    // OLED
    display.Fill(false);
    if(menu_field != MenuField::None)
        DrawMenu();
    else
        DrawMain();
    display.Update();

    System::Delay(20);

}

} // namespace pedal
