/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 Self-contained LL-DASH network persona simulator.
 Build: g++ -std=gnu++17 -O2 -o simnet simnet.cpp
 
 Usage:
		./simnet
			--persona /tmp/persona.json \
			--sizes 1400000 24000 1400000 24000 \
			--out sim
			--seed 123
 or:
		./simnet
			--persona /tmp/persons.json \
			--sizes-file sizes.txt
 			--out sim

Output:
	sim-requests.csv	// one row per simulated request
	sim-bursts.csv 		// per-burst timing/size rows
*/

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>


// ----------------------------- Persona types ------------------------------

struct NetworkCharacteristics {
	// Start/TTFB
	double base_rtt_ms = 175.0;
	double rtt_jitter_ms = 20.0;
	double ttfb_spike_p = 0.01;
	double ttfb_spike_ms = 120.0;
	
	// Capacity during bursts (lognormal AR(1) on ln(rate [B/s]))
	double mean_thr_mbps = 200.0; // convenient user-facing number
	double thr_sigma_ln = 0.80; // innovation  on ln(rate)
	double thr_rho = 0.15; // AR(1) correlation
	
	// Intra-segment burst structure
	int bursts_per_segment = 8;
	double burst_bytes_cv = 0.40;
	
	// Pacing/availability between bursts
	double cadence_ms = 175.0;
	double cadence_jitter_ms = 45.0;
	double flush_jitter_ms = 6.0;
	double late_chunk_p = 0.01;
	double late_chunk_extra_ms = 120.0;
	
	// Connection reuse
	double p_conn_reuse = 0.95;
	double new_conn_penalty_ms = 170.0;
	
	// Optional impairments (unused in this basic demo)
	double capacity_drop_p = 0.0;
	double capacity_drop_factor = 0.6;
	double rtt_inflation_ms = 0.0;
};

// ----------------------------- Tiny JSON loader ---------------------------
// We keep a trivial flat-number parser to avoid dependencies.
// Accepts a flat JSON with numeric values, e.g. {"base_rtt_ms": 175, ...}

static bool FindNumber(const std::string& s, const std::string& key, double &out) {
	const std::string kq = "\"" + key + "\"";
	size_t kp = s.find(kq);
	if (kp == std::string::npos) return false;
	size_t colon = s.find(':', kp + kq.size());
	if (colon == std::string::npos) return false;
	size_t start = s.find_first_of("-0123456789", colon + 1);
	if (start == std::string::npos) return false;
	size_t end = s.find_first_not_of("0123456789.eE+-", start);
	try { out = std::stod(s.substr(start, end - start)); }
	catch (...) { return false; }
	return true;
}

static bool FindInt(const std::string& s, const std::string& key, int &out) {
	double d;
	if (!FindNumber(s, key, d)) return false;
	out = static_cast<int>(std::llround(d));
	return true;
}

static NetworkCharacteristics LoadPersonaFromJson(const std::string& path) {
	NetworkCharacteristics nc;
	std::ifstream f(path);
	if (!f.is_open()) {
		std::cerr << "[simnet] WARN: persona file not found; using defaults\n";
		return nc;
	}
	std::stringstream buf; buf << f.rdbuf();
	std::string s = buf.str();
	
	// Populate from JSON if present; otherwise keep defaults
	FindNumber(s, "base_rtt_ms", nc.base_rtt_ms);
	FindNumber(s, "rtt_jitter_ms", nc.rtt_jitter_ms);
	FindNumber(s, "ttfb_spike_p", nc.ttfb_spike_p);
	FindNumber(s, "ttfb_spike_ms", nc.ttfb_spike_ms);
	
	FindNumber(s, "mean_thr_mbps", nc.mean_thr_mbps);
	FindNumber(s, "thr_sigma_ln", nc.thr_sigma_ln);
	FindNumber(s, "thr_rho", nc.thr_rho);
	
	FindInt(s, "bursts_per_segment", nc.bursts_per_segment);
	FindNumber(s, "burst_bytes_cv", nc.burst_bytes_cv);
	
	FindNumber(s, "cadence_ms", nc.cadence_ms);
	FindNumber(s, "cadence_jitter_ms", nc.cadence_jitter_ms);
	FindNumber(s, "flush_jitter_ms", nc.flush_jitter_ms);
	FindNumber(s, "late_chunk_p", nc.late_chunk_p);
	FindNumber(s, "late_chunk_extra_ms", nc.late_chunk_extra_ms);
	
	FindNumber(s, "p_conn_reuse", nc.p_conn_reuse);
	FindNumber(s, "new_conn_penalty_ms", nc.new_conn_penalty_ms);
	
	FindNumber(s, "capacity_drop_p", nc.capacity_drop_p);
	FindNumber(s, "capacity_drop_factor", nc.capacity_drop_factor);
	FindNumber(s, "rtt_inflation_ms", nc.rtt_inflation_ms);
	
	return nc;
}

// ----------------------------- Simulation helpers -------------------------

struct BurstRow {
	int req_id = 0;
	int burst_idx = 0;
	double t_start_ms = 0.0; // relative to request start
	double dur_ms = 0.0;
	std::uint64_t bytes = 0;
	double gap_before_ms = 0.0;
};

struct RequestRow {
	int req_id = 0;
	std::uint64_t size_bytes = 0;
	bool conn_reused = true;
	double ttfb_ms = 0.0;
	double total_ms = 0.0;
	int burst_count = 0;
	double sum_gap_ms = 0.0;
	double sum_burst_ms = 0.0;
	double avg_burst_rate_Bps = 0.0; // bytes / sum_burst
};

struct NetworkModel {
	explicit NetworkModel(const NetworkCharacteristics& nc, std::uint64_t seed = 0)
	: nc_(nc),
	rng_(seed ? seed : std::random_device{}()),
	unif01_(0.0, 1.0),
	norm01_(0.0, 1.0)
	{
		// Precompute stationary parameters for ln(rate) AR(1)
		mean_Bps_ = (nc_.mean_thr_mbps * 1e6) / 8.0; // Mb/s -> B/s
		// AR(1): x_{t+1} = (1-rho)*mu_stat + rho*x_t + sigma_e*eps
		// Stationary variance: var = sigma_e^2 / (1 - rho^2)
		double rho = nc_.thr_rho;
		double sigma_e = nc_.thr_sigma_ln;
		var_ln_ = sigma_e * sigma_e / std::max(1e-9, (1.0 - rho*rho));
		mu_stat_ = std::log(std::max(1.0, mean_Bps_)) - 0.5 * var_ln_;
		// Gamma shape from target CV for burst split: CV = 1/sqrt(k) => k = 1/CV^2
		shape_k_ = (nc_.burst_bytes_cv > 1e-6) ? 1.0 / (nc_.burst_bytes_cv * nc_.burst_bytes_cv) : 1e6;
		scale_theta_ = 1.0; // any positive value; we normalize anyway
	}
	
	// Simulate a single request of given size. Returns per-request + per-burst rows
	void simulate(std::uint64_t size_bytes, int req_id,
				  RequestRow& req_out, std::vector<BurstRow>& bursts_out)
	{
		req_out = {};
		req_out.req_id = req_id;
		req_out.size_bytes = size_bytes;
		
		// --- Connection reuse / TTFB ---
		bool reused = (unif01_(rng_) < nc_.p_conn_reuse);
		req_out.conn_reused = reused;
		
		double ttfb_ms = sampleNormalClipped(nc_.base_rtt_ms, nc_.rtt_jitter_ms, 0.0);
		if (!reused) ttfb_ms += nc_.new_conn_penalty_ms;
		if (unif01_(rng_) < nc_.ttfb_spike_p) ttfb_ms += nc_.ttfb_spike_ms;
		ttfb_ms += nc_.rtt_inflation_ms;
		req_out.ttfb_ms = ttfb_ms;
		
		// --- Determine burst count ---
		int k = std::max(1, nc_.bursts_per_segment);
		if (size_bytes < 1024 && k > 1) k = 1; // trivial tiny request
		
		// --- Partition bytes across bursts using Gamma weights (target CV) ---
		std::vector<double> w(k);
		double sumw = 0.0;
		for (int i = 0; i < k; ++i) {
			double g = sampleGamma(shape_k_, scale_theta_);
			if (g <= 0.0) g = 1e-9;
			w[i] = g; sumw += g;
		}
		for (int i = 0; i < k; ++i) w[i] /= sumw;
		
		std::vector<std::uint64_t> bytes(k, 0);
		std::uint64_t assigned = 0;
		for (int i = 0; i < k; ++i) {
			// round to nearest byte
			std::uint64_t bi = static_cast<std::uint64_t>(std::llround(w[i] * static_cast<long double>(size_bytes)));
			bytes[i] = bi; assigned += bi;
		}
		// Fix rounding drift
		if (assigned != size_bytes) {
			long long diff = static_cast<long long>(size_bytes) - static_cast<long long>(assigned);
			int idx = 0;
			int max_iterations = k * 2; // Safety limit to prevent infinite loop
			while (diff != 0 && k > 0 && max_iterations > 0) {
				if (diff > 0) { bytes[idx % k]++; diff--; }
				else if (bytes[idx % k] > 0) { bytes[idx % k]--; diff++; }
				idx++;
				max_iterations--;
			}
			// If we couldn't fully correct the drift, safely assign any small remainder to first chunk
			if (diff != 0 && k > 0) {
				if (diff > 0) {
					// Positive remainder: increase first chunk by remaining bytes
					bytes[0] += static_cast<std::uint64_t>(diff);
				} else {
					// Negative remainder: only reduce first chunk if it has enough bytes
					std::uint64_t absDiff = static_cast<std::uint64_t>(-diff);
					if (bytes[0] >= absDiff) {
						bytes[0] -= absDiff;
					}
					// If bytes[0] < absDiff, skip correction to avoid underflow; total may remain slightly off
				}
			}
		}
		
		// --- Simulate pacing gaps + burst rates via AR(1) on ln(rate) ---
		double t_ms = ttfb_ms;
		double sum_gap_ms = 0.0, sum_burst_ms = 0.0;
		std::vector<BurstRow> bursts;
		bursts.reserve(k);
		
		double x_prev = mu_stat_; // start at stationary mean
		for (int i = 0; i < k; ++i) {
			double gap_ms = 0.0;
			if (i > 0) {
				gap_ms = sampleNormalClipped(nc_.cadence_ms, nc_.cadence_jitter_ms, 0.0);
				if (unif01_(rng_) < nc_.late_chunk_p) gap_ms += nc_.late_chunk_extra_ms;
				sum_gap_ms += gap_ms;
				t_ms += gap_ms;
			}
			
			// AR(1) step on ln(rate)
			double eps = norm01_(rng_);
			double x = (1.0 - nc_.thr_rho) * mu_stat_ + nc_.thr_rho * x_prev + nc_.thr_sigma_ln * eps;
			x_prev = x;
			double rate_Bps = std::exp(x); // bytes per second
			rate_Bps = std::max(rate_Bps, 1000.0); // avoid degenerate rates
			
			double jitter_ms = unif01_(rng_) * nc_.flush_jitter_ms;
			double dur_ms = 0.0;
			if (bytes[i] > 0) {
				dur_ms = (static_cast<double>(bytes[i]) / rate_Bps) * 1000.0 + jitter_ms;
			} else {
				dur_ms = jitter_ms; // empty burst (can happen if size << k and rounding)
			}
			dur_ms = std::max(dur_ms, 0.5); // floor a bit for visibility
			
			BurstRow br;
			br.req_id = req_id;
			br.burst_idx = i;
			br.t_start_ms = t_ms;
			br.dur_ms = dur_ms;
			br.bytes = bytes[i];
			br.gap_before_ms = (i == 0 ? 0.0 : gap_ms);
			bursts.push_back(br);
			
			sum_burst_ms += dur_ms;
			t_ms += dur_ms;
		}
		
		req_out.burst_count = k;
		req_out.sum_gap_ms = sum_gap_ms;
		req_out.sum_burst_ms = sum_burst_ms;
		req_out.total_ms = t_ms; // request timeline ends after last burst
		req_out.avg_burst_rate_Bps = (sum_burst_ms > 0.0) ? (static_cast<double>(size_bytes) / (sum_burst_ms / 1000.0)) : 0.0;
		
		bursts_out.insert(bursts_out.end(), bursts.begin(), bursts.end());
	}
	
private:
	NetworkCharacteristics nc_;
	std::mt19937_64 rng_;
	std::uniform_real_distribution<double> unif01_;
	std::normal_distribution<double> norm01_;
	
	// Stationary parameters
	double mean_Bps_ = 25e6; //  200 Mb/s
	double mu_stat_ = std::log(25e6);
	double var_ln_ = 0.5;
	
	// Gamma weights for burst partition
	double shape_k_ = 6.25; // ~ CV=0.4
	double scale_theta_ = 1.0;
	
	double sampleNormalClipped(double mean, double sd, double minv) {
		if (sd <= 0.0) return std::max(mean, minv);
		double v = mean + sd * norm01_(rng_);
		return (v < minv) ? minv : v;
	}
	
	// Marsaglia-Tsang Gamma for shape>0
	double sampleGamma(double shape, double scale) {
		if (shape <= 0.0) return 0.0;
		
		// Handle shape < 1.0 iteratively to avoid recursion
		double correction = 1.0;
		if (shape < 1.0) {
			// Johnk's generator: Gamma(shape) = Gamma(shape+1) * U^(1/shape)
			double u = std::max(unif01_(rng_), 1e-12);
			correction = std::pow(u, 1.0 / shape);
			shape += 1.0;
		}
		
		// Marsaglia and Tsang method for shape >= 1.0
		double d = shape - 1.0 / 3.0;
		double c = 1.0 / std::sqrt(9.0 * d);
		constexpr int kMaxIterations = 10000;
		for (int iter = 0; iter < kMaxIterations; ++iter) {
			double x = norm01_(rng_);
			double v = 1.0 + c * x;
			if (v <= 0) continue;
			v = v * v * v;
			double u = unif01_(rng_);
			if (u < 1.0 - 0.0331 * (x * x) * (x * x)) return (d * v) * scale * correction;
			if (std::log(u) < 0.5 * x * x + d * (1 - v + std::log(v))) return (d * v) * scale * correction;
		}
		// Fallback: return mean of gamma distribution if max iterations exceeded
		return (shape * scale) * correction;
	}
};

// ----------------------------- CLI & I/O -----------------------------------

static void printUsage() {
	std::cerr <<
	"Usage:\n"
	" simnet --persona <persona.json> [--sizes <b1> <b2> ... | --sizes-file file]\n"
	" [--out prefix] [--seed N]\n\n"
	"Examples:\n"
	" simnet --persona personas/lldash_persona_fitted.json --sizes 1400000 24000 1400000 24000 --out sim\n"
	" simnet --persona personas/lldash_persona_fitted.json --sizes-file sizes.txt --seed 42 --out sim\n";
}

struct CLI {
	std::string personaPath;
	std::vector<std::uint64_t> sizes;
	std::string sizesFile;
	std::string outPrefix = "/tmp/sim";
	std::uint64_t seed = 0;
};

static bool ParseCLI(int argc, char** argv, CLI& cli) {
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		auto need = [&](int n){ return (i + n) < argc; };
		
		if (a == "--persona" && need(1)) {
			cli.personaPath = argv[++i];
		} else if (a == "--out" && need(1)) {
			cli.outPrefix = argv[++i];
		} else if (a == "--seed" && need(1)) {
			cli.seed = std::strtoull(argv[++i], nullptr, 10);
		} else if (a == "--sizes") {
			// consume remaining numeric args until next flag or end
			while (i + 1 < argc && argv[i+1][0] != '-') {
				++i;
				long double v = std::strtold(argv[i], nullptr);
				if (v > 0) cli.sizes.push_back(static_cast<std::uint64_t>(std::llround(v)));
			}
		} else if (a == "--sizes-file" && need(1)) {
			cli.sizesFile = argv[++i];
		} else {
			std::cerr << "[simnet] Unknown or incomplete flag: " << a << "\n";
			return false;
		}
	}
	
	if (cli.sizes.empty() && !cli.sizesFile.size()) {
		std::cerr << "[simnet] Provide sizes via --sizes or --sizes-file\n";
		return false;
	}
	if (cli.personaPath.empty()) {
		std::cerr << "[simnet] Missing --persona <path>\n";
		return false;
	}
	return true;
}

static bool LoadSizesFromFile(const std::string& path, std::vector<std::uint64_t>& out) {
	std::ifstream f(path);
	if (!f.is_open()) {
		std::cerr << "[simnet] Could not open sizes file: " << path << "\n";
		return false;
	}
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty()) continue;
		std::stringstream ss(line);
		long double v = 0.0L;
		ss >> v;
		if (v > 0) out.push_back(static_cast<std::uint64_t>(std::llround(v)));
	}
	return true;
}

int main(int argc, char** argv)
{
	std::ios::sync_with_stdio(false);
	CLI cli;
	if (!ParseCLI(argc, argv, cli)) { printUsage(); return 2; }
	if (!cli.sizesFile.empty()) {
		if (!LoadSizesFromFile(cli.sizesFile, cli.sizes)) return 3;
	}
	if (cli.sizes.empty()) {
		std::cerr << "[simnet] No sizes provided after reading file.\n";
		return 4;
	}
	
	NetworkCharacteristics nc = LoadPersonaFromJson(cli.personaPath);
	NetworkModel model(nc, cli.seed);
	
	// Open outputs
	std::ofstream reqcsv(cli.outPrefix + "-requests.csv");
	std::ofstream burcsv(cli.outPrefix + "-bursts.csv");
	if (!reqcsv.is_open() || !burcsv.is_open()) {
		std::cerr << "[simnet] Could not open output files with prefix: " << cli.outPrefix << "\n";
		return 5;
	}
	
	reqcsv <<"req_id,size_bytes,conn_reused,ttfb_ms,total_ms,burst_count,sum_gap_ms,sum_burst_ms,avg_burst_rate_Bps\n";
	burcsv << "req_id,burst_idx,t_start_ms,duration_ms,bytes,gap_before_ms\n";
	
	int req_id = 1;
	for (std::uint64_t sz : cli.sizes) {
		RequestRow R{};
		std::vector<BurstRow> BR;
		model.simulate(sz, req_id, R, BR);
		
		reqcsv << R.req_id << "," << R.size_bytes << "," << (R.conn_reused?1:0) << ","
		<< std::fixed << std::setprecision(3)
		<< R.ttfb_ms << "," << R.total_ms << ","
		<< R.burst_count << ","
		<< R.sum_gap_ms << ","
		<< R.sum_burst_ms << ","
		<< std::setprecision(6) << R.avg_burst_rate_Bps
		<< "\n";
		
		burcsv << std::fixed << std::setprecision(6);
		for (const auto& b : BR) {
			burcsv << b.req_id << "," << b.burst_idx << ","
			<< b.t_start_ms << "," << b.dur_ms << ","
			<< b.bytes << "," << b.gap_before_ms << "\n";
		}
		++req_id;
	}
	
	std::cout << "[simnet] Wrote: " << cli.outPrefix << "-requests.csv and "
	<< cli.outPrefix << "-bursts.csv\n";
	return 0;
}
