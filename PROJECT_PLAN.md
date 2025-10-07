# Metro Gnome – Phased Delivery Plan

This plan translates REQUIREMENTS.md into a pragmatic, phased roadmap with concrete deliverables, acceptance criteria, and risk controls. It aims to keep the codebase buildable early, validate critical timing behavior quickly, and de‑risk cross‑platform and deployment late.

## Guiding Principles
- Real‑time safety first: avoid dynamic allocation/locking in audio thread, preallocate, and keep deterministic timing.
- Host sync correctness over features: correctness of transport/PPQ/phase is critical for user value.
- Ship thin vertical slices: each phase should be buildable, testable, and demoable.

## Phase 0 – Planning & Repo Scaffolding
Goals
- Capture roadmap and risks; prepare docs area for future design notes.
Deliverables
- PROJECT_PLAN.md (this file)
- docs/ (placeholder for design notes)
Acceptance Criteria
- Plan is committed to repo and reflects REQUIREMENTS.md.
Risks & Mitigation
- Scope creep → keep phases small and traceable to requirements.

## Phase 1 – JUCE Plugin Skeleton (Windows)
Goals
- A minimal VST3 instrument plugin that builds and loads; no UI yet.
Deliverables
- Minimal PluginProcessor/PluginEditor stubs
- CMake targets build on Windows using JUCE FetchContent
Acceptance Criteria
- Builds in Debug/Release.
- Loads in a VST3 validator or DAW without errors; passes audio through or emits silence deterministically.
Risks & Mitigation
- CMake misconfig → start from JUCE examples; keep flags minimal.
Status
- Completed on 2025-10-06 20:42 (local). JUCE updated to 8.0.10; build succeeds for MetroGnome_VST3.

## Phase 2 – Host Sync & Timing Engine
Goals
- Phase‑locked internal clock synchronized to host tempo and transport.

We will break Phase 2 into smaller, verifiable sub‑phases to reduce risk and isolate errors.

Phase 2a – Host Transport Readout (PlayHead)
- Deliverables
  - Stable readout of isPlaying, bpm, ppqPosition, timeSigNumerator from host PlayHead
  - Cached HostTransportInfo with sane defaults and no allocations in processBlock
- Acceptance Criteria
  - Values update correctly in play/stop; fall back to cached values if host omits fields
  - No dynamic allocations in processBlock
- Risks & Mitigation
  - Some hosts omit fields → defensively keep last known valid values
- Status
  - Implemented on 2025-10-06 21:28 (local): processBlock reads PlayHead with caching and fallbacks; no allocations in audio thread.

Phase 2b – Timing Math Utilities
- Deliverables
  - Bar/beat computation from PPQ
  - Equal subdivision index and first‑crossing detection within a block
  - Unit tests covering tempos (40–240 BPM), signatures (3–7 numerator), subdivisions (1–64)
- Acceptance Criteria
  - Correct bar/beat and subdivision indices across bar boundaries and exact‑boundary cases
  - No allocations; pure functions where possible
- Risks & Mitigation
  - Floating‑point edge cases → epsilon nudges and tests around boundaries
- Status
  - Completed on 2025-10-06 22:00 (local): timing utilities implemented (bar/beat, subdivision index, first-crossing) with unit tests (tempos 40–240 BPM, signatures 3–7, subdivisions 1–64). Boundary edge cases handled with epsilon and verified by tests.

Phase 2c – Integration in Audio Path
- Deliverables
  - Wire timing utilities into processBlock; compute first subdivision crossing per block
  - Block‑level outputs remain silent (sequencer arrives in Phase 3)
- Acceptance Criteria
  - No regressions in build/load; CPU stable; timing values observable via debug/logs when enabled
- Risks & Mitigation
  - DAW differences → verify in at least two hosts (e.g., Reaper, VST3 validator)
- Status
  - Completed on 2025-10-06 21:59 (local): timing utilities are integrated in processBlock; first subdivision crossing computed per block. Output remains silent. Optional debug logs gated by METROG_DEBUG_TIMING compile-time flag.

## Phase 3 – Step Sequencer Core
Goals
- Gate emission per step following host‑locked clock.
Deliverables
- Parameters: stepCount (1–16, default 8), per‑step enabled flags, enableAll/disableAll actions
- Sequencer state machine with wraparound and deterministic indexing
- Parameter layout with automation and state persistence
Acceptance Criteria
- At each enabled step boundary, gate trigger occurs exactly once and is phase‑accurate.
- All parameters are automatable and persist in state.
Risks & Mitigation
- Off‑by‑one/edge cases at bar boundaries → unit tests over multiple bars.

## Phase 4 – Sound Generator
Goals
- Metronome‑style click with clear transient; zero‑latency trigger.
Deliverables
- Click synthesizer (e.g., short envelope/noise/sine composite)
- Volume parameter; optional tone/pitch/attack parameters (stretch goal)
Acceptance Criteria
- Click renders on gate within the same sample block (no added latency).
- CPU usage is low at typical rates (48/96 kHz).
Risks & Mitigation
- Denormals → flush‑to‑zero; use simple DSP without feedback paths.

## Phase 5 – UI/UX (Initial)
Goals
- Functional editor with step blocks and core controls.
Deliverables
- Step block grid with illuminated enabled state
- Rotary controls: step count, subdivision/time signature numerator, volume
- Buttons: Enable All, Disable All
Acceptance Criteria
- UI reflects parameter state; operations are smooth at 60 FPS.
- Keyboard focus and accessibility sane defaults.
Risks & Mitigation
- Repaints causing CPU spikes → throttle visuals, avoid heavy allocs in paint.

## Phase 6 – State, Automation, MIDI Learn
Goals
- Full project recall and host automation; optional MIDI mapping.
Deliverables
- APVTS (or equivalent) state serialization
- MIDI learn/assignment implementation for parameters (where JUCE allows)
Acceptance Criteria
- Reopening projects restores state reliably across DAWs.
- All parameters appear in host automation lanes.
Risks & Mitigation
- DAW quirks → test in at least two hosts.

## Phase 7 – Performance & RT Safety Hardening
Goals
- Audit and micro‑optimize audio path; ensure RT safety.
Deliverables
- RT safety checklist; allocation audits; profiling results
Acceptance Criteria
- No allocations/locks in audio thread; stable CPU usage under load.
Risks & Mitigation
- Hidden allocations (std::function, etc.) → prefer fixed storage and simple structs.

## Phase 8 – Cross‑Platform Builds
Goals
- Ensure builds on macOS and Linux; no UI regressions.
Deliverables
- CMake presets and CI jobs (build only) for all three platforms
Acceptance Criteria
- Successful CI builds on Windows/macOS/Linux.
Risks & Mitigation
- Platform specific flags → conditionally set in CMake; avoid deprecated APIs.

## Phase 9 – Packaging & Deployment
Goals
- Self‑contained installers; minimal user steps.
Deliverables
- Windows installer (MSIX/WiX); macOS pkg with VST3; Linux instructions or package
Acceptance Criteria
- One‑click install; plugin discoverable by host without manual steps.
Risks & Mitigation
- Static linking constraints/licensing → follow JUCE and third‑party licenses; bundle as needed.

## Phase 10 – Documentation & Release
Goals
- User docs, versioning, and initial public release.
Deliverables
- User guide, automation map, change log
- v0.1.0 tagged release
Acceptance Criteria
- Docs reflect implemented features; release artifacts validated.

---

## Traceability to REQUIREMENTS.md
- Platform/Build: JUCE VST3 focus (Phases 1, 8, 9)
- Performance/RT safety: Phases 2, 4, 7
- Deployment: Phase 9
- Core Features: Sequencer (3), Subdivision control (2), Sound generator (4), Host integration (2), UI (5)
- UI/State/Automation/MIDI: Phases 5–6
- Success Criteria: Verified progressively in Phases 2–7

## Milestones & Target Order
- M0: Plan committed (Phase 0)
- M1: Skeleton loads in host (Phase 1)
- M2: Accurate timing engine with tests (Phase 2)
- M3: Sequencer gates (Phase 3)
- M4: Click sound (Phase 4)
- M5: Usable UI (Phase 5)
- M6: State/automation/MIDI (Phase 6)
- M7: Perf/RT pass (Phase 7)
- M8: Cross‑platform builds (Phase 8)
- M9: Installers (Phase 9)
- M10: 0.1.0 release (Phase 10)

## Next Actions
- Verify Phase 2 (2a–2c) behavior in at least one DAW (e.g., Reaper) via debug logs; ensure CPU stability.
- Proceed to Phase 3 – Step Sequencer Core: add parameters and emit gate triggers at subdivision boundaries.
