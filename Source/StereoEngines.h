#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace stereofx
{
enum Engine
{
    widener = 0,
    haas,
    frequencyImager,
    spatial,
    monoMaker,
    correlation,
    numEngines
};

struct Params
{
    int engine = widener;
    int variant = 0;
    bool bypass = false;
    bool mono = false;

    float width = 100.0f;
    float balance = 0.0f;
    float midGainDb = 0.0f;
    float sideGainDb = 0.0f;
    float bassMonoHz = 120.0f;
    float haasMs = 0.0f;
    float mix = 100.0f;
    float outputDb = 0.0f;

    float wideFocus = 50.0f;
    float wideSafety = 50.0f;

    float haasTime = 8.0f;
    float haasFeedback = 0.0f;
    float haasTone = 55.0f;
    float haasSide = 70.0f;

    float imgLow = 85.0f;
    float imgMid = 110.0f;
    float imgHigh = 125.0f;
    float imgXover1 = 180.0f;
    float imgXover2 = 3200.0f;

    float spatialDepth = 35.0f;
    float spatialAngle = 0.0f;
    float spatialAir = 35.0f;
    float spatialFocus = 50.0f;

    float monoFreq = 140.0f;
    float monoStrength = 80.0f;
    float monoSlope = 50.0f;
    float monoAudition = 0.0f;

    float corrHold = 40.0f;
    float corrDecay = 45.0f;
    float corrZoom = 50.0f;
    float corrWarn = 35.0f;
};

struct Snapshot
{
    float correlation = 1.0f;
    float width = 0.0f;
    float balance = 0.0f;
    float lowMono = 0.0f;
    float monoRisk = 0.0f;
    int engine = 0;
    int variant = 0;
};

inline float dbToGain(float dB) noexcept
{
    return std::pow(10.0f, dB / 20.0f);
}

inline float percent(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, value / 100.0f);
}

inline void encodeMS(float left, float right, float& mid, float& side) noexcept
{
    mid = 0.5f * (left + right);
    side = 0.5f * (left - right);
}

inline void decodeMS(float mid, float side, float& left, float& right) noexcept
{
    left = mid + side;
    right = mid - side;
}

inline float safeWidth(float midEnergy, float sideEnergy) noexcept
{
    return sideEnergy / juce::jmax(1.0e-9f, midEnergy);
}

} // namespace stereofx
