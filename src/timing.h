#pragma once

#include "pedal_state.h"

namespace pedal {

// Default BPM for a given time signature.
//   4/4 and 3/4: 100 quarter notes per minute
//   6/8        :  75 dotted-quarter notes per minute
inline float DefaultBpmFor(TimeSig ts)
{
    return (ts == TimeSig::S68) ? 75.0f : 100.0f;
}
// Number of metronome "beats" per measure.
//   4/4 -> 4 quarters
//   3/4 -> 3 quarters
//   6/8 -> 2 dotted quarters (BPM is per dotted quarter for 6/8)
inline int BeatsPerMeasure(TimeSig ts)
{
    switch(ts)
    {
        case TimeSig::S44: return 4;
        case TimeSig::S34: return 3;
        case TimeSig::S68: return 2;
    }
    return 4;
}

inline const char* TimeSigLabel(TimeSig ts)
{
    switch(ts)
    {
        case TimeSig::S44: return "4/4";
        case TimeSig::S34: return "3/4";
        case TimeSig::S68: return "6/8";
    }
    return "?";
}

} // namespace pedal
