// Native API regression probe for Dechorder's Chordino fork.
#include "Chordino.h"
#include "chromamethods.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

class DecoderProbe : public Chordino {
public:
    DecoderProbe() : Chordino(44100) {
        require(initialise(1, 2048, 16384), "initialisation");
        getOutputDescriptors();
        setParameter("useNNLS", 0);
        // Two distinguishable spectral templates with the same display name
        // exercise label coalescing without depending on musical voicing bias.
        m_chordnames = {"same-label", "same-label", "N"};
        m_chorddict.assign(3 * 24, 0);
        m_chorddict[12] = 1;
        m_chorddict[24 + 19] = 1;
        m_chordnotes = {{60}, {67}, {}};
    }

    void frame(int pitchClass) {
        Feature spectrum;
        spectrum.hasTimestamp = true;
        spectrum.timestamp = Vamp::RealTime::frame2RealTime(
            m_logSpectrum.size() * 2048, 44100);
        spectrum.values.assign(nNote, 0);
        if (pitchClass >= 0) spectrum.values[3 + (36 + pitchClass) * 3] = 200;
        m_logSpectrum.push_back(spectrum);
    }

    int chords() const { return m_outputChords; }
    int notes() const { return m_outputChordnotes; }
};

class VocabularyProbe : public Chordino {
public:
    VocabularyProbe(bool extended) : Chordino(44100) {
        require(getParameter("extendedchords") == 1.f, "extended vocabulary defaults on");
        setParameter("extendedchords", extended ? 1.f : 0.f);
        setParameter("rollon", 1.f);
        require(getParameter("rollon") == 1.f, "base parameter delegation");
        require(initialise(1, 2048, 16384), "vocabulary initialisation");
    }
    bool contains(const std::string &label) const {
        for (const auto &name : m_chordnames) if (name == label) return true;
        return false;
    }
};

int main() {
    try {
        {
            VocabularyProbe extended(true);
            VocabularyProbe legacy(false);
            require(extended.contains("Csus4") && extended.contains("Cadd9") &&
                extended.contains("C5"), "extended templates passed to decoder");
            require(!legacy.contains("Csus4") && legacy.contains("C"),
                "legacy vocabulary remains selectable");
        }
        {
            DecoderProbe plugin;
            require(plugin.getPluginVersion() == 10, "transition experiment version");
            require(plugin.getRemainingFeatures().empty(), "empty input output");
        }
        {
            DecoderProbe plugin;
            plugin.frame(-1);
            const auto features = plugin.getRemainingFeatures();
            require(features.at(plugin.chords()).size() == 1, "one silent label");
            require(features.at(plugin.chords())[0].label == "N", "silent label N");
            require(features.find(plugin.notes()) == features.end(), "silent notes absent");
        }
        {
            DecoderProbe plugin;
            plugin.frame(0);
            const auto features = plugin.getRemainingFeatures();
            const auto &chords = features.at(plugin.chords());
            const auto &notes = features.at(plugin.notes());
            require(chords.size() == 1 && chords[0].label == "same-label",
                "first frame chosen from audio; no artificial terminal N");
            require(notes.size() == 1 && notes[0].values[0] == 60,
                "first frame notes initialised");
            require(notes[0].timestamp == Vamp::RealTime::zeroTime, "first note time");
            require(notes[0].duration == Vamp::RealTime::frame2RealTime(2048, 44100),
                "single frame duration is one hop");
        }
        {
            DecoderProbe plugin;
            for (int i = 0; i < 10; ++i) plugin.frame(0);
            for (int i = 0; i < 10; ++i) plugin.frame(7);
            const auto features = plugin.getRemainingFeatures();
            const auto &chords = features.at(plugin.chords());
            const auto &notes = features.at(plugin.notes());
            require(chords.size() == 1, "identical chord names coalesced");
            require(notes.size() == 2 && notes[0].values[0] == 60 && notes[1].values[0] == 67,
                "internal voicing change preserved in notes");
            require(notes[0].timestamp + notes[0].duration == notes[1].timestamp,
                "voicing durations meet");
            require(notes[1].timestamp + notes[1].duration ==
                Vamp::RealTime::frame2RealTime(20 * 2048, 44100), "final note duration");
        }
        {
            DecoderProbe plugin;
            for (int i = 0; i < 10; ++i) plugin.frame(0);
            for (int i = 0; i < 10; ++i) plugin.frame(-1);
            for (int i = 0; i < 10; ++i) plugin.frame(7);
            const auto features = plugin.getRemainingFeatures();
            const auto &chords = features.at(plugin.chords());
            require(chords.size() == 3 && chords[1].label == "N", "silence interrupts chord");
            require(chords[1].timestamp == Vamp::RealTime::frame2RealTime(10 * 2048, 44100),
                "silence starts at zero evidence");
            require(chords[2].timestamp == Vamp::RealTime::frame2RealTime(20 * 2048, 44100),
                "future chord starts after silence");
        }
        std::cout << "All six native decoder API cases passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
