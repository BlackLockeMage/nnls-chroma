// Experimental same-root, same-labelled-bass triad/seventh transition potentials.
#ifndef DECHORDER_SEVENTH_TRANSITIONS_H
#define DECHORDER_SEVENTH_TRANSITIONS_H

#include <cmath>
#include <string>
#include <vector>

namespace SeventhTransitions {

enum Quality { Unknown, Major, Minor, Dominant7, Major7, Minor7 };
struct Chord {
    int root, bass;
    Quality quality;
    Chord() : root(-1), bass(-1), quality(Unknown) {}
};

inline int pitch(const std::string &text, size_t &position) {
    if (position >= text.size()) return -1;
    const std::string letters = "CDEFGAB";
    const int notes[] = {0, 2, 4, 5, 7, 9, 11};
    const size_t letter = letters.find(text[position]);
    if (letter == std::string::npos) return -1;
    int result = notes[letter];
    ++position;
    while (position < text.size() && (text[position] == '#' || text[position] == 'b'))
        result += text[position++] == '#' ? 1 : -1;
    return (result % 12 + 12) % 12;
}

inline Chord parse(const std::string &label) {
    Chord chord;
    size_t position = 0;
    chord.root = pitch(label, position);
    if (chord.root < 0) return Chord();
    const size_t slash = label.find('/', position);
    const std::string suffix = label.substr(position, slash - position);
    if (suffix.empty() || suffix == ":maj") chord.quality = Major;
    else if (suffix == "m" || suffix == ":min") chord.quality = Minor;
    else if (suffix == "7" || suffix == ":7") chord.quality = Dominant7;
    else if (suffix == "maj7" || suffix == ":maj7") chord.quality = Major7;
    else if (suffix == "m7" || suffix == ":min7") chord.quality = Minor7;
    else return Chord();
    chord.bass = chord.root;
    if (slash == std::string::npos) return chord;
    const std::string bass = label.substr(slash + 1);
    position = 0;
    chord.bass = pitch(bass, position);
    if (chord.bass >= 0) return position == bass.size() ? chord : Chord();
    // Native Harte output uses root-relative scale degrees, including C/3.
    int accidental = 0;
    while (position < bass.size() && (bass[position] == 'b' || bass[position] == '#'))
        accidental += bass[position++] == '#' ? 1 : -1;
    const int degrees[] = {0, 2, 4, 5, 7, 9, 11};
    if (position + 1 != bass.size() || bass[position] < '1' || bass[position] > '7') return Chord();
    chord.bass = ((chord.root + degrees[bass[position] - '1'] + accidental) % 12 + 12) % 12;
    return chord;
}

inline bool eligible(const Chord &a, const Chord &b) {
    if (a.quality == Unknown || b.quality == Unknown || a.root != b.root || a.bass != b.bass) return false;
    return (a.quality == Major && (b.quality == Dominant7 || b.quality == Major7)) ||
        (b.quality == Major && (a.quality == Dominant7 || a.quality == Major7)) ||
        (a.quality == Minor && b.quality == Minor7) || (b.quality == Minor && a.quality == Minor7);
}

inline double validDiscount(double discount) {
    return std::isfinite(discount) && discount >= 0. && discount <= 1. ? discount : 0.;
}

inline std::vector<std::vector<double>> matrix(const std::vector<std::string> &names, double discount) {
    const int count = int(names.size());
    const double stay = .99;
    const double other = count > 1 ? (1 - stay) / (count - 1) : 0.;
    std::vector<std::vector<double>> trans;
    for (int i = 0; i < count; ++i) {
        std::vector<double> row(count, other);
        row[i] = stay;
        trans.push_back(row);
    }
    discount = validDiscount(discount);
    if (discount == 0. || count < 2) return trans; // No log/exp in baseline path.
    const double potential = stay * std::exp(-(1 - discount) * std::log(stay / other));
    std::vector<Chord> chords;
    for (const std::string &name : names) chords.push_back(parse(name));
    for (int i = 0; i < count; ++i) for (int j = i + 1; j < count; ++j)
        if (eligible(chords[i], chords[j])) trans[i][j] = trans[j][i] = potential;
    // Deliberately do not row-normalize: that would alter ineligible/stay ratios.
    // Viterbi uses positive potentials and one common scaling per frame.
    return trans;
}
} // namespace SeventhTransitions
#endif
