#!/bin/bash
# Quick test of fixed simulation

cd "$(dirname "$0")"

echo "Testing fixed abrsim simulation..."
echo ""

# Check if sample_network.json exists
if [ ! -f "sample_network.json" ]; then
    echo "Creating sample_network.json for testing..."
    cat > sample_network.json << 'EOF'
{
  "base_rtt_ms": 50.0,
  "rtt_jitter_ms": 10.0,
  "ttfb_spike_p": 0.01,
  "ttfb_spike_ms": 100.0,
  "mean_thr_mbps": 5.0,
  "thr_sigma_ln": 0.5,
  "thr_rho": 0.2,
  "bursts_per_segment": 8,
  "burst_bytes_cv": 0.3,
  "cadence_ms": 150.0,
  "cadence_jitter_ms": 30.0,
  "flush_jitter_ms": 5.0,
  "late_chunk_p": 0.005,
  "late_chunk_extra_ms": 100.0,
  "p_conn_reuse": 0.9,
  "new_conn_penalty_ms": 150.0
}
EOF
fi

# Run a short test simulation with seed for reproducibility
echo "Running 60-second simulation with seed=12345..."
./abrsim --persona sample_network.json --duration 60 --seed 12345 --out test_output.csv

echo ""
echo "Checking results..."

# Check for reasonable rebuffer stats
rebuffers=$(grep "Rebuffer events:" test_output.csv 2>/dev/null || echo "N/A")
echo "Rebuffer events: $rebuffers"

# Show first few events
echo ""
echo "First 10 simulation events:"
head -11 test_output.csv

# Run same simulation again to test seed consistency
echo ""
echo "Running same simulation again (testing seed reproducibility)..."
./abrsim --persona sample_network.json --duration 60 --seed 12345 --out test_output2.csv

echo ""
echo "Comparing outputs (should be identical)..."
if diff -q test_output.csv test_output2.csv > /dev/null; then
    echo "✓ Outputs match! Seed provides reproducible results."
else
    echo "⚠ Outputs differ - seed may not be working correctly"
    echo "First file line count: $(wc -l < test_output.csv)"
    echo "Second file line count: $(wc -l < test_output2.csv)"
fi

# Cleanup
rm -f test_output.csv test_output2.csv

echo ""
echo "Test complete!"
