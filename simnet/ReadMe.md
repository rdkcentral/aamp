# SimNet (LL-DASH Network Persona Simulator)

SimNet is a lightweight network simulator for LL-DASH request timing.
It generates synthetic request/burst traces from a persona JSON so you can:

- Replay network behavior in a deterministic way (`--seed`)
- Tune persona parameters from observed logs
- Visualize generated CSV/JSON quickly in browser dashboards

## Directory Layout

This folder contains:

- `build.sh` - Build helper for the standalone simulator binary
- `simnet/simnet.cpp` - Simulator source (`simnet` executable)
- `simnet/persona_fit.py` - Fit persona JSON from real CSV logs
- `index.html` - Dashboard for request/burst CSV visualization
- `jsonVisualization.html` - Dashboard for JSON persona/metrics visualization

## Prerequisites

### Build/Run Simulator

- `g++` with C++17 support
- `bash`

### Persona Fitting (`persona_fit.py`)

- Python 3
- Python packages: `numpy`, `pandas`

Install Python deps if needed:

```bash
python3 -m pip install --user numpy pandas
```

## Build

From repository root:

```bash
cd simnet
./build.sh
```

Expected artifact:

- `simnet/simnet`

## Simulator Usage

From `simnet/simnet` folder:

```bash
./simnet \
	--persona <persona.json> \
	--sizes <bytes1> <bytes2> ... \
	--out <output_prefix> \
	[--seed <N>]
```

or

```bash
./simnet \
	--persona <persona.json> \
	--sizes-file <sizes.txt> \
	--out <output_prefix> \
	[--seed <N>]
```

### Required Flags

- `--persona <path>`: Persona JSON file
- One of:
	- `--sizes <b1> <b2> ...` (inline segment sizes in bytes)
	- `--sizes-file <path>` (one size per line)

### Optional Flags

- `--out <prefix>`: Output filename prefix (default: `/tmp/sim`)
- `--seed <N>`: Random seed for deterministic simulation

### Example

```bash
cd simnet/simnet
./simnet \
	--persona ../../abrsim/personas/wifi_good.json \
	--sizes 1400000 24000 1400000 24000 \
	--seed 42 \
	--out ./sim
```

Outputs:

- `sim-requests.csv`
- `sim-bursts.csv`

## Output CSV Schema

### `<prefix>-requests.csv`

- `req_id`
- `size_bytes`
- `conn_reused` (0 or 1)
- `ttfb_ms`
- `total_ms`
- `burst_count`
- `sum_gap_ms`
- `sum_burst_ms`
- `avg_burst_rate_Bps`

### `<prefix>-bursts.csv`

- `req_id`
- `burst_idx`
- `t_start_ms`
- `duration_ms`
- `bytes`
- `gap_before_ms`

## Fit a Persona from Real Logs

Use `persona_fit.py` to estimate persona parameters from existing
network-request and burst CSV files:

```bash
cd simnet/simnet
python3 persona_fit.py \
	--requests <aamp_net_requests.csv> \
	--bursts <aamp_net_bursts.csv> \
	--out ./persona_fitted.json
```

This writes a JSON with parameters used by `simnet` such as RTT baseline,
TTFB spikes, throughput model, burst cadence, and connection reuse penalties.

## Visualization

### CSV Dashboard (`index.html`)

Use with generated CSV outputs.

1. Open `simnet/index.html` in a browser
2. Load/drop both files:
	 - `<prefix>-requests.csv`
	 - `<prefix>-bursts.csv`
3. Click **Load and Plot**

### JSON Dashboard (`jsonVisualization.html`)

Use for persona/JSON analysis.

1. Open `simnet/jsonVisualization.html` in a browser
2. Paste JSON or upload a JSON file

### Optional: Serve via local HTTP

```bash
cd simnet
python3 -m http.server 8080
```

Then browse:

- `http://localhost:8080/index.html`
- `http://localhost:8080/jsonVisualization.html`

## Troubleshooting

- `Missing --persona <path>`: pass a valid persona JSON path.
- `Provide sizes via --sizes or --sizes-file`: add at least one size source.
- `Could not open output files`: ensure `--out` points to a writable location.
- Empty/invalid plot: verify CSV headers and file pairing (`requests` + `bursts`).
