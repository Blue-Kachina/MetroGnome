#include <iostream>
#include <vector>
#include <cmath>
#include <cstdint>
#include "Timing.h"

using namespace metrog;

static bool approxEqual(double a, double b, double eps = 1e-9)
{
    return std::abs(a - b) <= eps;
}

// Brute-force stepping: simulate sample-by-sample over a block and detect first subdivision index change.
static SubdivisionCrossing bruteForceCrossing(const HostTransportInfo& host,
                                              int subdivisionsPerBar,
                                              int blockSize)
{
    SubdivisionCrossing res{};
    if (!host.isPlaying || blockSize <= 0 || host.sampleRate <= 0.0 || host.tempoBPM <= 0.0)
        return res;

    const double beatsPerSecond = host.tempoBPM / 60.0;
    const double beatsPerSample = beatsPerSecond / host.sampleRate;
    const double beatsPerBar = static_cast<double>(host.timeSigNumerator);

    auto idxAt = [&](double ppq) {
        return TimingEngine::computeSubdivisionIndex(ppq, host.timeSigNumerator, subdivisionsPerBar);
    };

    const int startIdx = idxAt(host.ppqPosition);
    const int startBar = static_cast<int>(std::floor(std::max(0.0, host.ppqPosition) / beatsPerBar));

    // Special-case: detect boundary exactly at sample 0 by checking modulo distance to subdivision length
    {
        const double beatsPerBarLocal = beatsPerBar;
        const double subLenBeats = beatsPerBarLocal / static_cast<double>(subdivisionsPerBar);
        const double distToBoundaryBelow = std::fmod(std::fmod(std::max(0.0, host.ppqPosition), beatsPerBarLocal), subLenBeats);
        const double boundaryEps = 1e-12 * beatsPerBarLocal;
        if (distToBoundaryBelow <= boundaryEps || subLenBeats - distToBoundaryBelow <= boundaryEps)
        {
            res.crosses = true;
            res.firstCrossingSample = 0;
            res.subdivisionIndex = idxAt(host.ppqPosition);
            const int barNow = static_cast<int>(std::floor(std::max(0.0, host.ppqPosition) / beatsPerBarLocal));
            res.barIndex = barNow;
            return res;
        }
    }

    double ppq = host.ppqPosition;
    for (int s = 0; s < blockSize; ++s)
    {
        // Check this exact sample (excluding s==0 case handled above)
        const int idx = idxAt(ppq);
        if (idx != startIdx)
        {
            res.crosses = true;
            res.firstCrossingSample = s;
            res.subdivisionIndex = idx;
            const int barNow = static_cast<int>(std::floor(std::max(0.0, ppq) / beatsPerBar));
            res.barIndex = barNow;
            return res;
        }
        // Advance one sample
        ppq += beatsPerSample;
    }
    return res;
}

static int runTests()
{
    int failures = 0;

    // Test computeBarBeat basic cases including boundaries
    {
        int bar, beat;
        // 4/4, ppq = 0..7
        for (int p = 0; p <= 7; ++p)
        {
            TimingEngine::computeBarBeat(static_cast<double>(p), 4, bar, beat);
            int expBar = p / 4;
            int expBeat = p % 4;
            if (bar != expBar || beat != expBeat)
            {
                std::cerr << "computeBarBeat failed at ppq=" << p << ": got (" << bar << "," << beat
                          << ") expected (" << expBar << "," << expBeat << ")\n";
                ++failures;
            }
        }
        // 3/4 wrapping
        TimingEngine::computeBarBeat(2.9, 3, bar, beat);
        if (bar != 0 || beat != 2) { std::cerr << "computeBarBeat 2.9 in 3/4 fail\n"; ++failures; }
        TimingEngine::computeBarBeat(3.0, 3, bar, beat);
        if (bar != 1 || beat != 0) { std::cerr << "computeBarBeat 3.0 in 3/4 fail\n"; ++failures; }
    }

    // Test computeSubdivisionIndex across boundaries
    {
        // For various signatures and subdivision counts
        for (int numer = 3; numer <= 7; ++numer)
        {
            const double beatsPerBar = static_cast<double>(numer);
            for (int subdiv : {1,2,3,4,6,8,12,16,32,64})
            {
                // Step across one bar in small increments just around boundaries
                const int steps = subdiv * 10;
                for (int i = 0; i < steps; ++i)
                {
                    const double frac = static_cast<double>(i) / static_cast<double>(steps);
                    const double ppq = frac * beatsPerBar;
                    const int idx = TimingEngine::computeSubdivisionIndex(ppq, numer, subdiv);
                    int expected = static_cast<int>(std::floor(frac * subdiv + 1e-9));
                    if (expected >= subdiv) expected = subdiv - 1;
                    if (idx != expected)
                    {
                        std::cerr << "computeSubdivisionIndex fail: numer=" << numer
                                  << " subdiv=" << subdiv << " ppq=" << ppq
                                  << " got=" << idx << " expected=" << expected << "\n";
                        ++failures;
                    }
                }
            }
        }
    }

    // Test findFirstSubdivisionCrossing against brute-force for a matrix of conditions
    {
        std::vector<double> tempos = {40.0, 60.0, 120.0, 240.0};
        std::vector<int> numers = {3,4,5,6,7};
        std::vector<int> subdivs = {1,2,3,4,6,8,12,16,32,64};
        std::vector<double> sampleRates = {44100.0, 48000.0};
        std::vector<int> blockSizes = {1, 32, 512};
        std::vector<double> starts = {1e-9, 0.5, 0.999999, 1.0, 1.000001, 2.5, 7.75};

        for (double sr : sampleRates)
        for (double bpm : tempos)
        for (int numer : numers)
        for (int subdiv : subdivs)
        for (int bs : blockSizes)
        for (double startPPQ : starts)
        {
            HostTransportInfo host{};
            host.sampleRate = sr;
            host.tempoBPM = bpm;
            host.timeSigNumerator = numer;
            host.isPlaying = true;
            host.ppqPosition = startPPQ;

            // Engine under test
            TimingEngine engine;
            engine.prepare(sr, bs);
            engine.setSubdivisionsPerBar(subdiv);
            auto got = engine.findFirstSubdivisionCrossing(host, bs);

            // Brute force reference
            auto ref = bruteForceCrossing(host, subdiv, bs);

            if (got.crosses != ref.crosses)
            {
                std::cerr << "crosses mismatch sr="<<sr<<" bpm="<<bpm<<" num="<<numer<<" subdiv="<<subdiv
                          <<" bs="<<bs<<" start="<<startPPQ<<" got="<<got.crosses<<" ref="<<ref.crosses<<"\n";
                ++failures;
                continue;
            }
            if (got.crosses)
            {
                if (got.firstCrossingSample != ref.firstCrossingSample
                    || got.subdivisionIndex != ref.subdivisionIndex
                    || got.barIndex != ref.barIndex)
                {
                    std::cerr << "crossing detail mismatch sr="<<sr<<" bpm="<<bpm<<" num="<<numer
                              <<" subdiv="<<subdiv<<" bs="<<bs<<" start="<<startPPQ
                              <<" got(s,i,b)=("<<got.firstCrossingSample<<","<<got.subdivisionIndex<<","<<got.barIndex
                              <<") ref=("<<ref.firstCrossingSample<<","<<ref.subdivisionIndex<<","<<ref.barIndex<<")\n";
                    ++failures;
                }
            }
        }
    }

    if (failures == 0)
        std::cout << "All Timing tests passed." << std::endl;
    else
        std::cout << failures << " Timing test(s) failed." << std::endl;

    return failures == 0 ? 0 : 1;
}

int main()
{
    return runTests();
}
