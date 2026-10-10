# Dechorder fork

This fork of `c4dm/nnls-chroma` starts from upstream commit
`4c5f214a75cb354f8d8c933e161377c5b4d83713`. It supplies the local analysis
engine for [Dechorder](https://github.com/BlackLockeMage/Dechorder), a personal
Windows guitar-learning application. Original copyright and GPL licensing remain
in place. This is an independently maintained fork, not an upstream release.

## Changes in plugin descriptor version 6

- A frame with no usable spectral evidence emits `N`, preventing the decoder
  from carrying chords across digital silence or pulling future chords into it.
  This reuses the existing evidence gate; it adds no amplitude threshold.
- A uniform initial prior lets the first frame contain a chord. The artificial
  terminal `N` marker is removed. Hosts should extend the last observed chord
  label to their known audio-file end.
- Adjacent identical chord labels are coalesced even when their internal
  voicing states differ. Note events retain those voicing changes; first-frame
  notes are initialized and final note durations include the last analysis hop.
  The Vamp API does not provide the exact audio-file duration to the plugin.
- The `extendedchords` parameter adds sus2, sus4, add9 and power-chord templates
  by default. Set it to zero for the original 18 template types. A valid custom
  dictionary continues to replace the entire built-in vocabulary.
- Custom dictionary loading validates complete 24-weight rows atomically.
  Empty, malformed, negative, nonfinite or unusable templates fall back to
  built-ins with a diagnostic. Parsing no longer requires Boost.
- Spectral roll-on measures total and cumulative energy over the same bins,
  excluding bins 0 and 1 from both, so DC cannot erase all the usable spectrum.
- Local tuning aggregates pitch evidence evenly before smoothing, removing
  pitch-order bias while retaining the former effective decay per frame.

The chord decoder still uses template similarities and the original uniform
off-diagonal HMM transition model. Scores are not calibrated confidence values.
This fork does not add key, beat, bar or progression constraints. Dechorder's
key-aware and strict-key interpretations remain separate application features.
Inversion bass recognition remains a known limitation.

## Reproducible Windows build

On Windows x64, run:

```powershell
.\build-windows.ps1 -OutputDirectory .build/windows-x64
```

The script downloads a SHA-256-pinned portable LLVM MinGW compiler and Vamp SDK
into `.build-cache/`, runs native regression tests, and builds a self-contained
C++ runtime into the plugin. It does not install tools or change PATH. Pass
`-CacheDirectory` to reuse a cache, and optionally `-SonicAnnotator` with a host
executable to verify plugin discovery. Windows UCRT and system APIs remain
operating-system dependencies. Fast-math is deliberately disabled.

Outputs include the DLL, Vamp metadata, original GPL licence, exact SDK and
runtime licences, and diagnostic `BUILD-INFO.json`. PE timestamps and source
path prefixes are removed for reproducibility. Git attributes retain LF endings
for text files on Windows, including the checksum-pinned runtime licences.
Source-byte hashes in build info describe the actual local inputs.

Native tests live under `tests/`. The independent end-to-end synthetic audio
suite, comparison runner and evaluation results are maintained in Dechorder.
Synthetic scores test known defects and regressions; they are not an accuracy
estimate for mastered songs.
