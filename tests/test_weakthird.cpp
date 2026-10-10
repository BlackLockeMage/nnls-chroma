#include "../Chordino.h"
#include "../chromamethods.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>

static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

class Probe : public Chordino {
public:
    Probe(float gate, bool diagnostics, bool nnlsEnabled = false) : Chordino(44100) {
        require(getParameter("thirdgate") == 0.f, "candidate must default off");
        require(getParameter("diagnostics") == 0.f, "diagnostics must default off");
        setParameter("thirdgate", gate);
        setParameter("diagnostics", diagnostics ? 1.f : 0.f);
        setParameter("useNNLS", nnlsEnabled ? 1.f : 0.f);
        require(initialise(1, 2048, 16384), "initialise");
        getOutputDescriptors();
    }

    void frame(bool silence = false) {
        Feature spectrum;
        spectrum.hasTimestamp = true;
        spectrum.timestamp = Vamp::RealTime::frame2RealTime(m_logSpectrum.size() * 2048, 44100);
        spectrum.values.assign(nNote, 0.f);
        if (!silence) {
            spectrum.values[3 + 39 * 3] = 50.f; // C
            spectrum.values[3 + 43 * 3] = 10.f; // E
            spectrum.values[3 + 46 * 3] = 50.f; // G
        }
        m_logSpectrum.push_back(spectrum);
    }

    const vector<float> &base() const { return m_chorddict; }
    const vector<float> &weak() const { return m_weakChorddict; }
    const vector<bool> &eligible() const { return m_weakEligible; }
    const vector<string> &names() const { return m_chordnames; }
    const vector<int> &groups() const { return m_scoreGroups; }
    bool permits(int group, const vector<float> &notes) const { return permitsWeakThird(group, notes); }
};

static bool sameFeature(const Vamp::Plugin::Feature &a, const Vamp::Plugin::Feature &b) {
    return a.hasTimestamp == b.hasTimestamp && a.timestamp == b.timestamp &&
        a.hasDuration == b.hasDuration && a.duration == b.duration &&
        a.label == b.label && a.values == b.values;
}

int main() {
    try {
        Probe disabled(0.f, false);
        Probe observed(0.f, true);
        require(disabled.weak().empty(), "off must have no alternative template scores");
        for (int i = 0; i < 3; ++i) {
            disabled.frame(i == 1);
            observed.frame(i == 1);
        }
        const auto a = disabled.getRemainingFeatures();
        const auto b = observed.getRemainingFeatures();
        require(a.find(4) == a.end() && a.find(5) == a.end() && a.find(6) == a.end(), "diagnostics disabled means no extra frames");
        for (int output = 0; output < 4; ++output) {
            require((a.find(output) == a.end()) == (b.find(output) == b.end()),
                "diagnostics preserve optional output presence");
            if (a.find(output) == a.end()) continue;
            require(a.at(output).size() == b.at(output).size(), "diagnostics preserve output sizes");
            for (size_t frame = 0; frame < a.at(output).size(); ++frame) {
                require(sameFeature(a.at(output)[frame], b.at(output)[frame]),
                    "diagnostics must not affect original outputs");
            }
        }
        require(b.at(4).size() == 3 && b.at(5).size() == 3, "one diagnostic record per frame");
        require(b.at(4)[0].values.size() == 24 && b.at(5)[0].values.size() == 36,
            "diagnostic dimensions");
        for (float value : b.at(4)[1].values) require(value == 0.f, "silent scoring inputs");
        for (float value : b.at(5)[1].values) require(value == 0.f, "silent raw scores");
        require(b.at(6).size() == 3 && b.at(6)[0].values.size() == 84,
            "octave-specific diagnostic dimensions");
        for (float value : b.at(6)[1].values) require(value == 0.f, "silent note activations");

        for (bool nnlsEnabled : {false, true}) {
            Probe notes(0.f, true, nnlsEnabled);
            const auto descriptors = notes.getOutputDescriptors();
            require(descriptors[6].binNames.front() == "A0" && descriptors[6].binNames.back() == "Ab7",
                "note diagnostic octave labels");
            notes.frame();
            const auto features = notes.getRemainingFeatures();
            const auto &activations = features.at(6)[0].values;
            vector<float> expectedChroma(24, 0.f);
            for (int note = 0; note < 84; ++note) {
                require(std::isfinite(activations[note]) && activations[note] >= 0.f,
                    "finite nonnegative note activations");
                expectedChroma[note % 12] += activations[note] * basswindow[note];
                expectedChroma[12 + note % 12] += activations[note] * treblewindow[note];
            }
            require(expectedChroma == features.at(4)[0].values,
                "octave-specific activations exactly reconstruct scoring chroma");
        }

        for (float gate : {1.f, 2.f}) {
            const float alpha = 0.75f;
            Probe candidate(gate, true, true);
            require(candidate.names() == disabled.names() && candidate.base() == disabled.base(),
                "candidate must not alter states or baseline templates");
            for (size_t state = 0; state < candidate.names().size(); ++state) {
                const int group = candidate.groups()[state];
                if (candidate.eligible()[state]) {
                    require(group >= 0 && group < 24, "only major/minor candidates can change");
                    const int root = group % 12;
                    const int third = (root + (group < 12 ? 4 : 3)) % 12;
                    double norm = 0;
                    for (int bin = 0; bin < 24; ++bin) {
                        const double value = candidate.weak()[24 * state + bin];
                        norm += value * value / 24;
                    }
                    require(std::abs(norm - 1.0) < 1e-6, "weak voicing remains RMS normalized");
                    for (int band = 0; band < 2; ++band) {
                        const int index = 24 * state + band * 12;
                        const double oldRatio = candidate.base()[index + third] /
                            candidate.base()[index + root];
                        const double newRatio = candidate.weak()[index + third] /
                            candidate.weak()[index + root];
                        require(std::abs(newRatio - alpha * oldRatio) < 1e-6,
                            "scale third bass and treble together");
                    }
                } else {
                    for (int bin = 0; bin < 24; ++bin) {
                        require(candidate.weak()[24 * state + bin] == candidate.base()[24 * state + bin],
                            "power, extension and N templates remain unchanged");
                    }
                }
            }
            candidate.frame();
            const auto features = candidate.getRemainingFeatures();
            const auto &chroma = features.at(4)[0].values;
            vector<float> expected(36, 0.f);
            for (size_t state = 0; state < candidate.names().size(); ++state) {
                const int group = candidate.groups()[state];
                if (group < 0) continue;
                double score = 0, weakScore = 0;
                for (int bin = 0; bin < 24; ++bin) {
                    score += candidate.base()[24 * state + bin] * chroma[bin];
                    if (candidate.eligible()[state] && candidate.permits(group, features.at(6)[0].values)) weakScore += candidate.weak()[24 * state + bin] * chroma[bin];
                }
                expected[group] = std::max(expected[group], static_cast<float>(std::max(score, weakScore)));
            }
            require(features.at(5)[0].values == expected, "diagnostics expose the actual scoring calculation");
        }
        for (float gate : {1.f, 2.f}) {
            Probe candidate(gate, false, true);
            vector<float> notes(84, 0.f);
            // A3 root (MIDI57), E4 fifth, C#4 major third.
            const int root = 36;
            notes[root] = 2.f; notes[root + 7] = 2.f;
            notes[root + 4] = gate == 1.f ? .21f : .41f;
            require(candidate.permits(0, notes), "above selected ratio");
            notes[root + 4] = gate == 1.f ? .19f : .39f;
            require(!candidate.permits(0, notes), "below selected ratio");
            notes[root] = 10.f; notes[root + 7] = 10.f;
            notes[root + 4] = gate;
            require(candidate.permits(0, notes), "ratio threshold is inclusive");
            notes[root] = 2.f; notes[root + 7] = 2.f;
            notes[root + 4] = 0.f; notes[root + 16] = 1.f;
            require(!candidate.permits(0, notes), "open third alone cannot enable close-position gate");
            notes[root + 3] = 1.f;
            require(candidate.permits(12, notes), "minor interval distinct from major");
            notes[root + 7] = 0.f;
            require(!candidate.permits(12, notes), "missing fifth withholds boost");
            notes[root + 7] = std::numeric_limits<float>::infinity();
            require(!candidate.permits(12, notes), "nonfinite fifth withholds boost");
            notes[root + 7] = 2.f; notes[root + 3] = -1.f;
            require(!candidate.permits(12, notes), "negative third withholds boost");
            notes[root + 3] = std::numeric_limits<float>::quiet_NaN();
            require(!candidate.permits(12, notes), "nonfinite third withholds boost");
            notes.assign(84, 0.f);
            notes[root] = 2.f; notes[root + 12] = 2.f;
            notes[root + 7] = 2.f; notes[root + 4] = 1.f;
            require(candidate.permits(0, notes), "root tie chooses lower octave");
            notes[root + 12] = 3.f;
            require(!candidate.permits(0, notes), "strongest root octave controls required fifth and third");
            notes[72 + 7] = 4.f;
            require(!candidate.permits(7, notes), "top-octave required notes out of range");
            require(!candidate.permits(-1, notes) && !candidate.permits(24, notes), "only triad groups can enable gate");
            Probe direct(gate, false, false);
            require(!direct.permits(0, notes), "direct mapping never enables NNLS gate");
        }
        Probe invalid(0.01f, false);
        require(invalid.getParameter("thirdgate") == 0.f && invalid.weak().empty(),
            "unsupported near-zero weight must not enable a candidate");
        std::cout << "Third-evidence experiment structural and gate tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
