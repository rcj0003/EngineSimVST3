#ifndef ATG_ENGINE_SIM_CONVOLUTION_FILTER_H
#define ATG_ENGINE_SIM_CONVOLUTION_FILTER_H

#include "filter.h"

class ConvolutionFilter : public Filter {
    public:
        ConvolutionFilter();
        virtual ~ConvolutionFilter();

        void initialize(int samples);
        void clearHistory();
        virtual float f(float sample) override;
        // Same sum as f(), one sample at a time, for a whole block.
        void process(const float *input, float *output, int count);
        // Writes the same history f() would, without the multiply-add.
        void advanceHistory(const float *input, int count);
        virtual void destroy();

        int getSampleCount() const { return m_sampleCount; }
        float *getImpulseResponse() { return m_impulseResponse; }

    protected:
        float *m_shiftRegister;
        int m_shiftOffset;

        float *m_impulseResponse;
        float *m_window;
        int m_sampleCount;
};

#endif /* ATG_ENGINE_SIM_CONVOLUTION_FILTER_H */
