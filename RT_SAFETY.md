MetroGnome — Real‑Time (RT) Safety & Performance Checklist

Date started: 2025-10-06 (local)
Phase: 7 — Performance & RT Safety Hardening

Scope
- Ensure the audio thread performs no dynamic memory allocations, blocking operations, or unbounded work.
- Verify parameter access patterns, MIDI handling, and transport polling are RT-safe.
- Micro-optimize hot code paths where beneficial without reducing clarity.

RT Safety Checklist
- Audio thread
  - [x] No heap allocations in processBlock (checked: only stack variables and atomics).
  - [x] No locks/mutexes/critical sections (none used).
  - [x] No file I/O, logging, or DBG calls in release (debug logging guarded; default off).
  - [x] No use of std::function or virtual dispatch in tight sample loops beyond JUCE primitives.
  - [x] Denormal protection active (juce::ScopedNoDenormals).
  - [x] Transport polling uses stack CurrentPositionInfo; no allocations.
  - [x] Parameter reads use cached std::atomic<float>* from APVTS (no lookups in audio thread).
  - [x] MIDI handling: bounded iteration over MidiBuffer; learn capture uses atomics; mapped CC writes use setValueNotifyingHost (JUCE-safe from audio thread).
  - [x] Per-sample mixing avoids repeated getWritePointer calls (hoisted channel pointers).
  - [x] No calls into UI from audio thread.
- State & MIDI learn
  - [x] Learn arming and commit occur on message thread; audio thread only sets pending CC via atomics.
  - [x] Fast CC→parameter map stored in a fixed-size array of atomics (size 128); no maps/vectors in RT path.
  - [x] State (ValueTree) read/write only on message thread; rebuild map on load.
- Synthesis path
  - [x] Simple sine burst click uses basic math; no tables or allocations.
  - [x] Envelope and phase math contain no branches that cause unpredictable spikes; decay quickly disables.

Micro-Optimizations Applied
- Hoisted channel write pointers outside per-sample loop to avoid repeated buffer.getWritePointer() calls.

Potential Future Optimizations (only if profiling warrants)
- Replace std::sin with a faster approximation or a small LUT for the short click tone.
- Use branchless envelope termination heuristics; currently negligible cost.
- Consider interleaved write or SIMD for stereo if click path becomes more complex.

Profiling Guidance
- Build a Release configuration with optimizations on.
- Use a DAW or JUCE AudioPluginHost to run at:
  - Sample rates: 44.1 kHz, 48 kHz, 96 kHz
  - Buffer sizes: 32, 64, 128, 256 samples
  - BPMs: 60, 120, 180
- Observe CPU meter; look for stability with Dance mode on and step grid animating.
- Verify zero-latency retriggers at subdivision crossings by monitoring output onset alignment.
- Optional tools: Windows Performance Analyzer, Xcode Instruments (macOS), perf (Linux), or JUCE Timer profiling for UI thread.

Acceptance Targets
- No allocations/locks in processBlock under all configurations.
- CPU usage stable and low across test matrix; no frame or audio dropouts.

Notes
- MIDI Learn CC mapping persists in APVTS state and is rebuilt on load without touching audio thread structures.
- Further changes should maintain the no-allocation/no-lock invariant in processBlock.
