# Native regression tests

On Windows x64, `./build-windows.ps1` downloads the pinned local compiler and SDK,
then builds and runs all three suites before producing the plugin. This is the
recommended command for a fresh clone; no preinstalled compiler or PATH changes
are required. Use `-CacheDirectory` to reuse existing downloads. The dictionary
test uses C++17 for its temporary-directory handling; the plugin itself uses C++11.

`test_decoder.cpp` exercises Chordino through its native plugin API, with known
spectral inputs. It checks empty and single-frame recordings, no-evidence silence,
the initial chord prior, chord-note durations, the extended-vocabulary switch,
and coalescing labels while keeping changes of note voicing. It does not establish chord accuracy on recorded music;
Dechorder's audio benchmark supplies that separate end-to-end check.

The test needs a C++11 compiler and the Vamp plugin SDK source. From the fork's
root, the following PowerShell commands build and run it with LLVM-MinGW. Set
`$taskCompilerDirectory` and `$taskSdk` to the installed compiler and SDK paths:

```powershell
New-Item -ItemType Directory -Force test-build | Out-Null
& "$taskCompilerDirectory\clang.exe" -O2 -c nnls.c -o test-build\nnls.o
if ($LASTEXITCODE -ne 0) { throw 'C compilation failed' }
& "$taskCompilerDirectory\clang++.exe" -std=c++11 -D_USE_MATH_DEFINES -O2 -static `
    -I . -I $taskSdk tests\test_decoder.cpp Chordino.cpp NNLSBase.cpp `
    chromamethods.cpp viterbi.cpp test-build\nnls.o `
    "$taskSdk\src\vamp-sdk\RealTime.cpp" -o test-build\test_decoder.exe
if ($LASTEXITCODE -ne 0) { throw 'C++ compilation failed' }
& .\test-build\test_decoder.exe
if ($LASTEXITCODE -ne 0) { throw 'Decoder regression failed' }
```

The decoder tests intentionally use two different internal spectral templates
with the same displayed chord name. Their note output must retain the change,
while their displayed chord output must contain only one label.

The final note event ends one analysis hop after the last input frame timestamp.
The Vamp API does not supply the exact audio-file duration to the plugin; clients
that display chord labels should extend the last label to their known file end.
The plugin no longer fabricates an `N` chord as an end marker.

`test_frontend.cpp` uses controlled frequency-domain input to check that DC and
the first FFT bin cannot erase useful pitch evidence through the bass roll-on
filter. It also checks that local tuning gives equal evidence equal weight
regardless of pitch traversal order, while preserving the old effective temporal
decay (`0.997^54` per frame for the current tuning range). Build it with the same
command, substituting `tests\test_frontend.cpp` and `test-build\test_frontend.exe`
for the decoder source and executable, then run the resulting executable.
