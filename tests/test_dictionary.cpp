// Standalone dictionary regression tests; build with chromamethods.cpp, C++17.
#include "../chromamethods.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct Dictionary {
    std::vector<float> values;
    std::vector<std::vector<int> > notes;
    std::vector<std::string> names;
    Dictionary(bool extended = false, bool harte = false) {
        names = chordDictionary(&values, &notes, 0.1f, harte ? 1.f : 0.f, extended);
        assert(values.size() == names.size() * 24);
        assert(notes.size() == names.size());
        for (float value : values) assert(std::isfinite(value) && value >= 0);
    }
};

void setPluginPath(const std::string &value) {
#ifdef _WIN32
    _putenv_s("VAMP_PATH", value.c_str());
#else
    setenv("VAMP_PATH", value.c_str(), 1);
#endif
}

int main() {
    namespace fs = std::filesystem;
    const char *oldPath = std::getenv("VAMP_PATH");
    const std::string originalPath = oldPath ? oldPath : "";
    const fs::path tempParent = fs::temp_directory_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path temp = tempParent / ("chordino-dictionary-test-" + std::to_string(stamp));
    assert(fs::create_directory(temp));
    setPluginPath(temp.string());
    const auto write = [&](const std::string &text) {
        std::ofstream file(temp / "chord.dict");
        file << text;
        assert(file.good());
    };
    const std::string major = "1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,0,0,0,0";
    const Dictionary baseline;
    assert(baseline.names.size() == 217);
    assert(baseline.names[3] == "C");
    assert(baseline.names.back() == "N");
    {
        std::vector<float> values;
        std::vector<std::vector<int> > notes;
        const auto names = chordDictionary(&values, &notes, 0.1f, 0.f);
        assert(names == baseline.names && values == baseline.values && notes == baseline.notes);
    }
    const Dictionary extended(true);
    assert(extended.names.size() == 265);
    const char *labels[] = {"Csus2", "Csus4", "Cadd9", "C5"};
    const std::vector<std::vector<int> > intervals = {{0,2,7}, {0,5,7}, {0,2,4,7}, {0,7}};
    for (size_t type = 0; type < 4; ++type) {
        assert(extended.names[(18 + type) * 12 + 3] == labels[type]);
        for (size_t root = 0; root < 12; ++root) {
            const size_t offset = ((18 + type) * 12 + root) * 24;
            for (size_t pitch = 0; pitch < 12; ++pitch) {
                bool expected = false;
                for (int interval : intervals[type]) {
                    if ((root + interval) % 12 == pitch) expected = true;
                }
                assert((extended.values[offset + 12 + pitch] > 0) == expected);
            }
            double norm = 0;
            for (size_t pitch = 0; pitch < 24; ++pitch) {
                norm += std::pow(extended.values[offset + pitch], 2) / 24;
            }
            assert(std::abs(norm - 1.0) < 1e-6);
        }
    }
    const Dictionary harte(true, true);
    assert(harte.names[216 + 3] == "C:sus2");
    assert(harte.names[228 + 3] == "C:sus4");
    assert(harte.names[240 + 3] == "C:maj(9)");
    assert(harte.names[252 + 3] == "C:(1,5)");

    // Standard and Harte labels, empty major suffixes, comments, tabs, CRLF,
    // commas, semicolons, and spaces all retain their meaning.
    write("  # comment\r\n\t\r\n =" + major + "\r\nm#5 =" + major + "\n");
    const Dictionary custom;
    assert(custom.names.size() == 25);
    assert(custom.names[3] == "C");
    assert(custom.names[15] == "Cm#5");
    std::string spaced = major;
    for (size_t i = 0; i < spaced.size(); ++i) if (spaced[i] == ',') spaced[i] = i % 2 ? ';' : '\t';
    write(":sus4 = " + spaced + "\n");
    const Dictionary mixed;
    assert(mixed.names.size() == 13 && mixed.names[3] == "C:sus4");

    const std::vector<std::string> invalid = {
        "", " # only comments\n\n", "m=1,0", "m=" + major + ",0",
        "m=nan," + major.substr(2), "m=inf," + major.substr(2),
        "m=-1," + major.substr(2), "m=1e300," + major.substr(2),
        "m=oops," + major.substr(2), "m=" + std::string(47, '0'),
        "m=0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0",
        "m=0.5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0",
        "m " + major, "m==" + major, "=" + major + "\nbad=1,0"
    };
    for (const std::string &text : invalid) {
        write(text);
        const Dictionary rejected;
        assert(rejected.names == baseline.names && rejected.values == baseline.values);
        const Dictionary rejectedExtended(true);
        assert(rejectedExtended.names == extended.names && rejectedExtended.values == extended.values);
    }
    write("m=1,0,0,0,0,0,0,0,0,0,0,0,3e38,0,0,0,3e38,0,0,3e38,0,0,0,0");
    const Dictionary large;
    assert(large.names.size() == 13 && large.values[12] > 0);
    // Only files created in our unique temporary directory are removed.
    fs::remove(temp / "chord.dict");
    assert(temp.parent_path() == tempParent);
    assert(fs::remove(temp));
    setPluginPath(originalPath);
    std::cout << "Dictionary tests passed: baseline, extensions, transposition, aliases, "
                 "custom parsing, malformed/empty fallback, finite normalization.\n";
}
