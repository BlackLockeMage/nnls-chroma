# Observational diagnostics for seventh-chord investigation

This document records the inherited version-9 diagnostic baseline. The current
version-10 experimental branch adds the default-off treatment described in
SEVENTH-TRANSITIONS.md; claims of unchanged transitions below apply to version 9
and to version 10 only while `seventhtransition` is zero.

This branch starts directly from production master `7c5cd2e` (audio changes
through `3ccf28d`). It adds logging and bumps only Chordino's descriptor to
version 9. NNLS Chroma and Tuning remain version 6. It does not transplant the
failed weak-third experiments, add treatment parameters, or change frontend,
templates, emissions, transitions, decoding or labels. The installed production
plugin and earlier draft PR are untouched.

`diagnostics` is a boolean parameter, default 0. At 1 it enables six appended
outputs; the original outputs remain at indices 0 through 3. All per-frame
diagnostics carry the original frame timestamps and have no duration.

| Output index / identifier | Numeric bins | Meaning |
| --- | --- | --- |
| 4 / `noteactivations` | 84 | Actual octave-specific coefficients, MIDI21..104 (A0..G#7), before window weighting and octave folding. With NNLS disabled, these are the direct semitone sums. |
| 5 / `decoderchroma` | 24 | Actual scoring inputs: bass A,Bb,B,C,C#,D,Eb,E,F,F#,G,Ab, then treble in the same order. Optional display-chroma normalization is not applied. |
| 6 / `staterawscores` | state count | Each state's dot product before the extra N factor, clipping or exponentiation. |
| 7 / `stateemissions` | state count | The actual normalized HMM emissions, including the existing zero-evidence silence gate. |
| 8 / `statepath` | 2 | Native Viterbi state index, then native local maximum-emission state index. Local ties choose the lowest index. |
| 9 / `statedictionary` | 25 | One row per state at timestamp zero: state index then 24 dictionary weights. The exact displayed state name is in the feature label. |

State indices are zero-based and identical across the dictionary, score/emission
bin positions and path values. Repeated display labels retain distinct state
indices and weights. The built-in extended vocabulary has 265 states, indices
0..264; N is state 264. With extended chords disabled it has 217 states,
indices 0..216. Custom dictionaries can change this: read the exported mapping.
Score/emission descriptors expose the exact state labels as bin names after
initialization; before initialization the state count is unspecified.

For Sonic Annotator CSV without end-time options, dictionary rows are
`time,index,24 weights,label`, i.e. 27 fields. Per-state score/emission rows are
`time,state0,...,stateN`; path rows are `time,viterbi,local`. Dictionary weights
already include expansion, normalization and `boostn` for N. The later N score
multiplier of 0.7 is separate. The unchanged scorer then clamps to [0,200],
raises 1.3 to that score, normalizes across states, and replaces zero-evidence
frames with a one-hot N emission. The raw score alone is not the HMM emission.

Vamp `Feature.values` are floats. Chroma, note and dictionary values come from
the actual float arrays; native raw-score and emission doubles must be cast to
float on export. CSV introduces further formatting error. Native Viterbi and
local-winner indices are exported directly: do not infer exact offline Viterbi
parity from rounded emissions near ties. Independent score reconstruction
should use a tolerance appropriate to float and CSV precision.

The production RDF does not describe the added outputs. Select them inline in
the transform, e.g. `vamp:output [ vamp:identifier "stateemissions" ]`, and set
`diagnostics` to 1. Omit the production plugin-version pin or change it to 9;
preserve all frontend parameters. Exporting the dictionary from the same
transform avoids a sidecar being generated under different vocabulary settings.
Empty audio emits no features, including no dictionary.

## Build and verification

Use the repository's `build-windows.ps1` with the pinned cached LLVM-MinGW
compiler and Vamp SDK. In the Dechorder workspace the tested command is:

```powershell
& .\.tools\chordino-seventh-diagnostics\build-windows.ps1 -CacheDirectory .\.tools\chordino-build -OutputDirectory .\.tools\chordino-build\seventh-diagnostics-v9
```

The resulting DLL SHA256 is
`478f2bf7530a2aaff110b340231524967bd1f14f302b9b38869e1de59a981de9`.
The production DLL used by the frozen Mozart baseline is
`c494e058ab2bc35971b2f466de4c67f1781fbe4f694156b6ab799254fa71ea83`.

All four native test executables pass: decoder (six API cases), dictionary,
frontend (three cases), and diagnostics (NNLS/direct mapping crossed with
extended/legacy vocabulary). The diagnostic cases verify unchanged original
outputs, dictionary indices and labels, exact 84-to-24 reconstruction, raw
scores before the N multiplier, normalized/silent emissions, and exported
native state choices from full-precision reconstructed test observations.

`tests/check_mozart_parity.py` verifies the three prepared Mozart WAVs in Default
and Pop, with diagnostics both off and on, against the frozen production-v1
baseline. All 12 output CSVs match byte for byte. It records audio, binary,
transform, baseline and output hashes and retains every raw CSV. Reproduce with:

```powershell
python .\.tools\chordino-seventh-diagnostics\tests\check_mozart_parity.py --workspace C:\Users\black\Dechorder --plugin-dir .\.tools\chordino-build\seventh-diagnostics-v9 --output-dir .\analysis\chordino-fork-tests\seventh-diagnostics-parity
```

Local evidence is in `analysis/chordino-fork-tests/seventh-diagnostics-parity/`
and the build log is `analysis/chordino-fork-tests/seventh-diagnostics-build.log`.
Native host smoke checks confirm the dictionary and per-frame CSV shapes.
These checks establish observation parity, not accuracy on Mozart or Suno.
