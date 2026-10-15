#pragma once

#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "biquad_notch.h"
#include "octave_down.h"

namespace pedal {
using MyOledDisplay = daisy::OledDisplay<daisy::SSD130x4WireSpi128x64Driver>;
extern daisy::DaisySeed hw;
extern MyOledDisplay display;
extern daisy::Switch footsw_loop, footsw_quant, footsw_oct, footsw_mute;
extern daisy::GPIO ledR, ledG, ledB;
extern OctaveDown pitch_down;
extern BiquadNotch hum_notch;
void InitHardware();
void SetLedRGB(bool r, bool g, bool b);
} // namespace pedal
