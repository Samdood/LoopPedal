#pragma once

#include <cmath>

// =====================================================================
// Simple granular octave-down pitch shifter
// =====================================================================
// Two grains, each reads from a circular buffer at half-speed (one octave
// down) with a sine-window envelope. The two grains are offset by half a
// grain length so that as one fades out, the other fades in, producing a
// continuous output.
class OctaveDown
{
  public:
    static constexpr int kBufLen   = 8192;   // ~170 ms at 48 kHz
    static constexpr int kGrainLen = 2400;   // ~50 ms grain

    void Init()
    {
        for(int i = 0; i < kBufLen; i++) buf_[i] = 0.0f;
        write_idx_ = 0;
        g1_        = 0.0f;
        g2_        = static_cast<float>(kGrainLen) * 0.5f;
    }

    float Process(float x)
    {
        // Write input into circular buffer
        buf_[write_idx_] = x;
        int wi           = write_idx_;
        write_idx_       = (write_idx_ + 1) % kBufLen;

        // Each grain advances at 0.5 sample/sample → octave down.
        g1_ += 0.5f;
        g2_ += 0.5f;
        if(g1_ >= kGrainLen) g1_ -= kGrainLen;
        if(g2_ >= kGrainLen) g2_ -= kGrainLen;

        // Read positions (samples behind the write head)
        float r1 = static_cast<float>(wi) - g1_;
        float r2 = static_cast<float>(wi) - g2_;
        while(r1 < 0.0f) r1 += kBufLen;
        while(r2 < 0.0f) r2 += kBufLen;

        // Linear interpolation
        int   i1  = static_cast<int>(r1);
        int   i1n = (i1 + 1) % kBufLen;
        float f1  = r1 - i1;
        float s1  = buf_[i1] * (1.0f - f1) + buf_[i1n] * f1;

        int   i2  = static_cast<int>(r2);
        int   i2n = (i2 + 1) % kBufLen;
        float f2  = r2 - i2;
        float s2  = buf_[i2] * (1.0f - f2) + buf_[i2n] * f2;

        // Sine windows over each grain (sum is roughly constant amplitude)
        float p1 = g1_ / static_cast<float>(kGrainLen);
        float p2 = g2_ / static_cast<float>(kGrainLen);
        float w1 = sinf(p1 * static_cast<float>(M_PI));
        float w2 = sinf(p2 * static_cast<float>(M_PI));

        return s1 * w1 + s2 * w2;
    }

  private:
    float buf_[kBufLen];
    int   write_idx_;
    float g1_, g2_;
};
