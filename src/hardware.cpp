#include "hardware.h"
#include "pedal_state.h"

namespace pedal {
using namespace daisy;

DaisySeed hw;
MyOledDisplay display;
Switch footsw_loop, footsw_quant, footsw_oct, footsw_mute;
GPIO ledR, ledG, ledB;
AdcChannelConfig adcCfg;
OctaveDown pitch_down;
BiquadNotch hum_notch;

void SetLedRGB(bool r, bool g, bool b)
{
    ledR.Write(r);
    ledG.Write(g);
    ledB.Write(b);
}

void InitHardware()
{
    hw.Init();
    hw.SetAudioBlockSize(48);

    adcCfg.InitSingle(seed::A0);
    hw.adc.Init(&adcCfg, 1);
    hw.adc.Start();

    footsw_loop.Init(seed::D1,  1000.0f);
    footsw_quant.Init(seed::D0,  1000.0f);
    footsw_oct.Init(seed::D9,    1000.0f);
    footsw_mute.Init(seed::D11,  1000.0f);

    // Custom granular octave-down pitch shifter
    pitch_down.Init();

    // 60 Hz mains-hum notch on the guitar input.
    // Q = 30 gives a very narrow notch (~2 Hz wide) so the guitar tone is
    // essentially untouched outside the hum band.
    hum_notch.Init(static_cast<float>(kSampleRate), 60.0f, 30.0f);

    ledR.Init(seed::D2, GPIO::Mode::OUTPUT);
    ledG.Init(seed::D3, GPIO::Mode::OUTPUT);
    ledB.Init(seed::D4, GPIO::Mode::OUTPUT);
    SetLedRGB(false, false, false);

    MyOledDisplay::Config disp_cfg;
    disp_cfg.driver_config.transport_config.pin_config.dc    = seed::D6;
    disp_cfg.driver_config.transport_config.pin_config.reset = seed::D5;
    display.Init(disp_cfg);

    for(uint32_t i = 0; i < kMaxLoopSample; i++)
    {
        loop_buf[i] = 0.0f;
        undo_buf[i] = 0.0f;
    }
}

} // namespace pedal
