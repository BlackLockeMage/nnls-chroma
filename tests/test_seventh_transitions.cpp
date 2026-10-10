#include "../Chordino.h"
#include "../chromamethods.h"
#include "../seventhtransitions.h"
#include "../viterbi.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
static bool edge(const std::string &a, const std::string &b) {
    return SeventhTransitions::eligible(SeventhTransitions::parse(a), SeventhTransitions::parse(b));
}
static std::vector<int> decode(const std::vector<std::vector<double>> &trans) {
    const int count = trans.size();
    std::vector<std::vector<double>> obs(7, std::vector<double>(count, 1e-10));
    for (auto &row : obs) row[0] = 1.;
    obs[3][1] = std::exp(8.);
    for (auto &row : obs) {
        double sum = 0.; for (double value : row) sum += value;
        for (double &value : row) value /= sum;
    }
    std::vector<double> delta(count * obs.size()), scale;
    return ViterbiPath(std::vector<double>(count, 1. / count), trans, obs, delta.data(), &scale);
}
class Probe : public Chordino {
public:
    Probe() : Chordino(44100) {
        setParameter("diagnostics", 1.f);
        setParameter("useNNLS", 0.f);
        require(initialise(1, 2048, 16384), "API initialisation");
        getOutputDescriptors();
    }
    const std::vector<std::string> &names() const { return m_chordnames; }
    void frames() {
        for (int i = 0; i < 12; ++i) {
            Feature spectrum;
            spectrum.hasTimestamp = true;
            spectrum.timestamp = Vamp::RealTime::frame2RealTime(i * 2048, 44100);
            spectrum.values.assign(nNote, 0.f);
            if (i != 6) {
                spectrum.values[3 + 36 * 3] = 100.f;
                spectrum.values[3 + 40 * 3] = 60.f;
                spectrum.values[3 + 43 * 3] = 80.f;
                if (i == 4) spectrum.values[3 + 46 * 3] = 70.f;
            }
            m_logSpectrum.push_back(spectrum);
        }
    }
};
static bool same(const Vamp::Plugin::Feature &a, const Vamp::Plugin::Feature &b) {
    return a.hasTimestamp == b.hasTimestamp && a.timestamp == b.timestamp &&
        a.hasDuration == b.hasDuration && a.duration == b.duration && a.values == b.values && a.label == b.label;
}
int main() {
    try {
        for (const auto &pair : std::vector<std::pair<std::string, std::string>>{
            {"C", "C7"}, {"C", "Cmaj7"}, {"Cm", "Cm7"}, {"C/E", "C7/E"},
            {"Bb/F", "A#7/E#"}, {"C/3", "C:7/3"}, {"C:min/b3", "C:min7/Eb"}, {"Cb", "Bmaj7"}}) {
            require(edge(pair.first, pair.second) && edge(pair.second, pair.first), "eligible edge and symmetry");
        }
        for (const auto &pair : std::vector<std::pair<std::string, std::string>>{
            {"C", "G7"}, {"C", "C7/E"}, {"C/E", "C7"}, {"C", "Cm7"}, {"Cm", "C7"},
            {"C7", "Cmaj7"}, {"C", "C/E"}, {"C", "C"}, {"N", "C7"}, {"C5", "C7"},
            {"Csus4", "C7sus4"}, {"C6", "C7"}, {"Cdim", "Cdim7"}, {"Cm", "Cmmaj7"},
            {"Cadd9", "C7"}, {"C9", "C7"}, {"C7/", "C"}, {"C7/E/3", "C/E"}, {"C/8", "C:7"}})
            require(!edge(pair.first, pair.second) && !edge(pair.second, pair.first), "ineligible edge and symmetry");

        Probe baseline, explicitOff, treatment;
        require(baseline.getPluginVersion() == 10 && baseline.getParameter("seventhtransition") == 0.f, "API version/default");
        explicitOff.setParameter("seventhtransition", 0.f);
        treatment.setParameter("seventhtransition", .75f);
        require(treatment.getParameter("seventhtransition") == .75f, "API parameter roundtrip");
        const auto names = baseline.names();
        const int count = names.size();
        const double stay = .99, other = (1 - stay) / (count - 1);
        std::vector<std::vector<double>> original;
        for (int i = 0; i < count; ++i) {
            std::vector<double> row(count, other); row[i] = stay; original.push_back(row);
        }
        require(SeventhTransitions::matrix(names, 0.) == original, "off matrix exactly baseline");
        size_t eligibleCount = 0;
        for (double discount : {.25, .5, .75, 1.}) {
            const auto trans = SeventhTransitions::matrix(names, discount);
            size_t changed = 0;
            for (int i = 0; i < count; ++i) for (int j = 0; j < count; ++j) {
                require(trans[i][j] == trans[j][i], "all transition potentials symmetric");
                require(std::isfinite(trans[i][j]) && trans[i][j] > 0., "finite positive potential");
                if (edge(names[i], names[j])) {
                    ++changed;
                    require(std::abs(std::log(stay / trans[i][j]) - (1 - discount) * std::log(stay / other)) < 1e-12,
                            "eligible log-cost discount");
                } else require(trans[i][j] == original[i][j], "every stay/ineligible/N entry bit exact");
            }
            eligibleCount = changed;
            require(changed > 0, "eligible native edges present");
        }
        require(eligibleCount == 168, "exact built-in eligible directed edge count");
        std::vector<float> harteWeights;
        std::vector<std::vector<int>> harteNotes;
        const auto harteNames = chordDictionary(&harteWeights, &harteNotes, .1f, 1.f, true);
        require(SeventhTransitions::matrix(harteNames, .75) == SeventhTransitions::matrix(names, .75),
                "all native Harte labels preserve eligibility by state index");
        for (double invalid : {-1., 2., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
            require(SeventhTransitions::matrix(names, invalid) == original, "invalid discount returns baseline");
        baseline.frames(); explicitOff.frames(); treatment.frames();
        const auto a = baseline.getRemainingFeatures(), b = explicitOff.getRemainingFeatures(), c = treatment.getRemainingFeatures();
        require(a.size() == b.size(), "default/off output presence parity");
        for (const auto &output : a) {
            require(output.second.size() == b.at(output.first).size(), "default/off row parity");
            for (size_t i = 0; i < output.second.size(); ++i)
                require(same(output.second[i], b.at(output.first)[i]), "default/off feature parity");
        }
        for (int output : {4, 5, 6, 7, 9}) {
            require(a.at(output).size() == c.at(output).size(), "treatment observation count unchanged");
            for (size_t i = 0; i < a.at(output).size(); ++i)
                require(same(a.at(output)[i], c.at(output)[i]), "treatment note/chroma/raw/emission/dictionary unchanged");
        }
        for (size_t i = 0; i < a.at(8).size(); ++i)
            require(a.at(8)[i].values[1] == c.at(8)[i].values[1], "native local winner unchanged");

        const std::vector<std::string> simple = {"C", "C7", "G", "N"};
        const auto off = decode(SeventhTransitions::matrix(simple, 0.));
        const auto on = decode(SeventhTransitions::matrix(simple, .5));
        require(std::all_of(off.begin(), off.end(), [](int state) { return state == 0; }), "original decoder suppresses short excursion");
        require(on == std::vector<int>({0, 0, 0, 1, 0, 0, 0}), "discounted decoder admits eligible excursion");
        require(decode(SeventhTransitions::matrix({"C", "C7/E", "G", "N"}, .75)) == off, "cross-bass decoder unchanged");
        // Same-labelled-bass duplicates still permit an indirect template bridge.
        const auto bridge = SeventhTransitions::matrix({"C", "C", "C7", "N"}, .75);
        require(bridge[0][1] == (1 - .99) / 3, "duplicate direct edge unchanged");
        require(bridge[0][2] * bridge[2][1] > bridge[0][1] * .99, "documented indirect duplicate bridge exists");
        std::cout << "Seventh transition tests passed; " << eligibleCount << " eligible directed edges across " << count << " states.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
