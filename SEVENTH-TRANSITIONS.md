# Experimental seventh-transition potentials — not validated for adoption

This isolated branch starts from observational diagnostic commit `41cc70f`.
Chordino version 10 adds only `seventhtransition`, a fractional discount `d`
in [0,1], default 0. The controlled experiment uses only 0, .25, .5 and .75.
Invalid/nonfinite values revert to zero. No frontend, dictionary, emission or
local-winner changes are made. The six diagnostic outputs remain available.

Eligible edges connect the same root pitch class **and same labelled bass pitch
class**, in either direction, for plain major ↔ dominant7, major ↔ major7, or
minor ↔ minor7. An unslashed bass means the root. Enharmonic spellings and native
Harte relative bass degrees are canonicalized. Other qualities, seventh ↔
seventh, same-quality voicing changes, N, root changes and differing labelled
basses retain their original direct edges. Unrecognized labels are ineligible.
All internal templates bearing eligible labels participate.

For `n` states the baseline stay potential is `s=.99`, other-state potential
`b=.01/(n-1)` (computed as `(1-s)/(n-1)`), and log switch penalty
`L=log(s/b)`. An eligible edge becomes `s*exp(-(1-d)*L)`. Every diagonal and
ineligible entry remains bit-for-bit unchanged. At zero the original matrix is
returned without evaluating log or exp.

Rows are deliberately **not normalized**. Viterbi multiplies positive transition
potentials and scales every frame by a common scalar; it does not require row
sums of one. Row normalization would change the intended comparison. With the
experiment enabled, the existing scale/loglikelihood output represents decoder
potential scores, not a calibrated likelihood from a stochastic HMM.

This preserves direct edge ratios, not global trajectories. Cheaper eligible
edges can change the winning root/bass path indirectly. Duplicate C or Cm
templates share a printed root bass while their bass weight vectors differ;
at discounts above .5 a two-edge excursion through a seventh can bridge these
templates more cheaply than their direct edge. C7 → C → Cmaj7 can likewise be
an indirect shortcut. These are explicit limitations, covered by a unit test;
same labelled bass does not imply identical complete spectral templates.
The built-in extended vocabulary has 168 eligible directed edges across 265
states. Native Harte syntax gives the same eligible state-index pairs.

Build with the pinned cached toolchain, into a separate directory:

```powershell
& .\.tools\chordino-seventh-transitions\build-windows.ps1 -CacheDirectory .\.tools\chordino-build -OutputDirectory .\.tools\chordino-build\seventh-transitions-v10
```

The build runs the four existing native suites plus `test_seventh_transitions`.
New tests cover eligible/excluded edges, enharmonic/Harte bass parsing, symmetry,
exact untouched entries, log-cost discounts, invalid values, default-off matrix
and API parity, unchanged observations/local winners, decoder response to a
controlled observation sequence, and the documented duplicate-template bridge.
Unit observations are not benchmark audio. Benchmark decoding requires a frozen
protocol; no installation, production merge or adoption is implied by this branch.

The candidate DLL SHA256 is
`dd4bb4a4f5e9d4d8819fa00cb6f8cb27b45a261e830500a0728a3436d3d1a098`.
`BUILD-INFO.json` in the separate output directory records the pinned compiler,
SDK and source hashes. The production DLL is not replaced.
