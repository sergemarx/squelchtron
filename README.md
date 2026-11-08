# SQUELCHTRON

Guitar -> pitch-tracked squawk synth -> resonant squelch filter -> fuzz. VST3 + Standalone, built with JUCE 8.

## Knobs
FUZZ, SQUELCH (filter resonance), CUTOFF, ENV (how much your picking dynamics sweep the filter, bipolar), SYNTH (dry guitar vs tracked synth), SQUAWK (ring-mod of guitar against the synth saw), WOBBLE (LFO filter wobble, rate and depth), LEVEL.

## MIDI
Knobs default to CC 20-27. Right-click a knob -> MIDI Learn, then move a control on your controller. Mappings are saved with the preset/project. All knobs are also normal host-automatable parameters.

## Build
Needs CMake 3.22+ and a C++17 compiler (Visual Studio 2022 on Windows, Xcode on macOS, or GCC + JUCE Linux deps).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel

The VST3 lands in build/Squelchtron_artefacts/Release/VST3/. Copy it to your VST3 folder
(Windows: C:\Program Files\Common Files\VST3, macOS: ~/Library/Audio/Plug-Ins/VST3).

No computer with a compiler handy? Push this folder to a GitHub repo and the included workflow
(.github/workflows/build.yml) builds Windows and macOS VST3s; download them from the Actions tab.

## Playing tips
Mono guitar, single notes track best. Use a clean DI signal, not a pre-distorted one.
For maximum whacko: SYNTH 70%, SQUAWK 40%, SQUELCH 75%, ENV +, WOBBLE 30%, FUZZ 60%.
