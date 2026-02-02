#!/usr/bin/env python3
import argparse, json, math, sys
import numpy as np
import pandas as pd

def robust_std(x):
    # robust stdev via IQR -> approximate σ ≈ IQR/1.349 for normal-like tails
    q1, q3 = np.nanpercentile(x, [25, 75])
    iqr = q3 - q1
    return float(iqr / 1.349) if iqr > 0 else float(np.nanstd(x, ddof=1))

def fit_requests(req_csv):
    df = pd.read_csv(req_csv)
    # Basic cleaning: drop corrupted rows (non-numeric ttfb_s/conn flags)
    df = df[pd.to_numeric(df.get('ttfb_s', np.nan), errors='coerce').notna()]
    df['ttfb_ms'] = df['ttfb_s'] * 1000.0
    if 'conn_reused' in df.columns:
        # some logs store this as {0,1} or empty
        df['conn_reused'] = pd.to_numeric(df['conn_reused'], errors='coerce').fillna(0).astype(int)
    else:
        df['conn_reused'] = 0

    reused = df[df['conn_reused'] == 1]['ttfb_ms'].dropna()
    fresh  = df[df['conn_reused'] == 0]['ttfb_ms'].dropna()

    if len(reused) < 5:
        base_rtt_ms = float(df['ttfb_ms'].median())
        rtt_jitter_ms = float(robust_std(df['ttfb_ms']))
    else:
        base_rtt_ms = float(reused.median())
        rtt_jitter_ms = float(robust_std(reused))

    p_conn_reuse = float((df['conn_reused'] == 1).mean())

    if len(reused) >= 5 and len(fresh) >= 3:
        new_conn_penalty_ms = float(fresh.median() - reused.median())
        new_conn_penalty_ms = max(0.0, new_conn_penalty_ms)
    else:
        new_conn_penalty_ms = max(0.0, base_rtt_ms * 0.95)  # fallback
    # Tail spikes relative to reused baseline
    if len(reused) >= 20:
        p90 = float(np.nanpercentile(reused, 90.0))
        spikes = reused[reused > p90]
        ttfb_spike_p = float(len(spikes) / len(reused))
        ttfb_spike_ms = float(spikes.mean() - base_rtt_ms) if len(spikes) else 0.0
    else:
        ttfb_spike_p, ttfb_spike_ms = 0.0, 0.0

    return dict(
        base_rtt_ms=base_rtt_ms,
        rtt_jitter_ms=rtt_jitter_ms,
        p_conn_reuse=p_conn_reuse,
        new_conn_penalty_ms=new_conn_penalty_ms,
        ttfb_spike_p=ttfb_spike_p,
        ttfb_spike_ms=ttfb_spike_ms
    )

def fit_bursts(bur_csv, guard_low=0.10, guard_high=0.50):
    bur = pd.read_csv(bur_csv)
    # sanitize
    bur = bur[pd.to_numeric(bur.get('duration_s', np.nan), errors='coerce').notna()]
    bur = bur[pd.to_numeric(bur.get('bytes', np.nan), errors='coerce').notna()]
    bur['duration_s'] = bur['duration_s'].clip(lower=1e-4)
    bur['rate_Bps'] = bur['bytes'] / bur['duration_s']
    bur['gap_before_s'] = pd.to_numeric(bur.get('gap_before_s', 0.0), errors='coerce').fillna(0.0)

    # cadence from guarded gaps
    gaps = bur['gap_before_s']
    mask_cadence = (gaps >= guard_low) & (gaps <= guard_high)
    cadence_ms = float(gaps[mask_cadence].mean() * 1000.0) if mask_cadence.any() else float(gaps.mean() * 1000.0)
    cadence_jitter_ms = float(gaps[mask_cadence].std(ddof=1) * 1000.0) if mask_cadence.any() elsefloat(gaps.std(ddof=1) * 1000.0)

    # late gap classification based on cadence + 2*std
    late_thr = (cadence_ms/1000.0) + 2.0 * (cadence_jitter_ms/1000.0)
    late_mask = gaps > late_thr
    late_chunk_p = float(late_mask.mean())
    if late_mask.any():
        late_chunk_extra_ms = float((gaps[late_mask].mean() - (cadence_ms/1000.0)) * 1000.0)
    else:
        late_chunk_extra_ms = 0.0

    # bursts per segment and CV of burst bytes per req
    counts = bur.groupby('req_id')['burst_idx'].max().fillna(0).astype(int) + 1
    bursts_per_segment = int(round(counts[counts > 0].median())) if not counts.empty else 4

    def cv_bytes(group):
        arr = group['bytes'].values.astype(float)
        m = arr.mean()
        return 0.0 if m <= 0 else float(arr.std(ddof=1)/m) if len(arr) > 1 else 0.0
    cv_series = bur.groupby('req_id').apply(cv_bytes)
    burst_bytes_cv = float(cv_series.replace([np.inf, -np.inf], np.nan).dropna().median()) if not cv_series.empty else 0.35

    # AR(1) on ln(rate)
    rates = bur.loc[bur['rate_Bps'] > 0, ['req_id','burst_idx','rate_Bps']].copy()
    rates = rates.sort_values(['req_id','burst_idx'])
    x = np.log(rates['rate_Bps'].values)
    # Use within-request sequencing for AR(1)
    if len(x) >= 3:
        x_prev = x[:-1]
        x_curr = x[1:]
        # (simple OLS for AR(1) with intercept)
        A = np.vstack([np.ones_like(x_prev), x_prev]).T
        coef = np.linalg.lstsq(A, x_curr, rcond=None)[0]
        c_hat, rho_hat = float(coef[0]), float(coef[1])
        mu_hat = c_hat / (1.0 - rho_hat) if abs(1.0 - rho_hat) > 1e-6 else float(np.nan)
        resid = x_curr - (c_hat + rho_hat * x_prev)
        sigma_eps = float(np.std(resid, ddof=1))
        # back out mean throughputs (approx; geometric mean)
        mean_thr_mbps = float(np.exp(mu_hat) * 8.0 / 1e6)  # B/s -> Mb/s
        thr_sigma_ln = float(sigma_eps)
        thr_rho = float(rho_hat)
    else:
        mean_thr_mbps, thr_sigma_ln, thr_rho = 200.0, 0.8, 0.15

    # small sender flush jitter; keep default unless you want to fit from sub-ms wiggles
    flush_jitter_ms = 6.0

    return dict(
        cadence_ms=cadence_ms,
        cadence_jitter_ms=cadence_jitter_ms,
        late_chunk_p=late_chunk_p,
        late_chunk_extra_ms=late_chunk_extra_ms,
        bursts_per_segment=bursts_per_segment,
        burst_bytes_cv=burst_bytes_cv,
        mean_thr_mbps=mean_thr_mbps,
        thr_sigma_ln=thr_sigma_ln,
        thr_rho=thr_rho,
        flush_jitter_ms=flush_jitter_ms
    )

def main():
    ap = argparse.ArgumentParser(description="Fit LL-DASH network persona from request/burst CSVs")
    ap.add_argument("--requests", required=True, help="Path to aamp_net_requests.<pid>.csv")
    ap.add_argument("--bursts", required=True, help="Path to aamp_net_bursts.<pid>.csv")
    ap.add_argument("--out", default="persona.json", help="Output persona JSON")
    args = ap.parse_args()

    req = fit_requests(args.requests)
    bur = fit_bursts(args.bursts)

    persona = {
        "base_rtt_ms": req["base_rtt_ms"],
        "rtt_jitter_ms": req["rtt_jitter_ms"],
        "ttfb_spike_p": req["ttfb_spike_p"],
        "ttfb_spike_ms": req["ttfb_spike_ms"],

        "mean_thr_mbps": bur["mean_thr_mbps"],
        "thr_sigma_ln": bur["thr_sigma_ln"],
        "thr_rho": bur["thr_rho"],

        "bursts_per_segment": bur["bursts_per_segment"],
        "burst_bytes_cv": bur["burst_bytes_cv"],

        "cadence_ms": bur["cadence_ms"],
        "cadence_jitter_ms": bur["cadence_jitter_ms"],
        "flush_jitter_ms": bur["flush_jitter_ms"],
        "late_chunk_p": bur["late_chunk_p"],
        "late_chunk_extra_ms": bur["late_chunk_extra_ms"],

        "p_conn_reuse": req["p_conn_reuse"],
        "new_conn_penalty_ms": req["new_conn_penalty_ms"],

        "capacity_drop_p": 0.0,
        "capacity_drop_factor": 0.6,
        "rtt_inflation_ms": 0.0
    }

    with open(args.out, "w") as f:
        json.dump(persona, f, indent=2, sort_keys=True)

    print(f"Wrote {args.out}")
    # quick display
    for k, v in persona.items():
        print(f"{k}: {v}")

if __name__ == "__main__":
    main()
