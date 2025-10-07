MetroGnome — macOS Installation (Manual)

This document provides clear, low‑friction steps to install the MetroGnome VST3 plugin on macOS without requiring an installer. A signed pkg may be added later; until then, follow these steps.

Supported macOS versions: 12 (Monterey) or later on Intel or Apple Silicon.

Quick install (user‑local, recommended)
- Copy MetroGnome.vst3 to: ~/Library/Audio/Plug-Ins/VST3
- Launch your DAW and rescan VST3 plugins (see Host cache refresh below)
- Insert MetroGnome in your DAW as a VST3 instrument

Details
1) Locate the built plugin
- After building with CMake/IDE, the plugin bundle is created by JUCE as MetroGnome.vst3 inside your build directory.
  Common locations (examples):
  - build/MetroGnome_artefacts/Release/VST3/MetroGnome.vst3
  - build/MetroGnome_artefacts/Debug/VST3/MetroGnome.vst3
  - Or your IDE’s configured CMake build directory under .../VST3/MetroGnome.vst3

2) Choose an install location
- Per‑user (no admin required, preferred):
  ~/Library/Audio/Plug-Ins/VST3
  Create the VST3 folder if it does not exist.
- System‑wide (admin required):
  /Library/Audio/Plug-Ins/VST3

3) Copy the bundle
- Drag‑and‑drop MetroGnome.vst3 into the chosen VST3 folder.
- Or via Terminal:
  - User‑local:  cp -R "/path/to/MetroGnome.vst3" "$HOME/Library/Audio/Plug-Ins/VST3/"
  - System‑wide: sudo cp -R "/path/to/MetroGnome.vst3" "/Library/Audio/Plug-Ins/VST3/"

4) Gatekeeper/quarantine (if blocked)
- If the plugin was downloaded from the internet, macOS may add a quarantine attribute.
- To remove quarantine:
  xattr -d com.apple.quarantine "/path/to/MetroGnome.vst3" 2>/dev/null || true
- If your DAW reports the plugin cannot be opened because the developer cannot be verified, remove quarantine and rescan. Signing/notarization will be addressed in a later phase.

5) Host cache refresh
- Reaper (macOS):
  - Reaper → Preferences → Plug‑ins → VST
  - Click Clear cache/re-scan
  - Ensure VST3 path includes: ~/Library/Audio/Plug-Ins/VST3 and /Library/Audio/Plug-Ins/VST3
- General guidance:
  - If your host does not show the plugin immediately, restart the host, or use its rescan function.

Apple Silicon notes
- MetroGnome is a VST3 plugin built via JUCE. Ensure your DAW architecture matches the plugin build (ARM64 vs x86_64). Most modern DAWs run natively on Apple Silicon and will scan ARM64 builds. If you are running the DAW under Rosetta, ensure the plugin build matches.

Uninstall
- Remove MetroGnome.vst3 from the install folder you chose:
  - User‑local:   rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/MetroGnome.vst3"
  - System‑wide:  sudo rm -rf "/Library/Audio/Plug-Ins/VST3/MetroGnome.vst3"

Troubleshooting
- Plugin not listed:
  - Verify the .vst3 bundle exists in the VST3 folder.
  - Rescan or restart the host.
  - Check for quarantine and remove it (see above).
- Multiple copies:
  - Remove duplicates from other VST3 folders to avoid confusion.
- Logic Pro / AU:
  - Logic uses AU, not VST3. MetroGnome is delivered as VST3. An AU wrapper is not provided in this phase.

Next steps
- A signed and (optionally) notarized macOS pkg installer may be added later. For now, this manual path delivers a simple, ≤3‑step flow that meets Phase 9.2 goals.
