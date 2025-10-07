MetroGnome — Linux Installation Guide

Overview
- MetroGnome is a VST3 plugin. On Linux, most hosts look for user‑installed VST3 plugins in ~/.vst3.
- This guide covers two ways to install:
  1) Using the packaged TGZ/ZIP archive built by CPack
  2) Manually copying the built MetroGnome.vst3 bundle into ~/.vst3

Prerequisites
- A Linux DAW/host that supports VST3 (e.g., REAPER for Linux)
- A recent build of MetroGnome (v0.1.0 or later)

Option A: Install from TGZ/ZIP package
- The Linux packages are created so that extracting them in your home directory installs the plugin to ~/.vst3.
- Steps:
  1) Place the MetroGnome-<version>-Linux.tar.gz (or .zip) file in your home directory (e.g., ~/Downloads is fine too).
  2) Extract it so that the archive's .vst3 folder ends up under your home.
     - Using a file manager: Right‑click → Extract Here (ensure it lands in your home directory).
     - Using a terminal (TGZ example):
       tar -xzf MetroGnome-<version>-Linux.tar.gz -C "$HOME"
  3) After extraction, you should have: ~/.vst3/MetroGnome.vst3

Option B: Manual copy from your build tree
- If you've built MetroGnome locally with CMake/JUCE, locate the VST3 bundle in your build directory. Common JUCE paths include one of:
  - <build>/MetroGnome_artefacts/VST3/Debug/MetroGnome.vst3
  - <build>/MetroGnome_artefacts/VST3/Release/MetroGnome.vst3
  - <build>/MetroGnome_artefacts/Debug/VST3/MetroGnome.vst3
  - <build>/MetroGnome_artefacts/Release/VST3/MetroGnome.vst3
- Create ~/.vst3 if it does not already exist and copy the bundle:
  mkdir -p "$HOME/.vst3"
  cp -r "/path/to/MetroGnome.vst3" "$HOME/.vst3/"

Verifying in your host (REAPER for Linux example)
1) Launch REAPER.
2) Open Preferences → Plug‑ins → VST.
3) Ensure "VST3 plug‑in paths" includes: ~/.vst3
   - If not, add it and click Re-scan.
4) Click Clear cache/re-scan or Re-scan.
5) Insert a new track, click the FX button, and search for "MetroGnome". The plugin should appear under VST3/Instrument.

Troubleshooting
- Plugin not found:
  - Confirm the path: ls ~/.vst3/MetroGnome.vst3
  - Verify the host has VST3 support enabled and is scanning ~/.vst3.
  - Restart the host after copying the plugin.
- Permissions:
  - Ensure your user owns the files under ~/.vst3: chown -R "$USER":"$USER" "$HOME/.vst3"
- Multiple builds/versions:
  - If you have multiple MetroGnome.vst3 copies in different locations, remove or rename extras to avoid confusion.

Uninstall
- Remove the bundle:
  rm -rf "$HOME/.vst3/MetroGnome.vst3"
- Re-scan plug‑ins in your host.

Notes
- No root/sudo is required for user‑local installations.
- The provided packages purposely install to ~/.vst3 to avoid system‑wide changes.
