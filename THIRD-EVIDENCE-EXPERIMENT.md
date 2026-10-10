# Not validated: close-position third evidence

**Do not merge or adopt this experiment.** The selected candidate failed the
preregistered improvement requirement on reserved validation. This isolated
branch starts at production commit `7c5cd2e` and preserves the tested code for
reproducibility. It is not installed or merged into production. The linked
application experiment is
[Dechorder PR #1](https://github.com/BlackLockeMage/Dechorder/pull/1).

The previous unrestricted weak-third experiment was rejected because it promoted
real power chords to major/minor. This experiment allows that score alternative
only when NNLS estimates a third in a particular close voicing. It is a bounded
voicing heuristic, not a general classifier of played fundamentals versus
overtones. A high third and an overtone can be acoustically ambiguous.

## Frozen treatment

`thirdgate` is the only treatment parameter. Mode 0 disables the experiment;
mode 1 requires a ratio of at least 0.05; mode 2 requires at least 0.10. Other
values disable it. `diagnostics` separately enables feature outputs and does
not alter scoring. The old ungated `weakthird` parameter is not exposed.

For each candidate major/minor root pitch class, select the octave-specific
NNLS root coefficient with greatest amplitude among its seven octaves. A tie
selects the lowest octave. Relative to that note, inspect the major third at
+4 semitones or minor third at +3, and fifth at +7. The ratio is
`third / (root + fifth)`. All required indices must be in the 84-note range;
root and fifth must be finite and strictly positive; third must be finite and
nonnegative. Invalid root coefficients, zero denominators, unsupported states,
and `useNNLS != 1` withhold the alternative. No key, fixture label, supplied MIDI
register, or global root assumption chooses these notes.

Only an eligible existing plain major/minor state whose gate passes can use the
larger of its original dot product and a fixed 0.75-third-weight alternative.
The alternative scales both bass and treble third coefficients after dictionary
expansion and restores RMS normalization. It is prepared once at initialization
or parameter change; gates are calculated once per root/quality per frame.
Power, extensions, N, transitions, frontend, states and original templates are
unchanged. Higher plain-triad scores can still affect their competition with
other chords, which must be evaluated. Open/high/missing-fundamental voicings
that fail the gate use the original score; that fallback does not establish
that the original chord is correct.

## Diagnostic evidence used to choose the finite family

Before enabled-candidate decoding, the previously evaluated development C5, E5
and quiet-third Cm files were inspected with diagnostic-only note outputs.
Their strongest root coefficients were C3/E3. C5 and E5 had zero NNLS activation
at the selected close major and minor thirds in all 76 interior frames, in both
profiles. Weak Cm had a median close-third ratio of 0.179 (Default) and 0.192
(Pop). False major evidence instead appeared in upper octaves. These limited
development observations motivate the rule; they do not validate it.

Chordino descriptor version is 8; NNLS Chroma and Tuning stay at version 6.
With `diagnostics=1`, appended streams are:

- `decoderchroma`: exact 24 scoring inputs, bass A..Ab then treble A..Ab.
- `triadscores`: best effective raw score across plain voicings, major A..Ab,
  minor A..Ab, then power A..Ab. Values precede clipping/exponentiation and are
  not probabilities.
- `noteactivations`: the 84 octave-specific note coefficients, MIDI 21..104
  (A0..G#7), before chroma window weighting and octave folding. Under the direct
  mapping mode these are semitone sums; that mode never enables the gate.

The production RDF does not declare the new outputs. Sonic Annotator transforms
must identify them inline, e.g. `vamp:output [ vamp:identifier "noteactivations" ]`,
enable `diagnostics`, and omit the production plugin-version pin or set it to 8.
The app repository's `benchmarks.third_evidence_diagnostics` extracts existing
fingerprinted development WAVs, records hashes, and independently reconstructs
the 24 chroma inputs from the 84 note coefficients. It refuses validation IDs.

## Validation and status

Four native suites pass: decoder, dictionary, frontend and third-evidence
structural/gate checks. They cover exact 84-to-24 reconstruction, finite and
zero handling, inclusive thresholds, top-of-range bounds, NNLS-only gating,
root ties, missing fifths, and open-third fallback. They do not establish audio
accuracy. All 110 development disabled-mode raw CSVs (70 challenging and 40
additional-control fixture/profile pairs) match production byte for byte.

Both finite modes passed the development gates: primary exact and harmonic
accuracy rose from 91.67% to 100% for both profiles. Mode 2 (ratio >= 0.10) was
selected under the frozen stricter-threshold tie rule, recorded in
[application commit 3a55dbc](https://github.com/BlackLockeMage/Dechorder/commit/3a55dbc)
before reserved validation was decoded. Mode 1 was not decoded on validation.

The selected mode then failed the required validation improvement. Primary
exact and harmonic accuracy stayed at 91.67% in both Default and Pop, below the
required gain of at least one percentage point. Validation isolated and
accompanied power-control accuracy remained 100% and 97.45%, respectively.
Additional reserved-control accuracy remained 33.33%. Preserving those outputs
does not satisfy the improvement requirement or fix the existing missed chords.

No further tuning or alternate-mode validation was performed. The enabled
candidate was not run against the old regression suite after failing validation;
this experiment makes no claim that an enabled mode preserves every legacy
case. Production remains unchanged. Detailed paired reports and the mechanical
verdict are preserved in the linked application PR.

Candidate DLL SHA256:
`964c643038a894823444cbcde0de813cd9127f172cb27af72965a451b217c3aa`.
The earlier diagnostic-only DLL SHA256 was
`dbf5a373fabf08a8493e2ce6ca283d4a9729facd370ba1d39f306ecea6a340b4`.

The application experiment froze 35 challenging development files, 20
additional matched voicing/control files, two profiles, and two finite candidate
modes. Selected mode 2 was evaluated once on the 35 reserved challenging and
six additional reserved files. In development the additional-suite accuracy
remained 65% for Default and 70% for Pop. All four brighter,
missing-fundamental or octave-spread power examples already falsely labelled
major by production remain unresolved. The gate did not solve those errors.

Use `build-windows.ps1` with separate cache/output directories to build this
branch and its four native suites. Do not replace the installed production DLL.
