#!/bin/bash
echo "================================================"
echo "ABR Simulator: Buffer Management Demonstration"
echo "================================================"
echo
echo "Scenario: 1-hour playback on 25 Mbps network"
echo
echo "--- VOD Mode: Testing Different Buffer Caps ---"
echo
for cap in 10 20 30; do
    echo "Buffer cap: ${cap}s"
    ./abrsim --persona sample_network.json --max-buffer $cap --duration 3600 --out /dev/null --seed 42 2>&1 | grep "Final buffer"
done
echo
echo "--- Live Mode: Buffer capped at target latency ---"
echo
./abrsim --persona sample_network.json --live --target-latency 8 --duration 3600 --out /dev/null --seed 42 2>&1 | grep "Final buffer"
echo
echo "Key Insight:"
echo "  VOD mode: Use --max-buffer to control memory usage (default: 20s)"
echo "  Live mode: Buffer automatically capped at --target-latency (default: 8s)"
echo
