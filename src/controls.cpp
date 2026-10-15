#include "controls.h"
#include "hardware.h"
#include "looper.h"
#include "metronome.h"

namespace pedal {
using namespace daisy;

// =====================================================================
// Quant switch event handlers
// =====================================================================
static void OnQuantSingleTap()
{
    if(menu_field == MenuField::None)
    {
        // Toggle quantization on/off
        quant_enabled = !quant_enabled;
        if(quant_enabled) metro.Reset();
    }
    else
    {
        // Advance to next menu field, exit when past the last
        switch(menu_field)
        {
            case MenuField::TimeSig:  menu_field = MenuField::Bpm;      break;
            case MenuField::Bpm:      menu_field = MenuField::MetroOn;  break;
            case MenuField::MetroOn:  menu_field = MenuField::MetroVol; break;
            case MenuField::MetroVol: menu_field = MenuField::None;     break;
            default: break;
        }
    }
}

static void OnQuantDoubleTap()
{
    if(menu_field == MenuField::None)
        menu_field = MenuField::TimeSig;     // open menu
    else
        menu_field = MenuField::None;        // save & exit
}

// =====================================================================
// Looper-switch press tracking (audio thread)
// =====================================================================
constexpr uint32_t kHoldBlocks         = 1000;   // 1 s @ 1 kHz block rate
constexpr uint32_t kDoubleTapWindowMs  = 400;
static bool        loopsw_active      = false;
static uint32_t    loopsw_count       = 0;
static bool        loopsw_long        = false;
static uint32_t    loopsw_last_rel_ms = 0;
static bool        loopsw_is_second   = false;

void PollLooperSwitch()
{
    footsw_loop.Debounce();
    uint32_t now = System::GetNow();

    if(footsw_loop.RisingEdge())
    {
        loopsw_active    = true;
        loopsw_count     = 0;
        loopsw_long      = false;
        loopsw_is_second = (loopsw_last_rel_ms != 0)
                           && ((now - loopsw_last_rel_ms) <= kDoubleTapWindowMs);
    }

    if(loopsw_active)
    {
        loopsw_count++;
        if(!loopsw_long && loopsw_count >= kHoldBlocks)
        {
            loopsw_long = true;
            if(loopsw_is_second) DoWipe();
            else                  DoLongHoldUndo();
        }
    }

    if(footsw_loop.FallingEdge())
    {
        bool was_long = loopsw_long;
        loopsw_active      = false;
        loopsw_long        = false;
        loopsw_last_rel_ms = now;
        if(!was_long) DoShortTap();
    }
}

// =====================================================================
// Quant-switch press tracking (deferred single tap to disambiguate from double tap)
// =====================================================================
constexpr uint32_t kQuantTapDeferMs = 300;
static bool      qsw_active           = false;
static bool      qsw_handled          = false;
static uint32_t  qsw_pending_single   = 0;
static bool      qsw_waiting_for_pair = false;

void PollQuantSwitch()
{
    footsw_quant.Debounce();
    uint32_t now = System::GetNow();

    if(footsw_quant.RisingEdge())
    {
        qsw_active = true;
        if(qsw_waiting_for_pair)
        {
            // Second tap of a pair while a single-tap was pending -> double tap
            qsw_pending_single   = 0;
            qsw_waiting_for_pair = false;
            qsw_handled          = true;
            OnQuantDoubleTap();
        }
    }

    if(footsw_quant.FallingEdge())
    {
        qsw_active = false;
        if(!qsw_handled)
        {
            qsw_pending_single   = now + kQuantTapDeferMs;
            qsw_waiting_for_pair = true;
        }
        qsw_handled = false;
    }

    if(qsw_pending_single != 0 && now >= qsw_pending_single)
    {
        qsw_pending_single   = 0;
        qsw_waiting_for_pair = false;
        OnQuantSingleTap();
    }
}


} // namespace pedal
