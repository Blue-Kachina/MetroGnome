# Metro Gnome - Project Requirements

## Project Overview
Metro Gnome is a C++/JUCE VST3 instrument plugin designed to help train musicians by providing an intermittent metronome that challenges them to maintain steady rhythm even when the click track isn't playing.

## Technical Requirements

### Platform & Build
- **Framework**: C++/JUCE
- **Plugin Format**: VST3 instrument (primary focus)
- **Future Consideration**: Standalone application version
- **Architecture**: 64-bit
- **Platforms**: Windows, macOS, Linux (all platforms)

### Performance Requirements
- **Latency**: Minimal system latency impact - critical for timing accuracy
- **CPU Efficiency**: Lightweight processing to avoid affecting host performance
- **Real-time Safety**: All audio processing must be real-time safe (no allocations, locks, or blocking operations)

## Core Features

### 1. Step Sequencer Section
**Purpose**: Control which beats in the sequence produce a metronome click

**Components**:
- Visual step blocks representing individual steps
- Each block can be toggled on/off
- Enabled blocks appear illuminated
- **Enable All** button - toggles all steps on
- **Disable All** button - toggles all steps off

**Step Count Control**:
- Rotary control to adjust number of steps (x)
- **Default**: 8 steps
- **Range**: 1-16 steps
- **Initial State**: All steps enabled by default

**Behavior**:
- Sequencer automatically progresses in sync with host transport
- When sequencer reaches an enabled step, triggers sound generator
- When sequencer reaches a disabled step, no sound plays (but timing continues)

### 2. Time Signature/Subdivision Control
**Purpose**: Allow rhythmic deviations from host time signature

**Components**:
- Rotary control for setting plugin time signature
- **Available Values**: 1/x through 64/x (sensible time signature numerators)
- **Common Values**: 1/4, 2/4, 3/4, 4/4, 5/4, 6/8, 7/8, etc.

**Behavior**:
- Syncs with host tempo (BPM) by default
- Allows different time signature subdivision than host
- **Example**: Host at 120 BPM 4/4, plugin set to 3/4
  - Tempo remains 120 BPM
  - Bar length remains constant
  - Plugin subdivides bar into 3 beats instead of 4
  - Metronome plays 3x per bar vs host's 4x per bar
- Maintains synchronization with host transport position

### 3. Sound Generator
**Purpose**: Produce metronome click sound

**Sound Design**:
- Generate metronome-style click sound
- Target sound quality similar to popular DAW metronomes
- Clean, clear transient suitable for timing reference

**Controls**:
- Volume control (minimum requirement)
- Additional sound shaping parameters (pitch, tone, attack, etc.)

### 4. Host Integration
**Synchronization**:
- Full sync with host transport (play/stop/position)
- Respect host tempo changes in real-time
- Maintain phase-accurate timing relative to host bar/beat position

**Audio Routing**:
- Plugin output can be routed as input source to other audio tracks in the host DAW
- Standard VST instrument audio output behavior (compatible with Cubase and other DAWs)

## User Interface Requirements

### Visual Design
- Professional studio hardware aesthetic
- Controls styled to resemble physical studio equipment
- Clear visual feedback for all states
- Illuminated/highlighted appearance for enabled sequencer steps

### Control Specifications
All controls must support:
- **State Persistence**: Settings saved and recalled with project
- **Host Automation**: All parameters automatable via DAW
- **MIDI Assignment**: All parameters can be MIDI-mapped/learned

## Educational Purpose
The plugin helps musicians develop internal timing by:
1. Providing a steady click reference (like traditional metronome)
2. Allowing clicks to be disabled on specific beats via step sequencer
3. Forcing musician to maintain tempo during silent beats
4. Automatically resuming clicks to provide immediate timing feedback
5. Revealing whether the musician played too fast, too slow, or maintained correct tempo

## Success Criteria
- Accurate, phase-locked synchronization with host
- Intuitive step sequencer interface with clear visual feedback
- Professional-sounding metronome click
- Zero-latency triggering of clicks
- Smooth parameter changes without audio artifacts
- Reliable state recall across sessions
- Full automation and MIDI control capability
