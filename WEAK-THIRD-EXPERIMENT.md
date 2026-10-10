# REJECTED: weak-third experiment

This isolated checkout starts at `7c5cd2e`. It is not the production plugin.
Chordino advertises descriptor version 7; NNLS Chroma and Tuning remain version 6.
This branch preserves an unsuccessful experiment for inspection. Do not install
it as an accuracy upgrade. Production remains at version 6 / commit `7c5cd2e`.

`weakthird` defaults to zero (off). The only enabled values are 0.25, 0.5 and
0.75. For each existing plain major/minor template, the alternative scales both
third-note bass and treble coefficients, after dictionary expansion, then restores
unit RMS norm. The score is the maximum of the original and alternative dot
products. No HMM states, transition rules, key assumptions, power templates,
extension templates or N templates are changed. Increasing plain-triad scores can
still change their competition with those other chords; regression gates must
measure those outcomes. Custom labels receive an alternative only when their
actual treble support is a complete plain triad.

For a root-position triad versus its corresponding power chord, let A be the
shared root/fifth evidence and T the third evidence, including their bass weights.
The existing templates require T/A greater than `sqrt(4.5/3.25)-1`, approximately
0.17670, for the triad to win. This is a rigid voicing assumption rather than a
normalization arithmetic error. An alternative with third weight alpha lowers
that boundary to `(sqrt(1+(1.25/3.25)*alpha^2)-1)/alpha`. The finite preregistered
positive weights prevent a continuous search towards zero from explaining every
tiny leakage component as a true third.

`diagnostics=1` emits two appended outputs. It is disabled by default and does
not change the original four outputs:

- `decoderchroma`: 24 exact scoring inputs, bass A/Bb/B/C/C#/D/Eb/E/F/F#/G/Ab,
  then treble in the same order. The diagnostic stream applies no normalization.
- `triadscores`: 36 best effective raw scores, major roots in that order, then
  minor, then power. Multiple voicing states sharing a label are maximized. Values
  precede clipping and exponentiation; absent candidates are zero. These scores
  are not probabilities or calibrated confidence.

Prepare alternatives once at initialization or a parameter change, not per frame.
The native build runs structural tests for the changed scorer and diagnostics.
Audio evaluation must establish default-off parity, measure development cases
before tuning, freeze a selected candidate, and then assess held-out examples and
legacy regressions. Synthetic results do not establish accuracy on real songs.

## Outcome

All three preregistered factors were rejected on development controls. No factor
was selected; the new validation set was not decoded, and enabled candidates were
not run against the old regression suite after failing the development gate.
The production application, installed DLL and saved song library were unchanged.

The independent suite contains 35 development and 35 validation files. Each
development file was decoded with Default and Pop profiles. For both profiles,
the following full-timeline exact and harmonic accuracies were identical:

| Development group | Production | 0.25 | 0.5 | 0.75 |
| --- | ---: | ---: | ---: | ---: |
| Primary major/minor, third gains 1/0.5/0.25 | 91.67% | 100% | 100% | 100% |
| Isolated genuine power chords | 100% | 0% | 0% | 50% |
| Accompanied genuine power chords | 98.03% | 0% | 0% | 73.03% |

Every factor recovers a quiet C-minor third but invents thirds in genuine power
chords. This fails the preregistered preservation requirement. No additional
factors or model changes were tried after inspecting the results.

Default-off output matches production byte for byte on all 70 development
fixture/profile pairs, plus six old-suite pairs covering majors, extensions and
silence. Four native test executables passed: decoder (six API cases), dictionary,
frontend (three cases), and the new weak-third structural/diagnostic checks.
An independent source review found no discrepancy from the declared treatment.

## Diagnostic evidence

The three development WAVs for C5, E5 and Cm with third gain 0.25 were decoded at
weights 0 and 0.75, with both profiles. The 24-bin inputs were identical between
weights. An independent recomputation of all 36 diagnostic scores from their
chroma CSVs matched within 0.000076 absolute, consistent with the CSV writer's
six significant digits. Thus the scoring arithmetic implements the proposed
treatment; the treatment fails to distinguish these signals reliably.

For Default, median scores over frames from 0.15 to 3.85 seconds were:

| Signal and competing triad | Original triad | Factor 0.75 triad | Unchanged power |
| --- | ---: | ---: | ---: |
| C5 versus C | 21.2242 | 22.5298 | 22.7368 |
| E5 versus E | 20.2045 | 21.5080 | 21.3396 |
| Quiet-third Cm versus Cm | 21.4450 | 22.5947 | 22.3606 |

These are maxima across plain-label voicings, not probabilities. The existing
neutral-bass triad state is crucial: after scaling its third, its squared weight
sum before normalization is `2.5 + 1.25 * alpha^2`, whereas the root-bass triad
uses `3.25 + 1.25 * alpha^2`; the root-bass power template uses `3.25`.
The root-position threshold derived above does not describe competition against
the neutral-bass state. On E5 at 0.75, median root-bass and neutral-bass E scores
are 20.2829 and 21.5079; the neutral state wins 75 of 76 interior frames. On weak
Cm they are 21.8831 and 22.5946, with the neutral state winning all 76 frames.
The power signal's nonzero major-third chroma plus this different weighting is
enough to promote an unplayed third. The current evidence does not isolate how
much of that chroma originates in harmonics, spectral leakage or NNLS fitting.

The evaluated experimental DLL SHA256 is
`e2e4afca2c01a0a312f2d4f88079441db9029e072e4200215dba8c1af29fcd65`.
Production SHA256 is
`c494e058ab2bc35971b2f466de4c67f1781fbe4f694156b6ab799254fa71ea83`.
The frozen design, fixture identities and development reports are documented in
[Dechorder's experiment record](https://github.com/BlackLockeMage/Dechorder/blob/main/docs/weak-third-experiment.md).

## Reproduction

Run `build-windows.ps1` in this checkout, choosing a separate output directory.
It builds the plugin and all four native tests with the pinned compiler and SDK.
Do not copy the result to the production plugin directory. `weakthird=0` is the
default; only 0.25, 0.5 and 0.75 enable the rejected treatment.

The production RDF file does not declare the experimental outputs. A Sonic
Annotator transform must identify them inline, e.g.:

```n3
vamp:output [ vamp:identifier "decoderchroma" ] ;
vamp:parameter_binding [ vamp:parameter [ vamp:identifier "diagnostics" ] ;
    vamp:value "1"^^xsd:float ] ;
vamp:parameter_binding [ vamp:parameter [ vamp:identifier "weakthird" ] ;
    vamp:value "0.75"^^xsd:float ] .
```

Use `triadscores` instead of `decoderchroma` for the other stream, and preserve
the original transform's frontend settings. Omit its production version pin or
set it to 7. These diagnostics do not add an accuracy claim for real Suno songs.
