// Native NNLS front-end regression tests with controlled FFT-bin evidence.
#include "../Chordino.h"
#include "../chromamethods.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

class FrontendProbe : public Chordino {
public:
    FrontendProbe(bool reversePitchOrder = false) : Chordino(44100) {
        require(initialise(1, 2048, 16384), "initialisation");
        // Map known FFT bins directly to two tuning phases. Moving either
        // phase between low and high pitches must not reweight its evidence.
        m_kernelValue = {1, 1};
        m_kernelFftIndex = {2, 3};
        m_kernelNoteIndex = reversePitchOrder ? std::vector<int>{153, 1} :
                                               std::vector<int>{0, 154};
    }

    void frame(float phaseZero, float phaseOne, float dc = 0, float firstBin = 0) {
        std::vector<float> fft(16384 + 2, 0);
        fft[0] = dc;
        fft[2] = firstBin;
        fft[4] = phaseZero;
        fft[6] = phaseOne;
        const float *inputs[] = {fft.data()};
        process(inputs, Vamp::RealTime::frame2RealTime(m_frameCount * 2048, 44100));
    }

    const std::vector<float> &lastSpectrum() const { return m_logSpectrum.back().values; }
    const std::vector<float> &localPhases() const { return m_localTunings; }
    float localTuning() const { return m_localTuning.back(); }
};

int main() {
    try {
        {
            FrontendProbe clean;
            FrontendProbe withDc;
            clean.setParameter("rollon", 1);
            withDc.setParameter("rollon", 1);
            clean.frame(10, 20);
            withDc.frame(10, 20, 5000, 1000);
            require(clean.lastSpectrum() == withDc.lastSpectrum(),
                "DC/first-bin energy must not erase the usable spectrum");
            require(clean.lastSpectrum()[0] > 0 && clean.lastSpectrum()[154] > 0,
                "useful pitch evidence retained");
        }
        {
            FrontendProbe lowFirst;
            FrontendProbe highFirst(true);
            lowFirst.frame(10, 20);
            highFirst.frame(10, 20);
            require(std::abs(lowFirst.localTuning() - highFirst.localTuning()) < 1e-7,
                "local tuning invariant to pitch traversal order");
            require(std::abs(lowFirst.localPhases()[1] / lowFirst.localPhases()[0] - 2.f) < 1e-6,
                "local tuning respects the input evidence ratio");
        }
        {
            FrontendProbe plugin;
            plugin.frame(10, 0);
            const float firstPhase = plugin.localPhases()[0];
            plugin.frame(0, 20);
            const float actualRetention = plugin.localPhases()[0] / firstPhase;
            const float expectedRetention = std::pow(0.997f, 54);
            require(std::abs(actualRetention - expectedRetention) < 1e-6,
                "preserve original effective frame retention");
        }
        std::cout << "All three native frontend cases passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
