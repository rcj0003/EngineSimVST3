#include "../include/jitter_filter.h"

#include <cstring>

JitterFilter::JitterFilter() {
    m_history = nullptr;
    m_maxJitter = 0;
    m_offset = 0;
    m_jitterScale = 0.0f;
}

JitterFilter::~JitterFilter() {
    /* void */
}

void JitterFilter::initialize(
    int maxJitter,
    float cutoffFrequency,
    float audioFrequency)
{
    m_maxJitter = maxJitter;

    m_history = new float[maxJitter];
    m_offset = 0;
    memset(m_history, 0, sizeof(float) * maxJitter);

    m_noiseFilter.setCutoffFrequency(cutoffFrequency, audioFrequency);
    const float jitterSpan = static_cast<float>(m_maxJitter > 0 ? m_maxJitter - 1 : 0);
    m_jitterDistribution = std::uniform_real_distribution<float>(0.0f, jitterSpan);
}

float JitterFilter::f(float sample) {
    return fast_f(sample);
}

void JitterFilter::reset() {
    m_offset = 0;
    if (m_history != nullptr && m_maxJitter > 0)
        memset(m_history, 0, sizeof(float) * static_cast<size_t>(m_maxJitter));
    m_noiseFilter.reset();
}
