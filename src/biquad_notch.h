#pragma once

#include <cmath>

// =====================================================================
// Biquad notch filter (RBJ cookbook) for stripping mains hum (60 Hz).
// =====================================================================
class BiquadNotch
{
  public:
    void Init(float sample_rate, float freq, float Q)
    {
        const float omega = 2.0f * static_cast<float>(M_PI) * freq
                            / sample_rate;
        const float c     = cosf(omega);
        const float s     = sinf(omega);
        const float alpha = s / (2.0f * Q);
        const float a0    = 1.0f + alpha;

        b0_ =  1.0f / a0;
        b1_ = -2.0f * c / a0;
        b2_ =  1.0f / a0;
        a1_ = -2.0f * c / a0;
        a2_ = (1.0f - alpha) / a0;

        x1_ = x2_ = y1_ = y2_ = 0.0f;
    }

    inline float Process(float x)
    {
        float y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
        x2_ = x1_;
        x1_ = x;
        y2_ = y1_;
        y1_ = y;
        return y;
    }

  private:
    float b0_, b1_, b2_, a1_, a2_;
    float x1_, x2_, y1_, y2_;
};
