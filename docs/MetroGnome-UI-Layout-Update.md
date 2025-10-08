# MetroGnome UI Layout Update – Sidebar Refactor and Size Reduction (Clarifications)

Context
- The current plugin window is too large and uses a header (top bar) layout with horizontally distributed controls.
- We are reducing the overall size, moving to a left sidebar with vertically stacked controls, keeping visual consistency with the current design, and relocating the step sequence buttons.
- Clarifications below supersede earlier bottom-strip guidance: Dance toggle now belongs in the sidebar; Enable/Disable remain near the bottom-right above the step row; no dedicated bottom control strip.

Scope of this document
- Define the new layout and visual rules so engineering can implement it consistently across platforms.
- Recommend a suitable new fixed size based on the existing background artwork (368×553).
- Provide implementation notes mapping the spec to the current JUCE codebase.

---

1) Replace the header with a left sidebar (vertical controls)
- Sidebar: 100 px wide (constant), anchored to the left edge.
- Controls in the sidebar (top-to-bottom):
  - Steps (rotary)
  - Time Signature Numerator (rotary)
  - Volume (rotary)
  - Dance Mode (toggle)  ← moved to sidebar (not at the bottom)
- Spacing: 12–16 px vertical spacing between controls; label sits directly above each rotary, centered.
- Rationale: This reclaims vertical space for the content area and matches the smaller overall footprint.

2) Sidebar colour and separation from background image
- Colour: Use the same source colour as the former header bar for visual continuity.
  - In code today, the header/sidebar colour derives from the LookAndFeel: findColour(Slider::rotarySliderFillColourId).darker(0.35f).
  - Keep using that derivation for the sidebar panel fill.
- Separation: The sidebar must NOT sit on top of the photographic/illustrative background image.
  - Do not draw the background image under the sidebar region.
  - Draw a solid sidebar rectangle first, then draw the background image only in the remaining "content" area to the right of the sidebar.

3) Step sequence row and controls near the bottom
- Step buttons (1×N) should sit visually near the bottom of the content area and be drawn directly over the background image (no separate section behind them).
- Enable All / Disable All:
  - Location: keep near the bottom of the screen, anchored to the right side, and positioned just above the step row.
  - Size: drastically reduced. Use icon-only buttons (no text) sourced from a clean, elegant icon pack.
  - Accessibility: provide tooltips and accessible names/ARIA labels so hosts/users can discover their purpose despite no visible text.
- Interaction: Keep the invisible overlay toggle technique aligned to each visible step cell (as implemented), updated on resize.

4) Recommended new plugin size
- Background artwork size: 368×553 (content area target).
- New total width = Sidebar (100) + Gutter between sidebar and content (16) + Content width (368) + Right padding (16) ≈ 500 px.
- New total height = Content height (553) + a small top/bottom padding budget (no dedicated bottom strip) ≈ 585 px overall.
- Recommendation: Make the editor fixed-size at 500×585.
  - If you must keep internal padding symmetric, budget the 16 px gutter on the right (as above) and allow the content image to scale to exactly 368×553 within its content rect.
  - HiDPI guidance: Allow OS/host scaling to handle 125%/150% displays; do not implement custom scaling at this time.
  - Min/max: Keep non-resizable for now; revisit resizability after validating how the artwork scales.

5) Implementation notes (JUCE, mapping to current code)
- File: src/PluginEditor.cpp.
- Size:
  - Change setSize(...) to 500×585; keep setResizable(false, false).
- Painting:
  - Define constants: kSidebarW = 100; kGutter = 16.
  - Draw sidebar: solid fill using findColour(Slider::rotarySliderFillColourId).darker(0.35f) across {0, 0, kSidebarW, getHeight()}.
  - Draw background: only in content rect = {kSidebarW + kGutter, top, contentW, contentH}. Target 368×553; center vertically as needed.
  - Draw step cells and their highlights directly over the background.
- Layout (resized):
  - Sidebar: Place rotaries and labels in a single column; place the Dance toggle within the sidebar below the rotaries.
  - Bottom-right controls: Place two small icon buttons (Enable All / Disable All) in the content area, anchored to the right side, immediately above the step row. Use fixed small square bounds (e.g., 20–24 px) and equal spacing.
  - Step row: A single row spanning the content area width, placed at the bottom of the content area, with the overlay-button technique for click targets.
- Assets:
  - Continue using assets/images/metrognome-a.png and -b.png for the background, clipped to the content rect.
  - Add two icons to the assets (future task if not embedded yet): e.g., enable-all.svg and disable-all.svg (or PNGs) from a consistent icon set. Wire them to the two small buttons as image-only buttons.

6) Acceptance criteria
- The editor opens at 500×585 and is non-resizable.
- The left 100 px are a solid sidebar with the specified colour; no background imagery shows through in that region.
- All former header controls appear in the sidebar (Dance toggle included), stacked vertically, with labels centered above the rotaries.
- The step row appears near the bottom of the content area and is drawn over the background image; no separate bottom control strip is present.
- Enable/Disable All appear as small icon-only buttons anchored to the right just above the step row, with tooltips and accessible names.
- Background image is visible only in the content area to the right of the sidebar.

7) Nice-to-have (future)
- Optional resizable editor with discrete size classes (e.g., 1×, 1.25×, 1.5×) that maintain the 100 px logical sidebar width scaled accordingly.
- Theming hook to override the sidebar colour independently of the knob fill colour if a future theme requires it.

Revision history
- 2025-10-07: Initial draft for the sidebar refactor and size reduction.
- 2025-10-07 23:00: Clarifications applied — Dance toggle in sidebar; icon-only Enable/Disable near bottom-right; no bottom control strip; step row overlays background.
