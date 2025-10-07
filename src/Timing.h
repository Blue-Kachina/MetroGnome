#pragma once

#include <cstdint>
#include <cmath>
#include <limits>

namespace metrog
{
    struct HostTransportInfo
    {
        double tempoBPM = 120.0;              // Host tempo in BPM
        double ppqPosition = 0.0;             // Host musical position in quarter notes (can be fractional)
        bool isPlaying = false;               // Host transport is playing
        int timeSigNumerator = 4;             // We assume quarter-note denominator per requirements
        double sampleRate = 48000.0;          // Current sample rate
    };

    struct SubdivisionCrossing
    {
        bool crosses = false;                 // Whether a subdivision boundary occurs within this block
        int firstCrossingSample = -1;         // Sample offset [0..blockSize-1] of first crossing; -1 if none
        int subdivisionIndex = -1;            // Subdivision index within the bar at the crossing (0-based)
        int barIndex = -1;                    // Bar index (0-based) at the crossing
    };

    class TimingEngine
    {
    public:
        void prepare(double sampleRate, int maxBlockSize)
        {
            sr = sampleRate; (void)maxBlockSize; // no dynamic allocations, but keep for future prealloc
        }

        // Set how many equal subdivisions per bar (e.g., 4=quarter notes in 4/4, 8=eighths, 16=sixteenths)
        void setSubdivisionsPerBar(int count) noexcept { subdivisionsPerBar = count > 0 ? count : 4; }
        int getSubdivisionsPerBar() const noexcept { return subdivisionsPerBar; }

        // Compute bar index and beat index (0-based) from PPQ.
        static void computeBarBeat(double ppqPosition, int timeSigNumerator, int& outBarIndex, int& outBeatInBar) noexcept
        {
            if (timeSigNumerator <= 0) timeSigNumerator = 4;
            const double beatsPerBar = static_cast<double>(timeSigNumerator);
            const double totalBeats = ppqPosition; // since denominator is quarter notes by JUCE convention
            const int bar = static_cast<int>(std::floor(totalBeats / beatsPerBar));
            const int beatInBar = static_cast<int>(std::floor(totalBeats - bar * beatsPerBar));
            outBarIndex = bar >= 0 ? bar : 0;
            outBeatInBar = (beatInBar >= 0 && beatInBar < timeSigNumerator) ? beatInBar : 0;
        }

        // Compute equal subdivision index (0-based) within current bar.
        static int computeSubdivisionIndex(double ppqPosition, int timeSigNumerator, int subdivisionsPerBar) noexcept
        {
            if (timeSigNumerator <= 0) timeSigNumerator = 4;
            if (subdivisionsPerBar <= 0) subdivisionsPerBar = 4;
            const double beatsPerBar = static_cast<double>(timeSigNumerator);
            const double barPosBeats = std::fmod(std::max(ppqPosition, 0.0), beatsPerBar);
            if (barPosBeats < 0.0) return 0;
            const double frac = barPosBeats / beatsPerBar; // 0..1
            int idx = static_cast<int>(std::floor(frac * subdivisionsPerBar + 1e-9));
            if (idx >= subdivisionsPerBar) idx = subdivisionsPerBar - 1;
            return idx;
        }

        // Determine whether the block crosses a subdivision boundary, and if so, where the first crossing occurs.
        SubdivisionCrossing findFirstSubdivisionCrossing(const HostTransportInfo& host, int blockSize) const noexcept
        {
            SubdivisionCrossing result{};
            if (!host.isPlaying || blockSize <= 0 || sr <= 0.0 || host.tempoBPM <= 0.0)
                return result;

            const double beatsPerSecond = host.tempoBPM / 60.0;
            const double secondsPerSample = 1.0 / sr;
            const double beatsPerSample = beatsPerSecond * secondsPerSample;

            // Compute bar and subdivision at block start
            int startBar = 0, startBeatInBar = 0;
            computeBarBeat(host.ppqPosition, host.timeSigNumerator, startBar, startBeatInBar);
            const double beatsPerBar = static_cast<double>(host.timeSigNumerator);
            const double startBarBeats = host.ppqPosition - (startBar * beatsPerBar);

            const double subLenBeats = beatsPerBar / static_cast<double>(subdivisionsPerBar);
            const double startSubIndexF = startBarBeats / subLenBeats; // fractional index
            const int startSubIndex = static_cast<int>(std::floor(startSubIndexF + 1e-12));

            // Epsilon check: if we're effectively on a boundary, report sample 0 crossing into current index
            const double boundaryEps = 1e-12 * beatsPerBar;
            const double distToBoundaryBelow = std::fmod(startBarBeats, subLenBeats);
            if (distToBoundaryBelow <= boundaryEps || subLenBeats - distToBoundaryBelow <= boundaryEps)
            {
                result.crosses = true;
                result.firstCrossingSample = 0;
                const int idxAtStart = computeSubdivisionIndex(host.ppqPosition, host.timeSigNumerator, subdivisionsPerBar);
                result.subdivisionIndex = idxAtStart;
                result.barIndex = startBar;
                return result;
            }

            // Otherwise, compute next boundary strictly after start
            const double nextBoundarySub = std::ceil(startSubIndexF - 1e-12); // next integer index > startSubIndexF
            const double beatsUntilBoundary = (nextBoundarySub * subLenBeats) - startBarBeats;

            // Convert beats to samples: first sample index where boundary is reached (ceil)
            long long samplesUntilBoundary = 0;
            if (beatsUntilBoundary > 0.0)
            {
                const double samplesUntilBoundaryD = beatsUntilBoundary / beatsPerSample;
                samplesUntilBoundary = static_cast<long long>(std::ceil(samplesUntilBoundaryD - 1e-12));
            }

            if (samplesUntilBoundary > 0 && samplesUntilBoundary <= static_cast<long long>(blockSize - 1))
            {
                result.crosses = true;
                result.firstCrossingSample = static_cast<int>(samplesUntilBoundary);
                const int nextIndex = (static_cast<int>(nextBoundarySub)) % subdivisionsPerBar;
                result.subdivisionIndex = nextIndex;
                result.barIndex = startBar + (static_cast<int>(nextBoundarySub) / subdivisionsPerBar);
            }
            return result;
        }

    private:
        double sr = 48000.0;
        int subdivisionsPerBar = 4;
    };
}
