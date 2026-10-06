#include "../include/convolution_filter.h"

#include <algorithm>
#include <assert.h>
#include <string.h>

#if defined(__SSE__)
#include <xmmintrin.h>
#endif
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace {

constexpr int kLanes = 4;
// History is linearized once per chunk, then every output reuses it.
constexpr int kChunk = 512;

// acc + factor * sample, each product rounded before the add. A fused
// multiply-add is a different float and will not match ConvolutionFilter::f.
void accumulateLanes(float *acc, const float *impulse, const float *newest, int taps) {
#if defined(__SSE__)
    __m128 sum = _mm_loadu_ps(acc);
    for (int k = 0; k < taps; ++k) {
        const __m128 h = _mm_set1_ps(impulse[k]);
        const __m128 x = _mm_loadu_ps(newest - k);
        sum = _mm_add_ps(sum, _mm_mul_ps(h, x));
    }
    _mm_storeu_ps(acc, sum);
#elif defined(__ARM_NEON)
    float32x4_t sum = vld1q_f32(acc);
    for (int k = 0; k < taps; ++k) {
        const float32x4_t h = vdupq_n_f32(impulse[k]);
        const float32x4_t x = vld1q_f32(newest - k);
        sum = vaddq_f32(sum, vmulq_f32(h, x));
    }
    vst1q_f32(acc, sum);
#else
    for (int k = 0; k < taps; ++k) {
        const float h = impulse[k];
        const float *x = newest - k;
        for (int lane = 0; lane < kLanes; ++lane)
            acc[lane] += h * x[lane];
    }
#endif
}

float dotFromNewest(const float *impulse, const float *newest, int taps) {
    float acc = 0.0f;
    for (int k = 0; k < taps; ++k)
        acc += impulse[k] * newest[-k];
    return acc;
}

// Oldest history at window[0], newest history at window[taps - 2].
// Samples live in the ring newest-first as the offset walks backward.
void copyHistory(float *window, const float *shift, int offset, int taps) {
    const int history = taps - 1;
    int index = (offset + taps - 1) % taps;
    int written = 0;
    while (written < history) {
        const int run = std::min(index + 1, history - written);
        for (int i = 0; i < run; ++i)
            window[written + i] = shift[index - i];
        written += run;
        index = taps - 1;
    }
}

} // namespace

ConvolutionFilter::ConvolutionFilter() {
    m_shiftRegister = nullptr;
    m_impulseResponse = nullptr;
    m_window = nullptr;

    m_shiftOffset = 0;
    m_sampleCount = 0;
}

ConvolutionFilter::~ConvolutionFilter() {
    assert(m_shiftRegister == nullptr);
    assert(m_impulseResponse == nullptr);
    assert(m_window == nullptr);
}

void ConvolutionFilter::initialize(int samples) {
    m_sampleCount = samples;
    m_shiftOffset = 0;
    const int stored = samples > 0 ? samples : 1;
    m_shiftRegister = new float[stored];
    m_impulseResponse = new float[stored];
    m_window = new float[stored + (kChunk - 1)];

    memset(m_shiftRegister, 0, sizeof(float) * static_cast<size_t>(stored));
    memset(m_impulseResponse, 0, sizeof(float) * static_cast<size_t>(stored));
}

void ConvolutionFilter::clearHistory() {
    if (m_shiftRegister == nullptr || m_sampleCount <= 0)
        return;

    memset(m_shiftRegister, 0, sizeof(float) * static_cast<size_t>(m_sampleCount));
    m_shiftOffset = 0;
}

void ConvolutionFilter::destroy() {
    delete[] m_shiftRegister;
    delete[] m_impulseResponse;
    delete[] m_window;

    m_shiftRegister = nullptr;
    m_impulseResponse = nullptr;
    m_window = nullptr;
}

float ConvolutionFilter::f(float sample) {
    if (m_sampleCount <= 0 || m_shiftRegister == nullptr)
        return 0.0f;

    m_shiftRegister[m_shiftOffset] = sample;

    float result = 0;
    for (int i = 0; i < m_sampleCount - m_shiftOffset; ++i) {
        result += m_impulseResponse[i] * m_shiftRegister[i + m_shiftOffset];
    }

    for (int i = m_sampleCount - m_shiftOffset; i < m_sampleCount; ++i) {
        result += m_impulseResponse[i] * m_shiftRegister[i - (m_sampleCount - m_shiftOffset)];
    }

    m_shiftOffset = (m_shiftOffset - 1 + m_sampleCount) % m_sampleCount;

    return result;
}

void ConvolutionFilter::advanceHistory(const float *input, int count) {
    if (m_sampleCount <= 0 || input == nullptr || count <= 0)
        return;

    for (int s = 0; s < count; ++s) {
        m_shiftRegister[m_shiftOffset] = input[s];
        m_shiftOffset = (m_shiftOffset - 1 + m_sampleCount) % m_sampleCount;
    }
}

void ConvolutionFilter::process(const float *input, float *output, int count) {
    if (output == nullptr || count <= 0)
        return;
    if (m_sampleCount <= 0 || input == nullptr) {
        memset(output, 0, sizeof(float) * static_cast<size_t>(count));
        return;
    }

    const int taps = m_sampleCount;
    int done = 0;
    while (done < count) {
        const int width = std::min(kChunk, count - done);
        if (taps > 1)
            copyHistory(m_window, m_shiftRegister, m_shiftOffset, taps);
        for (int s = 0; s < width; ++s)
            m_window[(taps - 1) + s] = input[done + s];

        int s = 0;
        for (; s + kLanes <= width; s += kLanes) {
            float acc[kLanes] = {0.0f, 0.0f, 0.0f, 0.0f};
            accumulateLanes(acc, m_impulseResponse, m_window + (taps - 1) + s, taps);
            for (int lane = 0; lane < kLanes; ++lane)
                output[done + s + lane] = acc[lane];
        }
        for (; s < width; ++s)
            output[done + s] = dotFromNewest(
                m_impulseResponse, m_window + (taps - 1) + s, taps);

        advanceHistory(input + done, width);
        done += width;
    }
}
