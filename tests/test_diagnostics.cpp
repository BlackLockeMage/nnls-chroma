#include "../Chordino.h"
#include "../chromamethods.h"
#include "../viterbi.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}

class Probe : public Chordino {
public:
    Probe(bool diagnostics, bool nnls, bool extended) : Chordino(44100) {
        require(getParameter("diagnostics") == 0.f, "diagnostics default off");
        setParameter("diagnostics", diagnostics ? 1.f : 0.f);
        setParameter("useNNLS", nnls ? 1.f : 0.f);
        setParameter("extendedchords", extended ? 1.f : 0.f);
        require(initialise(1, 2048, 16384), "initialise");
        getOutputDescriptors();
    }
    void frame(int root) {
        Feature spectrum;
        spectrum.hasTimestamp = true;
        spectrum.timestamp = Vamp::RealTime::frame2RealTime(m_logSpectrum.size() * 2048, 44100);
        spectrum.values.assign(nNote, 0.f);
        if (root >= 0) {
            spectrum.values[3 + (36 + root) * 3] = 100.f;
            spectrum.values[3 + (40 + root) * 3] = 60.f;
            spectrum.values[3 + (43 + root) * 3] = 80.f;
        }
        m_logSpectrum.push_back(spectrum);
    }
    const vector<string> &labels() const { return m_chordnames; }
    const vector<float> &weights() const { return m_chorddict; }
};

static bool same(const Vamp::Plugin::Feature &a, const Vamp::Plugin::Feature &b) {
    return a.hasTimestamp == b.hasTimestamp && a.timestamp == b.timestamp &&
        a.hasDuration == b.hasDuration && a.duration == b.duration &&
        a.values == b.values && a.label == b.label;
}

int main() {
    try {
        for (bool nnls : {false, true}) for (bool extended : {false, true}) {
            Probe plain(false, nnls, extended), observed(true, nnls, extended);
            require(observed.getRemainingFeatures().empty(), "empty audio emits no records");
            const int roots[] = {0, 0, 0, -1, 7, 7, 0};
            const int frames = sizeof(roots) / sizeof(roots[0]);
            for (int root : roots) { plain.frame(root); observed.frame(root); }
            const auto original = plain.getRemainingFeatures();
            const auto output = observed.getRemainingFeatures();
            for (const auto &entry : original) require(entry.first < 4, "off has no diagnostic output");
            for (int index = 0; index < 4; ++index) {
                require((original.find(index) == original.end()) == (output.find(index) == output.end()),
                    "original optional output presence");
                if (original.find(index) == original.end()) continue;
                require(original.at(index).size() == output.at(index).size(), "original output count");
                for (size_t row = 0; row < original.at(index).size(); ++row)
                    require(same(original.at(index)[row], output.at(index)[row]), "observational parity of all four original outputs");
            }
            const auto descriptors = observed.getOutputDescriptors();
            require(descriptors.size() == 10, "six appended diagnostics");
            require(descriptors[4].binNames.front() == "A0" && descriptors[4].binNames.back() == "Ab7", "MIDI21..104 note order");
            require(descriptors[5].binNames.front() == "bass A" && descriptors[5].binNames[12] == "treble A", "bass/treble ordering");
            const int states = observed.labels().size();
            require(descriptors[6].binCount == size_t(states) && descriptors[7].binCount == size_t(states), "state dimensions after initialisation");
            require(descriptors[6].binNames == observed.labels() && descriptors[7].binNames == observed.labels(), "state labels aligned including duplicate names");
            const auto &dictionary = output.at(9);
            require(dictionary.size() == size_t(states), "one dictionary row per state");
            for (int state = 0; state < states; ++state) {
                const auto &row = dictionary[state];
                require(row.timestamp == Vamp::RealTime::zeroTime && row.values.size() == 25, "dictionary shape and timestamp");
                require(row.values[0] == float(state) && row.label == observed.labels()[state], "dictionary index and label");
                for (int bin = 0; bin < 24; ++bin)
                    require(row.values[1 + bin] == observed.weights()[24 * state + bin], "exact expanded dictionary weight");
            }
            for (int outputIndex = 4; outputIndex < 9; ++outputIndex)
                require(output.at(outputIndex).size() == size_t(frames), "one diagnostic row per audio frame");
            vector<vector<double>> emissions;
            for (int frame = 0; frame < frames; ++frame) {
                const auto &notes = output.at(4)[frame];
                const auto &chroma = output.at(5)[frame];
                require(notes.values.size() == 84 && chroma.values.size() == 24, "note/chroma dimensions");
                vector<float> reconstructed(24, 0.f);
                for (int note = 0; note < 84; ++note) {
                    reconstructed[note % 12] += notes.values[note] * basswindow[note];
                    reconstructed[12 + note % 12] += notes.values[note] * treblewindow[note];
                }
                require(reconstructed == chroma.values, "84 activations reconstruct exact scoring chroma");
                vector<double> current;
                double total = 0;
                for (int state = 0; state < states; ++state) {
                    double raw = 0;
                    for (int bin = 0; bin < 24; ++bin)
                        raw += observed.weights()[24 * state + bin] * chroma.values[bin];
                    require(float(raw) == output.at(6)[frame].values[state], "raw score before N factor and clipping");
                    if (state == states - 1) raw *= .7;
                    const double value = pow(1.3, std::max(0., std::min(200., raw)));
                    current.push_back(value); total += value;
                }
                if (roots[frame] < 0) { current.assign(states, 0.); current.back() = 1.; }
                else for (double &value : current) value /= total;
                for (int state = 0; state < states; ++state)
                    require(float(current[state]) == output.at(7)[frame].values[state], "emissions after silence gate and normalization");
                const int winner = std::max_element(current.begin(), current.end()) - current.begin();
                require(output.at(8)[frame].values[1] == float(winner), "local winner uses native emission semantics");
                for (int index = 5; index < 9; ++index)
                    require(output.at(index)[frame].timestamp == notes.timestamp, "all diagnostic timestamps align");
                emissions.push_back(current);
            }
            vector<double> initial(states, 1. / states), scales;
            vector<vector<double>> transitions(states, vector<double>(states, (1. - .99) / (states - 1)));
            for (int state = 0; state < states; ++state) transitions[state][state] = .99;
            vector<double> delta(frames * states);
            const auto path = ViterbiPath(initial, transitions, emissions, delta.data(), &scales);
            for (int frame = 0; frame < frames; ++frame)
                require(output.at(8)[frame].values[0] == float(path[frame]), "exported native path matches full-precision reconstructed observations");
            std::cout << "Diagnostic case passed: NNLS=" << nnls << ", extended=" << extended
                      << ", state count=" << states << ", maximum state index=" << states - 1 << "\n";
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
